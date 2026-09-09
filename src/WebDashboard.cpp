#include "WebDashboard.h"

#include "Logger.h"

#if ENABLE_WEB_DASHBOARD

#include <WebServer.h>
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "WifiManager.h"
#include "RawCanLogger.h"
#include "GpsManager.h"
#include "EnvironmentManager.h"
#include "FanManager.h"
#include "StatusLed.h"

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
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0B0A0B">
<title>BMT · CAN Bring-Up</title>
<style>
/* Bali Micro Technology. Both colours are sampled from the logo file itself,
   not from the website CSS: green #04FB2D and dark #373435.

   Dark surface. The greys are warm — they carry the hue of the logo dark
   rather than a neutral grey, so the mark sits in the page instead of on it.
   On this ground #04FB2D reaches about 12:1, so unlike the light build green
   is legible as text here. It is still spent sparingly: the mark, the bars,
   one rule, and badges that need to be found at a glance. Everything a
   technician reads line by line stays near-white, because a page of green on
   black is a novelty, not an instrument. */
:root{
  --g:#04FB2D; --g-dk:#02C224;
  --ink:#EDEAE5; --ink-2:#A9A39C; --ink-3:#8F8880;
  --bg:#141314; --card:#1D1B1C; --hdr:#0B0A0B;
  --line:#302C2E; --line-2:#262324;
  --warn:#F2B233; --warn-bg:#2A1F0C; --warn-line:#4A3714;
  --bad:#FF7566;  --bad-bg:#2C1614; --bad-line:#4E2320;
  --r:10px;
  --mono:ui-monospace,"SF Mono",Menlo,Consolas,"Roboto Mono",monospace;
  --sans:system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue",sans-serif;
}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 var(--sans);
  font-variant-numeric:tabular-nums;padding-bottom:env(safe-area-inset-bottom)}
::selection{background:var(--g);color:var(--hdr)}
:focus-visible{outline:2px solid var(--ink);outline-offset:2px;border-radius:4px}

/* ---- header ---- */
header{background:var(--hdr);color:var(--ink);padding:14px 18px;
  display:flex;align-items:center;gap:13px;flex-wrap:wrap}
.mk{width:40px;height:auto;flex:none}
.wm{font-weight:700;letter-spacing:-.02em;line-height:1.15;font-size:13px}
.wm b{color:var(--g);font-weight:700}
.hs{margin-left:auto;display:flex;align-items:center;gap:7px;
  font-size:11px;font-weight:600;letter-spacing:.08em;text-transform:uppercase}
.hs .pip{width:7px;height:7px;border-radius:50%;background:var(--g);flex:none}
.sub{width:100%;font-family:var(--mono);font-size:11.5px;color:var(--ink-3);
  letter-spacing:.01em}
.sub a{color:var(--ink-3)}

/* ---- verdict ---- */
.verdict{padding:20px 18px 18px;border-bottom:1px solid var(--line)}
.vh{font-size:11px;font-weight:700;letter-spacing:.1em;text-transform:uppercase;
  color:var(--ink-3);margin:0 0 10px}
.vl{font-size:21px;font-weight:650;letter-spacing:-.025em;margin:0 0 4px;
  line-height:1.25}
.vl.bad{color:var(--bad)}
.vl.warn{color:var(--warn)}
.vw{font-size:13.5px;color:var(--ink-2);margin:0;max-width:62ch}
.metrics{display:flex;flex-wrap:wrap;gap:0 26px;margin-top:16px;
  font-family:var(--mono);font-size:12.5px}
.metrics div{padding:2px 0}
.metrics span{color:var(--ink-3)}
.metrics b{font-weight:600}
.metrics b.hot{color:var(--bad)}

/* ---- panels ---- */
main{padding:18px;display:grid;gap:18px;align-items:start;
  grid-template-columns:repeat(auto-fit,minmax(320px,1fr))}
section{background:var(--card);border:1px solid var(--line);border-radius:var(--r);
  overflow:hidden;min-width:0}
.sh{display:flex;align-items:baseline;gap:9px;padding:11px 14px;
  border-bottom:1px solid var(--line)}
.sh h2{font-size:12px;font-weight:700;letter-spacing:.07em;text-transform:uppercase;
  margin:0}
.sh .n{font-family:var(--mono);font-size:11.5px;color:var(--ink-3);margin-left:auto}
.body{padding:12px 14px}
.scroll{max-height:290px;overflow:auto;overscroll-behavior:contain}
.scroll::-webkit-scrollbar{width:9px;height:9px}
.scroll::-webkit-scrollbar-thumb{background:var(--line);border-radius:9px}
.scroll::-webkit-scrollbar-track{background:transparent}
.scroll{scrollbar-width:thin;scrollbar-color:var(--line) transparent}

/* ---- rows ---- */
.kv{display:flex;justify-content:space-between;gap:14px;padding:6px 0;
  border-bottom:1px solid var(--line-2);font-size:13.5px}
.kv:last-child{border-bottom:0}
.kv>span:first-child{color:var(--ink-2);flex:none}
.kv>span:last-child{font-family:var(--mono);font-size:12.5px;text-align:right;
  word-break:break-word}

/* ---- tables ---- */
table{width:100%;border-collapse:collapse;font-family:var(--mono);font-size:12px}
th{position:sticky;top:0;background:var(--card);text-align:left;padding:7px 14px;
  font-family:var(--sans);font-size:10.5px;font-weight:700;letter-spacing:.07em;
  text-transform:uppercase;color:var(--ink-3);border-bottom:1px solid var(--line)}
td{padding:5px 14px;border-bottom:1px solid var(--line-2);white-space:nowrap}
tr:last-child td{border-bottom:0}
td.id{font-weight:600}
td.num{text-align:right}
.bar{display:block;height:3px;background:var(--g);border-radius:2px;min-width:2px}

/* ---- badges / notes ---- */
.tag{display:inline-block;padding:2px 8px;border-radius:5px;font-family:var(--sans);
  font-size:10.5px;font-weight:700;letter-spacing:.05em;text-transform:uppercase}
