#!/usr/bin/env python3
"""
match_dashboard.py - match numbers read off the instrument cluster against a capture.

    python tools/match_dashboard.py captures/device-2026-09-20 --value 69620 \
        --scales 1 0.1 0.01 --label "odometer km"

For every identifier it tries every field position (start byte, 1-4 bytes,
big and little endian, and 12-bit halves of a byte pair), and reports the ones
that decode to the value you saw on the dashboard.

Why this is fast: a capture holds ~175,000 frames but only a few thousand
DISTINCT payloads per identifier, and a field that never changes is only worth
testing once. Payloads are de-duplicated first, so the search is over the
distinct values, not the frames.

What the columns mean:

    hits/frames   in how many frames the field held the wanted value
    coverage      hits as a share of that identifier's frames. A value shown on
                  a stationary dashboard should be close to 100%; a rolling
                  counter that merely passes through the value scores near 0
    span          how much the field moves across the whole capture. A field
                  that swings wildly is not an odometer

A candidate is a HYPOTHESIS. It becomes a signal only after it tracks the
dashboard while the vehicle moves (Blueprint 9.1 step 6). Nothing here may be
written into a fleet signals.cfg.
"""

import argparse
import glob
import sys
from collections import Counter, defaultdict
from pathlib import Path


def load(paths, stop_ms=None, start_ms=None):
    """{ (id, ext) : Counter(payload bytes) }, in file order."""
    per_id = defaultdict(Counter)
    frames = 0
    for path in paths:
        for line in open(path, encoding="utf-8", errors="replace"):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split("|")]
            try:
                ms = int(parts[0])
                cid = parts[1].split()[1]
                data = bytes.fromhex(parts[3].replace(" ", "")) if len(parts) > 3 else b""
            except (ValueError, IndexError):
                continue
            if (stop_ms is not None and ms >= stop_ms) or (start_ms is not None and ms < start_ms):
                continue
            per_id[cid][data] += 1
            frames += 1
    return per_id, frames


def fields(data: bytes):
    """Yield (label, value) for every plausible unsigned field in one payload."""
    n = len(data)
    for i in range(n):
        for width in (1, 2, 3, 4):
            if i + width > n:
                continue
            chunk = data[i:i + width]
            be = int.from_bytes(chunk, "big")
            yield (f"b{i}..{i + width - 1} BE", be)
            if width > 1:
                yield (f"b{i}..{i + width - 1} LE", int.from_bytes(chunk, "little"))
        if i + 2 <= n:                      # 12-bit fields, both halves
            pair = int.from_bytes(data[i:i + 2], "big")
            yield (f"b{i}..{i + 1} 12h", pair >> 4)
            yield (f"b{i}..{i + 1} 12l", pair & 0x0FFF)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("source", help="capture directory or a single capture file")
    ap.add_argument("--value", type=float, required=True, help="the number on the dashboard")
    ap.add_argument("--scales", type=float, nargs="+", default=[1.0],
                    help="raw = value / scale. 1 = whole units, 0.1 = tenths, 0.01 = hundredths")
    ap.add_argument("--offsets", type=float, nargs="+", default=[0.0],
                    help="raw = (value / scale) + offset. Temperatures often use 40")
    ap.add_argument("--tolerance", type=float, default=0.0, help="in raw counts")
    ap.add_argument("--label", default="value")
    ap.add_argument("--stop-ms", type=int, default=None)
    ap.add_argument("--start-ms", type=int, default=None)
    ap.add_argument("--min-coverage", type=float, default=0.0,
                    help="drop candidates seen in less than this share of the ID's frames")
    ap.add_argument("--top", type=int, default=20)
    args = ap.parse_args()

    src = Path(args.source)
    paths = sorted(glob.glob(str(src / "can-*.log"))) if src.is_dir() else [str(src)]
    if not paths:
        print(f"no capture files in {src}", file=sys.stderr)
        return 2

    per_id, frames = load(paths, args.stop_ms, args.start_ms)
    targets = {}
    for scale in args.scales:
        for off in args.offsets:
            raw = args.value / scale + off
            targets[(scale, off)] = raw
    print(f"{frames:,} frames, {len(per_id)} identifiers, "
          f"{sum(len(c) for c in per_id.values()):,} distinct payloads")
    print(f"looking for {args.label} = {args.value}, as raw "
          + ", ".join(f"{v:g} (scale {s:g}"
                      + (f", offset {o:+g})" if o else ")") for (s, o), v in targets.items()))

    rows = []
    for cid, payloads in per_id.items():
        total = sum(payloads.values())
        stats = defaultdict(lambda: [0, None, None])     # label -> [hits, min, max]
        for data, count in payloads.items():
            for label, value in fields(data):
                st = stats[label]
                st[1] = value if st[1] is None else min(st[1], value)
                st[2] = value if st[2] is None else max(st[2], value)
                for (scale, off), raw in targets.items():
                    if abs(value - raw) <= args.tolerance:
                        st[0] += count
                        break
        for label, (hits, lo, hi) in stats.items():
            if not hits:
                continue
            cov = hits / total
            if cov < args.min_coverage:
                continue
            rows.append((cov, hits, total, cid, label, lo, hi))

    if not rows:
        print("\nNo field in any identifier ever held that value.")
        print("That is a real result, not an error: the value may live on a bus that")
        print("does not reach OBD-II pins 6/14, or use a scale or offset not tried here.")
        return 1

    rows.sort(reverse=True)
    print(f"\n{'ID':<10}{'field':<16}{'hits/frames':>16}{'coverage':>10}"
          f"{'min':>12}{'max':>12}")
    for cov, hits, total, cid, label, lo, hi in rows[:args.top]:
        print(f"{cid:<10}{label:<16}{hits:>8,}/{total:<7,}{cov * 100:>9.1f}%{lo:>12,}{hi:>12,}")
    if len(rows) > args.top:
        print(f"... {len(rows) - args.top} more")
    print("\nEach line is a hypothesis. Confirm it by watching the field while the")
    print("dashboard value changes; a field that never moves proves nothing.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
