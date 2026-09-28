#!/usr/bin/env python3
"""
fake_device.py: the whole device, in one file, without the device.

    python tools/fake_device.py            http://127.0.0.1:8131
    python tools/fake_device.py --lan      also reachable from a phone on the WiFi

Serves BOTH pages and every endpoint they ask for, in the shapes the firmware
actually emits (StateJson.cpp, NotesStore.cpp, SystemHealth.cpp, WebDashboard.cpp):

    /            the engineering dashboard   web/dist/index.html
    /argo        the SELARIDE argo screen    web-argo/dist/index.html
    /api/state   /api/signals   /api/notes   /api/frames   /api/captures
    POST /api/argo   /api/mark   /api/sink

WHY THIS IS MORE THAN A STUB. One mock vehicle drives everything, and its values
are ENCODED BACK INTO CAN BYTES exactly as the Gelora E encodes them. So the
engineering dashboard evaluates the same `{le16(4)/256}` formulas from
notes.tsv against the same payloads a real bus would carry, and the argo screen
reads the same values through /api/signals. If a formula in notes.tsv and the
table in src/SignalDecoder.cpp ever disagree, this preview shows it before the
vehicle does.

WHAT IT CANNOT TELL YOU. Nothing here measures the device: not CPU load, not
rx_overrun under two clients, not how a phone behaves on a moving vehicle. It
answers "does the screen work and is the arithmetic right", and nothing else.
"""

import http.server
import json
import math
import pathlib
import socket
import socketserver
import sys
import threading
import time
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent
T0 = time.time()

# ---- the mock vehicle -------------------------------------------------------
# A shift a driver would recognise, not a ramp: leaving, open road, traffic, a
# crawl below the waiting threshold, then arriving. The crawl is there on
# purpose, because "below 5 km/jam while still moving" is the case that a route
# which only ever stops dead would never exercise.
ROUTE = [(20, 34), (30, 52), (25, 68), (40, 0), (30, 3), (25, 40), (15, 0)]
CYCLE = sum(d for d, _ in ROUTE)

state = {
    "odo": 30427.0,
    "soc": 66.0,
    "last": time.time(),
    "argo": {"running": False, "km": 0.0, "wait_ms": 0, "started": 0.0,
             "anchored": False, "odo_prev": None, "waiting": False},
    "reqs": 0,
    "frames": 0,
}


def target_speed(t):
    p = t % CYCLE
    for dur, kmh in ROUTE:
        if p < dur:
            return float(kmh)
        p -= dur
    return 0.0


speed_now = 0.0


LOCK = threading.Lock()


def step():
    """Advance the vehicle and the meter. Mirrors src/ArgoMeter.cpp.

    Called by a ticker thread, NOT by the request handlers. Two reasons, and the
    second one is why it is worth a thread. A vehicle that only moves when
    somebody asks is a vehicle that stands still on a slow poll. And when both
    dashboards are open, two pollers would each advance it, so the car would
    drive at double speed exactly in the situation the 29 Sep test is about.
    """
    global speed_now
    now = time.time()
    dt = min(1.0, now - state["last"])
    state["last"] = now

    tgt = target_speed(now - T0)
    speed_now += (tgt - speed_now) * min(1.0, dt * 0.55)
    if abs(speed_now) < 0.15:
        speed_now = 0.0

    state["odo"] += speed_now * dt / 3600.0
    state["soc"] = max(4.0, state["soc"] - (speed_now * dt) / 90000.0)
    state["frames"] = int((now - T0) * sum(RATES.values()))

    a = state["argo"]
    if a["running"]:
        a["waiting"] = speed_now < 5.0
        if a["waiting"]:
            a["wait_ms"] += int(dt * 1000)
        else:
            if a["odo_prev"] is not None:
                d = state["odo"] - a["odo_prev"]
                if 0.0 < d < 10.0:
                    a["km"] += d
            a["anchored"] = True
        a["odo_prev"] = state["odo"]
    else:
        a["waiting"] = False
        a["odo_prev"] = state["odo"]
    return speed_now


def ticker():
    while True:
        with LOCK:
            step()
        time.sleep(0.1)


def floor_step(v, s):
    return math.floor(v / s + 1e-6) * s if v > 0 else 0.0


# ---- the bus ----------------------------------------------------------------
# The values, put back into bytes the way the vehicle sends them, so the note
# formulas on the engineering dashboard have something real to chew on.
def le16(v):
    v = max(0, min(65535, int(round(v))))
    return [v & 0xFF, (v >> 8) & 0xFF]


# Per-identifier frame rate. The dashboard derives Hz from how fast the per-id
# counter grows between two polls, so a constant counter reads 0.0 Hz and hides
# the one column that tells a technician an identifier went quiet. These are
# plausible rates, NOT measured ones: the real rates come from the vehicle.
RATES = {
    0x18FFDC01: 20.0,
    0x18FEDCD5: 2.0,
    0x0CFF7D03: 10.0,
    0x0CFF7E03: 10.0,
    0x0CFF7902: 20.0,
    0x0CFF8203: 2.0,
    0x0CFF8303: 2.0,
    0x0CFF1601: 1.0,
}