.t-ok{background:var(--g);color:var(--hdr)}
.t-warn{background:var(--warn-bg);color:var(--warn)}
.t-bad{background:var(--bad-bg);color:var(--bad)}
.t-idle{background:#2A2628;color:var(--ink-2)}
.note{margin:10px 0 0;padding:10px 12px;border-radius:8px;background:#232021;
  border:1px solid var(--line);font-size:12.5px;line-height:1.5;color:var(--ink-2)}
.note.warn{background:var(--warn-bg);border-color:var(--warn-line);color:var(--warn)}
.note.bad{background:var(--bad-bg);border-color:var(--bad-line);color:var(--bad)}
.note b{color:inherit}
/* ---- signal probe ---- */
.pbf{display:grid;grid-template-columns:repeat(auto-fit,minmax(94px,1fr));
  gap:9px 10px;margin-bottom:13px}
.pbf label{display:flex;flex-direction:column;gap:4px;font-size:10px;font-weight:700;
  letter-spacing:.07em;text-transform:uppercase;color:var(--ink-3)}
.pbf select,.pbf input{background:#151314;color:var(--ink);border:1px solid var(--line);
  border-radius:6px;padding:5px 7px;font:12px var(--mono);min-width:0}
.pbf label.ck{flex-direction:row;align-items:center;gap:6px;text-transform:none;
  letter-spacing:0;font-size:11.5px;font-weight:500;color:var(--ink-2);align-self:end;
  padding-bottom:6px}
.pbf label.ck input{width:auto;accent-color:var(--g)}
.pbv{display:flex;align-items:baseline;gap:11px;flex-wrap:wrap}
.pbv b{font-size:27px;font-weight:650;letter-spacing:-.02em;font-family:var(--mono);
  color:var(--g);line-height:1.1}
.pbv span{font-family:var(--mono);font-size:11.5px;color:var(--ink-3)}
.spark{width:100%;height:48px;display:block;margin:9px 0 3px}

/* ---- event marker ----
   Sized to be hit without looking. The operator is holding a camera on the
   instrument cluster with the other hand. */
.mkrow{display:flex;gap:9px;margin:12px 0 4px}
.mkrow input{flex:1;min-width:0;background:#151314;color:var(--ink);
  border:1px solid var(--line);border-radius:7px;padding:10px 11px;
  font:13px var(--mono)}
.mkrow button{flex:none;padding:10px 22px;border:0;border-radius:7px;
  background:var(--g);color:var(--hdr);font:700 13px var(--sans);
  letter-spacing:.07em;cursor:pointer;-webkit-appearance:none}
.mkrow button:active{background:var(--g-dk)}
.mkrow button.sent{background:var(--ink-3);color:var(--hdr)}
.mkrow button.fail{background:var(--bad);color:var(--hdr)}
.ledtab{margin-top:11px;border-top:1px solid var(--line);padding-top:10px}
.ledtab h3{font-size:10px;font-weight:700;letter-spacing:.08em;
  text-transform:uppercase;color:var(--ink-3);margin:0 0 8px}
.ledtab>div{display:flex;gap:9px;align-items:center;padding:3px 0;
  font-size:12px;color:var(--ink-3)}
.ledtab>div.on{color:var(--ink)}
.ledtab i{flex:none;width:6px;height:6px;border-radius:50%;background:#3A3537}
.ledtab>div.on i{background:var(--g)}
.ledtab b{flex:none;width:125px;font-weight:500;color:inherit}
.ledtab>div.on b{font-weight:700}
.lednb{margin:9px 0 0;font-size:11.5px;line-height:1.45;color:var(--ink-3)}
.empty{padding:22px 14px;text-align:center;color:var(--ink-3);font-size:13px}
.empty b{display:block;color:var(--ink-2);font-weight:600;margin-bottom:3px}

footer{padding:14px 18px 22px;color:var(--ink-3);font-size:11.5px;
  border-top:1px solid var(--line);display:flex;gap:14px;flex-wrap:wrap}
footer .rule{flex:none;width:22px;height:3px;background:var(--g);border-radius:2px;
  align-self:center}
footer #ft{margin-left:auto}
@media(max-width:520px){
  main{padding:14px;gap:14px}
  .verdict{padding:16px 14px 14px}
  .vl{font-size:19px}
  header{padding:12px 14px}
}
</style></head><body>

<header>
  <!-- BMT mark, traced from BMT.png rather than drawn by eye. The three
       strokes are 45-degree capsules of equal width; the third is cut by the
       artboard edge, which is why its right side is flat.

       The source art also carries a dark parallelogram in the gap between the
       first and second stroke. It is not reproduced: on a dark ground it would
       be the ground, which is exactly what BMT do in their own dark-background
       icon. On a light surface this mark would need that shape back.

       stroke-linejoin rounds the traced corners back into the caps the logo
       has, at a width small enough not to thicken the strokes. -->
  <svg class="mk" viewBox="-1 -1 138 85" aria-hidden="true">
    <g fill="var(--g)" stroke="var(--g)" stroke-width="1.4" stroke-linejoin="round">
      <path d="M65 0 60 2 2 60 0 71 5 79 10 81 21 79 79 21 81 16 79 6 73 1Z"/>
      <path d="M118 1 56 61 54 70 60 80 67 82 75 80 133 22 135 18 133 6 126 1Z"/>
      <path d="M134 26 102 59 102 71 112 81 124 81 134 72Z"/>
    </g>
  </svg>
  <div class="wm">BALI <b>MICRO</b><br>TECHNOLOGY</div>
  <div class="hs"><span class="pip" id="pip"></span><span id="hstat">connecting</span></div>
  <div class="sub" id="sub">CAN bring-up · waiting for device</div>
</header>

<div class="verdict">
  <p class="vh">Bus status</p>
  <p class="vl" id="vl">Contacting device…</p>
  <p class="vw" id="vw"></p>
  <div class="metrics">
    <div><span>received </span><b id="m-rx">–</b></div>
    <div><span>identifiers </span><b id="m-id">–</b></div>
    <div><span>dropped </span><b id="m-drop">–</b></div>
    <div><span>missed </span><b id="m-miss">–</b></div>
    <div><span>bus errors </span><b id="m-err">–</b></div>
    <div><span>bitrate </span><b id="m-rate">–</b></div>
  </div>
</div>

<main>
  <section>
    <div class="sh"><h2>Identifiers</h2><span class="n" id="idn"></span></div>
    <div class="scroll" id="idwrap"></div>
    <div id="id-note" class="body" style="padding-top:0"></div>
  </section>

  <section>
    <div class="sh"><h2>Live frames</h2><span class="n" id="frn"></span></div>
    <div class="scroll" id="frwrap"></div>
  </section>

  <section>
    <div class="sh"><h2>GNSS</h2><span class="n" id="gn"></span></div>
    <div class="body">
      <div class="kv"><span>Position</span><span id="g-pos">–</span></div>
      <div class="kv"><span>Ground speed</span><span id="g-sog">–</span></div>
      <div class="kv"><span>Satellites / HDOP</span><span id="g-sat">–</span></div>
      <div class="kv"><span>UTC</span><span id="g-utc">–</span></div>
      <div class="kv"><span>Bytes / lines</span><span id="g-byt">–</span></div>
      <div class="kv"><span>GGA+RMC / bad CRC</span><span id="g-sen">–</span></div>
      <div class="kv"><span>Sky: in view / heard</span><span id="g-sky">–</span></div>
      <div class="kv"><span>Strongest signal</span><span id="g-cnr">–</span></div>
      <div id="g-note"></div>
    </div>
  </section>

  <section>
    <div class="sh"><h2>Thermal</h2><span class="n" id="tn"></span></div>
    <div class="body">
      <div class="kv"><span>Enclosure (DHT22)</span><span id="e-t">–</span></div>
      <div class="kv"><span>Humidity</span><span id="e-h">–</span></div>
      <div class="kv"><span>Fan sensor</span><span id="f-t">–</span></div>
      <div class="kv"><span>Fan</span><span id="f-s">–</span></div>
      <div class="kv"><span>Reads ok / no-reply / CRC</span><span id="e-c">–</span></div>
      <div class="kv"><span>DATA line idle</span><span id="e-l">–</span></div>
      <div id="e-note"></div>
    </div>
  </section>

  <section>
    <div class="sh"><h2>Capture</h2><span class="n" id="cn"></span></div>
    <div class="body">
      <div class="kv"><span>Frames written</span><span id="c-f">–</span></div>
      <div class="kv"><span>Size</span><span id="c-b">–</span></div>
      <div class="kv"><span>File</span><span id="c-p">–</span></div>
      <div class="kv"><span>Dropped before writing</span><span id="c-qd">–</span></div>
      <div class="kv"><span>Markers written</span><span id="c-mk">–</span></div>
      <div class="mkrow">
        <input id="mk-l" type="text" maxlength="48" placeholder="label, e.g. 40kmh"
               autocomplete="off" autocapitalize="characters" spellcheck="false">
        <button id="mk-b" type="button">MARK</button>
      </div>
      <div id="c-note"></div>
    </div>
  </section>

  <section>
    <div class="sh"><h2>Signal probe</h2><span class="n" id="pb-n"></span></div>
    <div class="body">
      <div class="pbf">
        <label>CAN ID<select id="pb-id"></select></label>
        <label>Start byte<input id="pb-off" type="number" min="0" max="7" value="0"></label>
        <label>Width<select id="pb-w">
          <option value="8">8 bit</option><option value="16" selected>16 bit</option>
          <option value="24">24 bit</option><option value="32">32 bit</option></select></label>
        <label>Byte order<select id="pb-e">
          <option value="be">big-endian</option>
          <option value="le">little-endian</option></select></label>
        <label>Scale<input id="pb-s" type="number" step="any" value="1"></label>
        <label>Offset<input id="pb-o" type="number" step="any" value="0"></label>
        <label class="ck"><input id="pb-sg" type="checkbox"> signed</label>
      </div>
      <div class="pbv"><b id="pb-val">–</b><span id="pb-raw"></span></div>
      <svg class="spark" id="pb-spark" viewBox="0 0 300 48" preserveAspectRatio="none"
           aria-hidden="true"></svg>
      <div class="kv"><span>Range seen</span><span id="pb-mm">–</span></div>
      <div id="pb-note"></div>
    </div>
  </section>

  <section>
    <div class="sh"><h2>Device</h2></div>
    <div class="body">
      <div class="kv"><span>Unit</span><span id="s-u">–</span></div>
      <div class="kv"><span>Firmware</span><span id="s-fw">–</span></div>
      <div class="kv"><span>Uptime</span><span id="s-up">–</span></div>
      <div class="kv"><span>Free heap</span><span id="s-hp">–</span></div>
      <div class="kv"><span>WiFi</span><span id="s-wf">–</span></div>
      <div class="kv"><span>Requests served</span><span id="s-rq">–</span></div>
      <div class="kv"><span>Wall clock (UTC)</span><span id="s-clk">–</span></div>
      <div class="kv"><span>Status light</span><span id="s-led">–</span></div>
      <div id="s-lednote"></div>
      <div class="ledtab" id="s-ledtab"></div>
    </div>
  </section>
</main>

<footer>
  <span class="rule"></span>
  <span>Listen-only diagnostic tool. Never transmits to the vehicle bus.</span>
  <span id="ft"></span>
</footer>

<script>
const $=i=>document.getElementById(i);
const hex=(n,w)=>'0x'+n.toString(16).toUpperCase().padStart(w,'0');
const tag=(t,c)=>'<span class="tag '+c+'">'+t+'</span>';
const esc=s=>String(s).replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
const note=(t,c)=>t?'<p class="note'+(c?' '+c:'')+'">'+t+'</p>':'';
const empty=(h,b)=>'<div class="empty"><b>'+h+'</b>'+b+'</div>';
/* Keyed by StatusLed::statusName(). First field is what the light looks like,
   second is what it means, third the tone. Red is not automatically a fault:
   GPS_NO_FIX winks red while the receiver is perfectly healthy. */
const LED={
  BOOT:            ['red / green alternating','Nothing has reported in yet. Past the first second of boot this means the housekeeping task has stopped updating.','warn','nothing reporting in'],
  MODEM_CONNECTING:['fast green blink','Joining WiFi.','','joining WiFi'],
  NETWORK_OK:      ['slow green blink','WiFi is up and the CAN driver is running. No frames yet — expected until a bus is attached.','','WiFi up, no CAN frames'],
  GPS_NO_FIX:      ['green, red wink','The GNSS module is streaming but has not locked a fix yet. The red wink is not a fault; it clears on first fix. Indoors it may never clear.','','GNSS streaming, no fix'],
  CAN_OK:          ['green heartbeat','CAN frames are arriving.','','CAN frames arriving'],
  CAN_ERROR:       ['fast red blink','The CAN driver is not running. Check the serial log.','bad','CAN driver down'],
  SYSTEM_ERROR:    ['solid red','The filesystem is down. Captures are not being written.','bad','filesystem down']
};
/* Most severe first — StatusLed shows the highest active condition, so reading
   the legend top-down is reading it in the order the light prefers. MqttOk and
   Buffering exist in StatusLed but this build never sets them, so listing them
   would invite someone to wait for a light that cannot come. */
const LED_ORDER=['SYSTEM_ERROR','CAN_ERROR','CAN_OK','GPS_NO_FIX',
                 'MODEM_CONNECTING','NETWORK_OK','BOOT'];
let fails=0,prevRx=null,prevT=null;

/* ---------------- signal probe ----------------------------------------------
   Blueprint 9.1 in the browser. The known-value method is normally run after the
   drive, with can_find_value.py over a capture; this makes it live, so a
   candidate can be confirmed against the speedometer while someone reads it out
   rather than an hour later at a desk.

   The decode runs HERE, not on the device. Nothing about the CAN path changes,
   the settings retune without reflashing, and — the point — the firmware still
   never claims to know what a byte means. It ships raw hex; this panel is
   openly a guess the operator is making, and can watch being right or wrong.

   It is a SAMPLE, not a capture. The page receives the most recent frames each
   poll, so a signal at 10 Hz or faster appears every tick and a slow one may
   skip some. For finding which bytes move that is plenty, and the file on flash
   remains the record. */
const PB_KEYS=['pb-id','pb-off','pb-w','pb-e','pb-s','pb-o','pb-sg'];
let pbHist=[],pbSig='',pbIds='';

function pbRead(){
  return {id:+$('pb-id').value, off:+$('pb-off').value, w:+$('pb-w').value,
          le:$('pb-e').value==='le', sc:+$('pb-s').value, of:+$('pb-o').value,
          sg:$('pb-sg').checked};
}
function pbSave(){
  try{const o={};PB_KEYS.forEach(k=>o[k]=$(k).type==='checkbox'?$(k).checked:$(k).value);
    localStorage.setItem('bmt.probe',JSON.stringify(o));}catch(e){}
}
function pbLoad(){
  try{const o=JSON.parse(localStorage.getItem('bmt.probe')||'{}');
    PB_KEYS.forEach(k=>{if(o[k]===undefined||!$(k))return;
      if($(k).type==='checkbox')$(k).checked=o[k];else $(k).value=o[k];});}catch(e){}
}
/* Multiplication, not shifts: JavaScript bitwise operators are 32-bit SIGNED,
   so a 32-bit unsigned CAN value would come back negative. */
function pbDecode(b,st,w,le,sg){
  const n=w/8;
  if(st<0||st+n>b.length) return null;
  let v=0;
  if(le){ for(let i=n-1;i>=0;i--) v=v*256+b[st+i]; }
  else  { for(let i=0;i<n;i++)    v=v*256+b[st+i]; }
  if(sg){ const half=Math.pow(2,w-1); if(v>=half) v-=Math.pow(2,w); }
  return v;
}
function pbSpark(h){
  const el=$('pb-spark');
  if(h.length<2){el.innerHTML='';return;}
  let lo=Math.min.apply(null,h),hi=Math.max.apply(null,h);
  if(hi===lo){hi=lo+1;lo=lo-1;}
  const n=h.length,W=300,H=48,p=4;
  const pts=h.map((v,i)=>(i*(W/(n-1))).toFixed(1)+','+
    (H-p-((v-lo)/(hi-lo))*(H-2*p)).toFixed(1)).join(' ');
  el.innerHTML='<polyline fill="none" stroke="var(--g)" stroke-width="1.7" '+
    'stroke-linejoin="round" stroke-linecap="round" points="'+pts+'"/>';
}
function pbFmt(v){
  return (Number.isInteger(v)||Math.abs(v)>=1000)
    ? v.toLocaleString(undefined,{maximumFractionDigits:3})
    : String(Math.round(v*1000)/1000);
}

function probe(d){
  /* Rebuild the identifier list only when the bus really shows a different set,
     so a new ID appearing mid-drive does not discard the operator's selection. */
  const sig=d.ids.map(x=>x.id).join(',');
  if(sig!==pbIds){
    pbIds=sig;
    const keep=$('pb-id').value;
    $('pb-id').innerHTML=d.ids.map(x=>'<option value="'+x.id+'">'+
      hex(x.id,x.ext?8:3)+'</option>').join('');
    if(keep&&d.ids.some(x=>String(x.id)===keep)) $('pb-id').value=keep;
    else pbLoad();
  }
  if(!d.ids.length){
    $('pb-val').textContent='-'; $('pb-raw').textContent='';
    $('pb-mm').textContent='-'; $('pb-n').textContent=''; pbSpark([]);
    $('pb-note').innerHTML=note('Nothing on the bus yet. Once frames arrive, pick '+
      'an identifier and watch this number while someone reads the speedometer '+
      'aloud. Speed follows the needle both ways, an odometer only climbs, a '+
      'rolling counter wraps to zero.');
    return;
  }

  const p=pbRead();
  const guess=[p.id,p.off,p.w,p.le,p.sg].join('|');
  if(guess!==pbSig){ pbSig=guess; pbHist=[]; }   /* new guess, new history */

  const fr=d.frames.filter(f=>f.id===p.id);
  if(fr.length){
    const b=fr[0].d.trim().split(/\s+/).map(x=>parseInt(x,16));
    const raw=pbDecode(b,p.off,p.w,p.le,p.sg);
    if(raw===null){
      $('pb-val').textContent='-'; $('pb-raw').textContent=''; pbSpark([]);
      $('pb-note').innerHTML=note('Byte '+p.off+' plus '+(p.w/8)+' byte(s) runs past '+
        'this frame, which carries '+b.length+'. Narrow the width or move the '+
        'start byte left.','warn');
      return;
    }
    pbHist.push(raw*p.sc+p.of); if(pbHist.length>90) pbHist.shift();
    $('pb-val').textContent=pbFmt(pbHist[pbHist.length-1]);
    $('pb-raw').textContent='raw '+raw+' · 0x'+(raw<0?'-':'')+
      Math.abs(raw).toString(16).toUpperCase()+
      ' · bytes '+p.off+'–'+(p.off+p.w/8-1);
  }

  $('pb-n').textContent=pbHist.length?pbHist.length+' samples':'waiting';
  if(pbHist.length){
    const lo=Math.min.apply(null,pbHist),hi=Math.max.apply(null,pbHist);
    $('pb-mm').textContent=(lo===hi)?'flat at '+pbFmt(lo)
      :pbFmt(lo)+' … '+pbFmt(hi);
    pbSpark(pbHist);
    $('pb-note').innerHTML=note(lo===hi
      ? 'Not moving. Either these bytes are not the signal, or nothing has '+
        'changed yet. Test it against something you can make change on demand.'
      : 'Moving. Read the dashboard aloud and compare: speed follows the needle '+
        'up AND down, an odometer only ever climbs, a rolling counter wraps to '+
        'zero. Confirm a candidate against GNSS ground speed before trusting it, '+
        'and never put an unvalidated identifier in the fleet signals.cfg.');
  }else{
    $('pb-mm').textContent='-';
    $('pb-note').innerHTML=note('No frame with this identifier in the last sample. '+
      'A slow signal can skip a tick; the capture on flash still holds every frame.');
  }
}

function paint(d){
  const c=d.can,g=d.gps,e=d.env,f=d.fan,cap=d.cap,now=Date.now();

  /* The authoritative identifier count, and whether the array below it was
     cut short by a full buffer. Declared here because the verdict sentence
     reads it before the metrics row does. */
  const uniq=(c.uniq===undefined)?d.ids.length:c.uniq;
  const cut=d.ids.length<uniq;

  $('sub').textContent=d.unit+' · '+d.ip+' · '+d.fw;
  $('hstat').textContent=c.listen_only?'listen-only':'UNLOCKED';
  $('pip').style.background=c.listen_only?'var(--g)':'var(--bad)';

  /* ---- verdict: a sentence, because the number alone answers nothing ---- */
  let fps=null;
  if(prevRx!==null&&now>prevT) fps=Math.round((c.rx-prevRx)*1000/(now-prevT));
  prevRx=c.rx; prevT=now;

  let vl,vw,cls='';
  if(!c.running){
    vl='CAN driver is down';
    vw='The TWAI driver failed to start. Check the serial log — this is a '+
       'firmware or wiring fault, not a quiet bus.'; cls='bad';
  }else if(c.rx===0&&c.err>0){
    /* Bus errors climbing with nothing received is the signature of the WRONG
       BITRATE: the controller is hearing transitions it cannot frame. Without
       this branch both cases read 'No frames yet', and a 250 kbps vehicle looks
       exactly like an unplugged connector — one failure wearing the other's
       clothes. It matters more now that the tool is meant to meet many vehicle
       types, not just one. */
    vl='Wrong bitrate, most likely';
    vw='Nothing decoded, but '+c.err.toLocaleString()+' bus error'+
       (c.err===1?'':'s')+'. The wire is live — the controller is hearing '+
       'transitions it cannot frame at '+(c.bitrate/1000)+' kbps. Most vehicles '+
       'run 500; many older and body buses run 250. Reflash with the other rate '+
       'and try again.'; cls='warn';
  }else if(c.rx===0){
    vl='No frames yet';
    vw='The driver is running at '+(c.bitrate/1000)+' kbps in listen-only mode '+
       'and has seen nothing. On a bench that is expected until a bus is '+
       'attached. On a vehicle, check the 60 Ω across CANH–CANL first — and '+
       'remember many cars keep the OBD-II channel silent until a scan tool asks.';
  }else if(c.silence_ms>2000){
    vl='Bus went quiet';
    vw=c.rx.toLocaleString()+' frames arrived, then nothing for '+
       Math.round(c.silence_ms/1000)+' s. The wiring was good, so this is the '+
       'bus stopping rather than a fault — ignition off, or a connector moved.';
    cls='warn';
  }else{
    /* Only state a rate once one has been measured. Two consecutive polls with
       the same counter mean this sample caught no frame, not a dead bus —
       silence_ms above is what decides that. */
    vl='Reading the bus'+(fps?' · '+fps.toLocaleString()+' frames/s':'');
    vw=c.rx.toLocaleString()+' frames from '+uniq+
       ' identifier'+(uniq===1?'':'s')+'.'+
       ((c.missed||c.drop)?' Some frames were lost — see the counters below.':'');
  }
  $('vl').className='vl '+cls;
  $('vl').textContent=vl;
  $('vw').textContent=vw;

  $('m-rx').textContent=c.rx.toLocaleString();
  $('m-id').textContent=uniq;
  $('m-drop').textContent=c.drop; $('m-drop').className=c.drop?'hot':'';
  $('m-miss').textContent=c.missed; $('m-miss').className=c.missed?'hot':'';
  $('m-err').textContent=c.err; $('m-err').className=c.err?'hot':'';
  $('m-rate').textContent=(c.bitrate/1000)+' kbps';

  /* ---- identifiers ---- */
  $('idn').textContent=uniq?(cut?d.ids.length+' of '+uniq:uniq+' seen'):'';
  if(d.ids.length){
    const tot=d.ids.reduce((a,b)=>a+b.n,0)||1;
    const rows=d.ids.slice().sort((a,b)=>b.n-a.n).map(x=>{
      const pct=100*x.n/tot;
      return '<tr><td class="id">'+hex(x.id,x.ext?8:3)+'</td>'+
        '<td class="num">'+x.n.toLocaleString()+'</td>'+
        '<td class="num">'+pct.toFixed(1)+'%</td>'+
        '<td style="width:34%"><i class="bar" style="width:'+
        Math.max(2,pct).toFixed(1)+'%"></i></td></tr>';}).join('');
    $('idwrap').innerHTML='<table><thead><tr><th>ID</th><th class="num">Frames</th>'+
      '<th class="num">Share</th><th></th></tr></thead><tbody>'+rows+'</tbody></table>';
  }else{
    $('idwrap').innerHTML=empty('Nothing on the bus yet',
      'Every ECU broadcasts its own identifier. A healthy vehicle bus shows tens of them.');
  }

  /* ---- live frames ---- */
  $('frn').textContent=d.frames.length?'last '+d.frames.length:'';
  if(d.frames.length){
    $('frwrap').innerHTML='<table><thead><tr><th>ms</th><th>ID</th><th>DLC</th>'+
      '<th>Data</th></tr></thead><tbody>'+d.frames.map(fr=>
      '<tr><td style="color:var(--ink-3)">'+fr.t+'</td><td class="id">'+
      hex(fr.id,fr.ext?8:3)+'</td><td class="num">'+fr.dlc+'</td><td>'+
      esc(fr.d)+'</td></tr>').join('')+'</tbody></table>';
  }else{
    $('frwrap').innerHTML=empty('No traffic',
      'Frames appear here newest first. Watch the data bytes change as the vehicle moves — that is how you tell real signals from a stuck frame.');
  }

  /* ---- GNSS ---- */
  $('gn').innerHTML=g.fix?tag('fix','t-ok')
    :(g.silent?tag('no data','t-bad'):tag('searching','t-warn'));
  $('g-pos').textContent=g.fix?g.lat.toFixed(6)+', '+g.lon.toFixed(6):'—';
  $('g-sog').textContent=g.fix?g.sog.toFixed(1)+' km/h':'—';
  $('g-sat').textContent=g.sats+(g.hdop?' / '+g.hdop.toFixed(1):' / —');
  $('g-utc').textContent=g.time_valid&&g.epoch
    ? new Date(g.epoch*1000).toISOString().replace('.000Z','Z'):'—';
  $('g-byt').textContent=g.bytes.toLocaleString()+' / '+g.lines.toLocaleString();
  $('g-sen').textContent=g.sentences.toLocaleString()+' / '+g.badcrc;
  /* An instrument has to be able to accuse itself. Three separate ways this
     table can mislead, each named rather than hidden. */
  let it='',ic='';
  const shown=d.ids.reduce((a,x)=>a+x.n,0);
  if(cut){ic='bad';
    it='<b>Showing '+d.ids.length+' of '+uniq+' identifiers.</b> The state '+
       'response filled up before the list ended, so the rows below are a '+
       'prefix, not a ranking. Do not choose reverse-engineering targets from '+
       'this table until it fits — raise WEB_JSON_BUF or send fewer frames.';
  }else if(c.idfull){ic='warn';
    it='<b>The identifier survey is full at '+uniq+'.</b> Identifiers first '+
       'seen after that point are not counted anywhere. This is the ceiling in '+
       'CanManager, not the bus.';
  }else if(c.rx>2000&&shown<c.rx*0.9){ic='bad';
    it='<b>These rows do not add up.</b> They total '+shown.toLocaleString()+
       ' against '+c.rx.toLocaleString()+' received, and neither the buffer '+
       'nor the survey ceiling explains the gap. Treat the table as unreliable '+
       'and report it.';
  }
  $('id-note').innerHTML=note(it,ic);

  $('g-sky').textContent=g.view===undefined?'–':g.view+' / '+g.trk;
  $('g-cnr').textContent=g.cnr?g.cnr+' dB-Hz':(g.view===undefined?'–':'nothing heard');
  let gt='',gc='';
  if(g.silent){gc='bad';
    gt=(g.bytes===0?'<b>No bytes at all</b> on the wire.':'<b>Only '+g.bytes+
      ' byte(s) in '+d.uptime_s+' s</b> — line noise, not a module.')+
      ' This is not searching. GPS TX must reach GPIO18 (module TX → ESP32 RX), '+
      'and the module needs 3V3 and GND. Baud is irrelevant until a steady '+
      'byte stream appears.';
  }else if(g.lines===0){gc='warn';
    gt='Steady byte stream but no complete lines — wrong baud rate. The module is talking; we listen at 9600.';
  }else if(g.sentences===0&&g.badcrc>0){gc='warn';
    gt='Lines arrive and every checksum fails. Baud close but wrong, or a noisy line.';
  }else if(g.sentences===0){gc='warn';
    gt='Lines parse but none are GGA or RMC. The module emits other sentence types only.';
  }else if(!g.fix&&g.view===0&&g.cnr===0){gc='bad';
    gt='<b>The receiver hears nothing at all.</b> It is streaming valid NMEA, so '+
       'the module, the wiring and the supply are fine — but GSV reports no '+
       'satellites in view and no signal on any channel. That is the antenna: '+
       'not connected, facing away from the sky, or dead. More waiting will not '+
       'change it.';
  }else if(!g.fix&&g.cnr===0){gc='bad';
    gt='<b>It knows where '+g.view+' satellites should be, and hears none of '+
       'them.</b> Those positions come from the stored almanac, not from '+
       'reception. Zero carrier-to-noise on every channel points at the antenna '+
       'rather than the sky.';
  }else if(!g.fix&&g.cnr<25){gc='warn';
    gt='<b>Hearing satellites, too weakly to lock.</b> Strongest is '+g.cnr+
       ' dB-Hz; a fix needs roughly 30 and four satellites at once. The antenna '+
       'works — this is sky view. Outdoors, ceramic patch facing up.';
  }else if(!g.fix){
    gt='<b>Signal is strong enough.</b> Strongest '+g.cnr+' dB-Hz across '+
       g.trk+' of '+g.view+' satellites. It needs four at once plus the '+
       'ephemeris, which takes up to 12.5 minutes to download on a cold start. '+
       'Keep it still and in the open.';
  }
  $('g-note').innerHTML=note(gt,gc);

  /* ---- thermal ---- */
  $('tn').innerHTML=!e.enabled?tag('off','t-idle')
    :(e.valid?tag('reading','t-ok'):tag('no reply','t-bad'));
  $('e-t').textContent=e.enabled?(e.valid?e.t.toFixed(1)+' °C':'—'):'disabled';
  $('e-h').textContent=(e.enabled&&e.valid)?e.h.toFixed(1)+' %RH':'—';
  $('f-t').textContent=f.tvalid?f.t.toFixed(1)+' °C':'—';
  $('f-s').innerHTML=f.mode+' · '+(f.on?tag('running','t-ok'):'off')+
    ' · '+f.run_s+' s total';
  $('e-c').textContent=e.ok+' / '+e.read_err+' / '+e.crc_err;
  $('e-l').innerHTML=e.idle===undefined?'–'
    :(e.idle?tag('high','t-ok'):tag('low','t-bad'))
     +(e.bits?' · '+e.bits+'/40 bits':'');

  /* Each branch names one fault and one thing to do about it. The old version
     said "sensor never answers" for every failure, which was a claim the
     counters could not support: they did not record which stage died. */
  let et='',ec='';
  if(!e.enabled) et='DHT22 is disabled in Config.h.';
  else if(e.ok>0&&e.valid)
    et='Enclosure air versus the SoC die is the measurement the fleet fan threshold is waiting on. Log the peak reached in a parked car.';
  else if(e.read_err===0&&e.crc_err===0)
    et='No read attempted yet. The first sample lands about 10 s after boot.';
  else if(e.idle===false){ec='bad';
    et='<b>DATA sits low between reads.</b> With the internal pull-up on, an idle line must read high, so nothing here is a timing problem: either no pull-up reaches GPIO15, DATA is shorted to ground, or the part is holding the line down. On a 3-pin module a swapped VCC and GND does exactly this — and usually kills the sensor.';
  }else if(e.nores>0){ec='bad';
    et='<b>The line is healthy and nothing answers on it.</b> DATA idles high, so the pull-up and the wire are fine; the sensor simply never pulls it down. That is power or the part: measure 3V3 at the sensor pins themselves, and check the pin order — 3-pin DHT22 boards ship as VCC-DATA-GND and as DATA-VCC-GND, and the two are not interchangeable.';
  }else if(e.hshake>0){ec='warn';
    et='<b>It starts to answer, then stops.</b> The sensor pulls the line down but never completes the 80/80 handshake. Usually a pull-up too weak for the cable, or a supply that sags when the sensor wakes.';
  }else if(e.trunc>0){ec='warn';
    et='<b>The frame is cut short</b> after '+e.bits+' of 40 bits. The sensor is alive and the handshake is good, so this is edge timing: pull-up strength, wire length, or interference.';
  }else if(e.crc_err>0){ec='warn';
    et='All 40 bits arrive but the checksum fails — signal integrity rather than wiring.';
  }else if(e.rng>0){ec='warn';
    et='<b>The checksum passes but the values are impossible.</b> Frame '+e.raw+
       '. Do not trust the checksum here: a frame captured one bit out of step '+
       'still passes it, because shifting doubles every byte and doubling is '+
       'linear mod 256. That is a framing fault, not a wiring one — the sensor '+
       'is answering correctly and the driver is misreading where the frame '+
       'starts. A DHT11 fitted in place of a DHT22 also lands here, sending '+
       'whole units in bytes 0 and 2 with zero decimals.';
  }
  $('e-note').innerHTML=note(et,ec);

  /* ---- capture ---- */
  const sinks=['off','serial','file','serial + file'];
  $('cn').innerHTML=cap.sink?tag(sinks[cap.sink]||cap.sink,'t-ok'):tag('off','t-idle');
  $('c-f').textContent=cap.frames.toLocaleString();
  $('c-b').textContent=(cap.bytes/1024).toFixed(1)+' KB';
  $('c-p').textContent=cap.path||'—';
  $('c-qd').textContent=cap.qdrop===undefined?'–':cap.qdrop.toLocaleString();
  $('c-qd').className=cap.qdrop?'hot':'';
  $('c-mk').textContent=d.marks===undefined?'–':d.marks;

  /* A capture header that says boot_epoch=0 has to be aligned to the run sheet
     by hand afterwards, so say plainly which of the two we are in. */
  $('s-clk').innerHTML=d.clock
    ? new Date(d.clock*1000).toISOString().replace('.000Z','Z')
    : tag('not set','t-warn');
  $('c-note').innerHTML=note(cap.frames<c.rx&&c.rx>0
    ? '<b>Written is behind received.</b> The flash writer cannot keep up, so the file is a sample. Bus reception is unaffected — <i>received</i> above still counts every frame.'
    : 'The page holds the last frames only. This file holds all of them, for analysis with can_find_value.py. Pull it with <b>cat</b> on the serial console.');

  /* ---- device ---- */
  $('s-u').textContent=d.unit; $('s-fw').textContent=d.fw;
  $('s-up').textContent=d.uptime_s<3600?d.uptime_s+' s'
    :(d.uptime_s/3600).toFixed(1)+' h';
  $('s-hp').textContent=(d.heap/1024).toFixed(1)+' KB';
  $('s-wf').textContent=d.wifi+(d.rssi?' · '+d.rssi+' dBm':'');
  $('s-rq').textContent=d.reqs.toLocaleString();

  /* The board has two outputs, the LED and this page, and until now the page
     could not explain the LED. A technician watching the light should not have
     to read the firmware to find out what it is saying. */
  const L=LED[d.led]||[d.led,'Unrecognised status.'];
  $('s-led').textContent=L[0];
  $('s-lednote').innerHTML=note(L[1],L[2]||'');
  $('s-ledtab').innerHTML='<h3>Every pattern this build can show</h3>'+
    LED_ORDER.map(k=>'<div class="'+(k===d.led?'on':'')+'"><i></i><b>'+
      LED[k][0]+'</b><span>'+LED[k][3]+'</span></div>').join('')+
    '<p class="lednb">Most severe first. The light always shows the highest '+
    'condition that is true, so a fault hides everything below it.</p>';
  probe(d);
  $('ft').textContent='refreshed every 500 ms';
}

async function tick(){
  try{
    const r=await fetch('/api/state',{cache:'no-store'});
    if(!r.ok) throw new Error(r.status);
    paint(await r.json()); fails=0;
  }catch(e){
    if(++fails>2){
      $('hstat').textContent='offline';
      $('pip').style.background='var(--bad)';
      $('vl').textContent='Lost contact with the device';
      $('vw').textContent='The page is still retrying. Check that the phone is on the same network and the board still has power.';
    }
  }
}
/* The marker POST is the only request this page makes that changes anything
   on the device, and it reaches the capture FILE only -- there is no path
   from it to the CAN bus. Feedback is visual because the operator will not be
   reading the screen when they press it. */
try{const l=localStorage.getItem('bmt.mark'); if(l) $('mk-l').value=l;}catch(e){}
$('mk-b').addEventListener('click',function(){
  const b=$('mk-b'), l=($('mk-l').value||'MARK').trim();
  try{localStorage.setItem('bmt.mark',l);}catch(e){}
  b.className='sent'; b.textContent='…';
  fetch('/api/mark?label='+encodeURIComponent(l),{method:'POST'})
    .then(function(r){ if(!r||!r.ok) throw 0;
      b.className='sent'; b.textContent='WRITTEN'; })
    .catch(function(){ b.className='fail'; b.textContent='FAILED'; })
    .then(function(){ setTimeout(function(){
      b.className=''; b.textContent='MARK'; },1100); });
});

pbLoad();
PB_KEYS.forEach(k=>{const e=$(k); if(e) e.addEventListener('change',()=>{
  pbSave(); pbHist=[]; pbSig='';
});});
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
          "\"err\":%lu,\"rec\":%lu,\"silence_ms\":%lu,"
          "\"uniq\":%u,\"idfull\":%s},",
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
          (unsigned long)(sil == UINT32_MAX ? 0 : sil),
          // Emitted here, in the first object written, so it survives even if
          // the identifier array below is cut short by a full buffer. The page
          // compares the array length against this to detect that.
          (unsigned)c.unique_ids_seen,
          CanManager::seenIdOverflow() ? "true" : "false");
}

