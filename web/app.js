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
  inp.placeholder = MON.notesOk ? 'name this ID…' : 'notes unavailable';
  inp.disabled = !MON.notesOk;
  inp.autocomplete = 'off';
  inp.spellcheck = false;
  inp.setAttribute('aria-label', 'Name for ' + hex(x.id, x.x ? 8 : 3));
  inp.value = MON.notes.get(k) || '';
  inp.addEventListener('change', () => saveNote(x.id, x.x, inp));
  inp.addEventListener('keydown', e => { if (e.key === 'Enter') inp.blur(); });
  nt.appendChild(inp);
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
  return {k, tr, hz, dlc, bytes, inp, win: []};
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
  if (!d.ids.length) return;          // the empty-state text stays in place
  const list = d.ids.slice().sort((a, b) => (a.x - b.x) || (a.id - b.id));
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
    else pbLoad();
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
  $('h-note').innerHTML = lesser.length ? note(lesser.map(r => r[1]).join(' '), 'warn') : '';
}

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
    const r = await fetch('/api/state', {cache: 'no-store'});
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
