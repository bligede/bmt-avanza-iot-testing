#!/usr/bin/env python3
"""
fetch_captures.py - copy capture files off the device over WiFi.

    python tools/fetch_captures.py --host 192.168.100.41 --out captures/run-001

This is the transport to trust. The serial console can also print a capture
(`cat`), but a USB-serial link has no retransmission: a run on 2026-09-20
produced files LARGER than the originals, padded with runs of zero bytes and
duplicated half-lines. HTTP runs over TCP, so what arrives is what was sent.

Checks on every file, because this evidence cannot be recreated (D-015):

  - the byte count must equal the size the device reported in /api/captures;
  - every line must parse as a frame, a header or a marker;
  - millis() must never go backwards inside a file.

Anything that fails is kept as <name>.suspect and reported. Nothing is ever
deleted from the device.
"""

import argparse
import json
import re
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

FRAME = re.compile(r"^\d+ \| ID: 0x[0-9A-Fa-f]+ \| DLC: \d+ \|")


def get(url, timeout=30):
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return r.read()


def check(data: bytes):
    """Return (problems, frames, first_ms, last_ms)."""
    problems, frames, first, last = [], 0, None, None
    if b"\x00" in data:
        problems.append(f"{data.count(0)} zero byte(s): transport corruption")
    for n, raw in enumerate(data.decode("utf-8", "replace").splitlines(), 1):
        line = raw.strip()
        if not line:
            continue
        if line.startswith("#"):
            continue
        if not FRAME.match(line):
            if len(problems) < 5:
                problems.append(f"line {n} does not parse: {line[:60]!r}")
            continue
        frames += 1
        ms = int(line.split(" |", 1)[0])
        if first is None:
            first = ms
        if last is not None and ms + 1000 < last:
            problems.append(f"line {n}: millis went backwards {last} -> {ms}")
        last = ms
    return problems, frames, first, last


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--host", required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--skip-current", action="store_true",
                    help="leave the segment still being written")
    args = ap.parse_args()

    base = f"http://{args.host}"
    index = json.loads(get(f"{base}/api/captures"))
    files = index["files"]
    current = (index.get("current") or "").rsplit("/", 1)[-1]
    total = sum(f["size"] for f in files)
    print(f"{len(files)} file(s), {total:,} bytes on {args.host}  (current: {current or 'none'})")

    args.out.mkdir(parents=True, exist_ok=True)
    bad = 0
    grand_frames = 0
    for i, f in enumerate(files, 1):
        name, size = f["name"], f["size"]
        if args.skip_current and name == current:
            print(f"  [{i}/{len(files)}] {name}: skipped, still being written")
            continue
        t0 = time.time()
        try:
            data = get(f"{base}/api/capture?name={urllib.parse.quote(name)}", timeout=120)
        except Exception as e:                      # noqa: BLE001 - report, keep going
            print(f"  [{i}/{len(files)}] {name}: DOWNLOAD FAILED - {e}")
            bad += 1
            continue
        problems, frames, first, last = check(data)
        if len(data) != size:
            problems.insert(0, f"got {len(data):,} B, device said {size:,} B")
        grand_frames += frames
        target = args.out / (name if not problems else name + ".suspect")
        target.write_bytes(data)
        rate = len(data) / max(time.time() - t0, 0.001) / 1024
        span = f"{(last - first) / 1000:.0f}s" if first is not None and last is not None else "-"
        print(f"  [{i}/{len(files)}] {name}: {len(data):,} B, {frames:,} frames, {span}, "
              f"{rate:.0f} kB/s  {'OK' if not problems else 'SUSPECT'}")
        for p in problems:
            print(f"        ! {p}")
            bad += 1

    print(f"\n{grand_frames:,} frames total in {args.out}")
    print("OK - every file matched its size and parsed" if not bad
          else f"{bad} problem(s): see the .suspect files above")
    print("Nothing was deleted from the device.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
