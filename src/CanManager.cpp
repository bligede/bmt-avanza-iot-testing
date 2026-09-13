#include "CanManager.h"

#include <driver/twai.h>
#include "Logger.h"

// MUST come after driver/twai.h: it poisons the transmit entry points, so any
// reference to them below — even a declaration — stops the build.
#include "CanBusSafety.h"

namespace {

const char* TAG = "CAN";

CanState  s_state    = CanState::Uninitialised;
uint32_t  s_bitrate  = CAN_DEFAULT_BITRATE;
bool      s_locked   = false;      // latched: driver installed listen-only
CanStats  s_stats    = {};

// Fase 0 ID survey. 128 distinct IDs comfortably covers a passenger-car bus;
// past that the survey stops growing and says so, rather than dropping frames.
constexpr uint16_t MAX_SEEN_IDS = 128;
//
// The survey is written by the CAN reader task on core 0 and read by the web and
// console tasks on core 1, with no lock. Counts alone could get away with that;
// a payload cannot — a reader copying the eight data bytes while the writer is
// replacing them would show a mixture of two frames, and a mixture looks exactly
// like a signal changing.
//
// So each entry carries a sequence number (a seqlock). The writer makes it odd
// before touching the entry and even again after; a reader copies the entry and
// keeps the copy only if the number was even and unchanged across the copy. The
// writer never waits, which is the property that matters on the bus task.
struct SeenId {
    volatile uint32_t seq;
    uint32_t id;
    uint32_t count;
    uint32_t last_ms;
    uint8_t  dlc;
    bool     extended;
    uint8_t  data[8];
};
SeenId   s_seen[MAX_SEEN_IDS];
uint16_t s_seen_count = 0;
bool     s_seen_full_warned = false;

// Last driver status, taken in poll() on the reader task and copied out under a
// spinlock by driverCounters() on another core.
portMUX_TYPE       s_status_mux = portMUX_INITIALIZER_UNLOCKED;
twai_status_info_t s_status     = {};
uint32_t           s_backlog_peak = 0;

twai_timing_config_t timingFor(uint32_t bitrate) {
    // Only the two bitrates the blueprint sanctions. Config.h refuses anything
    // else at compile time; this is the runtime backstop.
    if (bitrate == 250000) {
        twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
        return t;
    }
    twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
    return t;
}

// The one place a TWAI general config is built. Every caller goes through here,
// so listen-only cannot be lost in a second code path.
twai_general_config_t generalConfig() {
    gpio_num_t tx = static_cast<gpio_num_t>(CAN_TX_PIN);
#if CAN_DETACH_TX_PIN
    // Belt and braces: leave TX entirely unconfigured so the SoC cannot drive
    // the transceiver's D input even under a driver fault.
    tx = static_cast<gpio_num_t>(TWAI_IO_UNUSED);
#endif

    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        tx,
        static_cast<gpio_num_t>(CAN_RX_PIN),
        TWAI_MODE_LISTEN_ONLY);      // <-- the safety property, compiled in

    g.rx_queue_len = CAN_RX_QUEUE_LEN;
    g.tx_queue_len = 0;              // no transmit queue is allocated at all
    g.alerts_enabled = TWAI_ALERT_RX_QUEUE_FULL
                     | TWAI_ALERT_ERR_PASS
                     | TWAI_ALERT_BUS_ERROR
                     | TWAI_ALERT_BUS_OFF
                     | TWAI_ALERT_RX_DATA;
    return g;
}

bool installAndStart(uint32_t bitrate) {
    twai_general_config_t g = generalConfig();

    // Refuse to proceed if the mode word is anything but listen-only. This is
    // unreachable as written; it exists so that a future edit which changes the
    // mode fails at boot on the bench instead of silently on a vehicle.
    if (g.mode != TWAI_MODE_LISTEN_ONLY) {
        LOG_E(TAG, "REFUSING TO START: mode is not LISTEN_ONLY");
        s_state = CanState::InstallFailed;
        return false;
    }

    twai_timing_config_t t = timingFor(bitrate);
    // Accept every ID: filtering happens in software against the signal map, so
    // Fase 0 raw capture can see the whole bus without a rebuild.
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g, &t, &f);
    if (err != ESP_OK) {
        LOG_E(TAG, "twai_driver_install failed: %s", esp_err_to_name(err));
        s_state = CanState::InstallFailed;
        return false;
    }

    err = twai_start();
    if (err != ESP_OK) {
        LOG_E(TAG, "twai_start failed: %s", esp_err_to_name(err));
        twai_driver_uninstall();
        s_state = CanState::InstallFailed;
        return false;
    }

    s_bitrate = bitrate;
    s_locked  = true;
    s_state   = CanState::Running;
    return true;
}

}  // namespace

