#!/usr/bin/env python3
"""
verify_schematic.py - read bmt-can-logger.kicad_sch back and prove what it says.

It does not import gen_schematic.py. It parses the KiCad file with its own
s-expression reader, rebuilds connectivity from geometry alone (pin end
points, wire end points, label positions, no-connect flags), and then:

  1. compares the result with netlist.csv                    (the intent)
  2. fails on any pin that is neither connected nor flagged  (forgotten pins)
  3. fails on any net with a single connection               (typos)
  4. checks the safety rules of this board by name:
       - U3 (TJA1051T/3) pin 8 S sits on +3V3: Silent mode, transmitter off
       - no pin of J2 (the OBD-II connector) reaches a supply rail
       - nothing labelled with pin 16 / VIN reaches J2
       - GPIO19/GPIO20 (USB D-/D+) are unconnected
       - the modem only gets power through JP1

Exit 0 = every check passed.
"""

import csv
import re
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCH = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "bmt-can-logger.kicad_sch"


def tokenize(s):
    for m in re.finditer(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+', s):
        yield m.group(0)


def parse(s):
    stack, cur = [], []
    for t in tokenize(s):
        if t == "(":
            stack.append(cur)
            cur = []
        elif t == ")":
            done = cur
            cur = stack.pop()
            cur.append(done)
        else:
            cur.append(t[1:-1] if t.startswith('"') else t)
    return cur[0]


def find(node, key):
    return [c for c in node if isinstance(c, list) and c and c[0] == key]


def one(node, key):
    r = find(node, key)
    return r[0] if r else None


def key(x, y):
    return (round(float(x) * 100), round(float(y) * 100))    # 0.01 mm grid


def main():
    root = parse(SCH.read_text(encoding="utf-8"))
    errors = []

    # library pins
    libpins = {}
    for sym in find(one(root, "lib_symbols"), "symbol"):
        name = sym[1]
        pins = {}
        for unit in find(sym, "symbol"):
            for p in find(unit, "pin"):
                at = one(p, "at")
                pins[one(p, "number")[1]] = (float(at[1]), float(at[2]))
        libpins[name] = pins

    # instances -> pin end points on the sheet
    pin_at = {}                     # "REF.N" -> point
    for inst in find(root, "symbol"):
        lib = one(inst, "lib_id")[1]
        at = one(inst, "at")
        X, Y, rot = float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0
        if rot != 0 or one(inst, "mirror"):
            errors.append(f"{lib}: rotated/mirrored instances are not handled by this checker")
        ref = next(p[2] for p in find(inst, "property") if p[1] == "Reference")
        for num, (px, py) in libpins[lib].items():
            pin_at[f"{ref}.{num}"] = key(X + px, Y - py)

    # union-find over points
    parent = {}

    def fnd(a):
        parent.setdefault(a, a)
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        parent[fnd(a)] = fnd(b)

    for w in find(root, "wire"):
        pts = [key(p[1], p[2]) for p in one(w, "pts")[1:]]
        for a, b in zip(pts, pts[1:]):
            union(a, b)

    label_pts = defaultdict(list)
    for lb in find(root, "label"):
        at = one(lb, "at")
        label_pts[lb[1]].append(key(at[1], at[2]))
    for name, pts in label_pts.items():
        for p in pts:
            union(p, ("net", name))

    nc = {key(n[1][1], n[1][2]) for n in (one(x, "at") and [x, one(x, "at")] for x in find(root, "no_connect"))}

    # a pin is connected if its point shares a group with a named net
    group_net = {}
    for name in label_pts:
        group_net[fnd(("net", name))] = name
    nets = defaultdict(set)
    unconnected = []
    for rp, pt in pin_at.items():
        g = fnd(pt)
        if g in group_net:
            nets[group_net[g]].add(rp)
            if pt in nc:
                errors.append(f"{rp}: connected AND flagged no-connect")
        elif pt not in nc:
            unconnected.append(rp)
    if unconnected:
        errors.append(f"pins neither connected nor flagged: {sorted(unconnected)}")

    # two labels with different names on one group would short two nets
    by_group = defaultdict(set)
    for name in label_pts:
        by_group[fnd(("net", name))].add(name)
    for g, names in by_group.items():
        if len(names) > 1:
            errors.append(f"nets shorted together: {sorted(names)}")

    for n, m in nets.items():
        if len(m) < 2:
            errors.append(f"net {n} has one connection: {sorted(m)}")

    # 1. against the intent
    intent = {}
    with (HERE / "netlist.csv").open(encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            intent[row["net"]] = set(row["connections"].split())
    if intent != dict(nets):
        for n in sorted(set(intent) | set(nets)):
            if intent.get(n) != nets.get(n):
                errors.append(f"net {n}: netlist.csv {sorted(intent.get(n, []))} "
                              f"!= schematic {sorted(nets.get(n, []))}")

    # 4. safety rules
    def net_of(rp):
        return next((n for n, m in nets.items() if rp in m), None)

    rules = []
    rules.append(("U3 pin 8 (S) on +3V3 -> Silent mode", net_of("U3.8") == "+3V3"))
    rules.append(("U3 pin 5 (VIO) on +3V3", net_of("U3.5") == "+3V3"))
    rules.append(("U3 pin 3 (VCC) on +5V", net_of("U3.3") == "+5V"))
    rules.append(("U3 RXD -> GPIO4 (J10.4)", net_of("U3.4") == net_of("J10.4") == "CAN_RX"))
    rails = {"VIN_RAW", "VIN_F", "VIN_P", "+5V", "+5V_ESP", "+4V0", "MODEM_PWR", "+3V3"}
    j2 = {net_of(f"J2.{i}") for i in (1, 2, 3)}
    rules.append(("J2 (OBD-II) carries CANH, CANL, GND only", j2 == {"CANH", "CANL", "GND"}))
    rules.append(("J2 reaches no supply rail", not (j2 & rails)))
    rules.append(("GPIO19 (J11.20) unconnected", net_of("J11.20") is None))
    rules.append(("GPIO20 (J11.19) unconnected", net_of("J11.19") is None))
    rules.append(("GPIO0 (J11.14) = button", net_of("J11.14") == "BTN" and "SW1.1" in nets["BTN"]))
    rules.append(("MODEM_PWR fed only through JP1", {r.split(".")[0] for r in nets["MODEM_PWR"]} <= {"JP1", "J6", "C6", "C7"}))
    rules.append(("Terminator only via JP2", nets.get("TERM") == {"R5.2", "JP2.1"}))
    rules.append(("Every GND pin of the DevKit on GND", all(net_of(p) == "GND" for p in ("J10.22", "J11.1", "J11.21", "J11.22"))))
    for name, ok in rules:
        print(f"  {'PASS' if ok else 'FAIL'}  {name}")
        if not ok:
            errors.append(f"rule failed: {name}")

    print(f"pins {len(pin_at)}  connected {sum(len(m) for m in nets.values())}  "
          f"no-connect {len(nc)}  nets {len(nets)}")
    if errors:
        print("\nFAILED")
        for e in errors:
            print("  -", e)
        return 1
    print("OK - schematic geometry matches netlist.csv and every rule holds")
    return 0


if __name__ == "__main__":
    sys.exit(main())
