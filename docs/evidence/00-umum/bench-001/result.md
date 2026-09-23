# bench-001 — Firmware bring-up on the bench

**Status:** PASS (firmware only — no CAN bus, no peripherals wired yet)
**Date:** 2026-08-24
**Board:** ESP32-S3 (QFN56) rev v0.2, 16 MB flash, 8 MB embedded PSRAM
**MAC:** 14:c1:9f:cb:2d:98
**Port:** COM6, FTDI FT232R (VID:PID 0403:6001)

## What this run proves

The firmware compiles, flashes, boots, and brings up CAN in listen-only mode on
real silicon. Nothing more — no CAN bus was attached and no peripherals were
wired.

## Setup

| | |
|---|---|
| Toolchain | PlatformIO, espressif32 |
| Build | 0 warnings, 0 errors at `-Wall -Wextra` |
| RAM | 17.8% (58,472 / 327,680 B) |
| Flash | 38.2% (800,081 / 2,097,152 B app partition) |
| LittleFS | 14,272 KB |
| CAN bitrate | 500 kbps |
| Module 120 Ω terminator | not yet checked — bench, no bus attached |

## M1A acceptance evidence (D-003)

Present verbatim in `serial-log.txt`:

```
[CAN] Driver initialized
[CAN] Bitrate: 500000
[CAN] Mode: LISTEN_ONLY
[CAN] TX: DISABLED (listen-only)
```

Plus `listen-only locked: YES`.

## Result at 30 s

```
CAN   RUNNING @ 500 kbps LISTEN-ONLY=LOCKED
      rx=0 drop=0 missed=0 err=0 rec=0 ids=0 silence=no frames yet
CAP   sink=2 frames=0 bytes=0 path=/capture/can-000.log
GPS   fix=NO sats=0 sog=0.0 km/h sentences=0 badcrc=0
TEMP  enclosure=-- fan_sensor=45.0 C fan=AUTO/OFF run=0 s
WIFI  CONNECTING ip=0.0.0.0 rssi=0 dBm  web_requests=0
```

Every one of those is the correct reading for a board with nothing attached:

- `rx=0`, `silence=no frames yet` — no CAN bus connected
- `sentences=0` — GPS module not wired
- `enclosure=--` — DHT22 not wired; the fan correctly **fell back to the SoC die
  sensor** (45.0 °C) rather than guessing. That fallback path is now confirmed
  on hardware.
- `WIFI CONNECTING` — `Secrets.h` still holds the placeholder SSID

Nothing fabricated a value it did not have. That is the behaviour the whole
design is built around.

## Defects found and fixed during this run

1. **Flash configured as 8 MB on a 16 MB chip.** `esptool flash_id` reported
   16 MB; the partition table only spanned half of it, leaving ~6 MB for
   captures instead of ~14 MB. New `avanza_test_16mb.csv`; LittleFS now
   14,272 KB.
2. **GPIO19 pin conflict.** The blueprint pinout puts GPS TX on GPIO19, which
   is `USB_D-` on the ESP32-S3. With the native USB port in use the GPS UART and
   the USB peripheral contend for one pin, and it presents as a broken GPS
   module. `GPS_TX_PIN` is now -1; the firmware only listens to NMEA, so the pin
   was never needed.
3. **Upload failed on the native USB port.** Auto-reset there depends on the
   running firmware cooperating. Switched to the COM port (FTDI bridge, physical
   auto-reset) and `ARDUINO_USB_CDC_ON_BOOT=0`, which also keeps the console
   alive if the firmware crashes.
4. **Monitor filters corrupted the stream.** `time` + `esp32_exception_decoder`
   fragmented the output and repeated one line ~150,000 times, which initially
   looked like a boot loop. Filters removed from the default.
5. **`silence=4294967295 ms`** — the "never received a frame" sentinel printed
   raw. Now prints `no frames yet`.

## Known issues

None outstanding for this stage.

## Next action

Bench CAN reception with a second transmitting node, both 500 and 250 kbps,
with correct bench termination (~60 Ω across CANH–CANL, module terminators
kept). Then GPS and DHT22 wiring. Only then the vehicle.