def payloads(kmh):
    rpm = kmh * 87.8
    cur = 24 if kmh > 1 else 0
    return {
        # id: (extended, [bytes])
        0x18FFDC01: [0, 0, 0, 0] + le16(kmh * 256) + [0, 0],
        0x18FEDCD5: [min(255, int(kmh))] + le16(state["odo"]) + [0, 0, 0, 0, 0],
        0x0CFF7D03: [0, int(round(state["soc"] * 2))] + [0] * 6,
        0x0CFF7E03: [0, 97] + le16(350 - cur * 0.18) + le16(cur + 1000) + [71, 69],
        0x0CFF7902: [0, 0, 0, 0] + le16(rpm + 12000) + [0, 0],
        0x0CFF8203: [0, 3] + [0] * 6,
        0x0CFF8303: [0, 5] + [0] * 6,
        0x0CFF1601: [0, 0, 66] + [0] * 5,
    }


def read_notes():
    """The same notes.tsv the device would be carrying, in the device's shape."""
    out = []
    path = ROOT / "docs" / "evidence" / "dfsk-gelora-e" / "notes.tsv"
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if "\t" not in line:
            continue
        hexid, text = line.split("\t", 1)
        i = int(hexid, 16)
        out.append({"id": i, "x": 1 if i > 0x7FF else 0, "t": text})
    return out


NOTES = read_notes()


def state_json():
    kmh = speed_now
    now_ms = int((time.time() - T0) * 1000)
    ids = []
    for i, (cid, data) in enumerate(payloads(kmh).items()):
        hexs = "".join(f"{b:02X}" for b in data[:8])
        ids.append({"id": cid, "x": 1, "n": int(now_ms * RATES[cid] / 1000),
                    "t": now_ms - (i * 7), "l": 8, "d": hexs})
    return {
        "unit": "GELORA-E-FAKE", "fw": "fake_device.py",
        "uptime_s": int(time.time() - T0), "now_ms": now_ms,
        "can": {"running": True, "state": "RUNNING", "bitrate": 250000,
                "listen_only": True, "rx": state["frames"], "drop": 0, "missed": 0,
                "err": 0, "rec": 0, "silence_ms": 12, "uniq": len(ids), "idfull": False},
        "ids": ids, "hidden": 0,
        "gps": {"fix": True, "lat": -8.6705, "lon": 115.2126, "sats": 11, "hdop": 0.9,
                "sog": round(kmh, 1), "time_valid": True, "epoch": int(time.time()),
                "sentences": 4200, "badcrc": 0, "silent": False, "bytes": 260000,
                "lines": 4200, "view": 14, "trk": 11, "cnr": 38, "gsv": []},
        "env": {"enabled": True, "valid": True, "t": 31.4, "h": 68.0, "ok": 900,
                "read_err": 0, "crc_err": 0, "idle": True, "nores": 0, "hshake": 0,
                "trunc": 0, "rng": 0, "bits": 40, "raw": "1F 00 2A 00 49"},
        "fan": {"mode": "AUTO", "on": False, "t": 31.4, "tvalid": True,
                "run_s": 0, "trans": 0},
        "cap": {"sink": 0, "frames": 0, "bytes": 0, "path": "", "qdrop": 0,
                "net": {"on": False, "up": False, "n": 0, "b": 0, "drop": 0,
                        "dropall": 0, "stall": 0, "ses": 0}},
        "rssi": -58, "ip": "127.0.0.1", "wifi": "CONNECTED", "reqs": state["reqs"],
        "led": "OK", "clock": int(time.time()), "marks": 0, "notes": len(NOTES),
        "health": {
            "heap": {"total": 327680, "free": 177232, "min": 168000, "largest": 110000},
            "psram": {"total": 8388608, "free": 8200000},
            "cpu": [120, 80],
            "loop": {"expect": 50, "max": 62, "ever": 140},
            "q": {"depth": 0, "peak": 12, "cap": 256},
            "drv": {"backlog": 0, "peak": 3, "cap": 32, "missed": 0, "overrun": 0,
                    "berr": 0, "rec": 0},
            "fs": {"total": 14614528, "used": 20480},
            "app": {"used": 1057565, "size": 2097152},
            "tasks": [{"n": "can_reader", "c": 0, "s": 8192, "f": 4200},
                      {"n": "storage", "c": 1, "s": 8192, "f": 5100},
                      {"n": "web", "c": 1, "s": 8192, "f": 4600}],
        },
    }


