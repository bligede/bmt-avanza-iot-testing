'use strict';
/* =============================================================================
   BMT CAN bring-up dashboard.

   Reads /api/state every 500 ms and paints it. Every value arrives from the
   device already as a number or as raw bytes; this page never claims to know
   what a byte MEANS. Where it decodes — the signal probe — it says openly that
   the decode is the operator's guess.
   ============================================================================= */

const $ = i => document.getElementById(i);
const hex = (n, w) => '0x' + n.toString(16).toUpperCase().padStart(w, '0');
const tag = (t, c) => '<span class="tag ' + c + '">' + t + '</span>';
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const note = (t, c) => t ? '<p class="note' + (c ? ' ' + c : '') + '">' + t + '</p>' : '';
const kb = b => (b / 1024).toFixed(1) + ' KB';
const bytesOf = h => { const o = []; for (let i = 0; i + 1 < h.length; i += 2) o.push(parseInt(h.substr(i, 2), 16)); return o; };

/* Keyed by StatusLed::statusName(). First field is what the light looks like,
   second is what it means, third the tone, fourth the short legend label. Red is
   not automatically a fault: GPS_NO_FIX winks red while the receiver is healthy. */
const LED = {
  BOOT:            ['red / green alternating','Nothing has reported in yet. Past the first second of boot this means the housekeeping task has stopped updating.','warn','nothing reporting in'],
  MODEM_CONNECTING:['fast green blink','Joining WiFi.','','joining WiFi'],
  NETWORK_OK:      ['slow green blink','WiFi is up and the CAN driver is running. No frames yet — expected until a bus is attached.','','WiFi up, no CAN frames'],
  GPS_NO_FIX:      ['green, red wink','The GNSS module is streaming but has not locked a fix yet. The red wink is not a fault; it clears on first fix. Indoors it may never clear.','','GNSS streaming, no fix'],
  CAN_OK:          ['green heartbeat','CAN frames are arriving.','','CAN frames arriving'],
  CAN_ERROR:       ['fast red blink','The CAN driver is not running. Check the serial log.','bad','CAN driver down'],
  SYSTEM_ERROR:    ['solid red','The filesystem is down. Captures are not being written.','bad','filesystem down']
};
/* Most severe first — StatusLed shows the highest active condition. MqttOk and
   Buffering exist in StatusLed but this build never sets them, so listing them
   would invite someone to wait for a light that cannot come. */
const LED_ORDER = ['SYSTEM_ERROR','CAN_ERROR','CAN_OK','GPS_NO_FIX','MODEM_CONNECTING','NETWORK_OK','BOOT'];

let fails = 0, prevRx = null, prevT = null;

/* =============================================================================
   Identifier monitor

   The main instrument while reverse engineering: every identifier on the bus,
   its latest bytes in hex AND decimal, and which bytes just moved.

   Rows are built once per identifier and then updated in place. Rebuilding the
   table on every poll would be simpler and would throw away whatever a person
   is typing into a note field twice a second.
   ============================================================================= */
const CHANGE_MS = 2000;   // a changed byte stays lit this long
const STALE_MS  = 2000;   // an identifier silent this long is dimmed
const SPLIT_MQ  = window.matchMedia('(min-width:1280px)');

/* ---- live formulas in a note -----------------------------------------
   A note may carry {expressions} over the identifier's latest payload:

       SOC {b1*0.5} %        ->   SOC 89.5 %          b1=179
       Pack {le16(2)} V      ->   Pack 357 V          le16(2)=357
       Temp {b6-40} C        ->   Temp 30 C           b6=70

   The raw operands stay on screen next to the result: a decoded number
   nobody can trace back to a byte is a number nobody can check.

   Deliberately a tiny parser and not eval(): a note travels from the device
   to every browser that opens the dashboard, so it is untrusted text. */
const FN = {
  le16: (b, i) => b[i] | (b[i + 1] << 8),
  be16: (b, i) => (b[i] << 8) | b[i + 1],
  le24: (b, i) => b[i] | (b[i + 1] << 8) | (b[i + 2] << 16),
  be24: (b, i) => (b[i] << 16) | (b[i + 1] << 8) | b[i + 2],
  le32: (b, i) => (b[i] | (b[i + 1] << 8) | (b[i + 2] << 16) | (b[i + 3] << 24)) >>> 0,
  be32: (b, i) => ((b[i] << 24) | (b[i + 1] << 16) | (b[i + 2] << 8) | b[i + 3]) >>> 0
};

function compile(src) {                       // -> f(bytes, seen) or null
  const t = src.match(/\d+\.\d+|\d+|[A-Za-z_]\w*|[-+*/%(),]/g) || [];
  let p = 0;
  const peek = () => t[p], eat = v => (t[p] === v ? (p++, true) : false);

  function primary() {
    const tok = t[p++];
    if (tok === undefined) throw 0;
    if (tok === '(') { const e = expr(); if (!eat(')')) throw 0; return e; }
    if (tok === '-') { const e = primary(); return (b, s) => -e(b, s); }
    if (/^\d/.test(tok)) { const v = parseFloat(tok); return () => v; }
    if (FN[tok]) {
      if (!eat('(')) throw 0;
      const a = expr(); if (!eat(')')) throw 0;
      return (b, s) => {
        const i = a(b, s) | 0, need = +tok.slice(2) / 8;
        if (i < 0 || i + need > b.length) throw 0;
        const v = FN[tok](b, i);
        s.push(tok + '(' + i + ')=' + v);
        return v;
      };
    }
    const m = /^b([0-7])$/.exec(tok);
    if (m) {
      const i = +m[1];
      return (b, s) => { if (i >= b.length) throw 0; s.push('b' + i + '=' + b[i]); return b[i]; };
    }
    if (tok === 'dlc') return (b, s) => (s.push('dlc=' + b.length), b.length);
    throw 0;
  }
  function term() {
    let l = primary();
    for (;;) {
      const o = peek();
      if (o !== '*' && o !== '/' && o !== '%') return l;
      p++; const r = primary(), a = l;
      l = o === '*' ? (b, s) => a(b, s) * r(b, s)
        : o === '/' ? (b, s) => a(b, s) / r(b, s)
        : (b, s) => a(b, s) % r(b, s);
    }
  }
  function expr() {
    let l = term();
    for (;;) {
      const o = peek();
      if (o !== '+' && o !== '-') return l;
      p++; const r = term(), a = l;
      l = o === '+' ? (b, s) => a(b, s) + r(b, s) : (b, s) => a(b, s) - r(b, s);
    }
  }
  try { const e = expr(); return p === t.length ? e : null; } catch (_) { return null; }
}

const CACHE = new Map();                      // note text -> parsed parts
function parseNote(txt) {
  let parts = CACHE.get(txt);
  if (parts) return parts;
  parts = [];
  const re = /\{([^}]*)\}/g;
  let at = 0, m;
  while ((m = re.exec(txt))) {
    if (m.index > at) parts.push({lit: txt.slice(at, m.index)});
    parts.push({fn: compile(m[1]), src: m[1]});
    at = m.index + m[0].length;
  }
  if (at < txt.length) parts.push({lit: txt.slice(at)});
  if (!parts.some(x => x.fn !== undefined)) parts = null;   // plain text note
  CACHE.set(txt, parts);
  return parts;
}

const num = v => !isFinite(v) ? '?'
  : Math.abs(v - Math.round(v)) < 1e-9 ? String(Math.round(v))
  : String(Math.round(v * 1000) / 1000);

/* Returns {text, raw, nums} or null when the note carries no formula. `nums` is
   one entry per formula in note order, so a caller that wants to plot a value
   does not have to re-run the expression or parse it back out of the text.
   `fmt` lets a caller format numbers its own way; the table wants every digit,
   the vehicle panel wants a number readable at a glance. */
function renderNote(txt, bytes, fmt) {
  const shownum = fmt || num;
  const parts = parseNote(txt);
  if (!parts) return null;
  const seen = [], nums = [];
  let out = '';
  for (const part of parts) {
    if (part.lit !== undefined) { out += part.lit; continue; }
    if (!part.fn) { out += '{' + part.src + '?}'; continue; }
    try { const v = part.fn(bytes, seen); nums.push(v); out += shownum(v); }
    catch (_) { nums.push(NaN); out += '?'; }
  }
  return {text: out, raw: seen.join(' '), nums};
}

/* =============================================================================
   Filter, and the vehicle panel it feeds

   The device is asked for one of three identifier sets. This filters what is
   SENT and DRAWN, never what is captured: the recorder still writes every frame
   on the bus, because an identifier nobody has named yet is exactly what the
   next mapping session needs.

   It is also not a cure for lost frames. Frames are lost to flash writes
   stalling the CAN interrupt, and the dashboard has no part in that. What this
   does buy is a screen a person can read while a vehicle is moving, and a
   smaller document on both ends of a phone hotspot.

   TDS is the default the first time, because the tagged set is the one somebody
   in a vehicle came to look at. It falls back on its own when nothing is tagged
   yet, so a fresh vehicle never shows an empty table and looks broken.
   ============================================================================= */
const TDS_TAG = '#tds';

/* A card is read at arm's length in a moving vehicle, so it drops digits the
   table keeps. Small numbers keep their precision, because 3.979 V is a cell
   voltage where the third decimal is the whole point, while 47.016 km/h is
   just noise around 47. */
const cardnum = v => !isFinite(v) ? '?'
  : Math.abs(v - Math.round(v)) < 1e-9 ? String(Math.round(v))
  : Math.abs(v) < 10 ? String(Math.round(v * 1000) / 1000)
  : String(Math.round(v * 10) / 10);
const stripTag = t => t.slice(TDS_TAG.length).replace(/^[\s:]+/, '');
const isTds = t => !!t && t.slice(0, TDS_TAG.length).toLowerCase() === TDS_TAG;