// appendCan runs before this and carries "uniq", the true number of distinct
// identifiers. The array below can be cut short when the buffer fills, so the
// page compares its length against uniq and says so instead of quietly
// reporting the short count as the total. That silent substitution is exactly
// how the table came to look complete while missing 47% of the traffic.
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

void appendGps(Appender& j) {
    const GnssFix  f     = GpsManager::fix();
    const bool     fresh = GpsManager::hasFreshFix();
    const GpsStats g     = GpsManager::stats();

    // lat/lon are emitted only when there is a real fix. 0,0 is a place in the
    // Gulf of Guinea, not a way of saying "unknown".
    j.add("\"gps\":{\"fix\":%s,\"lat\":%.6f,\"lon\":%.6f,\"sats\":%u,"
          "\"hdop\":%.1f,\"sog\":%.1f,\"time_valid\":%s,\"epoch\":%llu,"
          "\"sentences\":%lu,\"badcrc\":%lu,\"silent\":%s,"
          "\"bytes\":%lu,\"lines\":%lu,"
          "\"view\":%u,\"trk\":%u,\"cnr\":%u,\"gsv\":\"%s\"},",
          fresh ? "true" : "false",
          fresh ? f.lat : 0.0, fresh ? f.lon : 0.0,
          (unsigned)f.satellites, (double)f.hdop, (double)f.speed_kmh,
          f.time_valid ? "true" : "false",
          (unsigned long long)f.epoch,
          (unsigned long)g.sentences_ok,
          (unsigned long)g.sentences_bad_checksum,
          GpsManager::isSilent() ? "true" : "false",
          (unsigned long)g.bytes_received,
          (unsigned long)g.lines_seen,
          (unsigned)f.sats_in_view, (unsigned)f.sats_tracked,
          (unsigned)f.best_cnr, g.last_gsv);
}

