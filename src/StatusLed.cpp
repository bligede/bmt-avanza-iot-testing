#include "StatusLed.h"
#include "Logger.h"

namespace {

// A pattern is a 16-slot bitmask per colour, played at LED_TICK_MS per slot.
// One full cycle is 16 * 50 ms = 800 ms.
struct Pattern {
    uint16_t red;
    uint16_t green;
};

// clang-format off
const Pattern PATTERNS[static_cast<int>(LedStatus::COUNT)] = {
    /* Boot            */ {0b1010101010101010, 0b0101010101010101}, // alternating
    /* MqttOk          */ {0b0000000000000000, 0b1111111111111111}, // solid green
    /* NetworkOk       */ {0b0000000000000000, 0b1111111100000000}, // slow green blink
    /* ModemConnecting */ {0b0000000000000000, 0b1100110011001100}, // fast green blink
    /* GpsNoFix        */ {0b0000000100000001, 0b1111111011111110}, // green + red wink
    /* Buffering       */ {0b1111000000000000, 0b0000111111111111}, // green with red pulse
    /* CanOk           */ {0b0000000000000000, 0b1000000010000000}, // green heartbeat
    /* CanError        */ {0b1100110011001100, 0b0000000000000000}, // fast red blink
    /* SystemError     */ {0b1111111111111111, 0b0000000000000000}, // solid red
};
// clang-format on

bool     s_active[static_cast<int>(LedStatus::COUNT)] = {false};
uint8_t  s_slot    = 0;
uint32_t s_last_ms = 0;
LedStatus s_current = LedStatus::Boot;

LedStatus highestActive() {
    for (int i = static_cast<int>(LedStatus::COUNT) - 1; i >= 0; --i) {
        if (s_active[i]) return static_cast<LedStatus>(i);
    }
    return LedStatus::Boot;
}

}  // namespace

namespace StatusLed {

void begin() {
#if ENABLE_STATUS_LED
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_GREEN_PIN, LOW);
    memset(s_active, 0, sizeof(s_active));
    s_active[static_cast<int>(LedStatus::Boot)] = true;
    s_current = LedStatus::Boot;
    s_slot    = 0;
    s_last_ms = millis();
    LOG_I("LED", "Ready (RED=GPIO%d GREEN=GPIO%d)", LED_RED_PIN, LED_GREEN_PIN);
#endif
}

void set(LedStatus status, bool active) {
#if ENABLE_STATUS_LED
    const int i = static_cast<int>(status);
    if (i < 0 || i >= static_cast<int>(LedStatus::COUNT)) return;
    if (s_active[i] == active) return;
    s_active[i] = active;

    const LedStatus next = highestActive();
    if (next != s_current) {
        s_current = next;
        s_slot    = 0;     // restart the pattern so the change is visible
    }
#else
    (void)status; (void)active;
#endif
}

void tick() {
#if ENABLE_STATUS_LED
    const uint32_t now = millis();
    if (now - s_last_ms < LED_TICK_MS) return;
    s_last_ms = now;

    const Pattern& p = PATTERNS[static_cast<int>(s_current)];
    const uint16_t mask = static_cast<uint16_t>(1u << s_slot);

    digitalWrite(LED_RED_PIN,   (p.red   & mask) ? HIGH : LOW);
    digitalWrite(LED_GREEN_PIN, (p.green & mask) ? HIGH : LOW);

    s_slot = static_cast<uint8_t>((s_slot + 1) & 0x0F);
#endif
}

LedStatus current() { return s_current; }

const char* statusName(LedStatus s) {
    switch (s) {
        case LedStatus::Boot:            return "BOOT";
        case LedStatus::MqttOk:          return "MQTT_OK";
        case LedStatus::NetworkOk:       return "NETWORK_OK";
        case LedStatus::ModemConnecting: return "MODEM_CONNECTING";
        case LedStatus::GpsNoFix:        return "GPS_NO_FIX";
        case LedStatus::Buffering:       return "BUFFERING";
        case LedStatus::CanOk:           return "CAN_OK";
        case LedStatus::CanError:        return "CAN_ERROR";
        case LedStatus::SystemError:     return "SYSTEM_ERROR";
        default:                         return "?";
    }
}

void allOff() {
#if ENABLE_STATUS_LED
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_GREEN_PIN, LOW);
#endif
}

}  // namespace StatusLed
