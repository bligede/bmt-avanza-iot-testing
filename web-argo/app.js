/* =============================================================================
   SELARIDE argo, the version that runs ON THE DEVICE.

   The frame in project-mdt-tds drives itself from a mock vehicle. This one has
   a real bus underneath, so the mock is gone and every number comes from
   /api/signals: the values the firmware decodes from the proven DFSK mappings,
   plus the meter's two billed quantities.

   THE SPLIT, and it is deliberate. The DEVICE counts quantities: billed
   kilometres from the odometer, waiting minutes from the speed frames. This
   PAGE turns them into rupiah, using tariff.js, which stays the one file in the
   system where a price is written down. Two places that both know the price are
   two places that can disagree about a passenger's money.

   The confidence rule still holds: a value whose signal is not `terbukti` in
   profile.js is removed from the DOM, not dimmed.
   ============================================================================= */

const $  = id => document.getElementById(id);
const nf = (v, d) => v.toLocaleString('id-ID', { minimumFractionDigits: d, maximumFractionDigits: d });
const id2 = n => String(Math.floor(Math.max(0, n))).padStart(2, '0');

/* ---- the confidence rule, enforced on the DOM ------------------------------ */
(function pruneUnproven() {
  document.querySelectorAll('[data-v]').forEach(el => {
    const id = el.dataset.v;
    // `tripKm` is the meter's billed distance, not a signal in the profile.
    if (id === 'tripKm' || isProven(id)) return;
    (el.closest('.metric, .act, .fact, .stat') || el).remove();
  });
  const notYet = SIGNALS.filter(s => s.status !== 'terbukti');
  const el = $('prov-hidden');
  if (el && notYet.length) {
    el.textContent = ` · ${notYet.length} sinyal belum dipetakan: `
      + notYet.map(s => s.label.toLowerCase()).join(', ');
  }
})();

/* The GPS readout belongs to the engineering dashboard, which has the GNSS fix,
   the satellite count and the HDOP. This screen would be repeating a word
   without the numbers behind it. */
document.querySelectorAll('.stat').forEach(el => {
  if (el.querySelector('#gps-b')) el.remove();
});

/* The distance bar measured progress toward a destination. This device has no
   destination: it has a meter. A bar with no real maximum is decoration
   wearing a measurement's clothes, so it goes. */
document.querySelector('#m-km .pbar')?.remove();

/* ---- identity --------------------------------------------------------------- */
(function driver() {
  if (DRIVER.photo) {
    const img = new Image();
    img.onload = () => { const f = $('who-face'); f.textContent = ''; f.appendChild(img); };
    img.alt = '';
    img.src = DRIVER.photo;
  }
  $('who-name').textContent  = DRIVER.name;
  $('who-star').textContent  = nf(DRIVER.rating, 1);
  $('who-trips').textContent = nf(DRIVER.trips, 0);
  $('who-phone').textContent = DRIVER.phone;
  $('who-org').textContent   = DRIVER.org;
  $('who-plate').textContent = DRIVER.plate;
})();

const cluster = $('cluster');
cluster.classList.add('booting');
setTimeout(() => cluster.classList.remove('booting'), 900);

/* ---- the button, which is the one live control on this screen --------------- */
/* On the product this flag comes from dispatch: the SELARIDE documentation puts
   the whole order flow in the Driver App and this screen is the argometer. On
   the test vehicle there is no dispatch, so a person presses it and acts as the
   passenger. Who sets it in production is an open question in
   project-mdt-tds `08`. */
let busy = false;
const btn = $('btn-argo');
btn.addEventListener('click', async () => {
  if (busy) return;
  busy = true;
  btn.disabled = true;
  try {
    const action = running ? 'stop' : 'start';
    const r = await fetch('/api/argo?action=' + action, { method: 'POST' });
    if (r.ok) paint(await r.json());
  } catch (e) {
    // The next poll will correct the screen; a failed press must not leave the
    // button stuck.
  } finally {
    busy = false;
    btn.disabled = false;
  }
});

/* ---- the feed ---------------------------------------------------------------- */
let running  = false;
let lastOk   = 0;      // when the device last answered
let everOk   = false;

const val = (s, fallback) => (s && typeof s.v === 'number') ? s.v : fallback;