void appendThermal(Appender& j) {
    const EnvReading e = EnvironmentManager::reading();
    const FanStats   f = FanManager::stats();

    // Both temperatures, deliberately. The gap between enclosure air and the
    // SoC die is the number the fleet's unresolved fan-threshold TODO needs.
    // The counters, not just the value, and the STAGE each failure reached.
    // read_err on its own cannot tell a sensor that never answered from one
    // that answered badly, and those need opposite fixes. "idle" is the most
    // decisive of the lot: DATA must sit high between reads.
    const EnvStats es = EnvironmentManager::stats();
    j.add("\"env\":{\"enabled\":%s,\"valid\":%s,\"t\":%.1f,\"h\":%.1f,"
          "\"ok\":%lu,\"read_err\":%lu,\"crc_err\":%lu,"
          "\"idle\":%s,\"nores\":%lu,\"hshake\":%lu,\"trunc\":%lu,"
          "\"rng\":%lu,\"bits\":%u,\"raw\":\"%02X %02X %02X %02X %02X\"},",
          EnvironmentManager::isEnabled() ? "true" : "false",
          e.valid ? "true" : "false",
          (double)e.temperature_c, (double)e.humidity_pct,
          (unsigned long)es.reads_ok,
          (unsigned long)es.read_errors,
          (unsigned long)es.checksum_errors,
          es.line_idle_high ? "true" : "false",
          (unsigned long)es.fail_no_response,
          (unsigned long)es.fail_handshake,
          (unsigned long)es.fail_truncated,
          (unsigned long)es.fail_range,
          (unsigned)es.last_bits,
          es.last_frame[0], es.last_frame[1], es.last_frame[2],
          es.last_frame[3], es.last_frame[4]);
    j.add("\"fan\":{\"mode\":\"%s\",\"on\":%s,\"t\":%.1f,\"tvalid\":%s,"
          "\"run_s\":%lu,\"trans\":%lu},",
          FanManager::modeName(f.mode), f.running ? "true" : "false",
          (double)f.last_temperature_c, f.temperature_valid ? "true" : "false",
          (unsigned long)f.run_seconds, (unsigned long)f.transitions);
}

