// =============================================================================
//  Config.h — every tunable for the Avanza bring-up firmware.
//
//  This is a DIAGNOSTIC build. It exists to answer one question:
//
//      Can an ESP32-S3 + SN65HVD230 passively read a real vehicle CAN bus?
//
//  It is not the fleet firmware. There is no modem, no MQTT, no TLS, no offline
//  buffer and no signal decoding — those live in bligede/bmt-can-bus-telemetry
//  and are deliberately absent, because every subsystem left out is one fewer
//  variable when the answer turns out to be "no frames".
//
//  GNSS, DHT22 and the fan ARE here. They are passive, sit on core 1 at low
//  priority, and touch the CAN path not at all — so the "fewer variables"
//  argument does not apply to them, while each retires an item from the fleet
//  project's NOT TESTED list.
//
//  Credentials live in Secrets.h, which is git-ignored.
// =============================================================================
#pragma once

#define FW_NAME         "Avanza CAN Bring-Up"
#define FW_VERSION      "0.1.0"

// -----------------------------------------------------------------------------
// 1. IDENTITY
// -----------------------------------------------------------------------------
#define UNIT_ID         "HRV-TEST-01"   // the vehicle actually under test

// -----------------------------------------------------------------------------
// 2. PIN MAP — SN65HVD230 transceiver
//    Same pins as the fleet device, so wiring notes transfer directly.
// -----------------------------------------------------------------------------
#define CAN_TX_PIN      5      // SN65HVD230 pin D  (never driven: listen-only)
#define CAN_RX_PIN      4      // SN65HVD230 pin R

// GNSS — GY-GPS6MV2 (u-blox NEO-6M), NMEA 0183
#define GPS_RX_PIN      18     // ESP32 RX  <- GPS TX   (the only one needed)

// ESP32 TX -> GPS RX. -1 = NOT CONNECTED, and that is the right default.
//
// Two reasons. First, this firmware only listens to NMEA; it never configures
// the receiver, so the pin does no work. Second, the blueprint pinout puts it
// on GPIO19 — which on the ESP32-S3 is USB_D- of the native USB peripheral.
// Assigning a UART to it while the native USB port is in use puts two
// peripherals on one pin, and the symptom looks like a broken GPS module
// rather than a pin conflict.
//
// Only set this to 19 if you are certain the native USB port is unused (i.e.
// ARDUINO_USB_CDC_ON_BOOT=0 and you are on the COM port) AND you actually need
// to send configuration to the receiver.
#define GPS_TX_PIN      -1

#define DHT_PIN         15     // DHT22 DATA
#define FAN_PIN         13     // 330R -> IRLZ44N gate

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
// Deep enough to ride out a filesystem stall. At ~1,300 frames/s a 256-slot
// queue overflows after 200 ms of the writer being busy, which is well within
// what LittleFS takes to do wear-levelling bookkeeping. 1024 slots is ~20 KB
// of internal RAM against 17.9% used, and buys 800 ms.
#define CAN_RAWLOG_QUEUE_LEN    1024
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
#define RAWLOG_MAX_BYTES        (12UL * 1024UL * 1024UL)  // ~12 MB, fits the 16 MB layout
#define RAWLOG_SEGMENT_BYTES    (256UL * 1024UL)

// Frames are staged in RAM and written in one block. Two File::print calls per
// frame at 1,300 frames/s is 2,600 trips through the filesystem every second,
// each paying LittleFS bookkeeping. That is what put the capture behind the bus
// and turned the record into a sample.
#define RAWLOG_WRITE_BUF        4096

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
// 128 identifiers plus 60 frames does not fit in 10 KB, and the overflow was
// silent: the identifier table simply stopped early and the page reported the
// truncated row count as the identifier total. See appendIds().
#define WEB_JSON_BUF            16384
#define WEB_POLL_TICK_MS        10

// -----------------------------------------------------------------------------
// 5b. GNSS — GY-GPS6MV2
//
//  Present for one specific reason: Blueprint §9.1 requires a CAN speed
//  candidate to be validated against GNSS ground speed. When a speed field
//  turns up in the Avanza capture, this is the reference that confirms it.
//  It also gives NmeaParser its first run on real silicon.
// -----------------------------------------------------------------------------
#define ENABLE_GPS              1
#define GPS_BAUD                9600
#define GPS_UART_NUM            2
#define GPS_FIX_TIMEOUT_MS      120000UL
#define GPS_STALE_FIX_MS        30000UL
#define GPS_INVALID_AS_NULL     1     // never publish 0,0 as a position

// -----------------------------------------------------------------------------
// 5c. DHT22 + FAN — thermal validation
//
//  The fleet project carries an open TODO: FAN_ON_TEMP and FAN_OFF_TEMP are
//  placeholders because nobody has measured the enclosure in a parked car in
//  Bali sun. This test is the cheapest chance to collect that data, so the
//  DHT22 is on and the fan reads it rather than the SoC die sensor.
//
//  The dashboard shows BOTH temperatures — enclosure and die — because the
//  difference between them is the number the fleet threshold decision needs.
// -----------------------------------------------------------------------------
#define ENABLE_DHT22            1
#define DHT_SAMPLE_INTERVAL_MS  10000UL   // DHT22 minimum period is 2 s

#define ENABLE_FAN              1
#define FAN_MODE_DEFAULT        1         // 0=OFF, 1=AUTO, 2=FORCED_ON
// TODO — VALIDASI TERMAL. Still placeholders. The point of this test is to
// replace them with measured numbers, not to trust them.
#define FAN_ON_TEMP             60.0f
#define FAN_OFF_TEMP            52.0f     // must stay below FAN_ON_TEMP
#define FAN_MIN_STATE_MS        10000UL   // anti-chatter dwell
// 0 = prefer the DHT22 (enclosure air), 1 = SoC die sensor.
// Enclosure air is what the fleet threshold is actually about. Falls back to
// the die sensor automatically if the DHT22 does not answer.
#define FAN_SENSOR_INTERNAL     0

// -----------------------------------------------------------------------------
// 6. TASKS
//    Core 0 does nothing but read CAN. Everything else is on core 1.
// -----------------------------------------------------------------------------
#define CORE_REALTIME           0
#define CORE_COMMS              1

#define TASK_CAN_READER_PRIO    20
#define TASK_CAN_READER_STACK   4096

// GNSS above storage: a dropped NMEA byte is unrecoverable — the UART buffer
// overruns and the sentence is lost — whereas a delayed capture write is not.
#define TASK_GPS_PRIO           8
#define TASK_GPS_STACK          4096
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
