#!/usr/bin/env python3
"""
gen_schematic.py - the BMT CAN logger board, as data.

This file is the single source of truth for the circuit. It writes:

    bmt-can-logger.kicad_sch   KiCad 7 schematic (EasyEDA Pro: File > Import > KiCad)
    bmt-can-logger.kicad_pro   minimal project file so KiCad opens it as a project
    netlist.csv                net -> ref.pin, for review and for manual entry
    bom.csv                    what to buy and solder

Edit PARTS below, run this, then run verify_schematic.py. Never edit the
generated files by hand: the next run overwrites them.

Connections are made with net labels on short wire stubs, not drawn wires.
Every pin that is not used carries an explicit no-connect flag, so "forgotten"
and "deliberately unused" can be told apart.

One board, two roles:
  - test device   fit everything except the MODEM section
  - fleet unit    fit the MODEM section too
"""

import csv
import json
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
NAME = "bmt-can-logger"
NS = uuid.UUID("6f1c2d4e-0b7a-4c1e-9a55-3b1f0e2d7c90")   # stable uuids across runs
G = 2.54


def uid(*parts):
    return str(uuid.uuid5(NS, "/".join(str(p) for p in parts)))


def f(v):
    """Coordinates are multiples of 1.27 mm; print them without float noise."""
    s = f"{round(v, 4):.4f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


# --------------------------------------------------------------------------
# Symbols
#
# Box symbols: pins on the left and right, 2.54 mm pitch, pin length 2.54.
# Two-pin symbols: pin 1 left, pin 2 right, except diodes (KiCad convention:
# pin 1 = cathode, drawn on the right).
# --------------------------------------------------------------------------

FONT = "(effects (font (size 1.27 1.27)))"
FONT_HIDE = "(effects (font (size 1.27 1.27)) hide)"


class Sym:
    def __init__(self, name, pins, body, ref_prefix, hide_pin_text=False, box=None):
        self.name = name
        self.pins = pins            # list of (number, pin_name, x, y, angle)
        self.body = body            # list of graphic s-expressions
        self.ref_prefix = ref_prefix
        self.hide_pin_text = hide_pin_text
        self.box = box              # (half_w, top, bottom) for property placement

    def pin_xy(self, number):
        for n, _, x, y, a in self.pins:
            if n == number:
                return x, y, a
        raise KeyError(f"{self.name} has no pin {number}")

    def lib_sexpr(self):
        pn = "(pin_names (offset 0) hide) (pin_numbers hide)" if self.hide_pin_text \
            else "(pin_names (offset 1.016))"
        out = [f'    (symbol "bmt:{self.name}" {pn} (in_bom yes) (on_board yes)',
               f'      (property "Reference" "{self.ref_prefix}" (at 0 0 0) {FONT})',
               f'      (property "Value" "{self.name}" (at 0 0 0) {FONT})',
               f'      (property "Footprint" "" (at 0 0 0) {FONT_HIDE})',
               f'      (property "Datasheet" "" (at 0 0 0) {FONT_HIDE})',
               f'      (symbol "{self.name}_0_1"']
        out += [f"        {b}" for b in self.body]
        out.append("      )")
        out.append(f'      (symbol "{self.name}_1_1"')
        for n, nm, x, y, a in self.pins:
            out.append(f'        (pin passive line (at {f(x)} {f(y)} {a}) (length 2.54) '
                       f'(name "{nm}" {FONT}) (number "{n}" {FONT}))')
        out.append("      )")
        out.append("    )")
        return "\n".join(out)


def stroke(w=0.254):
    return f"(stroke (width {w}) (type default))"


