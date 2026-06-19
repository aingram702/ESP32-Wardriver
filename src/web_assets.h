// ===========================================================================
//  web_assets.h  —  Hacker-themed dashboard (HTML/CSS/JS) stored in PROGMEM.
//  Served by the AsyncWebServer. No external CDN dependencies => works fully
//  offline from the device's own SoftAP.
// ===========================================================================
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTMLDOC(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WARDRIVER :: rogue-ap recon</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<canvas id="matrix"></canvas>

<header class="topbar">
  <div class="brand">
    <span class="logo">&#9608;</span>
    <h1 class="glitch" data-text="WARDRIVER">WARDRIVER</h1>
    <span class="ver" id="fwver">v0.0</span>
  </div>
  <div class="statusline">
    <span class="dot" id="scanDot"></span>
    <span id="scanState">BOOT</span>
    <span class="sep">|</span>
    <span id="ipInfo">--</span>
    <span class="sep">|</span>
    <span id="uptime">0s</span>
  </div>
</header>

<section class="stats">
  <div class="stat"><div class="k">WIFI APS</div><div class="v" id="sWifi">0</div></div>
  <div class="stat"><div class="k">BLE DEVS</div><div class="v" id="sBle">0</div></div>
  <div class="stat alert"><div class="k">ROGUE ALERTS</div><div class="v" id="sRogue">0</div></div>
  <div class="stat"><div class="k">CYCLES</div><div class="v" id="sCycles">0</div></div>
  <div class="stat"><div class="k">CLIENTS</div><div class="v" id="sClients">0</div></div>
  <div class="stat"><div class="k">FREE PSRAM</div><div class="v" id="sPsram">0</div></div>
</section>

<section class="control">
  <button class="btn" id="btnScan">&#9632; STOP SCAN</button>
  <label class="tgl"><input type="checkbox" id="tgWifi" checked> WIFI</label>
  <label class="tgl"><input type="checkbox" id="tgBle" checked> BLE</label>
  <span class="spacer"></span>
  <input type="text" id="filter" class="filt" placeholder="&#128269; filter...">
  <button class="btn ghost" id="btnClearWifi">CLR WIFI</button>
  <button class="btn ghost" id="btnClearBle">CLR BLE</button>
</section>

<nav class="tabs">
  <button class="tab active" data-tab="wifi">WIFI <span class="cnt" id="tWifi">0</span></button>
  <button class="tab" data-tab="rogue">ROGUE <span class="cnt warn" id="tRogue">0</span></button>
  <button class="tab" data-tab="ble">BLUETOOTH <span class="cnt" id="tBle">0</span></button>
  <button class="tab" data-tab="white">WHITELIST</button>
  <button class="tab" data-tab="logs">LOGS</button>
</nav>

<main>
  <!-- WIFI -->
  <section class="panel active" id="panel-wifi">
    <table class="grid" id="wifiTable">
      <thead><tr>
        <th data-s="ssid">SSID</th><th data-s="bssid">BSSID</th>
        <th data-s="rssi">RSSI</th><th>SIGNAL</th>
        <th data-s="channel">CH</th><th data-s="enc">ENCRYPTION</th>
        <th data-s="vendor">VENDOR</th><th data-s="seen">SEEN</th><th>STATUS</th>
      </tr></thead>
      <tbody></tbody>
    </table>
  </section>

  <!-- ROGUE -->
  <section class="panel" id="panel-rogue">
    <p class="hint">Evil-twin &amp; impersonation alerts. Add trusted APs under WHITELIST so the
       detector knows your bank's legitimate BSSIDs &mdash; anything else broadcasting those
       SSIDs is flagged.</p>
    <table class="grid" id="rogueTable">
      <thead><tr>
        <th>SEV</th><th>SSID</th><th>BSSID</th><th>RSSI</th><th>CH</th>
        <th>ENC</th><th>VENDOR</th><th>REASON</th>
      </tr></thead>
      <tbody></tbody>
    </table>
  </section>

  <!-- BLE -->
  <section class="panel" id="panel-ble">
    <table class="grid" id="bleTable">
      <thead><tr>
        <th data-s="name">NAME</th><th data-s="address">ADDRESS</th><th data-s="type">TYPE</th>
        <th data-s="rssi">RSSI</th><th>SIGNAL</th><th data-s="vendor">VENDOR</th>
        <th data-s="mfg">MFG</th><th data-s="seen">SEEN</th><th>SERVICES</th>
      </tr></thead>
      <tbody></tbody>
    </table>
  </section>

  <!-- WHITELIST -->
  <section class="panel" id="panel-white">
    <div class="wlform">
      <input type="text" id="wlSsid" placeholder="trusted SSID (e.g. BankCorp-WiFi)">
      <input type="text" id="wlBssid" placeholder="bssid aa:bb:cc:dd:ee:ff">
      <button class="btn" id="wlAdd">+ ADD TRUSTED</button>
    </div>
    <p class="hint">Tip: open the WIFI tab and use the &#9733; button on a row to add that exact
       AP as trusted.</p>
    <div id="wlList" class="wllist"></div>
  </section>

  <!-- LOGS -->
  <section class="panel" id="panel-logs">
    <p class="hint">Full survey history is written to flash as CSV (auto-rolled at 6&nbsp;MB).
       Download for offline analysis or import into a SIEM/spreadsheet.</p>
    <div class="logbtns">
      <a class="btn" href="/api/logs/wifi" download>&#11015; DOWNLOAD WIFI LOG</a>
      <a class="btn" href="/api/logs/ble" download>&#11015; DOWNLOAD BLE LOG</a>
      <button class="btn danger" id="btnWipe">&#10007; WIPE LOGS</button>
    </div>
  </section>
</main>

<footer>
  <span id="footMsg">esp32-s3 wardriver // authorised testing only</span>
</footer>

<script src="/app.js"></script>
</body>
</html>
)HTMLDOC";

