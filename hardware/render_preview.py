#!/usr/bin/env python3
"""
render_preview.py - draw bmt-can-logger.kicad_sch as schematic-preview.svg.

For reading the circuit without KiCad or EasyEDA (a browser is enough), and for
spotting overlapping labels. It draws what the file contains: symbol bodies,
pins with names and numbers, wire stubs, net labels, no-connect crosses, and
sheet text. It is a preview, not a replacement for opening the schematic in an
EDA tool.
"""

import sys
from pathlib import Path

from verify_schematic import find, one, parse

HERE = Path(__file__).resolve().parent
SCH = HERE / "bmt-can-logger.kicad_sch"
OUT = HERE / "schematic-preview.svg"

NET_COLOUR = {"GND": "#555", "+3V3": "#c0392b", "+5V": "#c0392b", "+5V_ESP": "#c0392b",
              "VIN_P": "#c0392b", "VIN_RAW": "#c0392b", "VIN_F": "#c0392b", "+4V0": "#c0392b",
              "MODEM_PWR": "#c0392b", "CANH": "#1f6feb", "CANL": "#1f6feb"}


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def main():
    root = parse(SCH.read_text(encoding="utf-8"))
    el = []
    lib = {}
    for sym in find(one(root, "lib_symbols"), "symbol"):
        lib[sym[1]] = sym

    def draw_graphics(sym, X, Y):
        for unit in find(sym, "symbol"):
            for g in unit[1:]:
                if not isinstance(g, list):
                    continue
                if g[0] == "rectangle":
                    s, e = one(g, "start"), one(g, "end")
                    x1, y1 = X + float(s[1]), Y - float(s[2])
                    x2, y2 = X + float(e[1]), Y - float(e[2])
                    fill = one(g, "fill")
                    ft = fill[1][1] if fill else "none"
                    colour = "#fffbe6" if ft == "background" else ("#333" if ft == "outline" else "none")
                    el.append(f'<rect x="{min(x1, x2)}" y="{min(y1, y2)}" width="{abs(x2 - x1)}" '
                              f'height="{abs(y2 - y1)}" fill="{colour}" stroke="#8b1a1a" stroke-width="0.25"/>')
                elif g[0] == "polyline":
                    pts = " ".join(f"{X + float(p[1])},{Y - float(p[2])}" for p in one(g, "pts")[1:])
                    el.append(f'<polyline points="{pts}" fill="none" stroke="#8b1a1a" stroke-width="0.25"/>')
                elif g[0] == "circle":
                    c, r = one(g, "center"), one(g, "radius")
                    el.append(f'<circle cx="{X + float(c[1])}" cy="{Y - float(c[2])}" r="{r[1]}" '
                              f'fill="none" stroke="#8b1a1a" stroke-width="0.25"/>')
                elif g[0] == "pin":
                    at = one(g, "at")
                    px, py, a = X + float(at[1]), Y - float(at[2]), int(float(at[3]))
                    ln = float(one(g, "length")[1])
                    dx, dy = {0: (ln, 0), 180: (-ln, 0), 90: (0, -ln), 270: (0, ln)}[a]
                    el.append(f'<line x1="{px}" y1="{py}" x2="{px + dx}" y2="{py + dy}" '
                              f'stroke="#8b1a1a" stroke-width="0.25"/>')
                    hidden = any(isinstance(x, list) and x[0] == "pin_names" and "hide" in x for x in sym)
                    if not hidden:
                        name, num = one(g, "name")[1], one(g, "number")[1]
                        inside = px + dx + (0.8 if a == 0 else -0.8)
                        anchor = "start" if a == 0 else "end"
                        el.append(f'<text x="{inside}" y="{py + 0.45}" font-size="1.3" '
                                  f'text-anchor="{anchor}" fill="#1a4d8b">{esc(name)}</text>')
                        el.append(f'<text x="{px + dx / 2}" y="{py - 0.4}" font-size="1" '
                                  f'text-anchor="middle" fill="#8b1a1a">{esc(num)}</text>')

    for inst in find(root, "symbol"):
        lid = one(inst, "lib_id")[1]
        at = one(inst, "at")
        X, Y = float(at[1]), float(at[2])
        draw_graphics(lib[lid], X, Y)
        for p in find(inst, "property"):
            if p[1] in ("Reference", "Value") and not any(isinstance(e, list) and "hide" in e
                                                           for e in find(p, "effects")):
                pa = one(p, "at")
                bold = ' font-weight="bold"' if p[1] == "Reference" else ""
                el.append(f'<text x="{pa[1]}" y="{pa[2]}" font-size="1.5" text-anchor="middle"'
                          f'{bold} fill="#222">{esc(p[2])}</text>')

    for w in find(root, "wire"):
        pts = one(w, "pts")[1:]
        el.append(f'<line x1="{pts[0][1]}" y1="{pts[0][2]}" x2="{pts[1][1]}" y2="{pts[1][2]}" '
                  f'stroke="#0a7d2c" stroke-width="0.25"/>')
    for lb in find(root, "label"):
        at = one(lb, "at")
        x, y, a = float(at[1]), float(at[2]), int(float(at[3]))
        anchor = "end" if a == 180 else "start"
        off = -0.5 if a == 180 else 0.5
        c = NET_COLOUR.get(lb[1], "#0a7d2c")
        el.append(f'<text x="{x + off}" y="{y - 0.4}" font-size="1.4" text-anchor="{anchor}" '
                  f'fill="{c}">{esc(lb[1])}</text>')
    for n in find(root, "no_connect"):
        at = one(n, "at")
        x, y = float(at[1]), float(at[2])
        el.append(f'<path d="M{x - 0.7},{y - 0.7}L{x + 0.7},{y + 0.7}M{x - 0.7},{y + 0.7}L{x + 0.7},{y - 0.7}" '
                  f'stroke="#1f6feb" stroke-width="0.25"/>')
    for t in find(root, "text"):
        at = one(t, "at")
        eff = one(t, "effects")
        font = one(eff, "font")
        size = float(one(font, "size")[1])
        just = one(eff, "justify")
        anchor = "end" if just and "right" in just else "start"
        weight = ' font-weight="bold"' if "bold" in font else ""
        style = ' font-style="italic"' if "italic" in font else ""
        el.append(f'<text x="{at[1]}" y="{at[2]}" font-size="{size}" text-anchor="{anchor}"'
                  f'{weight}{style} fill="#444">{esc(t[1])}</text>')

    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 420 297" width="1680" height="1188" '
           f'font-family="Consolas, monospace"><rect width="420" height="297" fill="#fff"/>'
           f'<rect x="5" y="5" width="410" height="287" fill="none" stroke="#8b1a1a" stroke-width="0.35"/>'
           + "".join(el) + "</svg>\n")
    OUT.write_text(svg, encoding="utf-8")
    print(f"wrote {OUT.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