def box_symbol(name, left, right, ref_prefix, width=15.24):
    """left/right: lists of (number, pin_name); None leaves a gap row."""
    rows = max(len(left), len(right))
    half_w = width / 2
    top = G * ((rows + 1) // 2)           # a multiple of G keeps every pin on grid
    bottom = top - G * (rows + 1)
    pins = []
    for side, lst in (("L", left), ("R", right)):
        for i, p in enumerate(lst):
            if p is None:
                continue
            y = top - G * (i + 1)
            if side == "L":
                pins.append((p[0], p[1], -(half_w + G), y, 0))
            else:
                pins.append((p[0], p[1], half_w + G, y, 180))
    body = [f"(rectangle (start {f(-half_w)} {f(top)}) (end {f(half_w)} {f(bottom)}) "
            f"{stroke()} (fill (type background)))"]
    return Sym(name, pins, body, ref_prefix, box=(half_w, top, bottom))


def two_pin(name, ref_prefix, body, diode=False):
    if diode:   # pin 2 = anode on the left, pin 1 = cathode on the right
        pins = [("2", "A", -5.08, 0, 0), ("1", "K", 5.08, 0, 180)]
    else:
        pins = [("1", "~", -5.08, 0, 0), ("2", "~", 5.08, 0, 180)]
    return Sym(name, pins, body, ref_prefix, hide_pin_text=True, box=(2.54, 1.27, -1.27))


def pl(*pts, w=0.254):
    xy = " ".join(f"(xy {f(x)} {f(y)})" for x, y in pts)
    return f"(polyline (pts {xy}) {stroke(w)} (fill (type none)))"


LEADS = [pl((-2.54, 0), (-1.27, 0)), pl((1.27, 0), (2.54, 0))]

SYMBOLS = {
    "R": two_pin("R", "R", [f"(rectangle (start -2.54 1.016) (end 2.54 -1.016) {stroke()} (fill (type none)))"]),
    "C": two_pin("C", "C", [pl((-0.508, 1.524), (-0.508, -1.524)), pl((0.508, 1.524), (0.508, -1.524)),
                            pl((-2.54, 0), (-0.508, 0)), pl((0.508, 0), (2.54, 0))]),
    "CP": two_pin("CP", "C", [pl((-0.508, 1.524), (-0.508, -1.524)),
                              f"(rectangle (start 0.508 1.524) (end 1.016 -1.524) {stroke()} (fill (type outline)))",
                              pl((-2.54, 0), (-0.508, 0)), pl((1.016, 0), (2.54, 0)),
                              pl((-1.778, 1.778), (-1.778, 0.762)), pl((-2.286, 1.27), (-1.27, 1.27))]),
    "L": two_pin("L", "L", [f"(rectangle (start -2.54 0.762) (end 2.54 -0.762) {stroke()} (fill (type outline)))"]),
    "D": two_pin("D", "D", [pl((-1.27, 1.27), (-1.27, -1.27), (1.27, 0), (-1.27, 1.27)),
                            pl((1.27, 1.27), (1.27, -1.27))] + LEADS, diode=True),
    "TVS": two_pin("TVS", "D", [pl((-1.27, 1.27), (-1.27, -1.27), (1.27, 0), (-1.27, 1.27)),
                                pl((0.762, 1.27), (1.27, 1.27), (1.27, -1.27), (1.778, -1.27))] + LEADS, diode=True),
    "PTC": two_pin("PTC", "F", [f"(rectangle (start -2.54 0.762) (end 2.54 -0.762) {stroke()} (fill (type none)))",
                                pl((-1.778, -1.524), (1.27, 1.524), (2.032, 1.524))]),
    "TP": Sym("TP", [("1", "~", -3.81, 0, 0)],
              [f"(circle (center 0 0) (radius 1.27) {stroke()} (fill (type none)))"],
              "TP", hide_pin_text=True, box=(1.27, 1.27, -1.27)),
}


def box(name, ref, left, right, width=15.24):
    SYMBOLS[name] = box_symbol(name, left, right, ref, width)


# ESP32-S3-DevKitC-1 headers, pin 1 at the top, as seen from the component side.
DEVKIT_J1 = ["3V3", "3V3", "RST", "GPIO4", "GPIO5", "GPIO6", "GPIO7", "GPIO15", "GPIO16",
             "GPIO17", "GPIO18", "GPIO8", "GPIO3", "GPIO46", "GPIO9", "GPIO10", "GPIO11",
             "GPIO12", "GPIO13", "GPIO14", "5V", "GND"]
DEVKIT_J3 = ["GND", "TX/GPIO43", "RX/GPIO44", "GPIO1", "GPIO2", "GPIO42", "GPIO41", "GPIO40",
             "GPIO39", "GPIO38", "GPIO37", "GPIO36", "GPIO35", "GPIO0", "GPIO45", "GPIO48",
             "GPIO47", "GPIO21", "GPIO20", "GPIO19", "GND", "GND"]

box("DevKitC_J1", "J", [], [(str(i + 1), n) for i, n in enumerate(DEVKIT_J1)], 12.7)
box("DevKitC_J3", "J", [(str(i + 1), n) for i, n in enumerate(DEVKIT_J3)], [], 12.7)
box("LM2576HV", "U", [("1", "VIN"), ("5", "ON/OFF"), ("3", "GND")], [("2", "OUT"), ("4", "FB")])
box("TJA1051T3", "U", [("1", "TXD"), ("4", "RXD"), ("8", "S"), ("5", "VIO")],
    [("3", "VCC"), ("7", "CANH"), ("6", "CANL"), ("2", "GND")])
box("PESD2CAN", "D", [("1", "CANH"), ("2", "CANL")], [("3", "GND")], 10.16)
box("NMOS_GDS", "Q", [("1", "G")], [("2", "D"), ("3", "S")], 10.16)       # IRLZ44N TO-220
box("NMOS_SOT23", "Q", [("1", "G")], [("3", "D"), ("2", "S")], 10.16)     # BSS138
box("NMOS_TO92", "Q", [("2", "G")], [("3", "D"), ("1", "S")], 10.16)      # 2N7000
box("LED_BICOLOR_CC", "D", [("1", "RED"), ("3", "GREEN")], [("2", "K")], 10.16)
box("SW_PUSH", "SW", [("1", "1")], [("2", "2")], 7.62)
for n in (2, 3, 4, 7):
    box(f"CONN_{n}", "J", [], [(str(i + 1), f"P{i + 1}") for i in range(n)], 15.24)
box("JUMPER_2", "JP", [("1", "1")], [("2", "2")], 7.62)
box("JUMPER_3", "JP", [], [("1", "1"), ("2", "COM"), ("3", "3")], 12.7)


# --------------------------------------------------------------------------
# The circuit. Each part: ref, symbol, value, (x, y) on the A3 sheet,
# {pin: net or None}, footprint, bom fields.
#   None  = deliberately unused, gets a no-connect flag
# Pin names on connectors are overridden per part via "pinnames".
# --------------------------------------------------------------------------

PARTS = []


def part(ref, sym, value, at, pins, fp="", mpn="", desc="", section="", fit="ALL", pinnames=None):
    if pinnames:    # a connector: its own symbol, so the pin names say what goes there
        base = SYMBOLS[sym]
        sym = f"{sym}_{ref}"
        SYMBOLS[sym] = Sym(sym, [(n, pinnames.get(n, nm), x, y, a) for n, nm, x, y, a in base.pins],
                           base.body, base.ref_prefix, box=base.box)
    PARTS.append(dict(ref=ref, sym=sym, value=value, at=at, pins=pins, fp=fp, mpn=mpn,
                      desc=desc, section=section, fit=fit, pinnames=pinnames or {}))


FP_R = "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal"
FP_C = "Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm"
FP_HDR = "Connector_PinHeader_2.54mm:PinHeader_1x{n:02d}_P2.54mm_Vertical"
FP_SOCK22 = "Connector_PinSocket_2.54mm:PinSocket_1x22_P2.54mm_Vertical"
FP_TB = "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-{n}-5.08_1x{n:02d}_P5.08mm_Horizontal"
FP_DO201 = "Diode_THT:D_DO-201AD_P15.24mm_Horizontal"
FP_DO41 = "Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal"
FP_SOT23 = "Package_TO_SOT_SMD:SOT-23"

# ---- 1. Power input and protection -------------------------------------
S = "1 POWER IN"
part("J1", "CONN_2", "12V IN", (30.48, 45.72), {"1": "VIN_RAW", "2": "GND"}, FP_TB.format(n=2),
     "Phoenix MKDS 1,5/2-5.08 (or any 5.08 mm 2-way)", "Switched 12 V from an ACC fuse tap. NEVER OBD-II pin 16", S,
     pinnames={"1": "+12V_ACC", "2": "GND"})
part("F1", "PTC", "MF-RX160/72", (60.96, 43.18), {"1": "VIN_RAW", "2": "VIN_F"}, "",
     "Bourns MF-RX160/72", "Resettable fuse, 1.6 A hold, 72 V, radial", S)
part("D2", "TVS", "1.5KE27A", (60.96, 53.34), {"1": "VIN_F", "2": "GND"}, "Diode_THT:D_DO-201_P15.24mm_Horizontal",
     "Littelfuse 1.5KE27A", "TVS 1500 W, unidirectional. Clamps surges; conducts on reverse polarity so F1 trips", S)
part("D1", "D", "1N5822", (60.96, 63.5), {"2": "VIN_F", "1": "VIN_P"}, FP_DO201,
     "1N5822", "Schottky 3 A 40 V, series reverse-polarity block", S)
part("C1", "CP", "100uF 50V", (60.96, 73.66), {"1": "VIN_P", "2": "GND"}, "Capacitor_THT:CP_Radial_D8.0mm_P3.50mm",
     "", "Electrolytic, 105 C", S)
part("C2", "C", "100nF 50V", (60.96, 83.82), {"1": "VIN_P", "2": "GND"}, FP_C, "", "Ceramic X7R", S)
part("TP1", "TP", "VIN_P", (35.56, 93.98), {"1": "VIN_P"}, "TestPoint:TestPoint_THTPad_D2.0mm_Drill1.0mm", "", "Test point", S)
part("TP2", "TP", "GND", (35.56, 104.14), {"1": "GND"}, "TestPoint:TestPoint_THTPad_D2.0mm_Drill1.0mm", "", "Test point", S)

# ---- 2. 5 V buck -----------------------------------------------------------
S = "2 BUCK 5V"
part("U1", "LM2576HV", "LM2576HVT-5.0", (114.3, 50.8),
     {"1": "VIN_P", "5": "GND", "3": "GND", "2": "SW5", "4": "+5V"},
     "Package_TO_SOT_THT:TO-220-5_P3.4x3.7mm_StaggerOdd_Lead3.8mm_Vertical",
     "TI LM2576HVT-5.0", "Buck 5 V 3 A, 60 V input. FB senses the output on the fixed version", S)
part("D7", "D", "1N5822", (114.3, 68.58), {"2": "GND", "1": "SW5"}, FP_DO201, "1N5822", "Catch diode", S)
part("L1", "L", "100uH 3A", (114.3, 78.74), {"1": "SW5", "2": "+5V"}, "",
     "", "Power inductor 100 uH, >= 3 A saturation (toroid or drum)", S)
part("C3", "CP", "1000uF 16V", (114.3, 88.9), {"1": "+5V", "2": "GND"}, "Capacitor_THT:CP_Radial_D10.0mm_P5.00mm",
     "", "Electrolytic low-ESR, 105 C", S)
part("C4", "C", "100nF", (114.3, 99.06), {"1": "+5V", "2": "GND"}, FP_C, "", "Ceramic", S)
part("D3", "D", "1N5819", (144.78, 99.06), {"2": "+5V", "1": "+5V_ESP"}, FP_DO41,
     "1N5819", "Stops USB 5 V back-feeding this board when the DevKit is plugged into a PC", S)
part("TP3", "TP", "+5V", (144.78, 88.9), {"1": "+5V"}, "TestPoint:TestPoint_THTPad_D2.0mm_Drill1.0mm", "", "Test point", S)

# ---- 3. Modem supply (fleet unit only) ------------------------------------
S = "3 BUCK 4V0 (MODEM)"
M = "FLEET"
part("U2", "LM2576HV", "LM2576HVT-ADJ", (190.5, 50.8),
     {"1": "VIN_P", "5": "GND", "3": "GND", "2": "SW4", "4": "FB4"},
     "Package_TO_SOT_THT:TO-220-5_P3.4x3.7mm_StaggerOdd_Lead3.8mm_Vertical",
     "TI LM2576HVT-ADJ", "Buck 3.94 V for modem VBAT: 1.23 x (1 + 2k2/1k)", S, M)
part("D8", "D", "1N5822", (190.5, 68.58), {"2": "GND", "1": "SW4"}, FP_DO201, "1N5822", "Catch diode", S, M)
part("L2", "L", "100uH 3A", (190.5, 78.74), {"1": "SW4", "2": "+4V0"}, "",
     "", "Power inductor 100 uH, >= 3 A saturation. Modem bursts reach ~2 A", S, M)
part("R1", "R", "2k2 1%", (220.98, 60.96), {"1": "+4V0", "2": "FB4"}, FP_R, "", "Feedback top", S, M)
part("R2", "R", "1k 1%", (220.98, 71.12), {"1": "FB4", "2": "GND"}, FP_R, "", "Feedback bottom", S, M)
part("C5", "CP", "1000uF 10V", (190.5, 88.9), {"1": "+4V0", "2": "GND"}, "Capacitor_THT:CP_Radial_D10.0mm_P5.00mm",
     "", "Electrolytic low-ESR", S, M)
part("JP1", "JUMPER_3", "MODEM SUPPLY", (228.6, 88.9), {"1": "+4V0", "2": "MODEM_PWR", "3": "VIN_P"},
     FP_HDR.format(n=3), "", "1-2: 4.0 V to a bare-module VBAT. 2-3: 12 V to a breakout with its own regulator", S, M,
     pinnames={"1": "4V0", "2": "MODEM", "3": "12V"})
part("TP4", "TP", "+4V0", (220.98, 81.28), {"1": "+4V0"}, "TestPoint:TestPoint_THTPad_D2.0mm_Drill1.0mm", "", "Test point", S, M)

# ---- 4. ESP32-S3-DevKitC-1 on sockets ------------------------------------
S = "4 ESP32-S3"
j1 = {str(i + 1): None for i in range(22)}
j1.update({"1": "+3V3", "2": "+3V3", "4": "CAN_RX", "5": "CAN_TX", "8": "DHT_DATA",
           "9": "MDM_RX_3V3", "10": "MDM_TX_3V3", "11": "GPS_TXD", "19": "FAN_IO",
           "20": "PWRKEY_IO", "21": "+5V_ESP", "22": "GND"})
j3 = {str(i + 1): None for i in range(22)}
j3.update({"1": "GND", "14": "BTN", "17": "LED_G_IO", "18": "LED_R_IO", "21": "GND", "22": "GND"})
part("J10", "DevKitC_J1", "DevKitC-1 J1", (40.64, 170.18), j1, FP_SOCK22,
     "2.54 mm female socket 1x22", "Socket for ESP32-S3-DevKitC-1 (N16R8) header J1", S)
part("J11", "DevKitC_J3", "DevKitC-1 J3", (91.44, 170.18), j3, FP_SOCK22,
     "2.54 mm female socket 1x22", "Socket for ESP32-S3-DevKitC-1 (N16R8) header J3", S)

# ---- 5. CAN, listen-only in hardware -------------------------------------
S = "5 CAN (LISTEN-ONLY)"
part("U3", "TJA1051T3", "TJA1051T/3", (160.02, 139.7),
     {"1": "TXD_T", "4": "CAN_RX", "8": "+3V3", "5": "+3V3",
      "3": "+5V", "7": "CANH", "6": "CANL", "2": "GND"},
     "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm", "NXP TJA1051T/3",
     "CAN transceiver. S tied HIGH = Silent mode: the transmitter is off in silicon", S)
part("R3", "R", "1k", (132.08, 160.02), {"1": "CAN_TX", "2": "TXD_T"}, FP_R, "", "Series, GPIO5 to TXD", S)
part("R4", "R", "10k", (132.08, 170.18), {"1": "TXD_T", "2": "+3V3"}, FP_R, "", "TXD held recessive", S)
part("C8", "C", "100nF", (190.5, 160.02), {"1": "+5V", "2": "GND"}, FP_C, "", "VCC decoupling, at U3 pin 3", S)
part("C9", "C", "100nF", (190.5, 170.18), {"1": "+3V3", "2": "GND"}, FP_C, "", "VIO decoupling, at U3 pin 5", S)
part("D4", "PESD2CAN", "PESD2CAN", (160.02, 187.96), {"1": "CANH", "2": "CANL", "3": "GND"}, FP_SOT23,
     "Nexperia PESD2CAN", "CAN bus ESD protection", S)
part("R5", "R", "120R", (160.02, 205.74), {"1": "CANH", "2": "TERM"}, FP_R, "", "Terminator, bench only", S)
part("JP2", "JUMPER_2", "TERM BENCH ONLY", (190.5, 205.74), {"1": "TERM", "2": "CANL"}, FP_HDR.format(n=2),
     "", "OPEN in a vehicle (it is already 60 R). CLOSED on a two-node bench", S)
part("J2", "CONN_3", "OBD-II CAN", (215.9, 139.7), {"1": "CANH", "2": "CANL", "3": "GND"}, FP_TB.format(n=3),
     "Phoenix MKDS 1,5/3-5.08", "To OBD-II pin 6 (CANH), 14 (CANL), 5 (signal GND). No pin 16 position exists", S,
     pinnames={"1": "OBD6_CANH", "2": "OBD14_CANL", "3": "OBD5_GND"})

# ---- 6. Peripherals ------------------------------------------------------
S = "6 PERIPHERALS"
part("J3", "CONN_4", "GPS NEO-6M", (256.54, 139.7), {"1": "+5V", "2": "GND", "3": "GPS_TXD", "4": None},
     FP_HDR.format(n=4), "", "GY-GPS6MV2 module. Its RX stays unconnected: GPIO19 is USB D- on the S3", S,
     pinnames={"1": "VCC", "2": "GND", "3": "GPS_TX", "4": "GPS_RX"})
part("J4", "CONN_3", "DHT22", (256.54, 160.02), {"1": "+3V3", "2": "DHT_DATA", "3": "GND"},
     FP_HDR.format(n=3), "", "DHT22 / AM2302", S, pinnames={"1": "VCC", "2": "DATA", "3": "GND"})
part("R6", "R", "10k", (256.54, 175.26), {"1": "DHT_DATA", "2": "+3V3"}, FP_R, "", "DHT22 data pull-up", S)
part("Q1", "NMOS_GDS", "IRLZ44N", (256.54, 187.96), {"1": "FAN_G", "2": "FAN_SW", "3": "GND"},
     "Package_TO_SOT_THT:TO-220-3_Vertical", "IRLZ44N", "Fan low-side switch (as on the proven prototype)", S)
part("R7", "R", "330R", (231.14, 198.12), {"1": "FAN_IO", "2": "FAN_G"}, FP_R, "", "Gate series", S)
part("R8", "R", "100k", (231.14, 208.28), {"1": "FAN_G", "2": "GND"}, FP_R, "", "Gate pull-down: fan off while the ESP32 boots", S)
part("J5", "CONN_2", "FAN", (292.1, 187.96), {"1": "FAN_V", "2": "FAN_SW"}, FP_HDR.format(n=2),
     "", "Fan connector", S, pinnames={"1": "FAN+", "2": "FAN-"})
part("D5", "D", "1N5819", (292.1, 208.28), {"2": "FAN_SW", "1": "FAN_V"}, FP_DO41, "1N5819", "Fan flyback", S)
part("JP3", "JUMPER_3", "FAN VOLTAGE", (322.58, 187.96), {"1": "+5V", "2": "FAN_V", "3": "VIN_P"},
     FP_HDR.format(n=3), "", "1-2: 5 V fan. 2-3: 12 V fan", S, pinnames={"1": "5V", "2": "FAN", "3": "12V"})
part("D6", "LED_BICOLOR_CC", "LED R/G CC", (256.54, 233.68), {"1": "LED_R_A", "3": "LED_G_A", "2": "GND"},
     "LED_THT:LED_D5.0mm-3", "", "5 mm bicolour, common cathode. CHECK the pin order of the LED you buy", S)
part("R9", "R", "330R", (228.6, 228.6), {"1": "LED_R_IO", "2": "LED_R_A"}, FP_R, "", "LED red", S)
part("R10", "R", "330R", (228.6, 238.76), {"1": "LED_G_IO", "2": "LED_G_A"}, FP_R, "", "LED green", S)
part("SW1", "SW_PUSH", "BUTTON", (292.1, 233.68), {"1": "BTN", "2": "GND"}, "Button_Switch_THT:SW_PUSH_6mm",
     "", "6 mm tactile. GPIO0: held at power-up it enters download mode", S)

# ---- 7. Modem connector (fleet unit only) ---------------------------------
S = "7 MODEM A7670C"
part("J6", "CONN_7", "A7670C", (391.16, 139.7),
     {"1": "MODEM_PWR", "2": "GND", "3": "GND", "4": "MDM_TXD_LV", "5": "MDM_RXD_LV", "6": "PWRKEY", "7": "MDM_VREF"},
     FP_HDR.format(n=7), "", "To the A7670C board. MATCH THIS TO THE BOARD YOU BUY before ordering the PCB", S, M,
     pinnames={"1": "VBAT/VIN", "2": "GND", "3": "GND", "4": "M_TXD", "5": "M_RXD", "6": "PWRKEY", "7": "VREF"})
part("C6", "CP", "2200uF 10V", (398.78, 170.18), {"1": "MODEM_PWR", "2": "GND"},
     "Capacitor_THT:CP_Radial_D12.5mm_P5.00mm", "", "Low-ESR bulk AT the modem connector (fleet 01-PINOUT)", S, M)
part("C7", "C", "100nF", (398.78, 180.34), {"1": "MODEM_PWR", "2": "GND"}, FP_C, "", "Decoupling", S, M)
part("Q3", "NMOS_SOT23", "BSS138", (360.68, 139.7), {"1": "MDM_VREF", "2": "MDM_TXD_LV", "3": "MDM_RX_3V3"},
     FP_SOT23, "BSS138", "Level shift modem TXD -> GPIO16", S, M)
part("Q4", "NMOS_SOT23", "BSS138", (360.68, 160.02), {"1": "MDM_VREF", "2": "MDM_RXD_LV", "3": "MDM_TX_3V3"},
     FP_SOT23, "BSS138", "Level shift GPIO17 -> modem RXD", S, M)
part("R11", "R", "10k", (322.58, 139.7), {"1": "MDM_TXD_LV", "2": "MDM_VREF"}, FP_R, "", "Pull-up, modem side", S, M)
part("R12", "R", "10k", (322.58, 149.86), {"1": "MDM_RX_3V3", "2": "+3V3"}, FP_R, "", "Pull-up, ESP32 side", S, M)
part("R13", "R", "10k", (322.58, 160.02), {"1": "MDM_RXD_LV", "2": "MDM_VREF"}, FP_R, "", "Pull-up, modem side", S, M)
part("R14", "R", "10k", (322.58, 170.18), {"1": "MDM_TX_3V3", "2": "+3V3"}, FP_R, "", "Pull-up, ESP32 side", S, M)
part("JP4", "JUMPER_2", "VREF=3V3", (360.68, 177.8), {"1": "MDM_VREF", "2": "+3V3"}, FP_HDR.format(n=2),
     "", "CLOSE only if the modem board's UART is 3.3 V and has no VREF output of its own", S, M)
part("Q2", "NMOS_TO92", "2N7000", (360.68, 205.74), {"2": "PWRKEY_G", "3": "PWRKEY", "1": "GND"},
     "Package_TO_SOT_THT:TO-92_Inline", "2N7000", "Pulls PWRKEY low on command (GPIO14)", S, M)
part("R15", "R", "1k", (322.58, 200.66), {"1": "PWRKEY_IO", "2": "PWRKEY_G"}, FP_R, "", "Gate series", S, M)
part("R16", "R", "100k", (322.58, 210.82), {"1": "PWRKEY_G", "2": "GND"}, FP_R, "", "Gate pull-down", S, M)
part("JP5", "JUMPER_2", "AUTO-ON", (398.78, 205.74), {"1": "PWRKEY", "2": "GND"}, FP_HDR.format(n=2),
     "", "Fit OPEN. Closing it holds PWRKEY low for auto power-on, which the A7670 manual does not promise: test on the real module first", S, M)

NOTES = [
    (20.32, 27.94, 3.0, "BMT CAN logger - ESP32-S3 + TJA1051T/3 (listen-only) + GNSS + DHT22 + fan + A7670C (optional)"),
    (20.32, 33.02, 1.8, "Generated by hardware/gen_schematic.py - do not edit by hand. Parts marked FLEET are left off the test device."),
    (20.32, 38.1, 1.5, "POWER: switched 12 V from an ACC fuse tap with an inline 3 A blade fuse. OBD-II pin 16 is NEVER connected (fleet 01-PINOUT, Blueprint 5.2)."),
    (20.32, 132.08, 1.8, "ESP32-S3-DevKitC-1 N16R8 plugs into J10/J11. Do not use GPIO19/20 (USB), 33-37 (octal PSRAM)."),
    (127.0, 124.46, 1.8, "LISTEN-ONLY IN SILICON: U3 pin 8 (S) is tied to 3V3 = Silent mode. No jumper, no GPIO can undo it."),
    (127.0, 220.98, 1.5, "JP2 OPEN in a vehicle (bus already 60 R). Measure OBD 6-14 with ignition off before connecting."),
    (304.8, 124.46, 1.8, "MODEM (FLEET only). A7670 UART is 1.8 V: Q3/Q4 shift it. Match J6 to the actual A7670C board before ordering."),
]


# --------------------------------------------------------------------------
# Emit
# --------------------------------------------------------------------------

ROOT = uid("root")


def emit():
    used_syms = sorted({p["sym"] for p in PARTS})
    out = ["(kicad_sch (version 20230121) (generator eeschema)",
           f'  (uuid {ROOT})',
           '  (paper "A3")',
           '  (title_block (title "BMT CAN logger") (date "2026-09-19") (rev "A")',
           '    (company "PT Bali Mikro Teknologi") (comment 1 "Listen-only CAN capture unit, hand-solder build"))',
           "  (lib_symbols"]
    for s in used_syms:
        out.append(SYMBOLS[s].lib_sexpr())
    out.append("  )")

    nets = {}     # net -> list of ref.pin
    for p in PARTS:
        sym = SYMBOLS[p["sym"]]
        X, Y = p["at"]
        # every symbol pin must be listed explicitly: connected or None
        sym_pins = {n for n, *_ in sym.pins}
        if set(p["pins"]) != sym_pins:
            raise SystemExit(f"{p['ref']}: pins {sorted(p['pins'])} != symbol pins {sorted(sym_pins)}")
        for num, net in p["pins"].items():
            px, py, ang = sym.pin_xy(num)
            sx, sy = X + px, Y - py
            if net is None:
                out.append(f"  (no_connect (at {f(sx)} {f(sy)}) (uuid {uid(p['ref'], num, 'nc')}))")
                continue
            nets.setdefault(net, []).append(f"{p['ref']}.{num}")
            # stub away from the body, label at its end
            dx, dy = {0: (-G, 0), 180: (G, 0), 90: (0, G), 270: (0, -G)}[ang]
            ex, ey = sx + dx, sy + dy
            out.append(f"  (wire (pts (xy {f(sx)} {f(sy)}) (xy {f(ex)} {f(ey)})) "
                       f"(stroke (width 0) (type default)) (uuid {uid(p['ref'], num, 'w')}))")
            if ang == 0:
                just, la = "right bottom", 180
            elif ang == 180:
                just, la = "left bottom", 0
            else:
                just, la = "left bottom", 90
            out.append(f'  (label "{net}" (at {f(ex)} {f(ey)} {la}) (fields_autoplaced) '
                       f'(effects (font (size 1.27 1.27)) (justify {just})) (uuid {uid(p["ref"], num, "l")}))')

        hw, top, bot = sym.box
        props = [("Reference", p["ref"], X, Y - top - 1.27, False),
                 ("Value", p["value"], X, Y - bot + 1.905, False),
                 ("Footprint", p["fp"], X, Y, True),
                 ("Datasheet", "", X, Y, True),
                 ("MPN", p["mpn"], X, Y, True),
                 ("Fit", p["fit"], X, Y, True)]
        out.append(f'  (symbol (lib_id "bmt:{p["sym"]}") (at {f(X)} {f(Y)} 0) (unit 1) '
                   f'(in_bom yes) (on_board yes) (dnp no)')
        out.append(f"    (uuid {uid(p['ref'])})")
        for k, v, x, y, hide in props:
            out.append(f'    (property "{k}" "{v}" (at {f(x)} {f(y)} 0) '
                       f'{FONT_HIDE if hide else FONT})')
        for num in p["pins"]:
            out.append(f'    (pin "{num}" (uuid {uid(p["ref"], num, "p")}))')
        out.append(f'    (instances (project "{NAME}" (path "/{ROOT}" (reference "{p["ref"]}") (unit 1))))')
        out.append("  )")

    for i, (x, y, size, txt) in enumerate(NOTES):
        out.append(f'  (text "{txt}" (at {f(x)} {f(y)} 0) (effects (font (size {size} {size}) bold) '
                   f'(justify left bottom)) (uuid {uid("note", i)}))')

    out.append('  (sheet_instances (path "/" (page "1")))')
    out.append(")")
    return "\n".join(out) + "\n", nets


def main():
    sch, nets = emit()
    (HERE / f"{NAME}.kicad_sch").write_text(sch, encoding="utf-8", newline="\n")
    (HERE / f"{NAME}.kicad_pro").write_text(json.dumps({"meta": {"filename": f"{NAME}.kicad_pro", "version": 1}},
                                                       indent=2) + "\n", encoding="utf-8", newline="\n")

    single = {n: m for n, m in nets.items() if len(m) < 2}
    if single:
        raise SystemExit(f"nets with one connection (a typo, or a missing part): {single}")

    with (HERE / "netlist.csv").open("w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["net", "connections"])
        for n in sorted(nets):
            w.writerow([n, " ".join(sorted(nets[n], key=lambda s: (s.split(".")[0], s)))])

    groups = {}
    for p in PARTS:
        key = (p["value"], p["mpn"], p["fp"], p["desc"] if p["sym"] in ("CONN_2", "CONN_3", "CONN_4", "CONN_7",
                                                                        "JUMPER_2", "JUMPER_3", "LM2576HV") else "",
               p["fit"])
        groups.setdefault(key, []).append(p)
    with (HERE / "bom.csv").open("w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["fit", "qty", "refs", "value", "part / MPN", "footprint", "notes"])
        rows = []
        for (value, mpn, fp, _, fit), ps in groups.items():
            refs = sorted((p["ref"] for p in ps), key=lambda r: (r.rstrip("0123456789"), int(''.join(c for c in r if c.isdigit()) or 0)))
            notes = []
            for q in sorted(ps, key=lambda q: refs.index(q["ref"])):
                if q["desc"] and q["desc"] not in [n.split(": ", 1)[-1] for n in notes]:
                    notes.append(q["desc"] if len(ps) == 1 else f'{q["ref"]}: {q["desc"]}')
            rows.append([fit, len(ps), " ".join(refs), value, mpn, fp, " | ".join(notes)])
        rows.sort(key=lambda r: (r[0] != "ALL", r[2]))
        w.writerows(rows)
        w.writerow(["ALL", 1, "-", "ESP32-S3-DevKitC-1 N16R8", "Espressif", "plugs into J10/J11", "the module already in use"])
        w.writerow(["ALL", 4, "-", "M3 mounting hole", "", "MountingHole:MountingHole_3.2mm_M3", "corners"])

    print(f"parts {len(PARTS)}  nets {len(nets)}  -> {NAME}.kicad_sch, netlist.csv, bom.csv")


if __name__ == "__main__":
    main()
