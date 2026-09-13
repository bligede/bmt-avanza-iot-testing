#!/usr/bin/env python3
"""
capture_to_webcan.py — turn this device's capture files into webCAN CSV.

webCAN CSV is what the cansub-reverse-engineering skill reads
(scripts/common.py: load_trace). Its header is fixed and checked:

    TimestampEpoch;BusChannel;ID;IDE;DLC;DataLength;Dir;EDL;BRS;ESI;RTR;DataBytes

Usage:
    python tools/capture_to_webcan.py can-003.log can-004.log -o run-002.csv
    python tools/capture_to_webcan.py captures/*.log -o run.csv --epoch-base 1789401234

What it reads, from RawCanLogger:

    # BMT CAN capture v1
    # unit=HRV-TEST-01 fw=0.1.0 bitrate=500000 listen_only=1
    # segment=3 millis_at_open=412345 boot_epoch=1789401000
    # MARK 415000 40kmh
    412401 | ID: 0x1A6 | DLC: 8 | 02 6B 3C 4D 5E 6F 80 9C
    412402 | ID: 0x18DAF110 | DLC: 8 | ... | EXT

Timestamps. Frames carry millis() since boot. With a header, boot_epoch turns
them into wall-clock epoch seconds, which is what lets a reference series — a
GPS log, or OCR of a dashboard video — line up with the frames. Captures made
before the header existed have no boot_epoch; pass --epoch-base with the wall
clock at boot, or accept relative seconds and align the reference by the
clapperboard (five brake presses) instead.

What it refuses to do silently. Files from two different boots cannot be put on
one timeline — millis() restarted between them — so mixing boots is an error
unless --allow-multiple-boots says it is intended. Unparseable lines are counted
and reported, never skipped without a word.

Operator markers (# MARK) go to <output>.markers.csv next to the trace.
"""

import argparse
import re
import sys
from pathlib import Path

HEADER = "TimestampEpoch;BusChannel;ID;IDE;DLC;DataLength;Dir;EDL;BRS;ESI;RTR;DataBytes"

FRAME_RE = re.compile(
    r"^\s*(?P<ms>\d+)\s*\|\s*ID:\s*0x(?P<id>[0-9A-Fa-f]+)\s*\|\s*DLC:\s*(?P<dlc>\d+)\s*\|"
    r"\s*(?P<data>(?:[0-9A-Fa-f]{2}\s*)*)(?P<tail>.*)$"
)
KV_RE = re.compile(r"(\w+)=(\S+)")
MARK_RE = re.compile(r"^#\s*MARK\s+(\d+)\s*(.*)$")


def parse_file(path: Path, stats: dict):
    """Yield ('frame', ms, id, ext, rtr, data) / ('mark', ms, label) / ('meta', dict)."""
    with path.open("r", encoding="utf-8", errors="replace") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip("\r\n")
            if not line.strip():
                continue
            if line.startswith("#"):
                m = MARK_RE.match(line)
                if m:
                    yield ("mark", int(m.group(1)), m.group(2).strip())
                else:
                    meta = dict(KV_RE.findall(line))
                    if meta:
                        yield ("meta", meta)
                continue
            m = FRAME_RE.match(line)
            if not m:
                stats["bad"] += 1
                if stats["bad"] <= 5:
                    print(f"  unparsed {path.name}:{lineno}: {line[:80]}", file=sys.stderr)
                continue
            tail = m.group("tail")
            ext = "EXT" in tail or len(m.group("id")) > 3
            rtr = "RTR" in tail
            data = bytes.fromhex(m.group("data")) if not rtr else b""
            dlc = int(m.group("dlc"))
            if not rtr and len(data) != dlc:
                stats["dlc_mismatch"] += 1
            yield ("frame", int(m.group("ms")), int(m.group("id"), 16), ext, rtr, data)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("inputs", nargs="+", type=Path, help="capture files, in order")
    ap.add_argument("-o", "--output", type=Path, required=True, help="webCAN CSV to write")
    ap.add_argument("--epoch-base", type=float, default=None,
                    help="wall-clock epoch at boot, for captures without a boot_epoch header")
    ap.add_argument("--channel", type=int, default=1, help="BusChannel value (default 1)")
    ap.add_argument("--allow-multiple-boots", action="store_true",
                    help="permit files from different boots (they will NOT share a timeline)")
    args = ap.parse_args()

    stats = {"bad": 0, "dlc_mismatch": 0}
    frames, marks = [], []
    boots = set()
    unit = None
    ids = set()

    for path in args.inputs:
        if not path.exists():
            print(f"error: {path} does not exist", file=sys.stderr)
            return 2
        boot_epoch = None
        last_ms = None
        for item in parse_file(path, stats):
            kind = item[0]
            if kind == "meta":
                meta = item[1]
                unit = meta.get("unit", unit)
                if "boot_epoch" in meta:
                    boot_epoch = float(meta["boot_epoch"])
                    if boot_epoch > 0:
                        boots.add(round(boot_epoch))
                continue
            ms = item[1]
            if last_ms is not None and ms + 1000 < last_ms:
                print(f"error: millis went backwards inside {path.name} ({last_ms} -> {ms}); "
                      f"the device rebooted mid-file", file=sys.stderr)
                return 3
            last_ms = ms
            base = boot_epoch if (boot_epoch and boot_epoch > 0) else args.epoch_base
            t = (base or 0.0) + ms / 1000.0
            if kind == "mark":
                marks.append((t, item[2]))
            else:
                _, _, cid, ext, rtr, data = item
                frames.append((t, cid, ext, rtr, data))
                ids.add((cid, ext))

    if len(boots) > 1 and not args.allow_multiple_boots:
        print(f"error: these files come from {len(boots)} different boots {sorted(boots)}. "
              f"Their millis() restarted, so they cannot share one timeline. Convert each boot "
              f"separately, or pass --allow-multiple-boots if you know why.", file=sys.stderr)
        return 4

    frames.sort(key=lambda f: f[0])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as out:
        out.write(HEADER + "\n")
        for t, cid, ext, rtr, data in frames:
            id_hex = f"{cid:08X}" if ext else f"{cid:03X}"
            out.write(f"{t:.3f};{args.channel};{id_hex};{1 if ext else 0};{len(data)};{len(data)};"
                      f"0;0;0;0;{1 if rtr else 0};{data.hex().upper()}\n")

    if marks:
        mpath = args.output.with_suffix(".markers.csv")
        with mpath.open("w", encoding="utf-8", newline="\n") as out:
            out.write("TimestampEpoch;Label\n")
            for t, label in marks:
                out.write(f"{t:.3f};{label.replace(';', ',')}\n")

    span = (frames[-1][0] - frames[0][0]) if len(frames) > 1 else 0.0
    timeline = ("wall clock (boot_epoch header)" if boots else
                "wall clock (--epoch-base)" if args.epoch_base else
                "RELATIVE seconds since boot — align references by the clapperboard")
    print(f"unit          {unit or 'unknown (no header)'}")
    print(f"frames        {len(frames):,} from {len(ids)} identifier(s) over {span:.1f} s")
    print(f"markers       {len(marks)}")
    print(f"timeline      {timeline}")
    print(f"unparsed      {stats['bad']}")
    if stats["dlc_mismatch"]:
        print(f"DLC mismatch  {stats['dlc_mismatch']} frame(s) where payload length != DLC")
    print(f"wrote         {args.output}")
    return 1 if stats["bad"] else 0


if __name__ == "__main__":
    sys.exit(main())
