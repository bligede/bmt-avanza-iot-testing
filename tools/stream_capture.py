#!/usr/bin/env python3
"""
stream_capture.py - record the CAN bus live over WiFi, straight to this laptop.

    python tools/stream_capture.py --host 10.215.81.4 --out captures/gelora-002

Why this exists instead of recording to the device's own flash: the TWAI
interrupt lives in flash on this framework, so every flash write disables the
instruction cache, the interrupt cannot run, and the controller FIFO overruns.
Measured at 644 frames/s on a DFSK Gelora E: 4.9 % of the bus lost while
recording, nothing lost with flash idle (docs/evidence/frame-loss.md).

Streaming costs the device nothing but WiFi, so the recording is complete, and
its length is limited by this disk rather than by 12 MB of flash.

What it writes: the same segmented capture files the device would have written
(can-000.log, can-001.log, ...), header lines included, so every tool in this
repo reads them unchanged.

Honest about gaps. If the link drops, it says so, reconnects, and writes a
`# LINK` line into the file at the point of the break. A hole in the recording
is marked, never quietly closed.

Press Ctrl+C to stop; the current segment is flushed and closed.
"""

import argparse
import json
import re
import signal
import socket
import sys
import time
import urllib.request
from pathlib import Path

FRAME = re.compile(rb"^\d+ \| ID: 0x[0-9A-Fa-f]+ \| DLC: \d+ \|")
SEGMENT_BYTES = 256 * 1024


class Segments:
    """Rolls capture files exactly like RawCanLogger does on the device."""

    def __init__(self, outdir: Path):
        self.dir = outdir
        self.dir.mkdir(parents=True, exist_ok=True)
        self.index = self._next_free()
        self.fh = None
        self.bytes_in_segment = 0
        self.total_bytes = 0
        self.frames = 0
        self._open()

    def _next_free(self) -> int:
        highest = -1
        for p in self.dir.glob("can-*.log"):
            m = re.fullmatch(r"can-(\d+)\.log", p.name)
            if m:
                highest = max(highest, int(m.group(1)))
        return highest + 1

    def _open(self):
        if self.fh:
            self.fh.close()
        path = self.dir / f"can-{self.index:03d}.log"
        # Unbuffered on purpose. A recording that only reaches disk when the
        # process exits cleanly is a recording you lose the moment the laptop
        # is closed, the terminal is killed, or the battery runs out.
        self.fh = path.open("ab", buffering=0)
        self.bytes_in_segment = 0
        return path

    def write(self, chunk: bytes):
        if self.bytes_in_segment >= SEGMENT_BYTES:
            self.index += 1
            self._open()
        self.fh.write(chunk)
        self.bytes_in_segment += len(chunk)
        self.total_bytes += len(chunk)
        self.frames += sum(1 for line in chunk.split(b"\n") if FRAME.match(line))

    def note(self, text: str):
        """A comment line in the capture: link drops are part of the evidence."""
        self.write(f"# LINK {int(time.time())} {text}\n".encode())

    def close(self):
        if self.fh:
            self.fh.flush()
            self.fh.close()
            self.fh = None


def sink(host: str, mode: str) -> str:
    """Pause or resume the device's own recording to flash.

    Writing to flash is what costs frames: measured on a DFSK Gelora E, 14.8 %
    of the bus lost while the device wrote to its own flash, 0 % while
    streaming with that writer paused. Leaving it on would quietly reintroduce
    the loss this tool exists to avoid, so pause it and put it back after.
    """
    try:
        with urllib.request.urlopen(f"http://{host}/api/sink?mode={mode}", data=b"", timeout=8) as r:
            return r.read().decode().strip()
    except Exception as e:                       # noqa: BLE001 - report, keep streaming
        return f"could not set sink={mode}: {e.__class__.__name__}"


def device_drops(host: str):
    """Frames the DEVICE threw away because this link could not take them.

    The progress line used to show only this end's link breaks, which stayed at
    zero through a run that lost 7 % of the stream inside the device. A counter
    that cannot show the loss it exists to catch is worse than none.
    """
    try:
        with urllib.request.urlopen(f"http://{host}/api/state", timeout=3) as r:
            return json.load(r)["cap"]["net"]["drop"]
    except Exception:                            # noqa: BLE001 - never interrupt a run
        return None