let FILTER = 'tds', FILTER_AUTO = true;
try {
  const f = localStorage.getItem('bmt.filter');
  if (f) { FILTER = f; FILTER_AUTO = false; }
} catch (e) {}

/* A vehicle nobody has mapped yet has no notes, so TDS and Named both come back
   empty and the table would look like a dead bus. Until the operator picks a
   mode themselves, step down to one that has something in it. */
const FILTER_FALLBACK = {tds: 'named', named: 'all'};
function autoRelax(d) {
  if (!FILTER_AUTO) return false;
  if (d.ids.length || !d.hidden) return false;
  const next = FILTER_FALLBACK[FILTER];
  if (!next) return false;
  setFilter(next, false);
  return true;
}

function setFilter(f, remember) {
  FILTER = f;
  if (remember !== false) {
    FILTER_AUTO = false;                    // an explicit choice is never undone
    try { localStorage.setItem('bmt.filter', f); } catch (e) {}
  }
  const box = $('flt');
  if (box) [...box.children].forEach(b => b.classList.toggle('on', b.dataset.f === f));
  MON.layout = '';          // the row set changes, so the table must be rebuilt
}

/* =============================================================================
   The vehicle cards

   One card per tagged note that carries a formula: the label is the text before
   the formula, the value is the formula rendered against the bytes that just
   arrived. Nothing here is hard-coded per vehicle. Name an identifier, tag it,
   and it appears.

   WHY A CARD DRAWS ITS OWN HISTORY. On a bus the question is almost never "what
   is the number". A technician already knows roughly what a pack voltage reads.
   The question is whether it is moving, which way, and whether it steps or
   drifts. A row of digits answers none of that, and the answer is gone before
   the next poll. Forty seconds of the reading behind each number answers it at
   a glance and costs the device nothing: the history lives in the page.

   WHAT A CARD IS ALLOWED TO ASSUME. Almost nothing, because the notes are
   written per vehicle and in whatever words the mapper used. So:

   * The ICON is chosen from the UNIT, never from the label. "Motor",
     "Kecepatan" and "SOC" are free text somebody typed; rpm, km/jam and % mean
     the same thing on every vehicle. A unit nobody recognises gets the neutral
     mark rather than a wrong one.
   * The GRAPH scales itself to what it has seen, because no range is known and
     inventing one would be a claim about the vehicle. The scale is relative,
     and the card says so by carrying no axis at all.
   * Only a PERCENTAGE gets a fixed 0 to 100 bar, because that is what the unit
     itself means. Nothing here paints a low reading amber: a threshold is a
     decision about the vehicle, and this tool does not get to make one.
   * A note with TWO OR MORE formulas is a sentence, not a quantity. It keeps
     the text treatment, set smaller so it stops being cut off, which the one
     size never managed.
   ============================================================================= */
const VWIN_MS = 40000;          // how much of the past each graph shows
const VHIST   = new Map();      // key|note -> [{t, v}] within that window
let   VCLOCK  = 0;              // last device clock, to notice a restart

/* Keyed on the unit as written after the formula, lowercased. The aliases are
   here because a mapper writes what is on the dash: km/h in one car, km/jam in
   the next, and both are a speed. */
const VKIND = {
  '%': 'pct',
  'rpm': 'rpm', 'r/min': 'rpm',
  'km': 'dist', 'm': 'dist', 'mi': 'dist',
  'km/jam': 'speed', 'km/h': 'speed', 'kmh': 'speed', 'mph': 'speed', 'm/s': 'speed',
  'v': 'volt', 'mv': 'volt', 'kv': 'volt',
  'a': 'amp', 'ma': 'amp',
  'c': 'temp', '°c': 'temp', 'degc': 'temp', 'f': 'temp', '°f': 'temp',
  'kpa': 'press', 'bar': 'press', 'psi': 'press'
};

/* 16x16, stroked in the label colour. Small enough that a wrong one reads as
   noise rather than as a claim, which is the other reason the neutral trace
   exists. */
const VICON = {
  pct:   'M2.5 5.5h9.5v5H2.5z M13.8 7.2v1.6',
  rpm:   'M3 12.2a5 5 0 1 1 10 0 M8 12.2 5.2 7.4',
  speed: 'M3 12.2a5 5 0 1 1 10 0 M8 12.2 11.2 7',
  dist:  'M2.6 8h10.8 M2.6 5.6v4.8 M13.4 5.6v4.8',
  volt:  'M9.2 1.8 4.2 8.4h3.3L6.8 14.2l5-6.6H8.5z',
  amp:   'M2.6 8h10.8 M10.2 4.8 13.4 8l-3.2 3.2',
  temp:  'M6.4 9.2V3.4a1.6 1.6 0 0 1 3.2 0v5.8a3 3 0 1 1-3.2 0z',
  press: 'M8 13.4a5.4 5.4 0 1 1 0-10.8 5.4 5.4 0 0 1 0 10.8z M8 8 11 5.6',
  none:  'M1.8 8h2.4l1.9-4.2 2.4 8.4 1.7-4.2h2'
};
const vicon = kind => '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="' +
  (VICON[kind] || VICON.none) + '"/></svg>';

/* The graph. Time runs left to right across a fixed forty seconds, so a card
   that has just appeared draws a short trace at the right and grows leftwards,
   instead of stretching two samples across the full width and pretending to be
   a history it does not have. */
function vkGraph(h, now, line, area, dot) {
  if (h.length < 2) {
    line.setAttribute('points', '');
    area.setAttribute('points', '');
    dot.setAttribute('d', '');
    return;
  }
  let lo = Infinity, hi = -Infinity;
  for (let i = 0; i < h.length; i++) {
    if (h[i].v < lo) lo = h[i].v;
    if (h[i].v > hi) hi = h[i].v;
  }
  /* It refuses to zoom in past two percent of the reading. Without a floor, a
     pack voltage sitting at 350.0 and rounding to 350.1 would draw a mountain
     range, and the flattest signal on the bus would be the most dramatic thing
     on the screen. */
  const floor = Math.max(Math.abs(hi), Math.abs(lo)) * 0.02 || 1;
  const flat = hi - lo < floor;
  if (flat) { const mid = (hi + lo) / 2; lo = mid - floor / 2; hi = mid + floor / 2; }

  const H = 30, P = 3;
  let pts = '', first = 100, lx = 0, ly = 0;
  for (let i = 0; i < h.length; i++) {
    const x = Math.max(0, 100 - ((now - h[i].t) / VWIN_MS) * 100);
    const y = H - P - ((h[i].v - lo) / (hi - lo)) * (H - 2 * P);
    if (i === 0) first = x;
    lx = x; ly = y;
    pts += x.toFixed(2) + ',' + y.toFixed(2) + ' ';
  }
  pts = pts.trim();
  line.setAttribute('points', pts);

  /* A flat reading gets no fill. The line still sits where it belongs, but
     filling under it turns a quantised counter that moved by one step in forty
     seconds into the loudest block on the panel, which is the opposite of what
     it is telling you. */
  area.setAttribute('points', flat ? ''
    : first.toFixed(2) + ',' + H + ' ' + pts + ' 100,' + H);

  /* A zero-length subpath with a round cap draws a dot, and unlike a circle it
     is not stretched into an ellipse by the graph being scaled to the card
     width. It marks which end is now, which a bare trace never says. */
  dot.setAttribute('d', 'M' + lx.toFixed(2) + ' ' + ly.toFixed(2) + 'l0 0');
}

const VCARDS = new Map();       // identifier key -> the elements to write into

