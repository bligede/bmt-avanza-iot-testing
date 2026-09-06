// =============================================================================
//  Avanza CAN Bring-Up — PT Bali Mikro Teknologi
//
//  Diagnostic firmware for milestone M1C-ALT: prove that an ESP32-S3 with an
//  SN65HVD230 can passively read a real vehicle CAN bus, and show it live on a
//  phone over a WiFi hotspot.
//
//  ---------------------------------------------------------------------------
//  THE DEVICE IS LISTEN-ONLY, PERMANENTLY.
//
//  It reads the vehicle CAN bus and never writes to it — no data frames, no
//  remote frames, no ACK, no error frames. Enforced in three places:
//
//    1. the TWAI driver is installed with TWAI_MODE_LISTEN_ONLY and CanManager
//       refuses to start in any other mode;
//    2. CanBusSafety.h poisons the transmit entry points, so a call fails to
//       compile;
//    3. tools/check_listen_only.sh greps for both before a release build.
//  ---------------------------------------------------------------------------
//
//  WHAT THIS FIRMWARE IS NOT: it is not the fleet firmware. There is no modem,
//  no MQTT, no TLS, no offline buffer, no GNSS, and no signal decoding. Those
//  live in bligede/bmt-can-bus-telemetry. Every subsystem left out here is one
//  fewer variable when the answer turns out to be "no frames".
//
//  Avanza CAN IDs discovered with this tool prove the READING PATH. They are
//  not a signal map for the fleet vehicle and must never be treated as one.
//
//  Build with PlatformIO — see README.md.
// =============================================================================

#include <Arduino.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "Config.h"
#include "Logger.h"
#include "CanManager.h"
#include "RawCanLogger.h"
#include "WifiManager.h"
#include "WebDashboard.h"
#include "SerialConsole.h"
#include "StatusLed.h"
#include "WatchdogManager.h"

static QueueHandle_t q_rawlog = nullptr;

// =============================================================================
//  CORE 0 — the only thing that runs there: read CAN.
//
//  It performs no filesystem I/O, no network I/O, and holds no lock that a
//  core 1 task can take. If everything on core 1 stalled, this would keep
//  receiving until the watchdog fired.
// =============================================================================
static void taskCanReader(void*) {
    WatchdogManager::subscribeCurrentTask("can_reader");
    LOG_I("CAN", "Reader task running on core %d", xPortGetCoreID());

    CanFrame frame;
    uint32_t queue_full_drops = 0;

    for (;;) {
        WatchdogManager::feed();
        CanManager::poll();

        // The 50 ms timeout keeps this task responsive to the watchdog on a
        // silent bus. It is not a poll interval — a frame wakes the task at
        // once.
        if (!CanManager::receive(&frame, 50)) continue;

        CanManager::noteId(frame.id);

        // Best-effort hand-off to core 1. Losing a line from the capture file
        // is acceptable; the bus itself is unaffected and `rx` on the dashboard
        // still counts every frame received.
        if (q_rawlog != nullptr && xQueueSend(q_rawlog, &frame, 0) != pdTRUE) {
            if ((++queue_full_drops % 500) == 1) {
                LOG_W("CAN", "Log queue full — %lu frames not written to the "
                             "capture (bus reception is unaffected)",
                      (unsigned long)queue_full_drops);
            }
        }
    }
}

// =============================================================================
//  CORE 1 — storage: capture writes and the dashboard's frame ring.
// =============================================================================
static void taskStorage(void*) {
    WatchdogManager::subscribeCurrentTask("storage");
    LOG_I("RAWLOG", "Storage task running on core %d", xPortGetCoreID());

    CanFrame frame;
    for (;;) {
        WatchdogManager::feed();
        if (xQueueReceive(q_rawlog, &frame, pdMS_TO_TICKS(200)) == pdTRUE) {
            RawCanLogger::write(frame);
            WebDashboard::noteFrame(frame);
        }
    }
}

// =============================================================================
//  CORE 1 — housekeeping: console, LED, WiFi state machine, periodic report.
// =============================================================================
static void taskHousekeeping(void*) {
    WatchdogManager::subscribeCurrentTask("housekeeping");
    LOG_I("SYSTEM", "Housekeeping task running on core %d", xPortGetCoreID());

    uint32_t last_report = 0;

    for (;;) {
        WatchdogManager::feed();
        vTaskDelay(pdMS_TO_TICKS(LED_TICK_MS));

        StatusLed::tick();
        SerialConsole::poll();
        WifiManager::poll();

        const bool can_up = CanManager::state() == CanState::Running;
        const bool alive  = CanManager::isAlive();
        StatusLed::set(LedStatus::Boot, false);
        StatusLed::set(LedStatus::CanOk, can_up && alive);
        StatusLed::set(LedStatus::CanError, !can_up || !alive);

        const uint32_t now = millis();
        if (now - last_report >= 30000) {
            last_report = now;
            SerialConsole::printStatus();
        }
    }
}

