#!/usr/bin/env python3
"""
pull_captures.py - copy capture files off the device over the serial console.

The dashboard can download captures over WiFi, but only since the phase-2
firmware. Anything recorded by an older build has to come out through the
console, and that is what this does: `ls`, then `cat` per file.

    python tools/pull_captures.py --port COM6 --out captures/hrv-run-001
    python tools/pull_captures.py --port COM6 --list          (just look)

It is slow: 115200 baud is about 11 kB/s, so a full 256 kB segment takes
roughly half a minute, and a full flash takes half an hour.

Two safeguards, because this is the only copy of evidence that cannot be
recreated (D-015):

  - the port is opened with DTR/RTS held low, so the ESP32 is NOT reset;
  - every file is size-checked against `ls`, and a short file is reported
    and kept as `<name>.partial` instead of silently passing as complete.

It never deletes anything from the device.
"""

import argparse
import re
import sys
import time
from pathlib import Path

import serial

PROMPT_IDLE = 0.4
LS_RE = re.compile(r"^\s+(\S+)\s+(\d+)\s*B\s*$")


def drain(ser, quiet_for=PROMPT_IDLE, limit=20.0):
    """Read until the device has been quiet for a while."""
    buf, last = bytearray(), time.time()
    start = last
    while time.time() - start < limit:
        chunk = ser.read(4096)
        if chunk:
            buf += chunk
            last = time.time()
        elif time.time() - last > quiet_for:
            break
    return buf.decode("utf-8", "replace")


def send(ser, line, quiet_for=PROMPT_IDLE, limit=20.0):
    ser.reset_input_buffer()
    ser.write((line + "\r\n").encode())
    ser.flush()
    return drain(ser, quiet_for, limit)


def list_files(ser, directory):
    out = send(ser, "ls")
    files = []
    for ln in out.splitlines():
        m = LS_RE.match(ln.rstrip())
        if m and not m.group(1).endswith(")"):
            files.append((m.group(1), int(m.group(2))))
    return files, out


def pull(ser, name, size, directory, outdir):
    path = f"{directory}/{name}"
    ser.reset_input_buffer()
    ser.write((f"cat {path}\r\n").encode())
    ser.flush()

    # everything between the BEGIN and END banners is the file
    begin = f"---- BEGIN {path}".encode()
    end = f"---- END {path}".encode()
    buf = bytearray()
    last = time.time()
    # a 256 kB file at 115200 baud is ~25 s; allow for a slow console
    deadline = time.time() + 60 + size / 4000.0
    while time.time() < deadline:
        chunk = ser.read(8192)
        if chunk:
            buf += chunk
            last = time.time()
            if end in buf:
                break
        elif time.time() - last > 5.0:
            break

    if begin not in buf:
        return None, "no BEGIN banner (file missing, or the console is busy)"
    body = buf.split(begin, 1)[1]
    body = body.split(b"\r\n", 1)[1] if b"\r\n" in body else body
    complete = end in body
    body = body.split(end, 1)[0] if complete else body
    body = body.rstrip(b"-") .rstrip()          # trailing banner dashes/newline
    data = body.replace(b"\r\n", b"\n")

    target = outdir / (name if complete else name + ".partial")
    target.write_bytes(data)
    return len(data), None if complete else "no END banner: file is incomplete"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--dir", default="/capture", help="directory on the device")
    ap.add_argument("--out", type=Path, help="where to write the files")
    ap.add_argument("--list", action="store_true", help="list only, copy nothing")
    args = ap.parse_args()

    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = args.port, args.baud, 0.3
    ser.dtr = False          # do not reset the device on open
    ser.rts = False
    ser.open()
    time.sleep(0.3)
    drain(ser, 0.3, 3.0)

    banner = send(ser, "")
    files, raw = list_files(ser, args.dir)
    if not files:
        print("No files listed. Raw reply:\n" + (raw or banner)[:800], file=sys.stderr)
        return 2

    total = sum(s for _, s in files)
    print(f"{len(files)} file(s), {total:,} bytes on the device")
    for n, s in files:
        print(f"  {n:<28} {s:>9,} B")
    if args.list:
        return 0
    if not args.out:
        print("give --out to copy", file=sys.stderr)
        return 2

    args.out.mkdir(parents=True, exist_ok=True)
    print(f"\ncopying to {args.out}  (about {total / 11000 / 60:.0f} min)")
    bad = 0
    for i, (name, size) in enumerate(files, 1):
        t0 = time.time()
        got, problem = pull(ser, name, size, args.dir, args.out)
        if got is None:
            print(f"  [{i}/{len(files)}] {name}: FAILED - {problem}")
            bad += 1
            continue
        # the console converts \n to \r\n; compare on the device's own count
        status = "ok" if (not problem and abs(got - size) <= 2) else "CHECK"
        if status == "CHECK":
            bad += 1
        print(f"  [{i}/{len(files)}] {name}: {got:,} B of {size:,} "
              f"in {time.time() - t0:.0f}s  {status}"
              + (f" - {problem}" if problem else ""))

    ser.close()
    print("\nDone." if not bad else f"\nDone with {bad} file(s) to check.")
    print("Nothing was deleted from the device.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