function vals(list, d) {
  const grid = $('vgrid'), panel = $('vals');
  const now = d.now_ms;
  if (now < VCLOCK) VHIST.clear();        // the device restarted: a new series
  VCLOCK = now;

  const cards = [], live = new Set();
  list.forEach(x => {
    const k = keyOf(x);
    const txt = MON.notes.get(k);
    if (!isTds(txt)) return;
    const body = stripTag(txt);
    const out = renderNote(body, bytesOf(x.d), cardnum);
    if (!out) return;                       // tagged but no formula: table only
    const parts = parseNote(body);
    const lead = parts && parts[0] && parts[0].lit !== undefined ? parts[0].lit.trim() : '';
    const rest = lead ? out.text.slice(parts[0].lit.length).trim() : out.text.trim();

    const fns  = parts.filter(p => p.fn !== undefined).length;
    const tail = parts[parts.length - 1];
    const one  = fns === 1 && isFinite(out.nums[0]);
    const unit = one && tail.lit !== undefined ? tail.lit.trim() : '';
    const kind = one ? (VKIND[unit.toLowerCase()] || 'none') : 'txt';

    let hist = null;
    if (one) {
      const hk = k + '|' + txt;             // an edited note starts a new series
      live.add(hk);
      hist = VHIST.get(hk);
      if (!hist) { hist = []; VHIST.set(hk, hist); }
      hist.push({t: now, v: out.nums[0]});
      while (hist.length > 2 && (now - hist[0].t > VWIN_MS || hist.length > 400)) hist.shift();
    }

    cards.push({
      k, kind, hist,
      label:  lead || hex(x.id, x.x ? 8 : 3),
      value:  one ? cardnum(out.nums[0]) : (rest || out.text),
      unit:   unit,
      pct:    kind === 'pct' ? Math.max(0, Math.min(100, out.nums[0])) : null,
      silent: now - x.t > STALE_MS
    });
  });

  // Notes get edited and identifiers come and go. Nothing should keep a series
  // for a card that no longer exists.
  if (VHIST.size > live.size) VHIST.forEach((_, hk) => { if (!live.has(hk)) VHIST.delete(hk); });

  if (!cards.length) { panel.hidden = true; return; }
  panel.hidden = false;

  /* The cluster claims the three readings it has a place for, and only appears
     when there is a percentage or a speed: with neither of those the drawing is
     two hairlines around an empty middle. Whatever it does not claim keeps the
     card it had. */
  const pctc  = cards.find(c => c.kind === 'pct');
  const speed = cards.find(c => c.kind === 'speed');
  const dists = (pctc || speed) ? cards.filter(c => c.kind === 'dist') : [];
  const hero  = (pctc || speed) ? [pctc, speed].concat(dists).filter(Boolean) : [];
  if (hero.length) cluPaint(pctc, speed, dists); else $('clu').hidden = true;
  const rest = cards.filter(c => hero.indexOf(c) < 0);

  const graphed = rest.some(c => c.hist);
  $('vn').textContent = (graphed ? (VWIN_MS / 1000) + ' s history · ' : '') +
    cards.length + (cards.length === 1 ? ' signal' : ' signals');

  // The label and the unit both come from the note, so the arrangement has to
  // be rebuilt when a note changes, not only when an identifier appears.
  const sig = rest.map(c => c.k + '|' + c.kind + '|' + c.label + '|' + c.unit).join(',');
  if (grid.dataset.sig !== sig) {
    grid.dataset.sig = sig;
    grid.textContent = '';
    VCARDS.clear();
    rest.forEach(c => {
      const box = document.createElement('div');
      box.className = 'vc' + (c.kind === 'txt' ? ' txt' : '');

      const head = document.createElement('div');
      head.className = 'vk-h';
      head.innerHTML = vicon(c.kind);             // a fixed table, never input
      const em = document.createElement('em');
      em.textContent = c.label;
      const sil = document.createElement('i');
      sil.className = 'vk-s';
      sil.textContent = 'silent';
      head.append(em, sil);

      const b  = document.createElement('b');
      const vv = document.createElement('span');
      const uu = document.createElement('u');
      uu.textContent = c.unit;
      b.append(vv, uu);
      box.append(head, b);

      const rec = {box: box, vv: vv};
      if (c.kind === 'pct') {
        const bar = document.createElement('div');
        bar.className = 'vk-b';
        rec.fill = document.createElement('i');
        bar.appendChild(rec.fill);
        box.appendChild(bar);
      } else if (c.hist) {
        const g = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        g.setAttribute('class', 'vk-g');
        g.setAttribute('viewBox', '0 0 100 30');
        g.setAttribute('preserveAspectRatio', 'none');
        g.setAttribute('aria-hidden', 'true');
        g.innerHTML = '<polygon class="vk-a"/>' +
          '<polyline class="vk-l" vector-effect="non-scaling-stroke"/>' +
          '<path class="vk-d" vector-effect="non-scaling-stroke"/>';
        rec.area = g.childNodes[0];
        rec.line = g.childNodes[1];
        rec.dot  = g.childNodes[2];
        box.appendChild(g);
      } else {
        const pad = document.createElement('div');
        pad.className = 'vk-p';                   // keeps the row of cards level
        box.appendChild(pad);
      }
      VCARDS.set(c.k, rec);
      grid.appendChild(box);
    });
  }

  rest.forEach(c => {
    const r = VCARDS.get(c.k);
    if (!r) return;
    r.vv.textContent = c.value;
    r.box.classList.toggle('silent', c.silent);
    if (r.fill) r.fill.style.width = c.pct.toFixed(1) + '%';
    if (r.line) vkGraph(c.hist, now, r.line, r.area, r.dot);
  });
}

/* =============================================================================
   The cluster

   The arrangement is the one in the reference the Direktur supplied: a ring in
   the middle carrying the charge, the odometer on the left, the speed large on
   the right, and two hairlines running the full width with the ring punched
   through them.

   WHICH SIGNAL GOES WHERE IS DECIDED BY ITS UNIT, not by its identifier. The
   panel still hard-codes nothing per vehicle: a note whose unit is a percentage
   takes the ring, one in km/jam takes the right, one in km takes the left, and
   everything else keeps the card it had. Map a new vehicle and it arranges
   itself. If a vehicle has neither a percentage nor a speed there is no cluster
   to draw, and the panel falls back to the plain grid it was before.

   The digits are drawn, not set in a font. A seven-segment face would be
   another file in flash on a device that serves its pages from flash, and the
   whole of one is ten lines: a table of which bars are lit, and two hexagons.
   ============================================================================= */

/* a b c d e f g, in the usual order. */
const S7 = {
  '0': 'abcdef', '1': 'bc', '2': 'abdeg', '3': 'abcdg', '4': 'bcfg',
  '5': 'acdfg', '6': 'acdefg', '7': 'abc', '8': 'abcdefg', '9': 'abcdfg',
  '-': 'g'
};

/* One digit lives in a 100 by 180 box. The bevelled ends are what make it read
   as an instrument rather than as a calculator. */
const S7P = (function () {
  const T = 18, h = T / 2, m = 3.5, xL = 9, xR = 91, yT = 9, yM = 90, yB = 171;
  const bar = (y, x0, x1) => 'M' + x0 + ' ' + y + 'L' + (x0 + h) + ' ' + (y - h) +
    'L' + (x1 - h) + ' ' + (y - h) + 'L' + x1 + ' ' + y +
    'L' + (x1 - h) + ' ' + (y + h) + 'L' + (x0 + h) + ' ' + (y + h) + 'Z';
  const col = (x, y0, y1) => 'M' + x + ' ' + y0 + 'L' + (x + h) + ' ' + (y0 + h) +
    'L' + (x + h) + ' ' + (y1 - h) + 'L' + x + ' ' + y1 +
    'L' + (x - h) + ' ' + (y1 - h) + 'L' + (x - h) + ' ' + (y0 + h) + 'Z';
  return {
    a: bar(yT, xL + m, xR - m), d: bar(yB, xL + m, xR - m), g: bar(yM, xL + m, xR - m),
    b: col(xR, yT + m, yM - m), c: col(xR, yM + m, yB - m),
    e: col(xL, yM + m, yB - m), f: col(xL, yT + m, yM - m)
  };
})();

function s7(text) {
  let x = 0, out = '';
  const str = String(text);
  for (let i = 0; i < str.length; i++) {
    const ch = str[i];
    if (ch === '.' || ch === ',') {
      out += '<path transform="translate(' + x + ' 0)" d="M6 152h27v27H6Z"/>';
      x += 47;
    } else if (ch === '%') {
      out += '<g transform="translate(' + x + ' 0)">' +
        '<path fill-rule="evenodd" d="M2 52h40v40H2Zm11 11h18v18H13Z"/>' +
        '<path fill-rule="evenodd" d="M56 124h40v40H56Zm11 11h18v18H67Z"/>' +
        '<path d="M74 44 94 54 24 172 4 162Z"/></g>';
      x += 118;
    } else if (S7[ch]) {
      let d = '';
      for (let j = 0; j < S7[ch].length; j++) d += S7P[S7[ch][j]];
      /* A one lights only the two right-hand bars, so in a full-width cell it
         leaves eighty points of blank to its left and 30461 reads as 3046 1.
         Every digital cluster gives the one a narrow cell; so does this. */
      const one = ch === '1';
      out += '<path transform="translate(' + (one ? x - 82 : x) + ' 0)" d="' + d + '"/>';
      x += one ? 40 : 122;
    } else {
      x += 60;                                   // anything else: its own blank
    }
  }
  return {markup: out, w: Math.max(1, x - 22)};
}

/* Writes a drawn number into an <svg>, and only when it changed: the digits are
   a dozen paths and there is no reason to rebuild them twice a second. */
function s7write(el, text) {
  if (el.dataset.t === text) return;
  el.dataset.t = text;
  const g = s7(text);
  el.setAttribute('viewBox', '0 0 ' + g.w + ' 180');
  el.innerHTML = g.markup;
}

const RING_N = 48;                               // ticks around the ring

function ringMarkup() {
  const C = 100;
  const pt = (r, a) => [
    C + r * Math.cos((a - 90) * Math.PI / 180),
    C + r * Math.sin((a - 90) * Math.PI / 180)
  ];
  const arc = (r, a0, a1) => {
    const p0 = pt(r, a0), p1 = pt(r, a1);
    return 'M' + p0[0].toFixed(2) + ' ' + p0[1].toFixed(2) + 'A' + r + ' ' + r +
      ' 0 ' + (a1 - a0 > 180 ? 1 : 0) + ' 1 ' + p1[0].toFixed(2) + ' ' + p1[1].toFixed(2);
  };
  let out = '';
  // The outermost pair of thin brackets, then the broken heavy arcs. Both are
  // ornament, carried over from the reference because that is the drawing that
  // was approved; neither one moves or means anything.
  out += '<path class="rg-br" d="' + arc(96, 32, 148) + '"/>' +
         '<path class="rg-br" d="' + arc(96, 212, 328) + '"/>';
  [[14, 68], [104, 152], [194, 248], [286, 338]].forEach(a => {
    out += '<path class="rg-or" d="' + arc(88, a[0], a[1]) + '"/>';
  });
  for (let i = 0; i < RING_N; i++) {
    const a = (i / RING_N) * 360, i0 = pt(66, a), i1 = pt(78, a);
    out += '<line class="rg-t" x1="' + i0[0].toFixed(2) + '" y1="' + i0[1].toFixed(2) +
           '" x2="' + i1[0].toFixed(2) + '" y2="' + i1[1].toFixed(2) + '"/>';
  }
  out += '<circle class="rg-in" cx="100" cy="100" r="59"/>';
  return out;
}

/* Built once. The cluster is three zones and they are the same three zones for
   every vehicle, so only their contents are rewritten. */
let CLU = null;

