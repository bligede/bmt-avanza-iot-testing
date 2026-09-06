#include "WebDashboard.h"

#include "Logger.h"

#if ENABLE_WEB_DASHBOARD

#include <WebServer.h>
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "WifiManager.h"
#include "RawCanLogger.h"

namespace {

const char* TAG = "WEB";

WebServer         s_server(WEB_PORT);
SemaphoreHandle_t s_ring_mutex = nullptr;
uint32_t          s_requests   = 0;
bool              s_started    = false;

CanFrame s_ring[WEB_FRAME_RING];
uint16_t s_ring_head  = 0;
uint32_t s_ring_total = 0;

char s_json[WEB_JSON_BUF];

inline bool lockRing() {
    return s_ring_mutex != nullptr &&
           xSemaphoreTake(s_ring_mutex, pdMS_TO_TICKS(20)) == pdTRUE;
}
inline void unlockRing() {
    if (s_ring_mutex != nullptr) xSemaphoreGive(s_ring_mutex);
}

// -----------------------------------------------------------------------------
//  The page. Self-contained: no CDN, no font, no framework. It has to load on a
//  phone tethered to its own hotspot, which may have no working internet route.
// -----------------------------------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Avanza CAN Bring-Up</title>
<style>
:root{--bg:#0e1116;--card:#171b22;--line:#252b36;--fg:#e6e9ef;--dim:#8b95a7;
--ok:#2ecc71;--warn:#f0b429;--err:#e74c3c;--acc:#4aa3ff}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);
font:14px/1.45 ui-monospace,Menlo,Consolas,monospace}
header{padding:12px 14px;border-bottom:1px solid var(--line);
display:flex;flex-wrap:wrap;gap:10px;align-items:baseline}
h1{font-size:15px;margin:0;font-weight:600}
.sub{color:var(--dim);font-size:12px}
.wrap{padding:12px;display:grid;gap:12px;
grid-template-columns:repeat(auto-fit,minmax(300px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:8px;
padding:12px;min-width:0}
.card h2{font-size:11px;letter-spacing:.09em;text-transform:uppercase;
color:var(--dim);margin:0 0 10px;font-weight:600}
.kv{display:flex;justify-content:space-between;gap:10px;padding:3px 0;
border-bottom:1px solid rgba(255,255,255,.04)}
.kv:last-child{border-bottom:0}
.kv span:first-child{color:var(--dim)}
.kv span:last-child{text-align:right;word-break:break-all}
.big{font-size:30px;font-weight:600;letter-spacing:-.5px}
.tag{display:inline-block;padding:2px 7px;border-radius:4px;font-size:11px;
font-weight:600;letter-spacing:.04em}
.t-ok{background:rgba(46,204,113,.15);color:var(--ok)}
.t-warn{background:rgba(240,180,41,.15);color:var(--warn)}
.t-err{background:rgba(231,76,60,.15);color:var(--err)}
table{width:100%;border-collapse:collapse;font-size:12px}
th{text-align:left;color:var(--dim);font-weight:600;padding:4px 6px;
border-bottom:1px solid var(--line);position:sticky;top:0;background:var(--card)}
td{padding:3px 6px;border-bottom:1px solid rgba(255,255,255,.04);
white-space:nowrap}
.scroll{max-height:340px;overflow:auto}
.idcol{color:var(--acc)}
.dim{color:var(--dim)}
.note{margin:12px 12px 0;padding:9px 12px;border-radius:8px;font-size:12px;
background:rgba(74,163,255,.08);border:1px solid rgba(74,163,255,.25);
color:#9dc7ff}
footer{padding:10px 14px;color:var(--dim);font-size:11px;
border-top:1px solid var(--line)}
</style></head><body>
<header><h1>Avanza CAN Bring-Up</h1><span class="sub" id="hdr">connecting…</span></header>

<div class="note">
Diagnostic tool. Device is <b>listen-only</b> and never transmits to the bus.
This page is read-only. Avanza CAN IDs prove the <b>reading path</b> — they are
<b>not</b> a signal map for the fleet vehicle.
</div>

<div class="wrap">
  <div class="card">
    <h2>CAN bus</h2>
    <div class="big" id="rx">–</div>
    <div class="sub" style="margin-bottom:10px">frames received</div>
    <div class="kv"><span>state</span><span id="st">–</span></div>
    <div class="kv"><span>bitrate</span><span id="rate">–</span></div>
    <div class="kv"><span>listen-only</span><span id="lock">–</span></div>
    <div class="kv"><span>rate now</span><span id="fps">–</span></div>
    <div class="kv"><span>silence</span><span id="sil">–</span></div>
    <div class="kv"><span>dropped (decode q)</span><span id="drop">–</span></div>
    <div class="kv"><span>rx missed (driver)</span><span id="miss">–</span></div>
    <div class="kv"><span>bus errors</span><span id="err">–</span></div>
    <div class="kv"><span>recoveries</span><span id="rec">–</span></div>
  </div>

  <div class="card">
    <h2>Distinct CAN IDs <span id="idn" class="dim"></span></h2>
    <div class="scroll">
      <table><thead><tr><th>ID</th><th>frames</th><th>share</th></tr></thead>
      <tbody id="idt"><tr><td colspan="3" class="dim">waiting…</td></tr></tbody></table>
    </div>
  </div>

  <div class="card">
    <h2>Live frames</h2>
    <div class="scroll">
      <table><thead><tr><th>ms</th><th>ID</th><th>DLC</th><th>data</th></tr></thead>
      <tbody id="frt"><tr><td colspan="4" class="dim">waiting…</td></tr></tbody></table>
    </div>
  </div>

  <div class="card">
    <h2>Capture</h2>
    <div class="kv"><span>sink</span><span id="csink">–</span></div>
    <div class="kv"><span>frames written</span><span id="cfr">–</span></div>
    <div class="kv"><span>bytes written</span><span id="cby">–</span></div>
    <div class="kv"><span>file</span><span id="cpath">–</span></div>
    <div class="sub" style="margin-top:8px">
      Pull with <code>cat &lt;file&gt;</code> on the serial console.<br><br>
      <b>frames written &lt; frames received</b> means the flash writer is behind
      the bus, so the file is a sample. Bus reception itself is unaffected —
      <i>rx</i> above still counts every frame.
    </div>
  </div>

  <div class="card">
    <h2>System</h2>
    <div class="kv"><span>unit</span><span id="unit">–</span></div>
    <div class="kv"><span>firmware</span><span id="fw">–</span></div>
    <div class="kv"><span>uptime</span><span id="up">–</span></div>
    <div class="kv"><span>free heap</span><span id="heap">–</span></div>
    <div class="kv"><span>WiFi</span><span id="wifi">–</span></div>
    <div class="kv"><span>RSSI</span><span id="rssi">–</span></div>
  </div>
</div>

<footer id="foot">polling every 500 ms</footer>

<script>
const $=i=>document.getElementById(i);
const hex=(n,w)=>'0x'+n.toString(16).toUpperCase().padStart(w,'0');
const tag=(t,c)=>'<span class="tag '+c+'">'+t+'</span>';
let fails=0, prevRx=null, prevT=null;

function paint(d){
  const now=Date.now();
  $('hdr').textContent=d.unit+' · '+d.ip+' · up '+d.uptime_s+'s';
  $('rx').textContent=d.can.rx.toLocaleString();
  $('st').innerHTML=d.can.running?tag('RUNNING','t-ok'):tag(d.can.state,'t-err');
  $('rate').textContent=(d.can.bitrate/1000)+' kbps';
  $('lock').innerHTML=d.can.listen_only?tag('LOCKED','t-ok'):tag('NOT LOCKED','t-err');

  if(prevRx!==null && now>prevT){
    const fps=Math.round((d.can.rx-prevRx)*1000/(now-prevT));
    $('fps').textContent=fps+' frames/s';
  }
  prevRx=d.can.rx; prevT=now;

  $('sil').innerHTML = d.can.rx===0 ? tag('NO FRAMES YET','t-warn')
    : (d.can.silence_ms>5000?tag(d.can.silence_ms+' ms','t-warn'):d.can.silence_ms+' ms');
  $('drop').innerHTML=d.can.drop?tag(d.can.drop,'t-warn'):'0';
  $('miss').innerHTML=d.can.missed?tag(d.can.missed,'t-warn'):'0';
  $('err').innerHTML=d.can.err?tag(d.can.err,'t-err'):'0';
  $('rec').textContent=d.can.rec;

  $('idn').textContent='— '+d.ids.length;
  const tot=d.ids.reduce((a,b)=>a+b.n,0)||1;
  $('idt').innerHTML = d.ids.length
    ? d.ids.slice().sort((a,b)=>b.n-a.n).map(x=>'<tr><td class="idcol">'+
        hex(x.id,x.ext?8:3)+(x.ext?' <span class="dim">EXT</span>':'')+
        '</td><td>'+x.n.toLocaleString()+'</td><td class="dim">'+
        (100*x.n/tot).toFixed(1)+'%</td></tr>').join('')
    : '<tr><td colspan="3" class="dim">none — bus silent, or not connected</td></tr>';

  $('frt').innerHTML = d.frames.length
    ? d.frames.map(f=>'<tr><td class="dim">'+f.t+'</td><td class="idcol">'+
        hex(f.id,f.ext?8:3)+'</td><td>'+f.dlc+'</td><td>'+f.d+'</td></tr>').join('')
    : '<tr><td colspan="4" class="dim">none yet</td></tr>';

  const sinks=['off','serial','file','serial+file'];
  $('csink').textContent=sinks[d.cap.sink]||d.cap.sink;
  $('cfr').textContent=d.cap.frames.toLocaleString();
  $('cby').textContent=(d.cap.bytes/1024).toFixed(1)+' KB';
  $('cpath').textContent=d.cap.path||'–';

  $('unit').textContent=d.unit;
  $('fw').textContent=d.fw;
  $('up').textContent=d.uptime_s+' s';
  $('heap').textContent=(d.heap/1024).toFixed(1)+' KB';
  $('wifi').textContent=d.wifi;
  $('rssi').textContent=d.rssi?d.rssi+' dBm':'–';
  $('foot').textContent='polling every 500 ms · '+d.reqs+' requests served';
}

async function tick(){
  try{
    const r=await fetch('/api/state',{cache:'no-store'});
    if(!r.ok) throw new Error(r.status);
    paint(await r.json()); fails=0;
  }catch(e){ if(++fails>2) $('hdr').textContent='connection lost — retrying…'; }
}
tick(); setInterval(tick,500);
</script></body></html>)HTMLPAGE";

// -----------------------------------------------------------------------------
//  JSON building. Hand-rolled with snprintf so the project keeps zero
//  third-party libraries. Every append is bounds-checked.
// -----------------------------------------------------------------------------
struct Appender {
    char*  buf;
    size_t cap;
    size_t len;
    bool   overflow;

    Appender(char* b, size_t c) : buf(b), cap(c), len(0), overflow(false) {
        if (cap > 0) buf[0] = '\0';
    }
    void add(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        if (overflow || len + 1 >= cap) { overflow = true; return; }
        va_list a;
        va_start(a, fmt);
        const int n = vsnprintf(buf + len, cap - len, fmt, a);
        va_end(a);
        if (n < 0 || static_cast<size_t>(n) >= cap - len) {
            overflow = true;
            buf[len] = '\0';
            return;
        }
        len += static_cast<size_t>(n);
    }
};

void appendCan(Appender& j) {
    const CanStats c   = CanManager::stats();
    const CanState st  = CanManager::state();
    const uint32_t sil = CanManager::silenceMs();

    j.add("\"can\":{\"running\":%s,\"state\":\"%s\",\"bitrate\":%lu,"
          "\"listen_only\":%s,\"rx\":%lu,\"drop\":%lu,\"missed\":%lu,"
          "\"err\":%lu,\"rec\":%lu,\"silence_ms\":%lu},",
          st == CanState::Running ? "true" : "false",
          st == CanState::Running       ? "RUNNING"
            : st == CanState::BusError      ? "BUS_ERROR"
            : st == CanState::InstallFailed ? "INSTALL_FAILED" : "DOWN",
          (unsigned long)CanManager::bitrate(),
          CanManager::isListenOnlyLocked() ? "true" : "false",
          (unsigned long)c.frames_received,
          (unsigned long)c.frames_dropped_queue,
          (unsigned long)c.rx_missed,
          (unsigned long)c.bus_errors,
          (unsigned long)c.recoveries,
          (unsigned long)(sil == UINT32_MAX ? 0 : sil));
}

void appendIds(Appender& j) {
    j.add("\"ids\":[");
    const uint16_t n = CanManager::seenIdCount();
    for (uint16_t i = 0; i < n; ++i) {
        uint32_t id = 0, count = 0;
        if (!CanManager::seenIdAt(i, &id, &count)) continue;
        j.add("%s{\"id\":%lu,\"n\":%lu,\"ext\":%s}", i ? "," : "",
              (unsigned long)id, (unsigned long)count,
              id > 0x7FF ? "true" : "false");
        if (j.overflow) break;
    }
    j.add("],");
}

void appendFrames(Appender& j) {
    j.add("\"frames\":[");
    if (lockRing()) {
        const uint16_t have =
            (s_ring_total < static_cast<uint32_t>(WEB_FRAME_RING))
                ? static_cast<uint16_t>(s_ring_total)
                : static_cast<uint16_t>(WEB_FRAME_RING);
        // Newest first — the eye goes to the top of a list.
        for (uint16_t k = 0; k < have; ++k) {
            const uint16_t idx = static_cast<uint16_t>(
                (s_ring_head + WEB_FRAME_RING - 1 - k) % WEB_FRAME_RING);
            const CanFrame& f = s_ring[idx];
            char bytes[32];
            Logger::formatBytes(bytes, sizeof(bytes), f.data, f.dlc);
            j.add("%s{\"t\":%lu,\"id\":%lu,\"dlc\":%u,\"ext\":%s,\"d\":\"%s\"}",
                  k ? "," : "", (unsigned long)f.rx_millis, (unsigned long)f.id,
                  (unsigned)f.dlc, f.extended ? "true" : "false", bytes);
            if (j.overflow) break;
        }
        unlockRing();
    }
    j.add("],");
}

void appendSystem(Appender& j) {
    const WifiStats w = WifiManager::stats();
    j.add("\"cap\":{\"sink\":%u,\"frames\":%lu,\"bytes\":%lu,\"path\":\"%s\"},",
          (unsigned)RawCanLogger::sink(),
          (unsigned long)RawCanLogger::framesWritten(),
          (unsigned long)RawCanLogger::bytesWritten(),
          RawCanLogger::currentPath());
    j.add("\"heap\":%lu,\"rssi\":%ld,\"ip\":\"%s\",\"wifi\":\"%s\",\"reqs\":%lu",
          (unsigned long)ESP.getFreeHeap(), (long)w.rssi, w.ip,
          WifiManager::stateName(WifiManager::state()),
          (unsigned long)s_requests);
}

// ---- routes -----------------------------------------------------------------
void handleIndex() {
    ++s_requests;
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleState() {
    ++s_requests;

    Appender j(s_json, sizeof(s_json));
    j.add("{\"unit\":\"%s\",\"fw\":\"%s\",\"uptime_s\":%lu,",
          UNIT_ID, FW_VERSION, (unsigned long)(millis() / 1000));
    appendCan(j);
    appendIds(j);
    appendFrames(j);
    appendSystem(j);
    j.add("}");

    if (j.overflow) {
        // Say so rather than serving malformed JSON the page would silently
        // fail to parse.
        LOG_W(TAG, "State JSON truncated — raise WEB_JSON_BUF or lower "
                   "WEB_FRAME_RING");
        s_server.send(503, "application/json",
                      "{\"error\":\"response buffer too small\"}");
        return;
    }

    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send(200, "application/json", s_json);
}

void handleNotFound() {
    ++s_requests;
    s_server.send(404, "text/plain", "Not found. This device serves / only.");
}

}  // namespace

namespace WebDashboard {

void begin() {
    if (s_ring_mutex == nullptr) s_ring_mutex = xSemaphoreCreateMutex();
    s_ring_head  = 0;
    s_ring_total = 0;

    s_server.on("/", HTTP_GET, handleIndex);
    s_server.on("/api/state", HTTP_GET, handleState);
    s_server.onNotFound(handleNotFound);
    s_server.begin();
    s_started = true;

    LOG_I(TAG, "Dashboard on port %d — read-only", WEB_PORT);
}

void poll() {
    if (s_started) s_server.handleClient();
}

void noteFrame(const CanFrame& frame) {
    if (!lockRing()) return;          // never stall the storage task
    s_ring[s_ring_head] = frame;
    s_ring_head = static_cast<uint16_t>((s_ring_head + 1) % WEB_FRAME_RING);
    ++s_ring_total;
    unlockRing();
}

bool     isEnabled()      { return true; }
uint32_t requestsServed() { return s_requests; }

}  // namespace WebDashboard

#else   // ENABLE_WEB_DASHBOARD == 0

namespace WebDashboard {
void begin() {}
void poll()  {}
void noteFrame(const CanFrame&) {}
bool     isEnabled()      { return false; }
uint32_t requestsServed() { return 0; }
}  // namespace WebDashboard

#endif