// =============================================================================
//  CORE 1 — the dashboard, at the lowest priority. A technician refreshing a
//  page must never delay anything else.
// =============================================================================
#if ENABLE_WEB_DASHBOARD
static void taskWeb(void*) {
    WatchdogManager::subscribeCurrentTask("web");
    LOG_I("WEB", "Dashboard task running on core %d", xPortGetCoreID());

    for (;;) {
        WatchdogManager::feed();
        WebDashboard::poll();
        vTaskDelay(pdMS_TO_TICKS(WEB_POLL_TICK_MS));
    }
}
#endif

// =============================================================================
//  setup
// =============================================================================
void setup() {
    Logger::begin(LOG_BAUD);

    LOG_I("BOOT", "%s %s", FW_NAME, FW_VERSION);
    LOG_I("BOOT", "Chip %s rev %d, %d MHz, %lu KB flash",
          ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz(),
          (unsigned long)(ESP.getFlashChipSize() / 1024));
    LOG_I("BOOT", "Heap %lu B", (unsigned long)ESP.getFreeHeap());
    LOG_I("BOOT", "----------------------------------------------------");
    LOG_I("BOOT", "CAN MODE: LISTEN ONLY (permanent, enforced at build time)");
    LOG_I("BOOT", "This device never transmits to the vehicle bus.");
    LOG_I("BOOT", "----------------------------------------------------");
    LOG_W("BOOT", "DIAGNOSTIC FIRMWARE — bring-up only.");
    LOG_W("BOOT", "Not the fleet firmware. No modem, no MQTT, no decoding.");
    LOG_W("BOOT", "----------------------------------------------------");

    StatusLed::begin();
    SerialConsole::begin();

    // --- Filesystem ---------------------------------------------------------
    // Mounted before CAN so the capture can start with the very first frame.
    LOG_I("BOOT", "Mounting LittleFS...");
    const bool fsOk = LittleFS.begin(true);   // true = format if the mount fails
    if (fsOk) {
        LOG_I("BOOT", "LittleFS %lu KB total, %lu KB used",
              (unsigned long)(LittleFS.totalBytes() / 1024),
              (unsigned long)(LittleFS.usedBytes() / 1024));
    } else {
        LOG_E("BOOT", "LittleFS mount FAILED — file capture unavailable. "
                      "CAN reading continues.");
        StatusLed::set(LedStatus::SystemError, true);
    }

    // --- CAN, locked in listen-only ----------------------------------------
    const bool canOk = CanManager::begin(CAN_DEFAULT_BITRATE);
    if (!canOk) {
        LOG_E("BOOT", "CAN did not start — check wiring, bitrate, and whether "
                      "the transceiver module's 120R terminator is correct for "
                      "this stage (bench: keep it; vehicle: remove it)");
        StatusLed::set(LedStatus::CanError, true);
    }

    RawCanLogger::begin(fsOk ? RAWLOG_DEFAULT_SINK : RAWLOG_SINK_NONE);

    // --- Queue + core 0 task, as early as possible --------------------------
    q_rawlog = xQueueCreate(CAN_RAWLOG_QUEUE_LEN, sizeof(CanFrame));
    if (q_rawlog == nullptr) {
        LOG_E("BOOT", "Cannot allocate the log queue — restarting");
        delay(1000);
        ESP.restart();
    }

    xTaskCreatePinnedToCore(taskCanReader, "can_reader", TASK_CAN_READER_STACK,
                            nullptr, TASK_CAN_READER_PRIO, nullptr, CORE_REALTIME);
    LOG_I("BOOT", "CAN acquisition is running. Everything below is optional to it.");

    // --- WiFi + dashboard ---------------------------------------------------
    // Started after CAN so the radio never competes with driver installation,
    // and so a WiFi failure can never delay acquisition.
    WifiManager::begin();
#if ENABLE_WEB_DASHBOARD
    WebDashboard::begin();
#endif

    // --- Core 1 tasks -------------------------------------------------------
    xTaskCreatePinnedToCore(taskStorage, "storage", TASK_STORAGE_STACK,
                            nullptr, TASK_STORAGE_PRIO, nullptr, CORE_COMMS);
    xTaskCreatePinnedToCore(taskHousekeeping, "housekeeping",
                            TASK_HOUSEKEEPING_STACK, nullptr,
                            TASK_HOUSEKEEPING_PRIO, nullptr, CORE_COMMS);
#if ENABLE_WEB_DASHBOARD
    xTaskCreatePinnedToCore(taskWeb, "web", TASK_WEB_STACK,
                            nullptr, TASK_WEB_PRIO, nullptr, CORE_COMMS);
#endif

    WatchdogManager::begin();

    LOG_I("SYSTEM", "READY");
    LOG_I("SYSTEM", "CAN=%s fs=%s capture=%s",
          canOk ? "OK" : "FAIL", fsOk ? "OK" : "FAIL",
          RawCanLogger::isCapturing() ? "ON" : "OFF");
    LOG_I("SYSTEM", "Type 'help' on this console. Dashboard URL appears once "
                    "WiFi connects.");
}

// =============================================================================
//  loop — deliberately empty.
//
//  Every responsibility belongs to a pinned FreeRTOS task with an explicit core
//  and priority. Work placed here would run at priority 1 on core 1 with no
//  watchdog supervision.
// =============================================================================
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