function cluBuild() {
  const box = $('clu');
  box.innerHTML =
    '<div class="clu-l"><div class="clu-lb">' +
      '<span class="clu-k" id="clu-dk"></span>' +
      '<div class="clu-dv"><svg class="s7 s7-m" id="clu-d"></svg>' +
        '<u id="clu-du"></u></div>' +
      '<div class="clu-x" id="clu-x"></div></div></div>' +
    '<div class="clu-c"><div class="clu-ring">' +
      '<svg viewBox="0 0 200 200" aria-hidden="true">' + ringMarkup() + '</svg>' +
      '<div class="clu-hub">' +
        '<svg class="clu-ic" id="clu-pi" viewBox="0 0 16 16" aria-hidden="true">' +
          '<path/></svg>' +
        '<div class="clu-pv"><svg class="s7 s7-b" id="clu-p"></svg></div>' +
        '<span class="clu-k" id="clu-pk"></span>' +
      '</div></div></div>' +
    '<div class="clu-r"><div class="clu-rb">' +
      '<svg class="s7 s7-b" id="clu-s"></svg>' +
      '<span class="clu-u" id="clu-su"></span></div></div>';
  CLU = {
    ticks: box.querySelectorAll('.rg-t'),
    ring:  box.querySelector('.clu-ring'),
    pi:    $('clu-pi').firstChild, p: $('clu-p'), pk: $('clu-pk'),
    d:     $('clu-d'), du: $('clu-du'), dk: $('clu-dk'), x: $('clu-x'),
    s:     $('clu-s'), su: $('clu-su'),
    zl:    box.querySelector('.clu-l'), zr: box.querySelector('.clu-r')
  };
}

/* A drawn number can only carry digits, a point and a minus. Anything else, and
   the reading is written out instead of being mangled into blanks. */
const S7OK = /^[-0-9.,]+$/;

function cluPaint(pctc, speed, dists) {
  const box = $('clu');
  if (!CLU) cluBuild();
  box.hidden = false;

  CLU.ring.hidden = !pctc;
  if (pctc) {
    const on = Math.round((pctc.pct / 100) * RING_N);
    for (let i = 0; i < CLU.ticks.length; i++) {
      CLU.ticks[i].classList.toggle('on', i < on);
    }
    CLU.pi.setAttribute('d', VICON[pctc.kind] || VICON.none);
    s7write(CLU.p, S7OK.test(pctc.value) ? pctc.value + '%' : '');
    CLU.pk.textContent = pctc.label;
    CLU.ring.classList.toggle('silent', pctc.silent);
  }

  CLU.zl.hidden = !dists.length;
  if (dists.length) {
    const d = dists[0];
    CLU.dk.textContent = d.label;
    s7write(CLU.d, S7OK.test(d.value) ? d.value : '');
    CLU.du.textContent = d.unit;
    CLU.x.textContent = '';
    dists.slice(1, 3).forEach((o, i) => {
      const r = document.createElement('div');
      const b = document.createElement('i');
      b.textContent = i ? 'B' : 'A';
      const v = document.createElement('span');
      v.textContent = o.value + (o.unit ? ' ' + o.unit : '');
      r.append(b, v);
      CLU.x.appendChild(r);
    });
    CLU.zl.classList.toggle('silent', d.silent);
  }

  CLU.zr.hidden = !speed;
  if (speed) {
    s7write(CLU.s, S7OK.test(speed.value) ? speed.value : '');
    CLU.su.textContent = speed.unit;
    CLU.zr.classList.toggle('silent', speed.silent);
  }
}

const MON = {
  rows:   new Map(),      // key -> row record
  layout: '',             // signature of the current arrangement
  notes:  new Map(),      // key -> note text as the device stored it
  notesOk: true
};
const keyOf = x => (x.x ? 'x' : 's') + x.id;

function monMakeRow(x) {
  const k = keyOf(x);
  const tr = document.createElement('tr');
  const cell = (cls, text) => {
    const td = document.createElement('td');
    if (cls) td.className = cls;
    if (text !== undefined) td.textContent = text;
    tr.appendChild(td);
    return td;
  };
  cell('id', hex(x.id, x.x ? 8 : 3));
  const nt = cell('nt');
  const inp = document.createElement('input');
  inp.className = 'ni';
  inp.maxLength = 60;
  inp.placeholder = MON.notesOk ? 'name this ID…  or  #tds SOC {b1*0.5} %' : 'notes unavailable';
  inp.disabled = !MON.notesOk;
  inp.autocomplete = 'off';
  inp.spellcheck = false;
  inp.setAttribute('aria-label', 'Name for ' + hex(x.id, x.x ? 8 : 3));
  inp.value = MON.notes.get(k) || '';
  inp.addEventListener('change', () => saveNote(x.id, x.x, inp));
  inp.addEventListener('keydown', e => { if (e.key === 'Enter') inp.blur(); });
  nt.appendChild(inp);
  // Shown instead of the input while a formula note is not being edited.
  const nv = document.createElement('div');
  nv.className = 'nv';
  nv.hidden = true;
  nv.title = 'click to edit the formula';
  nv.addEventListener('click', () => {
    nv.hidden = true; inp.hidden = false; inp.focus(); inp.select();
  });
  inp.addEventListener('blur', () => { if (MON.notes.get(k) !== undefined) monNote(r); });
  nt.appendChild(nv);
  const hz = cell('num hz', '–');
  const dlc = cell('num', String(x.l));
  const bytes = [];
  for (let i = 0; i < 8; i++) {
    const td = cell('b nil');
    const hx = document.createElement('span'); hx.className = 'hx'; hx.textContent = '·';
    const dc = document.createElement('span'); dc.className = 'dc';
    td.append(hx, dc);
    bytes.push({td, hx, dc, v: -1, at: 0});
  }
  const r = {k, tr, hz, dlc, bytes, inp, nv, win: []};
  return r;
}

/* Show a formula note as its result, with the operands it used, and keep the
   editable text one click away. A note without {..} stays an ordinary input. */
function monNote(r) {
  const raw = MON.notes.get(r.k) || '';
  const tds = isTds(raw);
  const txt = tds ? stripTag(raw) : raw;
  const out = r.bytes && txt ? renderNote(txt, r.lastBytes || []) : null;
  if (!out || document.activeElement === r.inp) {
    if (r.nv.hidden) return;
    r.nv.hidden = true; r.inp.hidden = false;
    return;
  }
  r.nv.textContent = '';
  if (tds) {
    // The tag is storage, not something to read forty times down a column.
    const tag = document.createElement('i');
    tag.className = 'tg';
    tag.textContent = 'TDS';
    tag.title = 'Consumed by the Taxi Dispatch System';
    r.nv.appendChild(tag);
  }
  const v = document.createElement('b');
  v.textContent = out.text;
  r.nv.appendChild(v);
  if (out.raw) {
    const raw = document.createElement('span');
    raw.textContent = out.raw;
    r.nv.appendChild(raw);
  }
  r.inp.hidden = true; r.nv.hidden = false;
}

/* Arrange rows into one table, or two side by side on a wide screen. Only when
   the set of identifiers or the width changes, and never while a note is being
   typed — moving a focused input out of the DOM would drop the focus. */
function monLayout(list) {
  const split = SPLIT_MQ.matches && list.length > 10;
  const sig = (split ? '2|' : '1|') + list.map(keyOf).join(',');
  if (sig === MON.layout) return;
  const typing = document.activeElement && document.activeElement.classList.contains('ni');
  if (typing && MON.layout) return;
  MON.layout = sig;

  const grid = $('idgrid');
  grid.className = 'idgrid' + (split ? ' split' : '');
  grid.textContent = '';
  const tables = split ? 2 : 1;
  const perTable = Math.ceil(list.length / tables);
  for (let t = 0; t < tables; t++) {
    const table = document.createElement('table');
    table.className = 'idt';
    let head = '<thead><tr><th>ID</th><th class="nth">Name</th>'
             + '<th class="num">Hz</th><th class="num">DLC</th>';
    for (let i = 0; i < 8; i++) head += '<th class="bh">B' + i + '</th>';
    table.innerHTML = head + '</tr></thead>';
    const body = document.createElement('tbody');
    list.slice(t * perTable, (t + 1) * perTable).forEach(x => body.appendChild(MON.rows.get(keyOf(x)).tr));
    table.appendChild(body);
    grid.appendChild(table);
  }
}
SPLIT_MQ.addEventListener('change', () => { MON.layout = ''; });

