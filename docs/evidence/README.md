# docs/evidence/

Artifacts from each test run.

```
docs/evidence/
└── <run>/
    ├── serial-log.txt        whole session, from the first boot line
    ├── dashboard.jpg         screenshot of the live page
    ├── wiring.jpg
    ├── resistance.jpg        the multimeter reading on pins 6-14
    └── result.md
```

## Rules

**RAW EVIDENCE ≠ ENGINEER INTERPRETATION.**

`serial-log.txt` is the primary source and is never edited after the test.
`result.md` carries the analysis and may be revised. If the two disagree, the
raw log is correct.

1. **The whole session.** From the first boot line to the end — not just the
   interesting part. Reset reason, init order, warnings, heap and startup timing
   are exactly what answers "was our hardware ever healthy?" when a later test
   goes wrong.
2. **Verbatim, not summarised.** A reformatted log cannot be audited.
3. **Record failures.** Never delete a failed run and keep only "PASS after
   troubleshooting" — that discards the diagnostic history.
4. **A re-run makes a new dated folder.** It never overwrites the original.

## Run naming

```
run-001-<date>/     FAIL
run-002-<date>/     PASS
```

Not a single folder that quietly becomes a pass.

## result.md

```markdown
# <run> Result

**Status:** PASS / FAIL / BLOCKED
**Date:**        **Technician:**        **Vehicle:**

## Setup
| | |
|---|---|
| Avanza model year | |
| OBD-II pin 6-14 resistance | ______ Ω |
| Module 120 Ω terminator | FITTED / REMOVED |
| CAN bitrate | 500 / 250 kbps |
| Hotspot SSID | |

## Result
- frames received:
- distinct CAN IDs:
- rx missed / dropped:      <- must be 0
- capture duration:
- dashboard reachable:      YES / NO

## Observations

## Known issues

## Next action
```