static const char STYLE_CSS[] PROGMEM = R"CSSDOC(
:root{
  --bg:#000600; --fg:#00ff41; --dim:#0a8a2a; --dark:#031a08;
  --warn:#ffb000; --alert:#ff2d4f; --panel:rgba(0,20,4,.78);
  --grid:#0c2f14; --mono:'Courier New',Courier,monospace;
}
*{box-sizing:border-box}
html,body{margin:0;height:100%}
body{
  background:var(--bg); color:var(--fg); font-family:var(--mono);
  font-size:13px; line-height:1.4; overflow-x:hidden;
}
#matrix{position:fixed;inset:0;z-index:0;opacity:.18;pointer-events:none}
header,section,nav,main,footer{position:relative;z-index:1}

/* scanline overlay */
body::after{content:"";position:fixed;inset:0;z-index:2;pointer-events:none;
  background:repeating-linear-gradient(0deg,rgba(0,0,0,.18) 0,rgba(0,0,0,.18) 1px,transparent 1px,transparent 3px)}

.topbar{display:flex;justify-content:space-between;align-items:center;
  padding:10px 16px;border-bottom:1px solid var(--dim);
  background:linear-gradient(180deg,rgba(0,40,10,.6),transparent)}
.brand{display:flex;align-items:center;gap:10px}
.logo{color:var(--fg);text-shadow:0 0 8px var(--fg);animation:blink 1.2s step-end infinite}
h1{font-size:22px;letter-spacing:4px;margin:0;text-shadow:0 0 10px var(--fg)}
.ver{color:var(--dim);font-size:11px}
.statusline{font-size:12px;color:var(--dim)}
.statusline .sep{margin:0 8px;opacity:.5}
#scanState{color:var(--fg)}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;background:var(--dim);
  margin-right:6px;vertical-align:middle}
.dot.live{background:var(--alert);box-shadow:0 0 8px var(--alert);animation:pulse 1s infinite}
.dot.idle{background:var(--fg);box-shadow:0 0 8px var(--fg)}
.dot.paused{background:var(--warn);box-shadow:0 0 8px var(--warn)}

.stats{display:grid;grid-template-columns:repeat(6,1fr);gap:8px;padding:12px 16px}
.stat{border:1px solid var(--grid);background:var(--panel);padding:8px 10px;border-radius:4px}
.stat .k{font-size:10px;color:var(--dim);letter-spacing:1px}
.stat .v{font-size:22px;text-shadow:0 0 8px var(--fg)}
.stat.alert .v{color:var(--alert);text-shadow:0 0 8px var(--alert)}