function monPaint(d) {
  const now = Date.now();
  const list = d.ids.slice().sort((a, b) => (a.x - b.x) || (a.id - b.id));
  vals(list, d);

  // The device already dropped what the filter excludes, but say so: a filter
  // that hides silently is how somebody concludes an ECU went quiet.
  const hid = $('hid');
  const n = d.hidden || 0;
  if (n && hid) {
    hid.hidden = false;
    hid.textContent = n + (n === 1 ? ' identifier is' : ' identifiers are')
                    + ' hidden by the ' + FILTER.toUpperCase() + ' filter. They are still'
                    + ' being received and still being recorded. Switch to All to map'
                    + ' them: the signal probe only offers what this table shows.';
  } else if (hid) {
    hid.hidden = true;
  }

  const empty = $('id-empty');
  if (!list.length) {
    if (empty && n) {
      empty.innerHTML = '';
      const t = document.createElement('b');
      t.textContent = 'Nothing matches this filter';
      empty.appendChild(t);
      empty.append('All ' + n + ' identifiers on the bus are hidden. '
                 + 'Nothing is wrong with the device: switch to All, or name an '
                 + 'identifier in the table to make it appear here.');
    }
    MON.layout = '';
    const grid = $('idgrid');
    if (grid && empty && !grid.contains(empty)) { grid.textContent = ''; grid.appendChild(empty); }
    return;
  }
  list.forEach(x => { if (!MON.rows.has(keyOf(x))) MON.rows.set(keyOf(x), monMakeRow(x)); });
  monLayout(list);

  list.forEach(x => {
    const r = MON.rows.get(keyOf(x));

    // Rate over a sliding window of about three seconds. One poll interval is
    // too short: a 1 Hz identifier would read 0 and 2 on alternate polls.
    // A count that went DOWN means the device restarted; old samples no longer
    // belong to the same series.
    if (r.win.length && x.n < r.win[r.win.length - 1].n) r.win = [];
    r.win.push({t: now, n: x.n});
    while (r.win.length > 2 && now - r.win[0].t > 3000) r.win.shift();
    if (r.win.length > 1) {
      const a = r.win[0], b = r.win[r.win.length - 1];
      const hz = (b.n - a.n) * 1000 / Math.max(1, b.t - a.t);
      r.hz.textContent = hz >= 10 ? Math.round(hz) : hz.toFixed(1);
    }
    r.dlc.textContent = x.l;

    const b = bytesOf(x.d);
    r.lastBytes = b;
    monNote(r);
    for (let i = 0; i < 8; i++) {
      const c = r.bytes[i];
      if (i >= b.length) {
        if (c.v !== -2) { c.td.className = 'b nil'; c.hx.textContent = '·'; c.dc.textContent = ''; c.v = -2; }
        continue;
      }
      if (b[i] !== c.v) {
        if (c.v >= 0) c.at = now;          // a real change, not the first value
        c.v = b[i];
        c.hx.textContent = b[i].toString(16).toUpperCase().padStart(2, '0');
        c.dc.textContent = b[i];
      }
      c.td.className = (now - c.at < CHANGE_MS) ? 'b chg' : 'b';
    }

    // Age against the DEVICE clock. The phone's clock drifts from it, and a
    // backgrounded tab would otherwise make everything look silent.
    r.tr.classList.toggle('stale', d.now_ms - x.t > STALE_MS);
  });
}

async function loadNotes() {
  try {
    const r = await fetch('/api/notes', {cache: 'no-store'});
    if (!r.ok) return;
    const j = await r.json();
    MON.notesOk = j.available !== false;
    MON.notes.clear();
    (j.notes || []).forEach(n => MON.notes.set((n.x ? 'x' : 's') + n.id, n.t));
    MON.rows.forEach((row, k) => {
      if (document.activeElement !== row.inp) row.inp.value = MON.notes.get(k) || '';
      row.inp.disabled = !MON.notesOk;
      monNote(row);
    });
  } catch (e) { /* the state poll reports connectivity; stay quiet here */ }
}

/* Saved on change (blur or Enter), not per keystroke. The device echoes what it
   actually kept — trimmed, length-limited — and the field shows that, so a note
   never looks saved in a form it was not. */
async function saveNote(id, ext, inp) {
  inp.classList.remove('saved', 'err');
  inp.title = '';
  try {
    const body = new URLSearchParams({id: String(id), x: ext ? '1' : '0', text: inp.value});
    const r = await fetch('/api/note', {method: 'POST', body});
    if (!r.ok) throw new Error(await r.text());
    const j = await r.json();
    const k = (ext ? 'x' : 's') + id;
    if (j.t) MON.notes.set(k, j.t); else MON.notes.delete(k);
    inp.value = j.t;
    inp.classList.add('saved');
    setTimeout(() => inp.classList.remove('saved'), 1200);
  } catch (e) {
    inp.classList.add('err');
    inp.title = 'Not saved: ' + (e.message || e);
  }
}

/* =============================================================================
   Signal probe

   Blueprint 9.1 in the browser: pick an identifier, a start byte, a width and a
   byte order, and watch the decoded number while someone reads the speedometer
   aloud. The decode runs HERE, not on the device, so the firmware still never
   claims to know what a byte means — this panel is openly the operator's guess.
   ============================================================================= */
const PB_KEYS = ['pb-id','pb-off','pb-w','pb-e','pb-s','pb-o','pb-sg'];
let pbHist = [], pbSig = '', pbIds = '', pbLastN = -1;

function pbRead() {
  return {id: +$('pb-id').value, off: +$('pb-off').value, w: +$('pb-w').value,
          le: $('pb-e').value === 'le', sc: +$('pb-s').value, of: +$('pb-o').value,
          sg: $('pb-sg').checked};
}
function pbSave() {
  try { const o = {}; PB_KEYS.forEach(k => o[k] = $(k).type === 'checkbox' ? $(k).checked : $(k).value);
        localStorage.setItem('bmt.probe', JSON.stringify(o)); } catch (e) {}
}
function pbLoad() {
  try { const o = JSON.parse(localStorage.getItem('bmt.probe') || '{}');
        PB_KEYS.forEach(k => { if (o[k] === undefined || !$(k)) return;
          if ($(k).type === 'checkbox') $(k).checked = o[k]; else $(k).value = o[k]; }); } catch (e) {}
}
/* Multiplication, not shifts: JavaScript bitwise operators are 32-bit SIGNED,
   so a 32-bit unsigned CAN value would come back negative. */
function pbDecode(b, st, w, le, sg) {
  const n = w / 8;
  if (st < 0 || st + n > b.length) return null;
  let v = 0;
  if (le) { for (let i = n - 1; i >= 0; i--) v = v * 256 + b[st + i]; }
  else    { for (let i = 0; i < n; i++)     v = v * 256 + b[st + i]; }
  if (sg) { const half = Math.pow(2, w - 1); if (v >= half) v -= Math.pow(2, w); }
  return v;
}
function pbSpark(h) {
  const el = $('pb-spark');
  if (h.length < 2) { el.innerHTML = ''; return; }
  let lo = Math.min.apply(null, h), hi = Math.max.apply(null, h);
  if (hi === lo) { hi = lo + 1; lo = lo - 1; }
  const n = h.length, W = 300, H = 48, p = 4;
  const pts = h.map((v, i) => (i * (W / (n - 1))).toFixed(1) + ',' +
    (H - p - ((v - lo) / (hi - lo)) * (H - 2 * p)).toFixed(1)).join(' ');
  el.innerHTML = '<polyline fill="none" stroke="var(--g)" stroke-width="1.7" ' +
    'stroke-linejoin="round" stroke-linecap="round" points="' + pts + '"/>';
}
function pbFmt(v) {
  return (Number.isInteger(v) || Math.abs(v) >= 1000)
    ? v.toLocaleString(undefined, {maximumFractionDigits: 3})
    : String(Math.round(v * 1000) / 1000);
}

function probe(d) {
  const sig = d.ids.map(x => x.id).join(',');
  if (sig !== pbIds) {
    pbIds = sig;
    const keep = $('pb-id').value;
    $('pb-id').innerHTML = d.ids.slice().sort((a, b) => a.id - b.id).map(x =>
      '<option value="' + x.id + '">' + hex(x.id, x.x ? 8 : 3) + '</option>').join('');
    if (keep && d.ids.some(x => String(x.id) === keep)) $('pb-id').value = keep;
    else /* The filter buttons. A mode switch rebuilds the table on the next poll rather
   than waiting for the device, so the press feels answered. */
(function () {
  const box = $('flt');
  if (!box) return;
  box.addEventListener('click', e => {
    const b = e.target.closest('button');
    if (!b) return;
    setFilter(b.dataset.f);
    tick();
  });
  setFilter(FILTER, false);
})();

pbLoad();
  }
  if (!d.ids.length) {
    $('pb-val').textContent = '–'; $('pb-raw').textContent = '';
    $('pb-mm').textContent = '–'; $('pb-n').textContent = ''; pbSpark([]);
    $('pb-note').innerHTML = note('Nothing on the bus yet. Once frames arrive, pick an identifier and watch this number while someone reads the speedometer aloud. Speed follows the needle both ways, an odometer only climbs, a rolling counter wraps to zero.');
    return;
  }

  const p = pbRead();
  const guess = [p.id, p.off, p.w, p.le, p.sg].join('|');
  if (guess !== pbSig) { pbSig = guess; pbHist = []; pbLastN = -1; }

  // The latest payload per identifier arrives with every poll. A sample is
  // taken only when the count moved, so one frame is never plotted twice.
  const e = d.ids.find(x => x.id === p.id);
  if (e && e.l && e.n !== pbLastN) {
    pbLastN = e.n;
    const b = bytesOf(e.d);
    const raw = pbDecode(b, p.off, p.w, p.le, p.sg);
    if (raw === null) {
      $('pb-val').textContent = '–'; $('pb-raw').textContent = ''; pbSpark([]);
      $('pb-note').innerHTML = note('Byte ' + p.off + ' plus ' + (p.w / 8) + ' byte(s) runs past this frame, which carries ' + b.length + '. Narrow the width or move the start byte left.', 'warn');
      return;
    }
    pbHist.push(raw * p.sc + p.of); if (pbHist.length > 90) pbHist.shift();
    $('pb-val').textContent = pbFmt(pbHist[pbHist.length - 1]);
    $('pb-raw').textContent = 'raw ' + raw + ' · 0x' + (raw < 0 ? '-' : '') +
      Math.abs(raw).toString(16).toUpperCase() + ' · bytes ' + p.off + '–' + (p.off + p.w / 8 - 1);
  }

  $('pb-n').textContent = pbHist.length ? pbHist.length + ' samples' : 'waiting';
  if (pbHist.length) {
    const lo = Math.min.apply(null, pbHist), hi = Math.max.apply(null, pbHist);
    $('pb-mm').textContent = (lo === hi) ? 'flat at ' + pbFmt(lo) : pbFmt(lo) + ' … ' + pbFmt(hi);
    pbSpark(pbHist);
    $('pb-note').innerHTML = note(lo === hi
      ? 'Not moving. Either these bytes are not the signal, or nothing has changed yet. Test it against something you can make change on demand.'
      : 'Moving. Read the dashboard aloud and compare: speed follows the needle up AND down, an odometer only ever climbs, a rolling counter wraps to zero. Confirm a candidate against an independent reference before trusting it, and never put an unvalidated identifier in the fleet signals.cfg.');
  } else {
    $('pb-mm').textContent = '–';
    $('pb-note').innerHTML = note('No frame from this identifier yet.');
  }
}