function paint(d) {
  const sig = d.signals || {};
  const a   = d.argo || {};
  running = !!a.running;

  // ---- the vehicle, straight off the bus
  const speed = val(sig.speed, 0);
  const soc   = val(sig.soc, 0);
  $('m-speed').querySelector('b').textContent = nf(speed, 0);
  $('bar-speed').style.width = (Math.max(0, Math.min(1, speed / 120)) * 100).toFixed(1) + '%';

  const bf = $('batt-fill'), bc = $('c-batt');
  if (bf && sig.soc) {
    const h = Math.max(0, Math.min(100, soc)) / 100 * 32.6;
    bf.setAttribute('height', h.toFixed(2));
    bf.setAttribute('y', (39.8 - h).toFixed(2));
    bc.classList.toggle('warn', soc < 25 && soc >= 12);
    bc.classList.toggle('low', soc < 12);
    bc.querySelector('b span').textContent = nf(soc, 0);
  }

  // ---- the meter, in quantities the device counted
  const km   = a.km_isi   || 0;
  const wait = a.wait_min || 0;
  $('fare-km').textContent   = nf(km, 1);
  $('fare-wait').textContent = nf(wait, 1);
  $('m-km').querySelector('b').textContent = nf(km, 1);

  const secs = a.elapsed_s || 0;
  $('dur-h').textContent = id2(secs / 3600);
  $('dur-m').textContent = id2((secs % 3600) / 60);
  $('dur-s').textContent = id2(secs % 60);

  // ---- and the money, computed here, from the one file that holds a price
  const total = TARIFF.flagFall
              + Math.round(km * TARIFF.perKm)
              + Math.round(wait * TARIFF.perMinute);
  $('fare').textContent = grouped(total);

  // ---- states
  cluster.classList.toggle('empty', !running);
  cluster.classList.toggle('waiting', !!a.waiting);
  $('hire-text').textContent = running ? 'Terisi' : 'Kosong';
  $('disp-b').textContent    = running ? 'Isi' : 'Siap';
  $('disp-s').textContent    = running ? 'penumpang' : 'terima order';
  $('btn-argo-text').textContent = running ? 'Akhiri trip' : 'Mulai trip';
  btn.classList.toggle('stop', running);

  $('m-state-b').textContent = running ? 'jalan' : 'berhenti';
  $('m-state-c').textContent = running
    ? (a.waiting ? ', menghitung waktu tunggu'
                 : (a.anchored ? ', menghitung kilometer isi'
                               : ', menunggu odometer dari bus'))
    : ', kilometer kosong tidak ditagih';

  lastOk = Date.now();
  everOk = true;
}

/* D-004: a number that stopped arriving looks exactly like a number that
   stopped changing. The only difference the driver can act on is this banner,
   and here it covers both halves: the device not answering, and the device
   answering with readings that have gone quiet. */
function checkStale(sig) {
  const ageDevice = everOk ? (Date.now() - lastOk) / 1000 : 999;
  const ageSignal = sig && sig.speed ? (sig.speed.age_ms || 0) / 1000 : 999;
  const bad = ageDevice > 10 || ageSignal > 30;
  $('stale').hidden = !bad;
  if (bad) $('stale-age').textContent = Math.round(Math.max(ageDevice, ageSignal));
  cluster.classList.toggle('stale-on', bad);
}

let lastSignals = null;
async function tick() {
  try {
    const r = await fetch('/api/signals', { cache: 'no-store' });
    if (r.ok) { const d = await r.json(); lastSignals = d.signals; paint(d); }
  } catch (e) {
    // Left to checkStale: one failed poll on a moving vehicle is normal, a
    // minute of them is not.
  }
  checkStale(lastSignals);
}
tick();
setInterval(tick, 500);

function tickClock() {
  const now = new Date();
  $('clock').textContent = id2(now.getHours()) + ':' + id2(now.getMinutes());
  $('date').textContent = now.toLocaleDateString('id-ID',
    { weekday: 'short', day: 'numeric', month: 'short', year: 'numeric' });
}
tickClock();
setInterval(tickClock, 10000);

$('wait-unapproved').hidden = TARIFF.waitApproved;