namespace CanManager {

bool begin(uint32_t bitrate) {
    if (bitrate != 500000 && bitrate != 250000) {
        LOG_W(TAG, "Bitrate %lu not sanctioned by Blueprint §6.2 — using %d",
              (unsigned long)bitrate, CAN_DEFAULT_BITRATE);
        bitrate = CAN_DEFAULT_BITRATE;
    }

    LOG_I(TAG, "Initializing...");

    memset(&s_stats, 0, sizeof(s_stats));
    s_seen_count = 0;
    s_seen_full_warned = false;

    if (!installAndStart(bitrate)) {
        LOG_E(TAG, "Initialization FAILED — the rest of the device keeps running");
        return false;
    }

    // ---- Milestone 1A acceptance evidence -------------------------------
    // These four lines are the acceptance criteria for M1A, locked by the
    // project leader on 2026-08-24 (D-003). A technician pastes them into the
    // phase report as proof. Do not reword them without reopening that
    // decision — see agent-documentation/03-DECISIONS-LOG.md.
    LOG_I(TAG, "Driver initialized");
    LOG_I(TAG, "Bitrate: %lu", (unsigned long)s_bitrate);
#if CAN_DETACH_TX_PIN
    LOG_I(TAG, "Mode: LISTEN_ONLY");
    LOG_I(TAG, "TX: DISABLED (listen-only + GPIO%d detached)", CAN_TX_PIN);
#else
    LOG_I(TAG, "Mode: LISTEN_ONLY");
    LOG_I(TAG, "TX: DISABLED (listen-only)");
#endif
    // ---------------------------------------------------------------------

    LOG_I(TAG, "Pins: TX=GPIO%d RX=GPIO%d, listen-only locked: %s",
          CAN_TX_PIN, CAN_RX_PIN, s_locked ? "YES" : "NO");
    return true;
}

bool receive(CanFrame* out, uint32_t timeout_ms) {
    if (out == nullptr || s_state == CanState::Uninitialised ||
        s_state == CanState::InstallFailed) {
        return false;
    }

    twai_message_t msg;
    if (twai_receive(&msg, pdMS_TO_TICKS(timeout_ms)) != ESP_OK) {
        return false;
    }

    out->id        = msg.identifier;
    out->dlc       = msg.data_length_code > 8 ? 8 : msg.data_length_code;
    out->extended  = msg.extd != 0;
    out->remote    = msg.rtr != 0;
    out->rx_millis = millis();
    memset(out->data, 0, sizeof(out->data));
    if (!out->remote) {
        memcpy(out->data, msg.data, out->dlc);
    }

    ++s_stats.frames_received;
    s_stats.last_frame_ms = out->rx_millis;
    return true;
}

void poll() {
    if (s_state == CanState::Uninitialised || s_state == CanState::InstallFailed) {
        return;
    }

    uint32_t alerts = 0;
    if (twai_read_alerts(&alerts, 0) != ESP_OK) return;

    if (alerts & TWAI_ALERT_RX_QUEUE_FULL) {
        ++s_stats.rx_missed;
        LOG_W(TAG, "RX queue full — frames missed (total %lu)",
              (unsigned long)s_stats.rx_missed);
    }
    if (alerts & TWAI_ALERT_BUS_ERROR) {
        ++s_stats.bus_errors;
        // Common on a wrong bitrate or with the module's 120R terminator left
        // fitted (Blueprint §5.2 note: the bus drops to ~40R and frames break).
        LOG_W(TAG, "Bus error (total %lu) — check bitrate and terminator",
              (unsigned long)s_stats.bus_errors);
    }
    if (alerts & TWAI_ALERT_ERR_PASS) {
        LOG_W(TAG, "Controller error-passive");
        s_state = CanState::BusError;
    }
    if (alerts & TWAI_ALERT_BUS_OFF) {
        // A listen-only controller does not transmit, so it should never reach
        // bus-off. If it does, something is wrong at a level worth shouting about.
        LOG_E(TAG, "BUS OFF in listen-only mode — recovering");
        s_state = CanState::BusError;
        twai_initiate_recovery();   // passive: waits for 128x11 recessive bits
        ++s_stats.recoveries;
    }

    twai_status_info_t st;
    if (twai_get_status_info(&st) == ESP_OK) {
        portENTER_CRITICAL(&s_status_mux);
        s_status = st;
        if (st.msgs_to_rx > s_backlog_peak) s_backlog_peak = st.msgs_to_rx;
        portEXIT_CRITICAL(&s_status_mux);
        if (st.state == TWAI_STATE_RUNNING && s_state == CanState::BusError) {
            LOG_I(TAG, "Bus recovered");
            s_state = CanState::Running;
        } else if (st.state == TWAI_STATE_STOPPED) {
            // Recovery finished; restart. installAndStart() is the only path
            // back in, so listen-only is re-asserted here too.
            LOG_I(TAG, "Restarting after recovery (listen-only re-asserted)");
            if (twai_start() != ESP_OK) {
                twai_driver_uninstall();
                s_locked = false;
                if (installAndStart(s_bitrate)) {
                    LOG_I(TAG, "Driver reinstalled in LISTEN ONLY");
                }
            } else {
                s_state = CanState::Running;
            }
        }
    }
}

CanState state()    { return s_state; }
CanStats stats()    { CanStats s = s_stats; s.unique_ids_seen = s_seen_count; return s; }
bool seenIdOverflow() { return s_seen_full_warned; }
uint32_t bitrate()  { return s_bitrate; }
bool isListenOnlyLocked() { return s_locked; }

uint32_t silenceMs() {
    if (s_stats.frames_received == 0) return UINT32_MAX;
    return millis() - s_stats.last_frame_ms;
}

bool isAlive() {
    return silenceMs() < CAN_SILENCE_TIMEOUT_MS;
}

void noteDecodeQueueDrop() {
    ++s_stats.frames_dropped_queue;
}

void survey(const CanFrame& frame) {
    for (uint16_t i = 0; i < s_seen_count; ++i) {
        SeenId& e = s_seen[i];
        if (e.id != frame.id || e.extended != frame.extended) continue;
        e.seq = e.seq + 1;                 // odd: an update is in progress
        ++e.count;
        e.last_ms = frame.rx_millis;
        e.dlc     = frame.dlc;
        memcpy(e.data, frame.data, sizeof(e.data));
        e.seq = e.seq + 1;                 // even: stable again
        return;
    }
    if (s_seen_count < MAX_SEEN_IDS) {
        // A new entry is filled completely BEFORE the count that makes it
        // visible is raised, so no reader can ever see it half-built.
        SeenId& e = s_seen[s_seen_count];
        e.seq      = 0;
        e.id       = frame.id;
        e.extended = frame.extended;
        e.count    = 1;
        e.last_ms  = frame.rx_millis;
        e.dlc      = frame.dlc;
        memcpy(e.data, frame.data, sizeof(e.data));
        ++s_seen_count;
    } else if (!s_seen_full_warned) {
        // Say so rather than quietly truncating the survey.
        LOG_W(TAG, "ID survey full at %u distinct IDs — later IDs not counted",
              (unsigned)MAX_SEEN_IDS);
        s_seen_full_warned = true;
    }
}

uint16_t seenIdCount() { return s_seen_count; }

bool seenIdSnapshot(uint16_t index, SeenIdView* out) {
    if (out == nullptr || index >= s_seen_count) return false;
    const SeenId& e = s_seen[index];
    auto copy = [&]() {
        out->id       = e.id;
        out->count    = e.count;
        out->last_ms  = e.last_ms;
        out->dlc      = e.dlc;
        out->extended = e.extended;
        memcpy(out->data, e.data, sizeof(out->data));
    };
    copy();                                // a value in hand whatever happens
    for (uint8_t attempt = 0; attempt < 32; ++attempt) {
        const uint32_t before = e.seq;
        if (before & 1u) continue;         // writer mid-update; it takes nanoseconds
        copy();
        if (e.seq == before) return true;
    }
    // Contended thirty-two times running, which a writer holding one entry for a
    // few nanoseconds at under a hundred frames a second does not do. Keep the
    // last copy rather than drop the row: a missing row reads as a truncated
    // table, and that is the worse lie.
    return true;
}

CanDriverCounters driverCounters() {
    CanDriverCounters d = {};
    portENTER_CRITICAL(&s_status_mux);
    d.rx_backlog       = s_status.msgs_to_rx;
    d.rx_backlog_peak  = s_backlog_peak;
    d.rx_missed        = s_status.rx_missed_count;
    d.rx_overrun       = s_status.rx_overrun_count;
    d.bus_errors       = s_status.bus_error_count;
    d.rx_error_counter = s_status.rx_error_counter;
    portEXIT_CRITICAL(&s_status_mux);
    d.rx_queue_len = CAN_RX_QUEUE_LEN;
    return d;
}

void end() {
    if (s_state == CanState::Uninitialised) return;
    twai_stop();
    twai_driver_uninstall();
    s_state = CanState::Stopped;
    LOG_I(TAG, "Stopped");
}

}  // namespace CanManager