/* =============================================================================
   Health — is the ESP32 keeping up, or overworked?

   The verdict leans on the numbers that move BEFORE frames are lost: how full
   the queues got, how close each stack came to its end, how late the
   housekeeping loop woke. CPU load is shown but not trusted for the verdict: it
   is measured with idle hooks and under-states load (see SystemHealth.h).
   ============================================================================= */
const HP = {missed: null, overrun: null, qdrop: null};

function meter(label, fraction, text, tone, title) {
  const w = Math.max(0, Math.min(100, fraction * 100));
  return '<div class="mtr"' + (title ? ' title="' + esc(title) + '"' : '') + '><span>' + label +
    '</span><i><b class="' + (tone || '') + '" style="width:' + w.toFixed(1) + '%"></b></i><em>' +
    text + '</em></div>';
}
const tone = (v, warn, bad) => v >= bad ? 'bad' : v >= warn ? 'warn' : '';

function health(d) {
  const h = d.health;
  if (!h) return;
  const reasons = [];            // [severity 1|2, sentence]
  const flag = (sev, text) => reasons.push([sev, text]);

  // ---- losses, now versus earlier --------------------------------------------
  const lostNow = HP.missed !== null && (h.drv.missed > HP.missed || h.drv.overrun > HP.overrun);
  const qdropNow = HP.qdrop !== null && d.cap.qdrop > HP.qdrop;
  if (lostNow) flag(2, 'The CAN driver is losing frames right now — its queue filled or the controller FIFO overran.');
  else if (h.drv.missed + h.drv.overrun > 0) flag(1, (h.drv.missed + h.drv.overrun).toLocaleString() + ' frame(s) lost by the driver earlier since boot; none in the last half second.');
  if (qdropNow) flag(2, 'The capture writer is falling behind right now — frames are reaching the bus counters but not the file.');
  HP.missed = h.drv.missed; HP.overrun = h.drv.overrun; HP.qdrop = d.cap.qdrop;

  // ---- core meters ------------------------------------------------------------
  const drvPeak = h.drv.cap ? h.drv.peak / h.drv.cap : 0;
  const qPeak = h.q.cap ? h.q.peak / h.q.cap : 0;
  if (drvPeak >= 0.9) flag(2, 'The CAN driver queue has been ' + Math.round(drvPeak * 100) + '% full — one busy moment from losing frames.');
  else if (drvPeak >= 0.5) flag(1, 'The CAN driver queue has reached ' + Math.round(drvPeak * 100) + '% full.');
  if (qPeak >= 0.9) flag(2, 'The capture queue has been ' + Math.round(qPeak * 100) + '% full.');
  else if (qPeak >= 0.5) flag(1, 'The capture queue has reached ' + Math.round(qPeak * 100) + '% full — the flash writer is the slow side.');

  const heapUsed = h.heap.total - h.heap.free;
  if (h.heap.min < 16384) flag(2, 'Free heap has dipped to ' + kb(h.heap.min) + '. An allocation could fail.');
  else if (h.heap.min < 32768) flag(1, 'Free heap has dipped to ' + kb(h.heap.min) + '.');
  if (h.heap.largest < 8192) flag(1, 'The largest free heap block is ' + kb(h.heap.largest) + ' — memory is fragmented.');

  const lagX = h.loop.expect ? h.loop.max / h.loop.expect : 0;
  if (lagX >= 20) flag(2, 'The housekeeping loop woke ' + h.loop.max + ' ms late in the last second (it sleeps ' + h.loop.expect + ' ms). Core 1 is starved.');
  else if (lagX >= 4) flag(1, 'The housekeeping loop ran ' + h.loop.max + ' ms between wake-ups; it sleeps ' + h.loop.expect + ' ms.');

  const cpu = h.cpu.map(v => v < 0 ? null : v / 10);
  cpu.forEach((v, i) => {
    if (v === null) return;
    if (v >= 90) flag(2, 'Core ' + i + ' is about ' + Math.round(v) + '% busy.');
    else if (v >= 70) flag(1, 'Core ' + i + ' is about ' + Math.round(v) + '% busy.');
  });

  let rows = '';
  rows += meter('Core 0 · CAN', cpu[0] === null ? 0 : cpu[0] / 100, cpu[0] === null ? 'measuring' : '≈' + cpu[0].toFixed(0) + '%', cpu[0] === null ? '' : tone(cpu[0], 70, 90), 'Approximate, from idle hooks. Under-states load.');
  rows += meter('Core 1 · other', cpu[1] === null ? 0 : cpu[1] / 100, cpu[1] === null ? 'measuring' : '≈' + cpu[1].toFixed(0) + '%', cpu[1] === null ? '' : tone(cpu[1], 70, 90), 'Approximate, from idle hooks. Under-states load.');
  rows += meter('Heap in use', heapUsed / h.heap.total, kb(heapUsed) + ' / ' + kb(h.heap.total), tone(heapUsed / h.heap.total, 0.8, 0.92));
  rows += meter('Heap low-water', 1 - h.heap.min / h.heap.total, kb(h.heap.min) + ' free at worst', h.heap.min < 16384 ? 'bad' : h.heap.min < 32768 ? 'warn' : '', 'The least free heap there has ever been since boot.');
  rows += meter('CAN driver queue', drvPeak, 'peak ' + h.drv.peak + ' / ' + h.drv.cap, tone(drvPeak, 0.5, 0.9), 'msgs_to_rx, highest since boot. Frames arrive here before the reader task takes them.');
  rows += meter('Capture queue', qPeak, 'peak ' + h.q.peak + ' / ' + h.q.cap, tone(qPeak, 0.5, 0.9), 'Frames waiting for the flash writer, highest since boot.');
  rows += meter('Loop lag', Math.min(1, lagX / 20), h.loop.max + ' ms · worst ' + h.loop.ever + ' ms', tone(lagX, 4, 20), 'Housekeeping sleeps ' + h.loop.expect + ' ms; this is how long it actually took, last second and since boot.');
  if (h.fs.total) rows += meter('Flash filesystem', h.fs.used / h.fs.total, kb(h.fs.used) + ' / ' + kb(h.fs.total), tone(h.fs.used / h.fs.total, 0.8, 0.95));
  if (h.app.size) rows += meter('Firmware image', h.app.used / h.app.size, kb(h.app.used) + ' / ' + kb(h.app.size), tone(h.app.used / h.app.size, 0.8, 0.95));
  rows += meter('Frames lost by driver', 0, (h.drv.missed + h.drv.overrun).toLocaleString() + ' · ' + h.drv.missed + ' queue + ' + h.drv.overrun + ' FIFO', (h.drv.missed + h.drv.overrun) ? 'bad' : '', 'rx_missed_count + rx_overrun_count, counted by the TWAI driver in frames. The "missed" figure in CanStats counts alert events instead.');
  $('h-core').innerHTML = rows;

  let tasks = '';
  h.tasks.forEach(t => {
    const used = (t.s - t.f) / t.s;
    const headroom = t.f / t.s;
    if (headroom < 0.1) flag(2, 'Task ' + t.n + ' has come within ' + t.f + ' bytes of its stack end.');
    else if (headroom < 0.25) flag(1, 'Task ' + t.n + ' has used ' + Math.round(used * 100) + '% of its stack at its deepest.');
    tasks += meter(t.n + ' · core ' + t.c, used, t.f + ' B spare of ' + t.s, tone(1 - headroom, 0.75, 0.9));
  });
  $('h-tasks').innerHTML = tasks;

  const worst = reasons.reduce((m, r) => Math.max(m, r[0]), 0);
  const word = worst === 2 ? ['Overworked', 'bad'] : worst === 1 ? ['Tight', 'warn'] : ['Healthy', 'ok'];
  $('hn').innerHTML = tag(word[0].toLowerCase(), worst === 2 ? 't-bad' : worst === 1 ? 't-warn' : 't-ok');
  $('h-verdict').innerHTML = '<div class="hv"><b class="' + word[1] + '">' + word[0] + '</b><span>' +
    (worst === 0 ? 'Keeping up with margin. No queue near full, no stack near its end, no frames lost.'
                 : reasons.filter(r => r[0] === worst).map(r => r[1]).join(' ')) + '</span></div>';
  const lesser = reasons.filter(r => r[0] < worst);
  let extra = lesser.length ? lesser.map(r => r[1]).join(' ') : '';
  // Naming the symptom without naming the cause sent somebody looking at the
  // dashboard's own load, which is not where the frames go.
  if (worst === 2 && d.cap && d.cap.sink) {
    extra += (extra ? ' ' : '')
          + 'Almost all of this is the recorder: the CAN interrupt lives in flash, '
          + 'so every write to flash stops it and the controller FIFO overruns. '
          + 'Stop the recording above and the loss goes to zero. Filtering what '
          + 'this page shows does not change it.';
  }
  $('h-note').innerHTML = extra ? note(extra, 'warn') : '';
}

/* =============================================================================
   Recording control

   The only switch on this page that changes whether frames are lost. The CAN
   interrupt lives in flash on this framework, so every flash write disables the
   instruction cache and the interrupt cannot run; the controller FIFO then
   overruns. Measured on a DFSK Gelora E: 14.8 % of the bus lost while recording
   to flash, and nothing at all lost with flash idle.

   So a session that only needs to WATCH the mapped values should not be
   recording at all, and this is how somebody standing at a vehicle turns it off
   without a laptop.
   ============================================================================= */
