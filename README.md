# bmt-avanza-iot-testing

Bring-up firmware for the **Avanza CAN reading test**. ESP32-S3 + SN65HVD230,
WiFi hotspot, live web dashboard. Built with **PlatformIO**.

Fleet firmware lives in
[bligede/bmt-can-bus-telemetry](https://github.com/bligede/bmt-can-bus-telemetry).
This is a separate, focused diagnostic tool — not a subset of the product.

---

## The one rule

**The device is listen-only, permanently.** It reads the vehicle CAN bus and
never writes to it — no data frames, no remote frames, no ACK, no error frames.

Enforced in three independent places:

| Layer | Mechanism |
|---|---|
| Controller | `TWAI_MODE_LISTEN_ONLY`; `CanManager` refuses to start in any other mode |
| Compiler | `#pragma GCC poison` on the transmit entry points — a call fails to build |
| Release gate | `sh tools/check_listen_only.sh` |

Run the gate before any build that goes near a vehicle.

---

## What this answers, and what it does not

| Question | |
|---|---|
| Can the device passively read a real vehicle CAN bus? | **this is the point** |
| Does listen-only hold on a live vehicle? | yes |
| Does WiFi hurt CAN acquisition? | measured — see below |
| Does the GNSS module work, and NmeaParser with it? | yes — first run on hardware |
| How hot does the enclosure actually get? | yes — the fleet's open thermal TODO |
| Are the fleet's odometer / SoC / speed CAN IDs found? | **no.** Wrong vehicle |
| Does the 4G + MQTT + TLS uplink work? | **no.** Not present at all |

Three modules get their first hardware run here, all of them on the fleet
project's *NOT TESTED* list: **`NmeaParser`** (21 unit tests, zero executions),
the **bit-banged DHT22 driver** (written from the datasheet, never run), and the
**fan hysteresis**. Retiring them costs nothing extra on this trip.

The thermal panel exists for a specific open question: `FAN_ON_TEMP` and
`FAN_OFF_TEMP` in the fleet firmware are placeholders marked *TODO — VALIDASI
TERMAL*, because nobody has measured an enclosure in a parked car in Bali sun.
The dashboard shows **both** the DHT22 enclosure reading and the SoC die
temperature; the gap between them is the number that decision needs.

> Avanza CAN IDs prove the **reading path**. They are **not** a signal map.
> Nothing found here may be copied into the fleet's `signals.cfg` — Toyota and
> Wuling identifiers have no relationship, and a plausible-looking wrong number
> is the worst failure mode a fleet dataset can have.

An Avanza is a petrol car: **there is no state of charge**, and unlike the fleet
EV it does have standard OBD-II PIDs. Neither fact helps the fleet; both are
recorded so nobody draws the wrong conclusion from an easy result here.

---

## Quick start

### 1. Open in VS Code

Install the **PlatformIO IDE** extension, then `File → Open Folder` on this
repository. PlatformIO reads `platformio.ini` and installs the ESP32 platform on
first build. No libraries to add — this project has **zero third-party
dependencies**.

### 2. Credentials

```sh
cp src/Secrets.h.example src/Secrets.h
```

Fill in the hotspot SSID and password. `src/Secrets.h` is git-ignored.

> **The ESP32-S3 radio is 2.4 GHz only.** A 5 GHz hotspot will never be found,
> and the symptom is an endless `CONNECTING → IDLE` loop. Android: *Hotspot
> settings → AP Band → 2.4 GHz*. iPhone: enable *Maximise Compatibility*.

### 3. Check the board

`platformio.ini` assumes an **8 MB** `esp32-s3-devkitc-1`. For a 4 MB board,
change two lines:

```ini
board_build.partitions  = partitions/avanza_test_4mb.csv
board_upload.flash_size = 4MB
```

### 4. Build, upload, monitor

| | PlatformIO toolbar | Command line |
|---|---|---|
| Build | ✓ | `pio run` |
| Upload | → | `pio run -t upload` |
| Monitor | 🔌 | `pio device monitor` |

If upload fails: hold **BOOT** (GPIO0), tap **RESET**, release BOOT, retry.

### 5. Verify before going near a vehicle

```sh
sh tools/check_listen_only.sh
```

Must print `LISTEN-ONLY GATE: PASS`. Do not flash a build that fails it.

---

## Wiring

| Function | Pin | Device |
|---|---|---|
| CAN TX | GPIO5 | SN65HVD230 `D` — never driven in listen-only |
| CAN RX | GPIO4 | SN65HVD230 `R` |
| GPS RX | GPIO18 | ← GY-GPS6MV2 TX |
| GPS TX | — | **not connected** — GPIO19 is USB_D− on the ESP32-S3. Firmware only listens to NMEA |
| DHT22 | GPIO15 | DATA |
| Fan | GPIO13 | → 330 Ω → IRLZ44N gate |
| LED red | GPIO21 | via resistor |
| LED green | GPIO47 | via resistor |
| Button | GPIO0 | momentary, active low — **RESERVED**, prints status |

The SN65HVD230 also needs 3V3 and GND from the ESP32.

Everything except CAN is optional. A missing GPS, an unwired DHT22 or no fan
clears its own reading and touches nothing else — CAN acquisition is unaffected.

| OBD-II | To |
|---|---|
| pin 6 | CANH |
| pin 14 | CANL |
| pin 5 | ESP32 GND |
| **pin 16** | **nothing** — power the ESP32 from USB |

LED common cathode to GND.

### The 120 Ω terminator — staged, not removed once

A CAN bus needs 120 Ω at each end, about **60 Ω across CANH–CANL**. Where those
come from changes between bench and vehicle:

| Stage | Termination source | Module's 120 Ω |
|---|---|---|
| **Bench**, two nodes | The two nodes *are* the two ends | **KEEP FITTED** |
| **Vehicle** | The car is already terminated at ~60 Ω | **REMOVE** |

Being listen-only does not exempt the receiver from being a properly terminated
node. Measure ~60 Ω across the bus in **both** cases before powering anything.

---

## Before connecting to the car

Ignition **off**, key removed. Measure OBD-II pin 6 to pin 14:

| Reading | Meaning | Action |
|---|---|---|
| ~60 Ω | CAN present, terminated | proceed |
| ~120 Ω | one terminator | record, proceed |
| **open** | **no CAN on these pins** | **stop** — see below |

An **Avanza Gen 1 (2003–2011)** may use K-line on pins 7/15 and have no CAN on
6/14 at all. The resistance check settles it in thirty seconds; use a different
vehicle rather than debugging firmware that is behaving correctly.

Record the model year.

---

## The failure that wastes an afternoon

**A healthy car can produce zero frames.**

On many vehicles the OBD-II port exposes a *diagnostic* CAN channel that stays
silent until a scan tool sends a request. This device is listen-only and never
sends anything. So a perfectly good Avanza can give you:

```
resistance 6–14 : 60 Ω     ✓ bus present
frames received : 0        ✗
```

That is not a broken device, a wrong bitrate, or bad wiring — it may simply be a
gateway that does not broadcast to the port.

**Prove the device on the bench first**, with a second CAN node transmitting.
Then "no frames" is a fact about the car rather than a mystery about the tool.
The bench test is not optional; it is what makes the vehicle result readable.

---

## Using it

Power up, open the serial monitor at 115200. Once WiFi connects:

```
[WIFI] Connected. IP 192.168.43.101, RSSI -52 dBm
[WIFI] Dashboard: http://192.168.43.101/
```

Open that URL on the phone.

### LED indicators

800 ms cycle. Highest-priority state wins.

**Red means something is broken. Red does NOT mean "waiting".**

| Pattern | Meaning | What to do |
|---|---|---|
| 🔴🟢 alternating | booting | wait ~0.5 s |
| 🔴 **solid** | filesystem dead | LittleFS would not mount |
| 🔴 **fast blink** | **CAN driver failed** | wiring, or `twai_driver_install` failed — check the log |
| 🟢 **heartbeat** (2 short flashes) | **CAN frames arriving** — the goal | nothing, this is success |
| 🟢 steady with a **brief red wink** | GPS is talking but has no fix yet | wait, or get sky view |
| 🟢 **fast blink** | WiFi connecting | wait, or check SSID/2.4 GHz |
| 🟢 **slow blink** | WiFi up, **no CAN frames yet** | normal on a bench with no bus attached |

A bench board with WiFi up and nothing on CANH/CANL shows the **slow green
blink**. That is the correct idle state, not a fault.

### Dashboard



| Panel | What matters |
|---|---|
| **CAN bus** | `frames received` climbing, `LISTEN-ONLY LOCKED` green, live frames/s |
| **Distinct CAN IDs** | the headline result — tens of IDs means the bus talks to us |
| **Live frames** | bytes changing as you drive = real data, not one stuck frame |
| **GNSS** | fix, position, satellites, **ground speed** — the speed reference |
| **Thermal** | DHT22 enclosure temp *and* SoC die temp, fan state |
| **Capture** | frames and bytes written to flash |
| **System** | heap, WiFi, uptime |

Watch **`rx missed`** and **`dropped`**. Both should stay **0**. See *Known
risk* below.

### Console

```
help              command list
status            full report
ids               distinct CAN IDs with counts
gps               GNSS fix, ground speed, UTC
env               DHT22 temperature and humidity
fan               fan state and thresholds
fan auto|on|off   fan mode
wifi              WiFi state and dashboard URL
capture file      raw frames to flash (default, on from boot)
capture off
ls                list capture files
cat <path>        print a capture — this is how you pull it off
clearcaptures
restart
```

Capture starts automatically at boot, so a session that only turns out to be
interesting halfway through has still been recorded.

### Pulling and analysing a capture

```
capture off
ls
cat /capture/can-000.log
```

Save the console output to `captures/`, then:

```sh
python tools/can_find_value.py captures/avanza-....log --value 40 \
    --name speed --tolerance 1 --no-stable
```

The tool searches for a value you read off the dashboard across every plausible
field position, width, endianness and scale. **A candidate is a hypothesis, not
a mapping** — and on an Avanza it is a hypothesis about a Toyota, which the
fleet does not run.

---

## Known risk: WiFi versus CAN acquisition

Disclosed because it is real and unmeasured.

The ESP-IDF WiFi driver runs its own tasks at a **higher priority than the CAN
reader**, and depending on the core build may schedule them on **core 0** — the
core reserved for acquisition.

Mitigations in place: the TWAI hardware FIFO plus a 64-deep driver queue absorb
short preemption; the dashboard is the lowest-priority task on core 1; frames
reach it through a queue on core 1, so the reader only does one non-blocking
enqueue.

**None of that has been measured.** Treat it as an acceptance criterion:

- [ ] `rx missed` stays 0 with the dashboard being polled
- [ ] `dropped` stays 0
- [ ] a 2-minute window with the dashboard open matches 2 minutes with it closed

If those counters move, record it. That finding matters more than a clean pass.

---

## Layout

```
platformio.ini            build config
src/
  main.cpp                setup + FreeRTOS tasks
  Config.h                every tunable
  Secrets.h.example       copy to Secrets.h
  CanBusSafety.h          the compile-time transmit ban
  CanManager.*            TWAI receive, listen-only locked
  GpsManager.* NmeaParser.*   GNSS fix, ground speed, UTC
  EnvironmentManager.*    DHT22 temperature and humidity
  FanManager.*            fan with hysteresis and dwell
  ButtonManager.*         GPIO0, reserved
  RawCanLogger.*          capture to serial and/or LittleFS
  WifiManager.*           non-blocking WiFi state machine
  WebDashboard.*          read-only HTTP dashboard
  SerialConsole.*         technician console
  StatusLed.*             non-blocking LED patterns
  WatchdogManager.*       10 s task watchdog
  Logger.*                levelled console
partitions/               8 MB and 4 MB flash layouts
tools/
  check_listen_only.sh    release gate
  can_find_value.py       known-value search over a capture
captures/                 raw logs (git-ignored)
docs/evidence/            test artifacts
```

`CanManager`, `CanBusSafety`, `RawCanLogger`, `Logger`, `StatusLed`,
`WatchdogManager` and `WifiManager` are copied from
`bmt-can-bus-telemetry@7586b27` **unmodified**, so the safety behaviour is
identical to the fleet firmware. Keep them that way; if one needs changing, fix
it there and re-copy, so the two do not drift.

---

## Evidence

Raw evidence is immutable after a test. `serial-log.txt` is the primary source;
`result.md` carries the interpretation and may be revised.

Save the **whole** serial session, from the first boot line — not just the
interesting part. Reset reason, init order, warnings, heap and startup timing
are what answer "was our hardware ever healthy?" when a later test goes wrong.

Record failures too. A folder saying FAIL with the log attached is worth more
than a missing one.

---

## Legal boundary

This device is **not** a legal measuring instrument. Its output must never be
used to calculate passenger fares.

## Classification

Internal — PT Bali Mikro Teknologi.
