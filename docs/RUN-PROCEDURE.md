# Capture run procedure — CAN bring-up

Applies to the diagnostic firmware in this repository, listen-only, on a vehicle
whose CAN bus is confirmed present at OBD-II pins 6 and 14 (60 Ω across them).

Written after the first successful bus contact on a Honda HR-V 2023,
8 September 2026: 40 identifiers, 900–1,282 frames/s, 0 dropped, 0 missed.

---

## 1. Why this is scripted, and not just a drive

A recording of somebody driving normally is close to useless. Forty identifiers
change at once, every byte moves, and nothing in the file says which change
belongs to which cause. You end up with a large correlated mess.

What makes a capture analysable is the opposite: **one thing changes, everything
else stays still, and you know exactly when.** Blueprint §9.1's known-value
method needs a value you can state with certainty and a window in which it was
true. The whole procedure below exists to manufacture those windows.

Three rules follow from that, and they matter more than the phase list:

1. **Separate every action with about ten seconds of doing nothing.** The quiet
   gap is what makes the changed bytes stand out from the counters.
2. **Hold values, do not sweep through them.** A speed held at 40 km/h for ten
   seconds is trivial to find. A speed sweeping past 40 km/h is not.
3. **Say what you are doing, out loud, on camera.** The video is the annotation
   track. Without it the capture is unlabelled data.

---

## 2. Clear these before the run

The run is wasted if any of these is still open.

- [ ] **Capture writer keeps up.** The Capture panel must NOT show *"Written is
      behind received"*, and `Dropped before writing` must read 0. Fixed on
      9 Sep 2026 (frames staged in a 4 KB buffer, queue 256 -> 1024), but
      **verify it on the actual bus before trusting it** — the fix has not yet
      met 1,300 frames/s in a vehicle.
- [ ] **`UNIT_ID` names the actual vehicle.** Now `HRV-TEST-01` in `src/Config.h`.
      It is written into the capture header, so it must match the vehicle in front
      of you before the run starts, never after (D-015).
- [ ] **Flash storage is empty**, or the existing captures are already pulled off
      and archived. `RAWLOG_MAX_BYTES` is 12 MB.
- [ ] **Vehicle change is logged** in `03-DECISIONS-LOG.md` and accepted by the
      Project Leader. The Avanza 2009 has no CAN at OBD-II (pins 6↔14 measured
      open, drifting ~22.5 kΩ); the test vehicle is now a Honda HR-V 2023.
- [ ] **Battery/power for the device** is sorted for the full run length.

### GNSS is currently unavailable

The module streams valid NMEA but hears no satellites — `Sky: in view / heard`
reads `0 / 0`. That is the antenna, and it means **the vehicle's own speed
signal has no independent reference on board**, which is exactly what §9.1 asks
for.

Substitute for it, do not skip it:

- Run a **GPS logger app on a second phone**, exporting timestamped speed.
- And/or **film the instrument cluster**, which gives speed, tachometer,
  odometer and fuel on one synchronised track.

Filming the cluster is the stronger of the two and is required regardless.

**And the video is now machine-readable.** The `cansub-reverse-engineering` skill
installed on 9 Sep 2026 has a VISION mode that OCRs an instrument-cluster video into a
reference series, then correlates it against the CAN log to find the ID, start bit,
length, endianness, scale and offset. That turns the cluster video from an annotation
track a human reads into the reference signal the search runs against — which matters
more than usual while the GNSS antenna is dead. Frame the cluster so the speedometer
digits are legible and stay in frame; a shaky or glare-washed video costs the whole run.

---

## 3. People, and why two are needed

| Role | Does |
|---|---|
| **Driver** | Drives. Nothing else. Does not touch the phone. |
| **Operator** | Films the cluster, calls each action aloud, watches the dashboard, keeps the run sheet. |

Do not attempt this alone. Phases A–D can be done stationary by one person;
phase E cannot.

**Equipment:** device powered and connected to the phone hotspot; dashboard open
on the operator's phone; a second phone filming the instrument cluster; a written
run sheet; a pen.

---

## 4. The sync marker — do this first, every run

The capture timestamps are `millis()` since boot. The video is wall clock. They
have to be tied together or the annotation cannot be applied.

**With the camera already rolling and pointed at the cluster:**

> Press the brake pedal five times, fast and distinctly.

That produces an unmistakable five-pulse signature in the data and an
unmistakable five-flash of the brake lamp on the video. It is the clapperboard.
Every later timestamp is measured from it.

Say the wall-clock time aloud as you do it.

---

## 5. Phases

Announce each action aloud before doing it. Wait the stated gap afterwards.

### A — Resting baseline. Ignition ON, engine OFF. 60 s.

Do **absolutely nothing**. No touching anything.

This is the most valuable minute of the run and the easiest to skip. Everything
that changes here changes on its own: heartbeats, rolling counters, alive
signals. Later, anything that moved during phase A can be **ruled out** as a
candidate for a signal you care about. Without this you will chase counters for
hours.

### B — Isolated discrete actions. Engine OFF. 10 s gap after each.

Each of these flips one thing, and is trivial to spot in a diff:

1. Steering wheel fully left → return to centre
2. Steering wheel fully right → return to centre
3. Brake pedal: three distinct presses
4. Headlights on → off
5. Left indicator on → off
6. Right indicator on → off
7. Driver door open → close
8. Driver seatbelt unbuckle → buckle
9. Parking brake engage → release