def connect(host: str, port: int, timeout: float) -> socket.socket:
    s = socket.create_connection((host, port), timeout=timeout)
    s.settimeout(timeout)
    return s


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--host", required=True, help="device IP, as shown in the dashboard header")
    ap.add_argument("--port", type=int, default=3333)
    ap.add_argument("--out", type=Path, required=True, help="directory for the capture files")
    ap.add_argument("--timeout", type=float, default=10.0, help="seconds of silence before reconnecting")
    ap.add_argument("--quiet", action="store_true", help="no per-second progress line")
    ap.add_argument("--keep-flash", action="store_true",
                    help="also leave the device recording to its own flash, which costs "
                         "about 15%% of the bus in lost frames")
    args = ap.parse_args()

    # A run that is killed rather than interrupted would otherwise leave the
    # device paused, and the next person would find a recorder that records
    # nothing. Turn a TERM into the same KeyboardInterrupt Ctrl+C raises.
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt()))

    if not args.keep_flash:
        print(f"  device recording to flash: {sink(args.host, 'off')}"
              f"   (flash writes cost ~15 % of the frames; --keep-flash overrides)")

    seg = Segments(args.out)
    print(f"writing to {args.out}/can-{seg.index:03d}.log onwards; Ctrl+C to stop")

    started = time.time()
    last_report = 0.0
    last_frames = 0
    drops = 0                 # link breaks at this end
    dev_drop = 0              # frames the device could not hand to the socket
    last_devpoll = 0.0
    tail = b""

    try:
        while True:
            try:
                sock = connect(args.host, args.port, args.timeout)
            except OSError as e:
                print(f"  waiting for {args.host}:{args.port} ({e.__class__.__name__})")
                time.sleep(2)
                continue

            print(f"  connected to {args.host}:{args.port}")
            while True:
                try:
                    chunk = sock.recv(16384)
                except socket.timeout:
                    print("  link silent, reconnecting")
                    seg.note("silent")
                    drops += 1
                    break
                except OSError:
                    chunk = b""
                if not chunk:
                    print("  link closed by the device, reconnecting")
                    seg.note("closed")
                    drops += 1
                    break

                # Only whole lines are written, so a file never ends mid-frame.
                buf = tail + chunk
                cut = buf.rfind(b"\n")
                if cut >= 0:
                    seg.write(buf[:cut + 1])
                    tail = buf[cut + 1:]
                else:
                    tail = buf

                now = time.time()
                if now - last_devpoll >= 5.0:
                    d = device_drops(args.host)
                    if d is not None:
                        dev_drop = d
                    last_devpoll = now
                if not args.quiet and now - last_report >= 1.0:
                    rate = (seg.frames - last_frames) / max(now - last_report, 1e-6)
                    mb = seg.total_bytes / 1e6
                    sys.stdout.write(f"\r  {seg.frames:>9,} frames  {rate:>6.0f}/s  "
                                     f"{mb:>7.2f} MB  {int(now - started):>5} s  "
                                     f"link breaks {drops}  device dropped {dev_drop:,}"
                                     + ("  <-- LINK TOO SLOW   " if dev_drop else "   "))
                    sys.stdout.flush()
                    last_report, last_frames = now, seg.frames
            sock.close()
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        if not args.keep_flash:
            print(f"  device recording to flash: {sink(args.host, 'file')}")
        if tail:
            seg.write(tail if tail.endswith(b"\n") else tail + b"\n")
        seg.close()

    span = time.time() - started
    print(f"{seg.frames:,} frames, {seg.total_bytes / 1e6:.2f} MB over {span:.0f} s "
          f"({seg.frames / max(span, 1):.0f}/s), {drops} link break(s)")
    final_drop = device_drops(args.host)
    if final_drop is not None:
        dev_drop = final_drop
    if dev_drop:
        share = 100 * dev_drop / max(seg.frames + dev_drop, 1)
        print(f"WARNING: the device dropped {dev_drop:,} frames, {share:.1f} % of the bus, "
              f"because this link could not take them. The recording has holes.")
        print("         Move the laptop closer to the device and to the hotspot, close the "
              "dashboard on the phone, and record again.")
    else:
        print("no frames dropped by the device: the recording is complete")
    print(f"files in {args.out}. Convert with:")
    print(f"  python tools/capture_to_webcan.py {args.out}/can-*.log -o {args.out}.csv")
    return 0


if __name__ == "__main__":
    sys.exit(main())