def signals_json():
    kmh = speed_now
    a = state["argo"]
    sig = {
        "speed": {"v": round(kmh), "age_ms": 40},
        "odometer": {"v": round(state["odo"]), "age_ms": 120},
        "soc": {"v": round(state["soc"]), "age_ms": 300},
        "pack": {"v": round(350 - (24 if kmh > 1 else 0) * 0.18, 1), "age_ms": 80},
        "current": {"v": 24 if kmh > 1 else 0, "age_ms": 80},
        "battTempMax": {"v": 31, "age_ms": 80},
        "battTempMin": {"v": 29, "age_ms": 80},
        "rpm": {"v": round(kmh * 87.8), "age_ms": 40},
    }
    return {"signals": sig, "argo": argo_json(), "uptime_s": int(time.time() - T0)}


def argo_json():
    a = state["argo"]
    return {
        "running": a["running"], "waiting": a["waiting"], "anchored": a["anchored"],
        "km_isi": round(floor_step(a["km"], 0.1), 1),
        "wait_min": round(floor_step(a["wait_ms"] / 60000.0, 0.1), 1),
        "elapsed_s": int(time.time() - a["started"]) if a["running"] else 0,
        "odometer": round(state["odo"]),
    }


PAGES = {
    "/": ROOT / "web" / "dist" / "index.html",
    "/argo": ROOT / "web-argo" / "dist" / "index.html",
}
ASSETS = {
    "/img/driver.jpg": ("driver.jpg", "image/jpeg"),
    "/img/car.jpg": ("car.jpg", "image/jpeg"),
    "/img/mark.png": ("mark.png", "image/png"),
    "/font/mono.woff2": ("mono.woff2", "font/woff2"),
}


class Handler(http.server.BaseHTTPRequestHandler):
    def _send(self, code, ctype, body, cache="no-store"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", cache)
        self.end_headers()
        self.wfile.write(body)

    def _json(self, obj):
        self._send(200, "application/json", json.dumps(obj).encode())

    def do_GET(self):
        state["reqs"] += 1
        p = urllib.parse.urlparse(self.path).path
        if p in PAGES:
            return self._send(200, "text/html; charset=utf-8", PAGES[p].read_bytes())
        if p in ASSETS:
            name, ctype = ASSETS[p]
            return self._send(200, ctype,
                              (ROOT / "web-argo" / "assets" / name).read_bytes(),
                              cache="public, max-age=604800")
        if p == "/api/state":
            return self._json(state_json())
        if p == "/api/signals":
            return self._json(signals_json())
        if p == "/api/notes":
            return self._json({"available": True, "notes": NOTES})
        if p == "/api/frames":
            kmh = step()
            fr = []
            for cid, data in payloads(kmh).items():
                fr.append({"id": cid, "x": 1, "t": int((time.time() - T0) * 1000),
                           "l": 8, "d": "".join(f"{b:02X}" for b in data[:8])})
            return self._json({"frames": fr})
        if p == "/api/captures":
            return self._json({"current": "", "files": [], "journal": 0})
        self._send(404, "text/plain", b"no such route on the fake device")

    def do_POST(self):
        state["reqs"] += 1
        u = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(u.query)
        if u.path == "/api/argo":
            act = (q.get("action") or [""])[0]
            a = state["argo"]
            if act == "start" and not a["running"]:
                a.update(running=True, km=0.0, wait_ms=0, started=time.time(),
                         anchored=False, odo_prev=state["odo"])
                changed = True
            elif act == "stop" and a["running"]:
                a["running"] = False
                changed = True
            elif act in ("start", "stop"):
                changed = False
            else:
                return self._send(400, "text/plain", b"action must be start or stop")
            step()
            return self._json({"changed": changed, "argo": argo_json()})
        if u.path in ("/api/mark", "/api/note", "/api/sink", "/api/clear"):
            return self._send(200, "text/plain", b"ok (fake)")
        self._send(404, "text/plain", b"no")

    def log_message(self, *a):
        pass


def lan_address():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("10.255.255.255", 1))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def main():
    args = [a for a in sys.argv[1:]]
    lan = "--lan" in args
    if lan:
        args.remove("--lan")
    port = int(args[0]) if args else 8131
    host = "0.0.0.0" if lan else "127.0.0.1"

    for p in PAGES.values():
        if not p.exists():
            raise SystemExit(f"fake_device: {p} belum ada. Jalankan dulu: python tools/embed_web.py")

    state["last"] = time.time()
    threading.Thread(target=ticker, daemon=True).start()

    socketserver.ThreadingTCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer((host, port), Handler) as httpd:
        shown = lan_address() if lan else "127.0.0.1"
        print(f"alat tiruan  http://{shown}:{port}/       dashboard teknis")
        print(f"             http://{shown}:{port}/argo   layar argo")
        if lan:
            print("\n  Terbuka ke WiFi lokal. Dua gawai bisa membuka dua alamat itu,")
            print("  persis seperti rencana uji jalan.")
        print("\nCtrl+C untuk berhenti.")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nberhenti")


if __name__ == "__main__":
    main()
