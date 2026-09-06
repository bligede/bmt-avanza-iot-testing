// =============================================================================
//  Config.h — every tunable for the Avanza bring-up firmware.
//
//  This is a DIAGNOSTIC build. It exists to answer one question:
//
//      Can an ESP32-S3 + SN65HVD230 passively read a real vehicle CAN bus?
//
//  It is not the fleet firmware. There is no modem, no MQTT, no TLS, no offline
//  buffer, no GNSS, no signal decoding. Those live in
//  bligede/bmt-can-bus-telemetry and are deliberately absent here: every
//  subsystem left out is one fewer variable when the answer turns out to be no.
//
//  Credentials live in Secrets.h, which is git-ignored.
// =============================================================================
#pragma once

#define FW_NAME         "Avanza CAN Bring-Up"
#define FW_VERSION      "0.1.0"

// -----------------------------------------------------------------------------
// 1. IDENTITY
// -----------------------------------------------------------------------------
#define UNIT_ID         "AVANZA-TEST-01"

// -----------------------------------------------------------------------------
// 2. PIN MAP — SN65HVD230 transceiver
//    Same pins as the fleet device, so wiring notes transfer directly.
// -----------------------------------------------------------------------------
#define CAN_TX_PIN      5      // SN65HVD230 pin D  (never driven: listen-only)
#define CAN_RX_PIN      4      // SN65HVD230 pin R

#define LED_RED_PIN     21
#define LED_GREEN_PIN   47
#define BUTTON_PIN      0      // RESERVED — status display only

// -----------------------------------------------------------------------------
// 3. CAN
// -----------------------------------------------------------------------------
// 500000 or 250000. Decide on the bench, flash once. Do NOT switch bitrates
// repeatedly while connected to a live vehicle.
#define CAN_DEFAULT_BITRATE     500000

#define CAN_RX_QUEUE_LEN        64
#define CAN_RAWLOG_QUEUE_LEN    256
#define CAN_SILENCE_TIMEOUT_MS  5000

// --- SAFETY ------------------------------------------------------------------
// Listen-only is compiled in, latched at init, and re-asserted on recovery.
// There is deliberately no symbol that disables it, and CanBusSafety.h makes a
// transmit call fail to compile.
//
// Optional belt-and-braces: leave the TWAI TX pin entirely unconfigured so the
// SoC cannot drive the transceiver's D input under any fault. Listen-only never
// needs TX. Pull GPIO5 high at the transceiver if you enable this.
#define CAN_DETACH_TX_PIN       0

// -----------------------------------------------------------------------------
// 4. RAW CAPTURE
// -----------------------------------------------------------------------------
#define RAWLOG_SINK_NONE        0
#define RAWLOG_SINK_SERIAL      1
#define RAWLOG_SINK_FILE        2
#define RAWLOG_SINK_BOTH        3

// This is a capture tool, so capture to flash from the moment it boots. A test
// session that is only realised to be interesting halfway through has still
// been recorded.
#define RAWLOG_DEFAULT_SINK     RAWLOG_SINK_FILE

#define RAWLOG_DIR              "/capture"
#define RAWLOG_MAX_BYTES        (5UL * 1024UL * 1024UL)   // ~5 MB of frames
#define RAWLOG_SEGMENT_BYTES    (256UL * 1024UL)

// -----------------------------------------------------------------------------
// 5. DIAGNOSTIC WiFi + WEB DASHBOARD
//    ON by default — unlike the fleet firmware, this is the whole point here.
// -----------------------------------------------------------------------------
#ifndef ENABLE_WIFI
  #define ENABLE_WIFI           1
#endif
#ifndef ENABLE_WEB_DASHBOARD
  #define ENABLE_WEB_DASHBOARD  1
#endif

#define WIFI_CONNECT_TIMEOUT_MS     20000UL
#define WIFI_RECONNECT_BACKOFF_MS   5000UL
#define WIFI_RECONNECT_BACKOFF_MAX  60000UL

#define WEB_PORT                80
// Live frames held for the dashboard. Each costs ~20 B of RAM and ~60 B of JSON.
#define WEB_FRAME_RING          60
#define WEB_JSON_BUF            10240
#define WEB_POLL_TICK_MS        10

// -----------------------------------------------------------------------------
// 6. TASKS
//    Core 0 does nothing but read CAN. Everything else is on core 1.
// -----------------------------------------------------------------------------
#define CORE_REALTIME           0
#define CORE_COMMS              1

#define TASK_CAN_READER_PRIO    20
#define TASK_CAN_READER_STACK   4096
#define TASK_STORAGE_PRIO       5
#define TASK_STORAGE_STACK      6144
#define TASK_HOUSEKEEPING_PRIO  4
#define TASK_HOUSEKEEPING_STACK 4096
#define TASK_WEB_PRIO           3
#define TASK_WEB_STACK          8192

// -----------------------------------------------------------------------------
// 7. WATCHDOG
// -----------------------------------------------------------------------------
#define WDT_TIMEOUT_MS          10000UL
#define WDT_PANIC_ON_TIMEOUT    1

// -----------------------------------------------------------------------------
// 8. LED / BUTTON
// -----------------------------------------------------------------------------
#define ENABLE_STATUS_LED       1
#define LED_TICK_MS             50
#define BUTTON_DEBOUNCE_MS      50
#define BUTTON_LONG_PRESS_MS    3000

// -----------------------------------------------------------------------------
// 9. LOGGING
// -----------------------------------------------------------------------------
#define LOG_LEVEL_NONE          0
#define LOG_LEVEL_ERROR         1
#define LOG_LEVEL_WARN          2
#define LOG_LEVEL_INFO          3
#define LOG_LEVEL_DEBUG         4

// DEBUG prints every CAN frame to the console. On a busy bus that is more than
// the serial port can carry — use `capture serial` deliberately, not as a
// default. INFO is the sane setting.
#ifndef LOG_LEVEL
  #define LOG_LEVEL             LOG_LEVEL_INFO
#endif

#define LOG_BAUD                115200
#define LOG_LINE_MAX            256

// -----------------------------------------------------------------------------
// 10. CREDENTIALS
// -----------------------------------------------------------------------------
#if __has_include("Secrets.h")
  #include "Secrets.h"
#else
  #warning "Secrets.h not found — copy src/Secrets.h.example to src/Secrets.h. \
The firmware still builds and reads CAN; only WiFi stays down."
#endif

#ifndef WIFI_SSID
  #define WIFI_SSID             ""
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS             ""
#endif

// -----------------------------------------------------------------------------
// 11. BUILD GUARDS
// -----------------------------------------------------------------------------
#if CAN_DEFAULT_BITRATE != 500000 && CAN_DEFAULT_BITRATE != 250000
  #error "CAN_DEFAULT_BITRATE must be 500000 or 250000."
#endif

#if ENABLE_WEB_DASHBOARD && !ENABLE_WIFI
  #error "ENABLE_WEB_DASHBOARD requires ENABLE_WIFI — the dashboard is served \
over the WiFi link."
#endif
