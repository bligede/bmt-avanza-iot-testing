#!/usr/bin/env python3
"""
can_find_value.py — find a known dashboard value inside a raw CAN capture.

This implements the known-value method from Blueprint §9.1 directly:

    1. Photograph the odometer on the dashboard, e.g. 12345 km.
    2. Convert it to hex, giving the byte sequence 00 30 39.
    3. Record CAN traffic for thirty seconds with the vehicle stationary.
    4. Search the log for that sequence, big-endian and little-endian.
    5. If nothing matches, retry assuming 0.1 km resolution, i.e. 123450.
    6. Validate the candidate by driving exactly two kilometres.

Steps 4 and 5 are what this script automates. It reads a capture produced by
RawCanLogger and reports every (CAN ID, start bit, length, endianness, scale)
combination that decodes to the value you observed — and, crucially, how STABLE
each candidate is across the capture, because a stationary vehicle's odometer
must not change while a counter that happens to pass through the same value will.

Blueprint §9.1 recommends doing SPEED first: it is the only one of the four
signals with an independent reference (GNSS ground speed), and a correct speed
decode also proves the wiring and the bitrate are right.

USAGE
    # Odometer reads 12345 km on the dashboard
    ./can_find_value.py capture.log --value 12345 --unit km

    # Same, allowing the common 0.1 km and 10 m resolutions
    ./can_find_value.py capture.log --value 12345 --scales 1 0.1 0.01

    # State of charge reads 82 %
    ./can_find_value.py capture.log --value 82 --max-bits 16 --stable

    # Speed reads 40 km/h and the vehicle is MOVING, so do not require stability
    ./can_find_value.py capture.log --value 40 --tolerance 1 --no-stable

INPUT FORMAT (as written by RawCanLogger)
    123456 | ID: 0x123 | DLC: 8 | 00 30 39 00 00 00 00 00
    123457 | ID: 0x1A2 | DLC: 8 | 12 34 56 78 9A BC DE F0 | EXT

NOTE
    A candidate from this script is a HYPOTHESIS, not a mapping. Nothing goes
    into signals.cfg until it has been validated against the dashboard while the
    vehicle moves (Blueprint §9.1 step 6). Master prompt §8 forbids guessing.
"""

import argparse
import re
import sys
from collections import defaultdict

LINE_RE = re.compile(
    r"^\s*(\d+)\s*\|\s*ID:\s*0x([0-9A-Fa-f]+)\s*\|\s*DLC:\s*(\d+)\s*\|\s*([0-9A-Fa-f ]*)"
)


class Frame:
    __slots__ = ("ts", "can_id", "dlc", "data", "extended")

    def __init__(self, ts, can_id, dlc, data, extended):
        self.ts = ts
        self.can_id = can_id
        self.dlc = dlc
        self.data = data
        self.extended = extended


def parse_capture(path):
    """Reads a RawCanLogger capture. Skips blank and unparsable lines."""
    frames = []
    skipped = 0
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            m = LINE_RE.match(line)
            if not m:
                skipped += 1
                continue
            ts = int(m.group(1))
            can_id = int(m.group(2), 16)
            dlc = int(m.group(3))
            data = bytes(int(b, 16) for b in m.group(4).split())
            if len(data) < dlc:
                skipped += 1
                continue
            frames.append(Frame(ts, can_id, dlc, data[:dlc], "EXT" in line))
    return frames, skipped


def extract_le(data, start_bit, bit_length):
    """Intel: start_bit is the LSB, the field grows toward higher bit numbers."""
    value = 0
    for i in range(bit_length):
        pos = start_bit + i
        byte_idx, bit_idx = divmod(pos, 8)
        if byte_idx >= len(data):
            return None
        value |= ((data[byte_idx] >> bit_idx) & 1) << i
    return value


def extract_be(data, start_bit, bit_length):
    """Motorola: start_bit is the MSB, DBC sawtooth order."""
    value = 0
    pos = start_bit
    for _ in range(bit_length):
        byte_idx, bit_idx = divmod(pos, 8)
        if byte_idx >= len(data):
            return None
        value = (value << 1) | ((data[byte_idx] >> bit_idx) & 1)
        pos = (byte_idx + 1) * 8 + 7 if bit_idx == 0 else pos - 1
    return value


def sign_extend(raw, bit_length):
    if bit_length >= 64:
        return raw
    if raw & (1 << (bit_length - 1)):
        return raw - (1 << bit_length)
    return raw