void appendSystem(Appender& j) {
    const WifiStats w = WifiManager::stats();
    j.add("\"cap\":{\"sink\":%u,\"frames\":%lu,\"bytes\":%lu,\"path\":\"%s\","
          "\"qdrop\":%lu},",
          (unsigned)RawCanLogger::sink(),
          (unsigned long)RawCanLogger::framesWritten(),
          (unsigned long)RawCanLogger::bytesWritten(),
          RawCanLogger::currentPath(),
          (unsigned long)RawCanLogger::queueDrops());
    j.add("\"heap\":%lu,\"rssi\":%ld,\"ip\":\"%s\",\"wifi\":\"%s\",\"reqs\":%lu,"
          "\"led\":\"%s\",\"clock\":%lld,\"marks\":%lu",
          (unsigned long)ESP.getFreeHeap(), (long)w.rssi, w.ip,
          WifiManager::stateName(WifiManager::state()),
          (unsigned long)s_requests,
          StatusLed::statusName(StatusLed::current()),
          // Zero until SNTP answers. The page shows the difference between "no
          // wall clock yet" and a time, because a capture whose header says
          // boot_epoch=0 has to be aligned by hand.
          (long long)(time(nullptr) > 1600000000 ? time(nullptr) : 0),
          (unsigned long)RawCanLogger::markCount());
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
    appendGps(j);
    appendThermal(j);
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

// Writes an operator marker into the capture. It touches the FILE only — there
// is no path from here to the CAN bus, and the label is stripped of anything
// that could forge a frame or header line inside RawCanLogger::mark().
void handleMark() {
    if (!RawCanLogger::isCapturing()) {
        s_server.send(409, "text/plain", "Capture is not running");
        return;
    }
    String label = s_server.arg("label");
    if (label.length() == 0) label = "MARK";
    if (label.length() > 48) label = label.substring(0, 48);
    RawCanLogger::mark(label.c_str());
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send(200, "text/plain", String(RawCanLogger::markCount()));
}

void handleNotFound() {
    ++s_requests;
    s_server.send(404, "text/plain",
                  "Not found. This device serves /, /api/state and /api/mark.");
}

}  // namespace

namespace WebDashboard {

void begin() {
    if (s_ring_mutex == nullptr) s_ring_mutex = xSemaphoreCreateMutex();
    s_ring_head  = 0;
    s_ring_total = 0;

    s_server.on("/", HTTP_GET, handleIndex);
    s_server.on("/api/state", HTTP_GET, handleState);
    s_server.on("/api/mark", HTTP_POST, handleMark);
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