let recBusy = false;

function recPaint(sink) {
  const on = !!sink;
  const st = $('r-state'), why = $('r-why'), btn = $('r-btn');
  if (!st) return;
  st.textContent = on ? 'Recording to flash' : 'Not recording';
  st.className = on ? 'on' : 'off';
  why.textContent = on
    ? 'Flash writes stall the CAN interrupt: about 15 % of the bus is being lost '
    + 'right now. Leave this on only while the recording is the point.'
    : 'Nothing is being written, so no frames are being lost to flash. '
    + 'The bus is still being received and the dashboard is still live.';
  if (!recBusy) {
    btn.textContent = on ? 'Stop' : 'Record';
    btn.className = '';
  }
}

(function () {
  const btn = $('r-btn');
  if (!btn) return;
  btn.addEventListener('click', () => {
    if (recBusy) return;
    const wasOn = $('r-state').className === 'on';
    recBusy = true;
    btn.className = 'busy';
    btn.textContent = '…';
    fetch('/api/sink?mode=' + (wasOn ? 'off' : 'file'), {method: 'POST'})
      .then(r => { if (!r || !r.ok) throw 0; })
      .catch(() => { btn.className = 'fail'; btn.textContent = 'FAILED'; })
      .then(() => {
        // The next poll is the authority on what the device actually did; this
        // only releases the button.
        setTimeout(() => { recBusy = false; tick(); }, 600);
      });
  });
})();

/* =============================================================================
   Capture files
   ============================================================================= */
async function loadFiles() {
  try {
    const r = await fetch('/api/captures', {cache: 'no-store'});
    if (!r.ok) throw new Error(r.status);
    const j = await r.json();
    const cur = (j.current || '').split('/').pop();
    let html = j.files.sort((a, b) => a.name.localeCompare(b.name)).map(f =>
      '<a href="/api/capture?name=' + encodeURIComponent(f.name) + '"' + (f.name === cur ? ' class="cur"' : '') +
      '><span>' + esc(f.name) + '</span><span>' + kb(f.size) + '</span></a>').join('');
    if (j.journal) html += '<a href="/api/capture?name=journal.log"><span>notes journal</span><span>' + kb(j.journal) + '</span></a>';
    $('c-files').innerHTML = (html || '<p class="lednb">No capture files yet.</p>') +
      '<p class="lednb">Download a finished segment, convert it with <b>tools/capture_to_webcan.py</b>, and hand the CSV to the reverse-engineering skill. The segment marked <i>writing</i> is still missing what sits in RAM.</p>';
  } catch (e) {
    $('c-files').innerHTML = '<p class="lednb">Could not list files.</p>';
  }
}

/* =============================================================================
   Paint
   ============================================================================= */