class Candidate:
    __slots__ = ("can_id", "extended", "start_bit", "bit_length", "endian",
                 "scale", "signed", "values", "hits", "total",
                 "len_min", "len_max")

    def __init__(self, can_id, extended, start_bit, bit_length, endian, scale, signed):
        self.can_id = can_id
        self.extended = extended
        self.start_bit = start_bit
        self.bit_length = bit_length
        self.endian = endian
        self.scale = scale
        self.signed = signed
        self.values = set()
        self.hits = 0
        self.total = 0
        self.len_min = bit_length
        self.len_max = bit_length

    @property
    def key(self):
        return (self.can_id, self.start_bit, self.bit_length, self.endian,
                self.scale, self.signed)

    @property
    def stable(self):
        return len(self.values) == 1

    @property
    def hit_ratio(self):
        return self.hits / self.total if self.total else 0.0

    def likely_length(self):
        """Prefer a byte-aligned width inside the ambiguous range."""
        for l in range(self.len_min, self.len_max + 1):
            if l % 8 == 0:
                return l
        return self.len_min

    def signals_cfg_line(self, name, payload_multiplier=1.0):
        return (
            "signal={name} id=0x{cid:X} start={sb} len={ln} endian={en} "
            "scale={sc:g} offset=0 signed={sg}{ext}".format(
                name=name, cid=self.can_id, sb=self.start_bit,
                ln=self.likely_length(), en="little" if self.endian == "LE" else "big",
                sc=self.scale * payload_multiplier, sg=1 if self.signed else 0,
                ext=" ext=1" if self.extended else "",
            )
        )


def search(frames, target, scales, min_bits, max_bits, tolerance, signed_too):
    """Tries every plausible field position against every frame."""
    candidates = {}
    by_id = defaultdict(list)
    for f in frames:
        by_id[(f.can_id, f.extended)].append(f)

    for (can_id, extended), id_frames in by_id.items():
        max_dlc = max(f.dlc for f in id_frames)
        total_bits = max_dlc * 8

        for bit_length in range(min_bits, min(max_bits, 32) + 1):
            for start_bit in range(0, total_bits):
                for endian in ("LE", "BE"):
                    if endian == "LE" and start_bit + bit_length > total_bits:
                        continue
                    for signed in ((False, True) if signed_too else (False,)):
                        for scale in scales:
                            cand = Candidate(can_id, extended, start_bit,
                                             bit_length, endian, scale, signed)
                            matched_any = False
                            for f in id_frames:
                                raw = (extract_le(f.data, start_bit, bit_length)
                                       if endian == "LE"
                                       else extract_be(f.data, start_bit, bit_length))
                                if raw is None:
                                    continue
                                if signed:
                                    raw = sign_extend(raw, bit_length)
                                physical = raw * scale
                                cand.total += 1
                                cand.values.add(round(physical, 6))
                                if abs(physical - target) <= tolerance:
                                    cand.hits += 1
                                    matched_any = True
                            if matched_any and cand.hits > 0:
                                prev = candidates.get(cand.key)
                                if prev is None or cand.hits > prev.hits:
                                    candidates[cand.key] = cand
    return list(candidates.values())