### C — Engine start. Idle, stationary. 60 s.

Start the engine and leave it idling, untouched.

Expect new identifiers to appear and the identifier count to rise. Note the new
count on the run sheet.

### D — Throttle without motion. Transmission in P or N. 10 s gap after each.

1. Raise to ~2,000 rpm, hold 5 s, release
2. Raise to ~3,000 rpm, hold 5 s, release
3. Raise to ~4,000 rpm, hold 5 s, release

Read the tachometer aloud at each hold. RPM is an analogue value you can state
precisely, the vehicle is not moving, and almost nothing else is changing — the
best conditions in the whole run for solving a 16-bit scaled field.

### E — Speed plateaus. On a quiet road. This is the core of the run.

Hold each speed steady for a full **10 seconds** and call it aloud.

| Step | Target | Hold |
|---|---|---|
| 1 | 20 km/h | 10 s |
| 2 | 0 (stopped) | 10 s |
| 3 | 40 km/h | 10 s |
| 4 | 0 (stopped) | 10 s |
| 5 | 60 km/h | 10 s |
| 6 | 0 (stopped) | 10 s |

Three separate plateaus is deliberate. Two known points are the minimum needed
to solve a candidate for both scale and offset; the third one checks the answer
rather than fitting it. Stopping fully between them gives a clean zero, which
pins the offset directly.

If the road allows only lower speeds, use 20 / 30 / 40. Spacing matters more
than the absolute numbers.

### F — Odometer leg.

1. Photograph the odometer. Read it aloud.
2. Drive a measured distance — 2 km is what §9.1 suggests.
3. Stop. Photograph the odometer again. Read it aloud.

The before and after readings are two known values of the same field, taken far
enough apart that no counter can imitate the difference.

### G — Close out.

1. Engine off, ignition off, camera still rolling for 10 s.
2. Stop the video.
3. Note the final frame count, identifier count, dropped, missed and bus errors
   from the dashboard, and photograph that panel.
4. Pull the capture file off the device before powering down.

---

## 6. Evidence handling

Per D-015: **raw evidence is not edited after a test, and failures are never
deleted.** A run that went wrong is still evidence — it is labelled as such, not
discarded.

Each run gets its own directory under `docs/evidence/`:

```
docs/evidence/RUN-00N/
  serial-log.txt        raw, immutable
  can-0NN.log           the capture pulled off flash
  cluster.mp4           the instrument-cluster video
  gps-track.gpx         phone GPS log, if used
  run-sheet.md          times, actions, observed values
  dashboard-end.jpg     final counters
  NOTES.md              engineer interpretation — SEPARATE from the raw files
```

**Raw evidence ≠ engineer interpretation.** Interpretation goes in `NOTES.md`
and nowhere else.

The device restarted between the two photographs taken on 8 September — the
identifier counts decreased, which only a reboot can do. Those are therefore two
separate runs, not one, and must be recorded as such.

### Run sheet template

```
RUN-00N   vehicle: Honda HR-V 2023   date: ____  start (wall clock): ____
device UNIT_ID: ______   firmware: ______   bitrate: 500 kbps

sync marker (5 brake presses) at wall clock: ______

phase  t+      action                          observed value
A      00:00   resting baseline, 60 s          IDs seen: ___
B      01:00   steering full left              -
B      01:15   steering full right             -
...
D      __:__   hold 2000 rpm                   tacho reads: ____
E      __:__   hold 20 km/h                    speedo reads: ____
F      __:__   odometer before                 ____ km
F      __:__   odometer after                  ____ km

end: received ____  identifiers ____  dropped ____  missed ____  bus errors ____
capture file: ____  size ____  written vs received: ____
```

---

## 7. After the run

Order matters — §9.1 puts speed first, because it is the only signal with an
independent reference, and a correct speed decode simultaneously proves the
wiring and the bitrate.

1. **Speed**, using the phase E plateaus. Search for the plateau values.
   Cross-check against the video and the phone GPS track.
2. **RPM**, using the phase D holds.
3. **Odometer**, using `tools/can_find_value.py` with the phase F readings, both
   endiannesses, and a retry at 0.1 km resolution.
4. **Discrete signals** from phase B, by diffing the quiet gaps against the
   action windows.

A candidate is not a signal until it has been confirmed against a second,
independent observation. Until then it is a hypothesis.

**No identifier discovered here goes into the fleet `signals.cfg` without that
confirmation.** These are Honda identifiers on a test vehicle; the fleet is a
different platform entirely.

### The live probe

Once the signal probe build is flashed, a second short drive can confirm a
candidate in the car rather than at a desk: select the identifier, set the start
byte, width and byte order, and watch the number while the driver calls out the
speedometer. It samples rather than captures, so the file on flash remains the
record — but it turns a one-hour desk loop into a ten-second one.

---

## 8. Safety

- The device is **listen-only**, enforced three ways and verified by
  `tools/check_listen_only.sh` on every build. It cannot transmit to the bus.
- The **driver drives**. Phone, dashboard and run sheet belong to the operator.
- Phases A–D belong in a stationary parking space. Only phase E and F need road.
- Route the OBD-II cable so it cannot reach the pedals.
- Abort the run rather than chase a number. The capture can be repeated; the
  vehicle and the people in it cannot.