.control{display:flex;gap:8px;align-items:center;padding:0 16px 12px;flex-wrap:wrap}
.spacer{flex:1}
.btn{background:var(--dark);color:var(--fg);border:1px solid var(--dim);
  padding:7px 12px;font-family:var(--mono);font-size:12px;cursor:pointer;border-radius:3px;
  text-decoration:none;display:inline-block;letter-spacing:1px;transition:.15s}
.btn:hover{background:var(--fg);color:#000;box-shadow:0 0 12px var(--fg)}
.btn.ghost{border-color:var(--grid);color:var(--dim)}
.btn.danger{border-color:var(--alert);color:var(--alert)}
.btn.danger:hover{background:var(--alert);color:#000;box-shadow:0 0 12px var(--alert)}
.tgl{border:1px solid var(--grid);padding:6px 10px;border-radius:3px;cursor:pointer;user-select:none}
.tgl input{accent-color:var(--fg)}
.filt{background:#020e05;border:1px solid var(--dim);color:var(--fg);padding:7px 10px;
  border-radius:3px;font-family:var(--mono);min-width:180px}
.filt:focus{outline:none;box-shadow:0 0 8px var(--dim)}

.tabs{display:flex;gap:2px;padding:0 12px;border-bottom:1px solid var(--dim);flex-wrap:wrap}
.tab{background:transparent;border:1px solid transparent;border-bottom:none;color:var(--dim);
  padding:8px 14px;cursor:pointer;font-family:var(--mono);font-size:12px;letter-spacing:1px}
.tab:hover{color:var(--fg)}
.tab.active{color:var(--fg);border-color:var(--dim);background:var(--panel);
  border-radius:4px 4px 0 0;text-shadow:0 0 6px var(--fg)}
.cnt{background:var(--grid);border-radius:8px;padding:0 6px;font-size:10px;margin-left:4px}
.cnt.warn{background:var(--alert);color:#000}

main{padding:12px 16px 40px}
.panel{display:none}
.panel.active{display:block;animation:fade .2s}
.hint{color:var(--dim);font-size:11px;border-left:2px solid var(--grid);padding-left:8px;margin:4px 0 12px}

table.grid{width:100%;border-collapse:collapse;font-size:12px}
.grid th{text-align:left;color:var(--dim);border-bottom:1px solid var(--dim);
  padding:6px 8px;position:sticky;top:0;background:#020a04;cursor:pointer;white-space:nowrap}
.grid th:hover{color:var(--fg)}
.grid td{padding:5px 8px;border-bottom:1px solid var(--grid);white-space:nowrap;
  overflow:hidden;text-overflow:ellipsis;max-width:240px}
.grid tr:hover td{background:rgba(0,255,65,.06)}

.bar{display:inline-block;width:70px;height:8px;background:#03190a;border:1px solid var(--grid);
  border-radius:2px;overflow:hidden;vertical-align:middle}
.bar > i{display:block;height:100%}
.enc{padding:1px 6px;border-radius:3px;font-size:10px;border:1px solid}
.enc.open{color:var(--alert);border-color:var(--alert)}
.enc.weak{color:var(--warn);border-color:var(--warn)}
.enc.ok{color:var(--fg);border-color:var(--dim)}
.badge{padding:1px 6px;border-radius:3px;font-size:10px;font-weight:bold}
.badge.alert{background:var(--alert);color:#000;animation:pulse 1.2s infinite}
.badge.watch{background:var(--warn);color:#000}
.badge.ok{color:var(--dim)}
tr.rogue td{background:rgba(255,45,79,.10)}
.star{cursor:pointer;color:var(--dim)}
.star:hover{color:var(--warn)}

.wlform{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:8px}
.wlform input{background:#020e05;border:1px solid var(--dim);color:var(--fg);
  padding:8px 10px;border-radius:3px;font-family:var(--mono);min-width:220px}
.wllist .wlentry{border:1px solid var(--grid);background:var(--panel);padding:8px 10px;
  border-radius:4px;margin-bottom:6px}
.wllist .ss{color:var(--fg);font-weight:bold}
.wllist .bb{color:var(--dim);display:flex;justify-content:space-between;align-items:center;
  padding:2px 0;border-top:1px dashed var(--grid);margin-top:4px}
.wllist .rm{color:var(--alert);cursor:pointer}
.logbtns{display:flex;gap:10px;flex-wrap:wrap}

footer{position:fixed;bottom:0;left:0;right:0;z-index:3;font-size:10px;color:var(--dim);
  padding:4px 16px;background:#000;border-top:1px solid var(--grid)}

@keyframes blink{50%{opacity:.2}}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.35}}
@keyframes fade{from{opacity:0}to{opacity:1}}
.glitch{position:relative}
.glitch::before,.glitch::after{content:attr(data-text);position:absolute;left:0;top:0;
  width:100%;overflow:hidden;opacity:.7}
.glitch::before{color:#00fff2;animation:g1 2.5s infinite;clip-path:inset(0 0 60% 0)}
.glitch::after{color:var(--alert);animation:g2 3s infinite;clip-path:inset(60% 0 0 0)}
@keyframes g1{0%,100%{transform:translateX(0)}45%{transform:translateX(-2px)}50%{transform:translateX(2px)}}
@keyframes g2{0%,100%{transform:translateX(0)}48%{transform:translateX(2px)}52%{transform:translateX(-2px)}}

@media(max-width:760px){
  .stats{grid-template-columns:repeat(3,1fr)}
  h1{font-size:18px}
  .grid td{max-width:120px}
}
)CSSDOC";

static const char APP_JS[] PROGMEM = R"JSDOC(
'use strict';
const $ = s => document.querySelector(s);
const $$ = s => [...document.querySelectorAll(s)];
let WIFI = [], BLE = [], ROGUE = [], WL = {};
let sortKey = 'rssi', sortDir = -1, filterTxt = '';

// ---- matrix rain background ----
(function(){
  const c = $('#matrix'), x = c.getContext('2d');
  let cols, drops;
  const chars = 'アイウカキ0123456789ABCDEF#@$%&*';
  function resize(){ c.width = innerWidth; c.height = innerHeight;
    cols = Math.floor(c.width/14); drops = Array(cols).fill(1); }
  resize(); addEventListener('resize', resize);
  setInterval(()=>{
    x.fillStyle='rgba(0,6,0,.08)'; x.fillRect(0,0,c.width,c.height);
    x.fillStyle='#00ff41'; x.font='14px monospace';
    for(let i=0;i<drops.length;i++){
      x.fillText(chars[Math.random()*chars.length|0], i*14, drops[i]*14);
      if(drops[i]*14>c.height && Math.random()>.975) drops[i]=0;
      drops[i]++;
    }
  },60);
})();

// ---- tabs ----
$$('.tab').forEach(t=>t.onclick=()=>{
  $$('.tab').forEach(x=>x.classList.remove('active'));
  $$('.panel').forEach(x=>x.classList.remove('active'));
  t.classList.add('active');
  $('#panel-'+t.dataset.tab).classList.add('active');
});

// ---- column sort ----
function bindSort(tableSel){
  $$(tableSel+' th[data-s]').forEach(th=>th.onclick=()=>{
    const k = th.dataset.s;
    if(sortKey===k) sortDir*=-1; else { sortKey=k; sortDir=1; }
    render();
  });
}
bindSort('#wifiTable'); bindSort('#bleTable');

$('#filter').oninput = e => { filterTxt = e.target.value.toLowerCase(); render(); };

// ---- helpers ----
function encClass(e){ if(e==='OPEN')return 'open'; if(e==='WEP')return 'weak'; return 'ok'; }
function barColor(q){ return q>66?'#00ff41':q>33?'#ffb000':'#ff2d4f'; }
function sigBar(q){ return `<span class="bar"><i style="width:${q}%;background:${barColor(q)}"></i></span>`; }
function esc(s){ return (s==null?'':(''+s)).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }
function age(s){ return s<60?s+'s':s<3600?(s/60|0)+'m':(s/3600|0)+'h'; }
function matchF(o){ if(!filterTxt) return true; return JSON.stringify(o).toLowerCase().includes(filterTxt); }
function sortRows(rows){
  return rows.sort((a,b)=>{
    let A=a[sortKey], B=b[sortKey];
    if(typeof A==='string'){ A=A.toLowerCase(); B=(''+B).toLowerCase(); }
    return (A<B?-1:A>B?1:0)*sortDir;
  });
}

// ---- renderers ----
function render(){
  // WIFI
  let rows = sortRows(WIFI.filter(matchF));
  $('#wifiTable tbody').innerHTML = rows.map(a=>{
    const sev = a.sev===2?'<span class="badge alert">ALERT</span>'
              : a.sev===1?'<span class="badge watch">WATCH</span>'
              : '<span class="badge ok">ok</span>';
    return `<tr class="${a.sev===2?'rogue':''}">
      <td title="${esc(a.reason)}">${esc(a.ssid)}</td>
      <td>${a.bssid} <span class="star" title="trust this AP"
            data-ssid="${esc(a.ssid)}" data-bssid="${esc(a.bssid)}">&#9733;</span></td>
      <td>${a.rssi}</td><td>${sigBar(a.quality)}</td><td>${a.channel}</td>
      <td><span class="enc ${encClass(a.enc)}">${a.enc}</span></td>
      <td>${esc(a.vendor)}</td><td>${a.seen}</td><td>${sev}</td></tr>`;
  }).join('') || emptyRow(9);

  // ROGUE
  $('#rogueTable tbody').innerHTML = ROGUE.map(a=>{
    const sev=a.sev===2?'<span class="badge alert">ALERT</span>':'<span class="badge watch">WATCH</span>';
    return `<tr class="${a.sev===2?'rogue':''}"><td>${sev}</td><td>${esc(a.ssid)}</td>
      <td>${a.bssid}</td><td>${a.rssi}</td><td>${a.channel}</td>
      <td><span class="enc ${encClass(a.enc)}">${a.enc}</span></td>
      <td>${esc(a.vendor)}</td><td>${esc(a.reason)}</td></tr>`;
  }).join('') || emptyRow(8,'no rogue activity detected');

  // BLE
  let brows = sortRows(BLE.filter(matchF));
  $('#bleTable tbody').innerHTML = brows.map(d=>`<tr>
      <td>${esc(d.name)||'<i style=opacity:.4>--</i>'}</td><td>${d.address}</td>
      <td>${d.type}</td><td>${d.rssi}</td><td>${sigBar(d.quality)}</td>
      <td>${esc(d.vendor)}</td><td>${esc(d.mfg)}</td><td>${d.seen}</td>
      <td title="${esc(d.services)}">${esc(d.services)||'--'}</td></tr>`
  ).join('') || emptyRow(9);

  $('#tWifi').textContent=WIFI.length; $('#tBle').textContent=BLE.length;
  $('#tRogue').textContent=ROGUE.length;
}
function emptyRow(n,msg){ return `<tr><td colspan="${n}" style="text-align:center;color:#0a8a2a;padding:20px">${msg||'// no data yet — scanning...'}</td></tr>`; }

function renderWL(){
  const keys = Object.keys(WL).sort();
  $('#wlList').innerHTML = keys.length ? keys.map(s=>`
    <div class="wlentry"><div class="ss">${esc(s)}
      <span class="rm" data-ssid="${esc(s)}" data-bssid="">[remove ssid]</span></div>
      ${WL[s].map(b=>`<div class="bb"><span>${esc(b)}</span>
        <span class="rm" data-ssid="${esc(s)}" data-bssid="${esc(b)}">&#10007;</span></div>`).join('')}
    </div>`).join('')
    : '<p class="hint">No trusted APs yet. Add your bank\'s legitimate access points so evil twins stand out.</p>';
}

// ---- status ----
function applyStatus(s){
  $('#fwver').textContent = 'v'+s.version;
  $('#sWifi').textContent = s.wifi_live; $('#sBle').textContent = s.ble_live;
  $('#sRogue').textContent = s.rogue_count; $('#sCycles').textContent = s.cycles;
  $('#sClients').textContent = s.clients;
  $('#sPsram').textContent = (s.psram_free/1024/1024).toFixed(1)+'M';
  $('#uptime').textContent = age(s.uptime_s);
  let ip = 'AP '+s.ap_ip; if(s.sta_connected) ip += ' / STA '+s.sta_ip;
  $('#ipInfo').textContent = ip;
  const dot=$('#scanDot');
  dot.className='dot '+(!s.scanning?'paused':s.phase==='idle'?'idle':'live');
  $('#scanState').textContent = !s.scanning?'PAUSED':('SCAN:'+s.phase.toUpperCase());
  $('#btnScan').innerHTML = s.scanning?'&#9632; STOP SCAN':'&#9658; START SCAN';
  $('#tgWifi').checked=s.wifi_enabled; $('#tgBle').checked=s.ble_enabled;
}

// ---- API ----
async function post(url,data){
  const b=new URLSearchParams(data);
  return fetch(url,{method:'POST',body:b}).then(r=>r.json()).catch(()=>{});
}
async function refresh(){
  try{
    const [w,b,r]=await Promise.all([
      fetch('/api/wifi').then(r=>r.json()),
      fetch('/api/ble').then(r=>r.json()),
      fetch('/api/rogues').then(r=>r.json())]);
    WIFI=w; BLE=b; ROGUE=r; render();
  }catch(e){}
}
async function loadWL(){ WL=await fetch('/api/whitelist').then(r=>r.json()).catch(()=>({})); renderWL(); }

function trust(ssid,bssid){ post('/api/whitelist',{action:'add',ssid,bssid}).then(()=>{loadWL();refresh();flash('trusted '+bssid);}); }
function rmWL(ssid,bssid){ post('/api/whitelist',{action:'remove',ssid,bssid}).then(()=>{loadWL();refresh();}); }

// Delegated clicks — keeps SSID/BSSID values in data-* attributes so names
// containing quotes/backslashes can never break an inline handler.
document.addEventListener('click', e => {
  const star = e.target.closest('.star');
  if (star) { trust(star.dataset.ssid, star.dataset.bssid); return; }
  const rm = e.target.closest('.rm');
  if (rm) { rmWL(rm.dataset.ssid, rm.dataset.bssid); return; }
});

function flash(m){ const f=$('#footMsg'); f.textContent='>> '+m; setTimeout(()=>f.textContent='esp32-s3 wardriver // authorised testing only',2500); }

// ---- controls ----
let scanning=true;
$('#btnScan').onclick=()=>{ scanning=!scanning; post('/api/control',{scanning:scanning?1:0}).then(applyStatus); };
$('#tgWifi').onchange=e=>post('/api/control',{wifi:e.target.checked?1:0}).then(applyStatus);
$('#tgBle').onchange=e=>post('/api/control',{ble:e.target.checked?1:0}).then(applyStatus);
$('#btnClearWifi').onclick=()=>post('/api/clear',{what:'wifi'}).then(()=>{refresh();flash('wifi table cleared');});
$('#btnClearBle').onclick=()=>post('/api/clear',{what:'ble'}).then(()=>{refresh();flash('ble table cleared');});
$('#wlAdd').onclick=()=>{ const s=$('#wlSsid').value.trim(),b=$('#wlBssid').value.trim();
  if(!s){flash('ssid required');return;} post('/api/whitelist',{action:'add',ssid:s,bssid:b})
    .then(()=>{$('#wlSsid').value='';$('#wlBssid').value='';loadWL();refresh();flash('added '+s);}); };
$('#btnWipe').onclick=()=>{ if(confirm('Erase all CSV logs on the device?'))
  fetch('/api/logs',{method:'DELETE'}).then(()=>flash('logs wiped')); };

// ---- live status via websocket, table data via polling ----
function connectWS(){
  try{
    const ws=new WebSocket('ws://'+location.host+'/ws');
    ws.onmessage=e=>{ try{ const s=JSON.parse(e.data); scanning=s.scanning; applyStatus(s);}catch(_){} };
    ws.onclose=()=>setTimeout(connectWS,2000);
  }catch(e){ setTimeout(connectWS,2000); }
}

fetch('/api/status').then(r=>r.json()).then(s=>{scanning=s.scanning;applyStatus(s);});
loadWL(); refresh(); connectWS();
setInterval(refresh, 3000);
)JSDOC";
