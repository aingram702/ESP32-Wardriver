// ============================================================================
//  web_assets.h  -  The entire web UI embedded as a single PROGMEM string so
//  the firmware flashes as ONE binary (no separate filesystem upload step).
//  Hacker/terminal aesthetic: dark CRT-green theme, radar logo, color-coded
//  rows. SSIDs are rendered with textContent only (never innerHTML) to defeat
//  XSS from hostile network names.
// ============================================================================
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"=====(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 // WARDRIVER</title>
<style>
:root{--bg:#04080a;--panel:#0a1014;--edge:#10301c;--fg:#39ff14;--dim:#1f7a3a;
--cyan:#22d3ee;--red:#ff2b3e;--amber:#ffb300;--mut:#5b6b60;}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);
font-family:"JetBrains Mono",Consolas,Menlo,monospace;font-size:14px}
body::before{content:"";position:fixed;inset:0;pointer-events:none;z-index:9;
background:repeating-linear-gradient(0deg,rgba(0,0,0,0),rgba(0,0,0,0) 2px,
rgba(0,20,0,.25) 3px);opacity:.35}
header{display:flex;align-items:center;gap:14px;padding:12px 18px;
border-bottom:1px solid var(--edge);background:linear-gradient(180deg,#06120a,#04080a)}
.logo{width:38px;height:38px;filter:drop-shadow(0 0 6px var(--fg))}
h1{font-size:18px;margin:0;letter-spacing:3px;text-shadow:0 0 8px var(--dim)}
h1 small{color:var(--mut);letter-spacing:1px;font-size:11px;display:block}
.spacer{flex:1}
.mode{display:flex;border:1px solid var(--edge);border-radius:6px;overflow:hidden}
.mode button{background:transparent;color:var(--mut);border:0;padding:8px 14px;
cursor:pointer;font-family:inherit;font-size:13px}
.mode button.on{background:var(--fg);color:#04080a;font-weight:700}
.mode button.on.atk{background:var(--red);color:#fff}
.bar{display:flex;flex-wrap:wrap;gap:10px;padding:10px 18px;border-bottom:1px solid var(--edge);
background:var(--panel)}
.stat{border:1px solid var(--edge);border-radius:6px;padding:6px 12px;min-width:96px}
.stat .k{color:var(--mut);font-size:10px;letter-spacing:1px}
.stat .v{font-size:18px;text-shadow:0 0 6px var(--dim)}
.stat.red .v{color:var(--red);text-shadow:0 0 8px var(--red)}
nav{display:flex;gap:4px;padding:8px 18px;border-bottom:1px solid var(--edge)}
nav button{background:transparent;border:1px solid var(--edge);color:var(--mut);
padding:6px 14px;border-radius:6px 6px 0 0;cursor:pointer;font-family:inherit}
nav button.on{color:var(--fg);border-bottom-color:var(--bg);background:var(--panel)}
main{padding:16px 18px 60px}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-bottom:12px}
input,select{background:#02110a;border:1px solid var(--edge);color:var(--fg);
padding:7px 10px;border-radius:5px;font-family:inherit}
.btn{background:var(--fg);color:#04080a;border:0;padding:8px 14px;border-radius:5px;
cursor:pointer;font-family:inherit;font-weight:700}
.btn.ghost{background:transparent;color:var(--fg);border:1px solid var(--fg)}
.btn.red{background:var(--red);color:#fff}
.btn.amber{background:var(--amber);color:#04080a}
table{width:100%;border-collapse:collapse;font-size:12.5px}
th,td{text-align:left;padding:7px 9px;border-bottom:1px solid #0c1d12;white-space:nowrap}
th{color:var(--mut);position:sticky;top:0;background:var(--panel);cursor:pointer;
font-size:11px;letter-spacing:1px}
td.ssid{white-space:normal;max-width:260px;word-break:break-all}
tr.wifi td:first-child{border-left:3px solid var(--fg)}
tr.ble td:first-child{border-left:3px solid var(--cyan)}
tr.ble{color:var(--cyan)}
tr.danger{background:rgba(255,43,62,.12);color:var(--red);
animation:blink 1.4s steps(2,start) infinite}
tr.danger td:first-child{border-left:3px solid var(--red)}
@keyframes blink{50%{background:rgba(255,43,62,.03)}}
.tag{font-size:10px;padding:1px 6px;border:1px solid currentColor;border-radius:4px}
.hidden{display:none}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:10px}
.muted{color:var(--mut)}
.note{color:var(--amber);font-size:12px;border:1px dashed var(--amber);
padding:8px 10px;border-radius:6px;margin-bottom:12px}
footer{position:fixed;bottom:0;left:0;right:0;background:#06120a;
border-top:1px solid var(--edge);padding:4px 18px;font-size:11px;color:var(--mut);
display:flex;gap:18px}
a{color:var(--cyan)}
</style></head>
<body>
<header>
<svg class="logo" viewBox="0 0 64 64" fill="none" stroke="#39ff14" stroke-width="2">
<circle cx="32" cy="32" r="28"/><circle cx="32" cy="32" r="18"/>
<circle cx="32" cy="32" r="8"/><line x1="32" y1="32" x2="58" y2="14"/>
<circle cx="32" cy="32" r="2.5" fill="#39ff14"/></svg>
<h1>WARDRIVER<small>ESP32-S3 // RF RECON UNIT</small></h1>
<div class="spacer"></div>
<div class="mode">
<button id="mWar" class="on" onclick="setMode('wardrive')">WARDRIVE</button>
<button id="mAtk" onclick="setMode('attack')">ATTACK</button>
</div>
</header>

<div class="bar">
<div class="stat"><div class="k">DEVICES</div><div class="v" id="sDev">0</div></div>
<div class="stat"><div class="k">WIFI APs</div><div class="v" id="sWifi">0</div></div>
<div class="stat red"><div class="k">THREATS</div><div class="v" id="sThreat">0</div></div>
<div class="stat"><div class="k">GPS</div><div class="v" id="sGps">--</div></div>
<div class="stat"><div class="k">UPTIME</div><div class="v" id="sUp">0s</div></div>
<div class="stat"><div class="k">HEAP KB</div><div class="v" id="sHeap">0</div></div>
</div>

<nav>
<button id="tabDev" class="on" onclick="tab('dev')">DISCOVERED</button>
<button id="tabPkt" onclick="tab('pkt')">PACKET MONITOR</button>
<button id="tabInfo" onclick="tab('info')">SYSTEM</button>
</nav>

<main>
<!-- DEVICES -->
<section id="vDev">
  <div class="row">
    <input id="filter" placeholder="filter ssid / mac / vendor..." oninput="render()">
    <select id="ftype" onchange="render()">
      <option value="">all networks</option><option value="danger">threats only</option>
    </select>
    <span class="spacer" style="flex:1"></span>
    <a class="btn" href="/api/export.csv" download="wardrive.csv">EXPORT CSV</a>
    <button class="btn ghost" onclick="clearAll()">CLEAR</button>
  </div>
  <div style="overflow:auto;max-height:64vh">
  <table id="tbl"><thead><tr>
    <th onclick="sortBy('ssid')">SSID / NAME</th><th onclick="sortBy('mac')">MAC</th>
    <th onclick="sortBy('rssi')">RSSI</th><th onclick="sortBy('type')">TYPE</th>
    <th onclick="sortBy('channel')">CH</th><th>ENC</th><th>VENDOR</th>
    <th onclick="sortBy('hits')">HITS</th><th>GPS</th></tr></thead>
  <tbody id="rows"></tbody></table></div>
</section>

<!-- PACKETS -->
<section id="vPkt" class="hidden">
  <div id="atkOff" class="note">Switch to <b>ATTACK</b> mode to enable the packet
   monitor and deauth tools. Scanning pauses while attacking.</div>
  <div class="row">
    <label>CH <select id="chan" onchange="setChan()"></select></label>
    <button class="btn" id="snBtn" onclick="toggleSniff()">START SNIFF</button>
    <button class="btn ghost" onclick="resetPkt()">RESET COUNTERS</button>
  </div>
  <div class="grid" style="margin-bottom:14px">
    <div class="stat"><div class="k">TOTAL</div><div class="v" id="pTot">0</div></div>
    <div class="stat"><div class="k">MGMT</div><div class="v" id="pMgmt">0</div></div>
    <div class="stat"><div class="k">DATA</div><div class="v" id="pData">0</div></div>
    <div class="stat"><div class="k">CTRL</div><div class="v" id="pCtrl">0</div></div>
    <div class="stat"><div class="k">BEACON</div><div class="v" id="pBeac">0</div></div>
    <div class="stat"><div class="k">PROBE</div><div class="v" id="pProbe">0</div></div>
    <div class="stat red"><div class="k">DEAUTH</div><div class="v" id="pDeau">0</div></div>
    <div class="stat"><div class="k">EAPOL</div><div class="v" id="pEap">0</div></div>
  </div>
  <div class="note">DEAUTH transmits 802.11 deauthentication frames. Use ONLY on
   networks you own or are authorised to test &mdash; doing otherwise is illegal.</div>
  <div class="row">
    <input id="apMac" placeholder="target AP BSSID AA:BB:CC:DD:EE:FF" size="26">
    <input id="clMac" placeholder="client MAC (blank = all clients)" size="26">
    <input id="bursts" type="number" value="20" min="1" max="500" style="width:80px">
    <button class="btn red" onclick="deauth()">DEAUTH</button>
  </div>
  <div style="overflow:auto;max-height:42vh">
  <table><thead><tr><th>T(ms)</th><th>KIND</th><th>SRC</th><th>DST</th>
   <th>CH</th><th>RSSI</th></tr></thead><tbody id="pkts"></tbody></table></div>
</section>

<!-- INFO -->
<section id="vInfo" class="hidden">
  <p class="muted">ESP32-S3 Wardriver &mdash; WiFi recon, GPS-tagged,
   CSV export. AP-hosted control plane.</p>
  <ul class="muted">
   <li>Color key: <span style="color:var(--fg)">WiFi</span> /
       <span style="color:var(--red)">THREAT (e.g. Flock camera)</span></li>
   <li id="iGps">GPS: unknown</li><li id="iAp">AP: --</li>
   <li>Duplicates are merged by MAC; RSSI shows the strongest sighting.</li>
  </ul>
  <p class="muted">Flock detection uses MAC-OUI + name heuristics and must be
   field-verified. Update <code>flock_data.h</code> with confirmed signatures.</p>
</section>
</main>

<footer><span id="fMode">MODE: WARDRIVE</span><span id="fScan">scan: --</span>
<span class="spacer" style="flex:1"></span><span>// stay legal // stay curious</span></footer>

<script>
let DEV=[],SORT='rssi',ASC=false,SNIFF=false;
const $=id=>document.getElementById(id);
const esc=s=>s==null?'':String(s);
async function jget(u){const r=await fetch(u);return r.json();}
async function jpost(u,p){const b=new URLSearchParams(p||{});
 const r=await fetch(u,{method:'POST',body:b});return r.json().catch(()=>({}));}

function tab(t){for(const x of['dev','pkt','info']){
 $('v'+x.charAt(0).toUpperCase()+x.slice(1)).classList.toggle('hidden',x!==t);
 $('tab'+x.charAt(0).toUpperCase()+x.slice(1)).classList.toggle('on',x===t);}}

async function setMode(m){await jpost('/api/mode',{mode:m});refreshStatus();}
function applyMode(m){const a=m==='attack';
 $('mWar').className=a?'':'on';$('mAtk').className=a?'on atk':'';
 $('atkOff').classList.toggle('hidden',a);$('fMode').textContent='MODE: '+m.toUpperCase();}

async function refreshStatus(){try{const s=await jget('/api/status');
 $('sDev').textContent=s.devices;$('sWifi').textContent=s.wifi;
 $('sThreat').textContent=s.threats;
 $('sGps').textContent=s.gps.fix?s.gps.sats+'sat':(s.gps.present?'srch':'--');
 $('sUp').textContent=fmtUp(s.uptime);$('sHeap').textContent=Math.round(s.heap/1024);
 const ago=s.lastScan>0?Math.max(0,s.uptime-((s.lastScan/1000)|0)):-1;
 $('fScan').textContent='wifi scan: '+(ago>=0?ago+'s ago':'--');
 applyMode(s.mode);
 $('iGps').textContent='GPS: '+(s.gps.fix?(s.gps.lat.toFixed(5)+', '+s.gps.lon.toFixed(5)+
   ' ('+s.gps.sats+' sats)'):(s.gps.present?'searching':'no module / no fix'));
 $('iAp').textContent='AP: '+s.ssid+'  ('+s.ip+', ch '+s.apChannel+')';
 if(!$('chan').options.length){for(let c=1;c<=13;c++){const o=document.createElement('option');
   o.value=c;o.text=c;if(c==s.apChannel)o.selected=true;$('chan').appendChild(o);}}
}catch(e){}}

function fmtUp(s){s|=0;const d=s/86400|0,h=(s%86400)/3600|0,m=(s%3600)/60|0;
 return d?d+'d'+h+'h':h?h+'h'+m+'m':m?m+'m'+(s%60)+'s':s+'s';}

async function refreshDevices(){try{DEV=await jget('/api/devices');render();}catch(e){}}
function sortBy(k){if(SORT===k)ASC=!ASC;else{SORT=k;ASC=(k==='ssid'||k==='mac');}render();}

function render(){const f=$('filter').value.toLowerCase(),ft=$('ftype').value;
 let list=DEV.filter(d=>{
   if(ft==='danger'&&!d.dangerous)return false;
   if(!f)return true;
   return (esc(d.ssid)+esc(d.mac)+esc(d.vendor)).toLowerCase().includes(f);});
 list.sort((a,b)=>{let x=a[SORT],y=b[SORT];
   if(typeof x==='string'){x=x.toLowerCase();y=esc(y).toLowerCase();
     return ASC?(x<y?-1:x>y?1:0):(x>y?-1:x<y?1:0);}
   return ASC?x-y:y-x;});
 const tb=$('rows');tb.textContent='';
 for(const d of list){const tr=document.createElement('tr');
   tr.className=d.dangerous?'danger':'wifi';
   const cells=[];
   const c0=document.createElement('td');c0.className='ssid';
   c0.textContent=d.ssid||'<hidden>';
   if(d.dangerous){const t=document.createElement('span');t.className='tag';
     t.textContent=' '+d.threat;t.style.marginLeft='6px';c0.appendChild(t);}
   tr.appendChild(c0);
   for(const v of [d.mac,d.rssi+'dBm',d.type,d.channel||'-',d.enc||'-',
     d.vendor||'-',d.hits,d.hasGps?'●':'-']){
     const td=document.createElement('td');td.textContent=v;tr.appendChild(td);}
   tb.appendChild(tr);}}

async function clearAll(){if(!confirm('Clear all discovered devices?'))return;
 await jpost('/api/clear');refreshDevices();refreshStatus();}

/* packet monitor */
async function toggleSniff(){SNIFF=!SNIFF;
 await jpost('/api/sniff',{action:SNIFF?'start':'stop',channel:$('chan').value});
 $('snBtn').textContent=SNIFF?'STOP SNIFF':'START SNIFF';
 $('snBtn').className=SNIFF?'btn red':'btn';}
async function setChan(){await jpost('/api/sniff',{action:'channel',channel:$('chan').value});}
async function resetPkt(){await jpost('/api/sniff',{action:'reset'});refreshPackets();}
async function deauth(){const ap=$('apMac').value.trim();
 if(!/^([0-9a-f]{2}:){5}[0-9a-f]{2}$/i.test(ap)){alert('Enter a valid AP BSSID');return;}
 const r=await jpost('/api/deauth',{ap:ap,client:$('clMac').value.trim(),bursts:$('bursts').value});
 if(r.error)alert(r.error);}
async function refreshPackets(){if($('vPkt').classList.contains('hidden'))return;
 try{const p=await jget('/api/packets');const s=p.stats;
   $('pTot').textContent=s.total;$('pMgmt').textContent=s.mgmt;$('pData').textContent=s.data;
   $('pCtrl').textContent=s.ctrl;$('pBeac').textContent=s.beacon;$('pProbe').textContent=s.probe;
   $('pDeau').textContent=s.deauth;$('pEap').textContent=s.eapol;
   const tb=$('pkts');tb.textContent='';
   for(const r of p.recent.slice().reverse()){const tr=document.createElement('tr');
     for(const v of [r.t,r.kind,r.src,r.dst,r.ch,r.rssi+'dBm']){
       const td=document.createElement('td');td.textContent=v;tr.appendChild(td);}
     tb.appendChild(tr);}}catch(e){}}

setInterval(refreshStatus,1500);
setInterval(refreshDevices,2500);
setInterval(refreshPackets,1500);
refreshStatus();refreshDevices();
</script>
</body></html>)=====";