function paint(d) {
  const c = d.can, g = d.gps, e = d.env, f = d.fan, cap = d.cap, h = d.health, now = Date.now();

  /* The authoritative identifier count, and whether the array was cut short.
     Declared first: the verdict sentence reads it before the metrics row does. */
  const uniq = (c.uniq === undefined) ? d.ids.length : c.uniq;
  const cut = d.ids.length < uniq;
  const lost = h ? h.drv.missed + h.drv.overrun : c.missed;

  $('sub').textContent = d.unit + ' · ' + d.ip + ' · ' + d.fw;
  $('hstat').textContent = c.listen_only ? 'listen-only' : 'UNLOCKED';
  $('pip').style.background = c.listen_only ? 'var(--g)' : 'var(--bad)';

  /* ---- verdict: a sentence, because the number alone answers nothing ---- */
  let fps = null;
  if (prevRx !== null && now > prevT) fps = Math.round((c.rx - prevRx) * 1000 / (now - prevT));
  prevRx = c.rx; prevT = now;

  let vl, vw, cls = '';
  if (!c.running) {
    vl = 'CAN driver is down';
    vw = 'The TWAI driver failed to start. Check the serial log — this is a firmware or wiring fault, not a quiet bus.'; cls = 'bad';
  } else if (c.rx === 0 && c.err > 0) {
    /* Bus errors climbing with nothing received is the signature of the WRONG
       BITRATE. Without this branch it reads 'No frames yet', and a 250 kbps
       vehicle looks exactly like an unplugged connector. */
    vl = 'Wrong bitrate, most likely';
    vw = 'Nothing decoded, but ' + c.err.toLocaleString() + ' bus error' + (c.err === 1 ? '' : 's') +
      '. The wire is live — the controller is hearing transitions it cannot frame at ' + (c.bitrate / 1000) +
      ' kbps. Most vehicles run 500; many older and body buses run 250. Reflash with the other rate and try again.'; cls = 'warn';
  } else if (c.rx === 0) {
    vl = 'No frames yet';
    vw = 'The driver is running at ' + (c.bitrate / 1000) + ' kbps in listen-only mode and has seen nothing. On a bench that is expected until a bus is attached. On a vehicle, check the 60 Ω across CANH–CANL first — and remember many cars keep the OBD-II channel silent until a scan tool asks.';
  } else if (c.silence_ms > 2000) {
    vl = 'Bus went quiet';
    vw = c.rx.toLocaleString() + ' frames arrived, then nothing for ' + Math.round(c.silence_ms / 1000) +
      ' s. The wiring was good, so this is the bus stopping rather than a fault — ignition off, or a connector moved.'; cls = 'warn';
  } else {
    vl = 'Reading the bus' + (fps ? ' · ' + fps.toLocaleString() + ' frames/s' : '');
    vw = c.rx.toLocaleString() + ' frames from ' + uniq + ' identifier' + (uniq === 1 ? '' : 's') + '.' +
      ((lost || c.drop) ? ' Some frames were lost — see Health.' : '');
  }
  $('vl').className = 'vl ' + cls;
  $('vl').textContent = vl;
  $('vw').textContent = vw;

  $('m-rx').textContent = c.rx.toLocaleString();
  $('m-id').textContent = uniq;
  $('m-drop').textContent = c.drop; $('m-drop').className = c.drop ? 'hot' : '';
  $('m-miss').textContent = lost.toLocaleString(); $('m-miss').className = lost ? 'hot' : '';
  $('m-err').textContent = c.err; $('m-err').className = c.err ? 'hot' : '';
  $('m-rate').textContent = (c.bitrate / 1000) + ' kbps';

  /* ---- identifiers ---- */
  $('idn').textContent = uniq ? (cut ? d.ids.length + ' of ' + uniq : uniq + ' seen') : '';
  monPaint(d);

  /* An instrument has to be able to accuse itself. Three separate ways this
     table can mislead, each named rather than hidden. */
  let it = '', ic = '';
  const shown = d.ids.reduce((a, x) => a + x.n, 0);
  if (cut) { ic = 'bad';
    it = '<b>Showing ' + d.ids.length + ' of ' + uniq + ' identifiers.</b> The state response filled up before the list ended, so rows are missing. Do not choose reverse-engineering targets from this table until it fits — raise WEB_JSON_BUF.';
  } else if (c.idfull) { ic = 'warn';
    it = '<b>The identifier survey is full at ' + uniq + '.</b> Identifiers first seen after that point are not counted anywhere. This is the ceiling in CanManager, not the bus.';
  } else if (c.rx > 2000 && shown < c.rx * 0.9) { ic = 'bad';
    it = '<b>These rows do not add up.</b> They total ' + shown.toLocaleString() + ' against ' + c.rx.toLocaleString() + ' received, and neither the buffer nor the survey ceiling explains the gap. Treat the table as unreliable and report it.';
  }
  $('id-note').innerHTML = note(it, ic);

  /* ---- GNSS ---- */
  $('gn').innerHTML = g.fix ? tag('fix', 't-ok') : (g.silent ? tag('no data', 't-bad') : tag('searching', 't-warn'));
  $('g-pos').textContent = g.fix ? g.lat.toFixed(6) + ', ' + g.lon.toFixed(6) : '—';
  $('g-sog').textContent = g.fix ? g.sog.toFixed(1) + ' km/h' : '—';
  $('g-sat').textContent = g.sats + (g.hdop ? ' / ' + g.hdop.toFixed(1) : ' / —');
  $('g-utc').textContent = g.time_valid && g.epoch ? new Date(g.epoch * 1000).toISOString().replace('.000Z', 'Z') : '—';
  $('g-byt').textContent = g.bytes.toLocaleString() + ' / ' + g.lines.toLocaleString();
  $('g-sen').textContent = g.sentences.toLocaleString() + ' / ' + g.badcrc;
  $('g-sky').textContent = g.view === undefined ? '–' : g.view + ' / ' + g.trk;
  $('g-cnr').textContent = g.cnr ? g.cnr + ' dB-Hz' : (g.view === undefined ? '–' : 'nothing heard');
  let gt = '', gc = '';
  if (g.silent) { gc = 'bad';
    gt = (g.bytes === 0 ? '<b>No bytes at all</b> on the wire.' : '<b>Only ' + g.bytes + ' byte(s) in ' + d.uptime_s + ' s</b> — line noise, not a module.') +
      ' This is not searching. GPS TX must reach GPIO18 (module TX → ESP32 RX), and the module needs 3V3 and GND. Baud is irrelevant until a steady byte stream appears.';
  } else if (g.lines === 0) { gc = 'warn';
    gt = 'Steady byte stream but no complete lines — wrong baud rate. The module is talking; we listen at 9600.';
  } else if (g.sentences === 0 && g.badcrc > 0) { gc = 'warn';
    gt = 'Lines arrive and every checksum fails. Baud close but wrong, or a noisy line.';
  } else if (g.sentences === 0) { gc = 'warn';
    gt = 'Lines parse but none are GGA or RMC. The module emits other sentence types only.';
  } else if (!g.fix && g.view === 0 && g.cnr === 0) { gc = 'bad';
    gt = '<b>The receiver hears nothing at all.</b> It is streaming valid NMEA, so the module, the wiring and the supply are fine — but GSV reports no satellites in view and no signal on any channel. Indoors or under a roof, take it outside and give it two to five minutes. Already outside: that is the antenna — not connected, facing away from the sky, or dead.';
  } else if (!g.fix && g.cnr === 0) { gc = 'bad';
    gt = '<b>It knows where ' + g.view + ' satellites should be, and hears none of them.</b> Those positions come from the stored almanac, not from reception. Zero carrier-to-noise on every channel points at the antenna rather than the sky.';
  } else if (!g.fix && g.cnr < 25) { gc = 'warn';
    gt = '<b>Hearing satellites, too weakly to lock.</b> Strongest is ' + g.cnr + ' dB-Hz; a fix needs roughly 30 and four satellites at once. The antenna works — this is sky view. Outdoors, ceramic patch facing up.';
  } else if (!g.fix) {
    gt = '<b>Signal is strong enough.</b> Strongest ' + g.cnr + ' dB-Hz across ' + g.trk + ' of ' + g.view + ' satellites. It needs four at once plus the ephemeris, which takes up to 12.5 minutes to download on a cold start. Keep it still and in the open.';
  }
  $('g-note').innerHTML = note(gt, gc);

  /* ---- thermal ---- */
  $('tn').innerHTML = !e.enabled ? tag('off', 't-idle') : (e.valid ? tag('reading', 't-ok') : tag('no reply', 't-bad'));
  $('e-t').textContent = e.enabled ? (e.valid ? e.t.toFixed(1) + ' °C' : '—') : 'disabled';
  $('e-h').textContent = (e.enabled && e.valid) ? e.h.toFixed(1) + ' %RH' : '—';
  $('f-t').textContent = f.tvalid ? f.t.toFixed(1) + ' °C' : '—';
  $('f-s').innerHTML = f.mode + ' · ' + (f.on ? tag('running', 't-ok') : 'off') + ' · ' + f.run_s + ' s total';
  $('e-c').textContent = e.ok + ' / ' + e.read_err + ' / ' + e.crc_err;
  $('e-l').innerHTML = e.idle === undefined ? '–' : (e.idle ? tag('high', 't-ok') : tag('low', 't-bad')) + (e.bits ? ' · ' + e.bits + '/40 bits' : '');
  let et = '', ec = '';
  if (!e.enabled) et = 'DHT22 is disabled in Config.h.';
  else if (e.ok > 0 && e.valid) et = 'Enclosure air versus the SoC die is the measurement the fleet fan threshold is waiting on. Log the peak reached in a parked car.';
  else if (e.read_err === 0 && e.crc_err === 0) et = 'No read attempted yet. The first sample lands about 10 s after boot.';
  else if (e.idle === false) { ec = 'bad'; et = '<b>DATA sits low between reads.</b> With the internal pull-up on, an idle line must read high, so nothing here is a timing problem: either no pull-up reaches GPIO15, DATA is shorted to ground, or the part is holding the line down. On a 3-pin module a swapped VCC and GND does exactly this — and usually kills the sensor.'; }
  else if (e.nores > 0) { ec = 'bad'; et = '<b>The line is healthy and nothing answers on it.</b> DATA idles high, so the pull-up and the wire are fine; the sensor simply never pulls it down. That is power or the part: measure 3V3 at the sensor pins themselves, and check the pin order — 3-pin DHT22 boards ship as VCC-DATA-GND and as DATA-VCC-GND, and the two are not interchangeable.'; }
  else if (e.hshake > 0) { ec = 'warn'; et = '<b>It starts to answer, then stops.</b> The sensor pulls the line down but never completes the 80/80 handshake. Usually a pull-up too weak for the cable, or a supply that sags when the sensor wakes.'; }
  else if (e.trunc > 0) { ec = 'warn'; et = '<b>The frame is cut short</b> after ' + e.bits + ' of 40 bits. The sensor is alive and the handshake is good, so this is edge timing: pull-up strength, wire length, or interference.'; }
  else if (e.crc_err > 0) { ec = 'warn'; et = 'All 40 bits arrive but the checksum fails — signal integrity rather than wiring.'; }
  else if (e.rng > 0) { ec = 'warn'; et = '<b>The checksum passes but the values are impossible.</b> Frame ' + esc(e.raw) + '. Do not trust the checksum here: a frame captured one bit out of step still passes it, because shifting doubles every byte and doubling is linear mod 256. That is a framing fault, not a wiring one. A DHT11 fitted in place of a DHT22 also lands here.'; }
  $('e-note').innerHTML = note(et, ec);

  /* ---- capture ---- */
  const sinks = ['off', 'serial', 'file', 'serial + file'];
  $('cn').innerHTML = cap.sink ? tag(sinks[cap.sink] || cap.sink, 't-ok') : tag('off', 't-idle');
  recPaint(cap.sink);
  $('c-f').textContent = cap.frames.toLocaleString();
  $('c-b').textContent = kb(cap.bytes);
  $('c-p').textContent = cap.path || '—';
  $('c-qd').textContent = cap.qdrop === undefined ? '–' : cap.qdrop.toLocaleString();
  $('c-qd').className = cap.qdrop ? 'hot' : '';
  $('c-mk').textContent = d.marks === undefined ? '–' : d.marks;
  $('c-nt').textContent = d.notes === undefined ? '–' : d.notes;
  $('c-note').innerHTML = (cap.frames < c.rx && c.rx > 0 && cap.qdrop)
    ? note('<b>Written is behind received.</b> The flash writer dropped ' + cap.qdrop.toLocaleString() + ' frame(s), so the file is a sample. Bus reception is unaffected — <i>received</i> above still counts every frame.', 'warn')
    : '';

  /* ---- device ---- */
  $('s-u').textContent = d.unit; $('s-fw').textContent = d.fw;
  $('s-up').textContent = d.uptime_s < 3600 ? d.uptime_s + ' s' : (d.uptime_s / 3600).toFixed(1) + ' h';
  $('s-wf').textContent = d.wifi + (d.rssi ? ' · ' + d.rssi + ' dBm' : '');
  $('s-rq').textContent = d.reqs.toLocaleString();
  /* A capture header that says boot_epoch=0 has to be aligned to the run sheet by
     hand afterwards, so say plainly which of the two we are in. */
  $('s-clk').innerHTML = d.clock ? new Date(d.clock * 1000).toISOString().replace('.000Z', 'Z') : tag('not set', 't-warn');
  const L = LED[d.led] || [d.led, 'Unrecognised status.'];
  $('s-led').textContent = L[0];
  $('s-lednote').innerHTML = note(L[1], L[2] || '');
  $('s-ledtab').innerHTML = '<h3>Every pattern this build can show</h3>' +
    LED_ORDER.map(k => '<div class="' + (k === d.led ? 'on' : '') + '"><i></i><b>' + LED[k][0] + '</b><span>' + LED[k][3] + '</span></div>').join('') +
    '<p class="lednb">Most severe first. The light always shows the highest condition that is true, so a fault hides everything below it.</p>';

  health(d);
  probe(d);
  $('ft').textContent = 'refreshed every 500 ms';
}

async function tick() {
  let d;
  try {
    const r = await fetch('/api/state?ids=' + FILTER, {cache: 'no-store'});
    if (!r.ok) throw new Error('HTTP ' + r.status);
    d = await r.json();
    fails = 0;
  } catch (err) {
    if (++fails > 2) {
      $('hstat').textContent = 'offline';
      $('pip').style.background = 'var(--bad)';
      $('vl').className = 'vl bad';
      $('vl').textContent = 'Lost contact with the device';
      $('vw').textContent = 'The page is still retrying. Check that this device is on the same network and the board still has power.';
    }
    return;
  }
  // A fault in painting is a bug in this page, not a lost connection, and must
  // not be reported as one. It once was: a JavaScript scoping error showed up as
  // "Lost contact with the device".
  if (autoRelax(d)) { tick(); return; }
  try { paint(d); }
  catch (err) {
    $('vl').className = 'vl bad';
    $('vl').textContent = 'Dashboard error';
    $('vw').textContent = 'The device answered, but this page failed to draw it: ' + (err && err.message ? err.message : err);
    console.error(err);
  }
}

/* The marker POST writes a line into the capture FILE only — there is no path
   from it to the CAN bus. Feedback is visual because the operator will not be
   reading the screen when they press it. */
try { const l = localStorage.getItem('bmt.mark'); if (l) $('mk-l').value = l; } catch (e) {}
$('mk-b').addEventListener('click', function () {
  const b = $('mk-b'), l = ($('mk-l').value || 'MARK').trim();
  try { localStorage.setItem('bmt.mark', l); } catch (e) {}
  b.className = 'sent'; b.textContent = '…';
  fetch('/api/mark?label=' + encodeURIComponent(l), {method: 'POST'})
    .then(function (r) { if (!r || !r.ok) throw 0; b.className = 'sent'; b.textContent = 'WRITTEN'; })
    .catch(function () { b.className = 'fail'; b.textContent = 'FAILED'; })
    .then(function () { setTimeout(function () { b.className = ''; b.textContent = 'MARK'; }, 1100); });
});

pbLoad();
PB_KEYS.forEach(k => { const el = $(k); if (el) el.addEventListener('change', () => { pbSave(); pbHist = []; pbSig = ''; }); });
loadNotes();
loadFiles();
setInterval(loadFiles, 15000);
tick();
setInterval(tick, 500);