def main():
    ap = argparse.ArgumentParser(
        description="Find a known dashboard value in a raw CAN capture "
                    "(Blueprint §9.1 known-value method).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument("capture", help="capture file written by RawCanLogger")
    ap.add_argument("--value", type=float, required=True,
                    help="the value read off the dashboard (e.g. 12345 for 12345 km)")
    ap.add_argument("--scales", type=float, nargs="+",
                    default=[1.0, 0.1, 0.5, 0.01, 10.0],
                    help="resolutions to try (default: 1 0.1 0.5 0.01 10)")
    ap.add_argument("--tolerance", type=float, default=0.0,
                    help="accepted difference from --value (default 0)")
    ap.add_argument("--min-bits", type=int, default=8)
    ap.add_argument("--max-bits", type=int, default=32)
    ap.add_argument("--unit", default="", help="label for the report only")
    ap.add_argument("--signed", action="store_true",
                    help="also try signed interpretations")
    ap.add_argument("--stable", dest="stable", action="store_true", default=True,
                    help="require the field to be constant across the capture "
                         "(correct for a STATIONARY vehicle; default)")
    ap.add_argument("--no-stable", dest="stable", action="store_false",
                    help="allow the field to vary (use when the vehicle is moving)")
    ap.add_argument("--top", type=int, default=25, help="candidates to print")
    ap.add_argument("--name", default="odometer",
                    choices=["odometer", "soc", "speed", "ignition"],
                    help="signal name for the generated signals.cfg line")
    ap.add_argument("--payload-multiplier", type=float, default=None,
                    help="multiplies the emitted scale so the firmware decodes "
                         "straight into the Blueprint §7.1 payload unit "
                         "(metre / percent / km per hour). Searching the "
                         "odometer with --unit km needs 1000. Defaults to 1000 "
                         "for the odometer when --unit is km, otherwise 1.")
    args = ap.parse_args()

    # Blueprint §7.1 fixes the payload units. A scale that decodes to kilometres
    # would put the odometer 1000x low in every record, which is the kind of
    # mistake that only shows up after a week of fleet data.
    multiplier = args.payload_multiplier
    if multiplier is None:
        multiplier = 1000.0 if (args.name == "odometer"
                                and args.unit.lower() in ("km", "kilometre",
                                                          "kilometer")) else 1.0

    frames, skipped = parse_capture(args.capture)
    if not frames:
        print("No frames parsed. Is this a RawCanLogger capture?", file=sys.stderr)
        return 2

    ids = {(f.can_id, f.extended) for f in frames}
    span_ms = frames[-1].ts - frames[0].ts if len(frames) > 1 else 0

    print("=" * 74)
    print("CAN known-value search — Blueprint §9.1")
    print("=" * 74)
    print(f"capture      : {args.capture}")
    print(f"frames       : {len(frames)} ({skipped} lines skipped)")
    print(f"distinct IDs : {len(ids)}")
    print(f"time span    : {span_ms} ms")
    print(f"looking for  : {args.value:g} {args.unit}".rstrip())
    print(f"scales       : {' '.join(format(s, 'g') for s in args.scales)}")
    print(f"payload unit : x{multiplier:g} applied to the emitted scale "
          f"({'metre' if args.name == 'odometer' else 'percent' if args.name == 'soc' else 'km/h' if args.name == 'speed' else 'raw'})")
    print(f"stability    : {'required (stationary vehicle)' if args.stable else 'not required (moving vehicle)'}")
    print()

    cands = search(frames, args.value, args.scales, args.min_bits,
                   args.max_bits, args.tolerance, args.signed)

    if args.stable:
        cands = [c for c in cands if c.stable]

    # Collapse candidates that differ only in bit length.
    #
    # A field holding 123450 matches at any length from 17 bits upward, because
    # the extra high bits are all zero. Listing those as eight separate
    # candidates buries the finding in noise. The honest presentation is one
    # candidate with a length RANGE: the low end is the minimum that fits the
    # observed value, and the true length is only pinned down by watching the
    # value grow past it (Blueprint §9.1 step 6).
    groups = {}
    for c in cands:
        gk = (c.can_id, c.extended, c.start_bit, c.endian, c.scale, c.signed)
        g = groups.get(gk)
        if g is None:
            groups[gk] = {"rep": c, "min_len": c.bit_length, "max_len": c.bit_length}
        else:
            g["min_len"] = min(g["min_len"], c.bit_length)
            g["max_len"] = max(g["max_len"], c.bit_length)
            if c.hit_ratio > g["rep"].hit_ratio:
                g["rep"] = c

    collapsed = []
    for g in groups.values():
        rep = g["rep"]
        rep.len_min = g["min_len"]
        rep.len_max = g["max_len"]
        collapsed.append(rep)
    cands = collapsed

    # Rank by how consistently the field held the target, then prefer a
    # byte-aligned width (real DBC signals usually are), then the narrower field.
    def byte_aligned_penalty(c):
        return 0 if any(l % 8 == 0 for l in range(c.len_min, c.len_max + 1)) else 1

    cands.sort(key=lambda c: (-c.hit_ratio, byte_aligned_penalty(c),
                              c.len_min, c.can_id, c.start_bit))

    if not cands:
        print("NO CANDIDATES.")
        print()
        print("Next steps, in the order Blueprint §9.1 suggests:")
        print("  * retry with --scales 1 0.1 0.01 (the 0.1 km resolution case)")
        print("  * widen with --min-bits 8 --max-bits 32 and add --signed")
        print("  * relax with --tolerance 1")
        print("  * confirm the capture is from the right bus and bitrate")
        print("  * if the bus is silent entirely, see Blueprint §9.2 fallbacks")
        return 1

    print(f"{len(cands)} candidate(s). Showing up to {args.top}:")
    print()
    print(f"{'ID':>9} {'start':>5} {'len':>9} {'end':>4} {'scale':>7} "
          f"{'sgn':>4} {'match':>7} {'distinct':>9}")
    print("-" * 74)
    for c in cands[:args.top]:
        span = (str(c.len_min) if c.len_min == c.len_max
                else f"{c.len_min}-{c.len_max}")
        print(f"{'0x%X' % c.can_id:>9} {c.start_bit:>5} {span:>9} "
              f"{c.endian:>4} {c.scale:>7g} {'yes' if c.signed else 'no':>4} "
              f"{c.hit_ratio * 100:>6.1f}% {len(c.values):>9}")
    print()
    print("A length RANGE means the high bits were all zero in this capture, so "
          "the true width is not yet determined.")
    print("The suggested line below picks the byte-aligned width, which is the "
          "usual case — confirm it by watching the value grow.")

    print()
    print("=" * 74)
    print("signals.cfg lines for the strongest candidates — NOT YET VALIDATED")
    print("=" * 74)
    for c in cands[: min(5, len(cands))]:
        print("# " + c.signals_cfg_line(args.name, multiplier))
    print()
    print("Before any of these becomes a mapping (Blueprint §9.1 step 6):")
    print("  1. drive the vehicle a known distance, or change the value on purpose;")
    print("  2. re-capture and confirm the field tracked the dashboard;")
    print("  3. for speed, cross-check against GNSS ground speed;")
    print("  4. only then uncomment the line into /config/signals.cfg.")
    print()
    print("A candidate is a hypothesis. Master prompt §8: do not guess a CAN ID.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
