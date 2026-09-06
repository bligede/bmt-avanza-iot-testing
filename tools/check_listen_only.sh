#!/bin/sh
# =============================================================================
#  check_listen_only.sh — release gate for the permanent listen-only rule.
#
#  Run this in CI and before every release build. It is layer 3 of the
#  enforcement described in CanBusSafety.h; layers 1 and 2 are the TWAI mode
#  word and the #pragma GCC poison.
#
#  It fails the build if:
#    1. any source file references a CAN transmit entry point;
#    2. the TWAI driver is installed in any mode other than LISTEN_ONLY;
#    3. a file that includes driver/twai.h does not also include CanBusSafety.h;
#    4. CanBusSafety.h has lost its poison directives.
#
#  Exit 0 = safe to ship.  Exit 1 = do not flash this to a vehicle.
# =============================================================================
set -u

DIR="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$DIR/src"
FAIL=0

say()  { printf '%s\n' "$*"; }
bad()  { printf '  FAIL: %s\n' "$*"; FAIL=1; }
good() { printf '  ok:   %s\n' "$*"; }

say ""
say "=== listen-only release gate ==="

# --- 1. no transmit call anywhere -------------------------------------------
# The definition inside CanBusSafety.h's own poison lines is expected; anything
# else is a violation.
HITS=$(grep -rn -E 'twai_transmit|twai_clear_transmit_queue' "$SRC" 2>/dev/null \
       | grep -v 'CanBusSafety.h' || true)
if [ -n "$HITS" ]; then
    bad "a CAN transmit entry point is referenced in the firmware:"
    printf '%s\n' "$HITS" | sed 's/^/        /'
else
    good "no CAN transmit call in the firmware"
fi

# --- 2. the driver is only ever installed listen-only -----------------------
MODES=$(grep -rn -E 'TWAI_MODE_[A-Z_]+' "$SRC" 2>/dev/null \
        | grep -v 'CanBusSafety.h' \
        | grep -v -E 'TWAI_MODE_LISTEN_ONLY' || true)
if [ -n "$MODES" ]; then
    bad "a TWAI mode other than LISTEN_ONLY appears in the firmware:"
    printf '%s\n' "$MODES" | sed 's/^/        /'
else
    good "TWAI_MODE_LISTEN_ONLY is the only mode referenced"
fi

if ! grep -q 'TWAI_MODE_LISTEN_ONLY' "$SRC/CanManager.cpp" 2>/dev/null; then
    bad "CanManager.cpp does not reference TWAI_MODE_LISTEN_ONLY at all"
else
    good "CanManager.cpp installs the driver in LISTEN_ONLY"
fi

# --- 3. every twai-facing file carries the guard -----------------------------
# Match the #include DIRECTIVE, not any mention of the path. A file that merely
# documents "this module never includes driver/twai.h" is not a violation, and
# an earlier version of this check flagged exactly that.
TWAI_INCLUDE='^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]driver/twai\.h[>"]'
for f in $(grep -rlE "$TWAI_INCLUDE" "$SRC" 2>/dev/null || true); do
    case "$f" in *CanBusSafety.h) continue ;; esac
    if ! grep -q 'CanBusSafety.h' "$f"; then
        bad "$(basename "$f") includes driver/twai.h without CanBusSafety.h"
    fi
done
[ "$FAIL" -eq 0 ] && good "every twai-facing file includes CanBusSafety.h"

# --- 4. the guard itself is intact -------------------------------------------
GUARD="$SRC/CanBusSafety.h"
if [ ! -f "$GUARD" ]; then
    bad "CanBusSafety.h is missing — the compile-time guard has been deleted"
else
    for sym in twai_transmit twai_clear_transmit_queue; do
        if ! grep -q "pragma GCC poison .*$sym" "$GUARD"; then
            bad "CanBusSafety.h no longer poisons $sym"
        fi
    done
    # A conditional poison is how this guard silently failed once before.
    if grep -qE '^\s*#\s*if' "$GUARD"; then
        bad "CanBusSafety.h contains a conditional — the poison must be unconditional"
    fi
    [ "$FAIL" -eq 0 ] && good "CanBusSafety.h poison directives intact and unconditional"
fi

say ""
if [ "$FAIL" -eq 0 ]; then
    say "LISTEN-ONLY GATE: PASS — safe to build for a vehicle"
    exit 0
fi
say "LISTEN-ONLY GATE: FAIL — DO NOT FLASH THIS TO A VEHICLE"
exit 1
