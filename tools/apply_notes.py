#!/usr/bin/env python3
"""
apply_notes.py - put a vehicle's identifier names onto the device, in one go.

    python tools/apply_notes.py --host 10.215.81.4 docs/evidence/dfsk-gelora-e/notes.tsv

Why this exists: the names are what turn the dashboard from a wall of hex into
something a person can read, and they are also what the TDS filter and the
Vehicle panel are built from. Typing nine of them into a phone in a parked car,
once per device and once per reflash, is how they end up wrong or missing.

Names are INTERPRETATION, not evidence. They live in /notes/<UNIT_ID>/ on the
device, never inside a capture file, so a wrong guess is corrected without
touching a recording (D-015 in bmt-can-bus-telemetry).

File format, tab separated:

    0x18FFDC01<TAB>#tds Kecepatan {le16(4)/256} km/jam

Lines starting with # are comments. An identifier above 0x7FF is sent as
extended. A row with an empty note CLEARS that identifier's name, which is how
a mapping that turned out wrong is withdrawn.

The device echoes what it actually stored, trimmed and length limited, and this
prints that echo rather than what was sent. A name that did not fit must not
look applied.
"""

import argparse
import json
import sys
import urllib.parse
import urllib.request
from pathlib import Path

NOTE_MAX = 60          # NOTE_TEXT_MAX in src/Config.h, bytes of UTF-8


def rows(path: Path):
    """Yields (line_no, can_id, extended, text) and reports malformed lines."""
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        parts = line.split("\t")
        ident = parts[0].strip()
        text = parts[1].strip() if len(parts) > 1 else ""
        try:
            can_id = int(ident, 16) if ident.lower().startswith("0x") else int(ident, 16)
        except ValueError:
            print(f"  line {n}: not a hex identifier: {ident!r}", file=sys.stderr)
            continue
        yield n, can_id, can_id > 0x7FF, text


def post(host: str, can_id: int, ext: bool, text: str, timeout: float):
    body = urllib.parse.urlencode({"id": can_id, "x": 1 if ext else 0, "text": text})
    req = urllib.request.Request(f"http://{host}/api/note", data=body.encode(),
                                 method="POST")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("file", type=Path, help="the notes file for this vehicle")
    ap.add_argument("--host", required=True, help="device IP, as the dashboard header shows it")
    ap.add_argument("--timeout", type=float, default=8.0)
    ap.add_argument("--dry-run", action="store_true",
                    help="check the file and print what would be sent, touching nothing")
    args = ap.parse_args()

    if not args.file.is_file():
        print(f"no such file: {args.file}", file=sys.stderr)
        return 2

    planned = list(rows(args.file))
    if not planned:
        print("nothing to apply: the file has no identifier rows", file=sys.stderr)
        return 2

    # Checked before anything is sent, so a file that is wrong is rejected whole
    # rather than half applied.
    bad = [(n, i, t) for n, i, _, t in planned if len(t.encode()) > NOTE_MAX]
    if bad:
        for n, i, t in bad:
            print(f"  line {n}: 0x{i:X} note is {len(t.encode())} bytes, limit {NOTE_MAX}: {t}",
                  file=sys.stderr)
        print(f"{len(bad)} note(s) too long; nothing was sent.", file=sys.stderr)
        return 1

    tds = sum(1 for _, _, _, t in planned if t.lower().startswith("#tds"))
    print(f"{len(planned)} identifier(s) from {args.file}, {tds} tagged #tds")
    if args.dry_run:
        for _, i, x, t in planned:
            print(f"  0x{i:X}{' EXT' if x else ''}  {t or '(clear)'}")
        print("dry run: nothing sent")
        return 0

    ok = fail = 0
    for n, can_id, ext, text in planned:
        try:
            got = post(args.host, can_id, ext, text, args.timeout)
        except Exception as e:                       # noqa: BLE001 - report and continue
            print(f"  0x{can_id:X}  FAILED  {e.__class__.__name__}: {e}")
            fail += 1
            continue
        stored = got.get("t", "")
        mark = "ok  " if stored == text else "CUT "
        print(f"  0x{can_id:X}  {mark}{stored or '(cleared)'}")
        ok += 1

    print(f"\n{ok} applied, {fail} failed")
    if fail:
        print("The device keeps what it already had for the failed ones.")
        return 1
    print("Open the dashboard: the TDS filter and the Vehicle panel read these names.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
