#pragma once
// Web-App von Trilumag. Wird vom ESP32 unter "/" ausgeliefert.

const char INDEX_HTML[] = R"HTML(<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Trilumag</title>
<style>
:root{--bg:#0d0f13;--panel:#161a21;--line:#2a303a;--fg:#e7eaef;--muted:#8d96a3;--accent:#6e8bff;--ghost:#3a4250;--snap:#6e8bff;color-scheme:dark}
*{box-sizing:border-box}
[hidden]{display:none!important}
html,body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif}
.wrap{max-width:960px;margin:0 auto;padding:16px;display:flex;flex-direction:column;gap:14px}
header{display:flex;justify-content:space-between;align-items:baseline;gap:12px;flex-wrap:wrap}
h1{font-size:22px;margin:0;font-weight:650;letter-spacing:-.01em}
.status{font:12px ui-monospace,Menlo,monospace;color:var(--muted)}
.wall{background:#090b0e;border:1px solid var(--line);border-radius:14px;overflow:hidden;position:relative}
#wall{display:block;width:100%;height:min(62vh,560px);touch-action:none;user-select:none;-webkit-user-select:none}
.hint{position:absolute;left:12px;bottom:10px;font:12px ui-monospace,Menlo,monospace;color:var(--muted);pointer-events:none}
.tri{stroke:#05060a;stroke-width:1.5;cursor:grab}
.tri.main{cursor:default;stroke:#7d8796;stroke-width:2}
.tri.sel{stroke:#fff;stroke-width:2.5}
.dark{fill:#1b1e24}
.pulse{animation:pl 1.6s ease-in-out infinite}
@keyframes pl{0%,100%{fill:#0f2350}50%{fill:#3d6dff}}
.ghost{fill:none;stroke:var(--ghost);stroke-width:1.2;stroke-dasharray:4 4}
.ghost.hot{stroke:var(--snap);stroke-width:2.2;fill:rgba(110,139,255,.12)}
.lbl{font:9px ui-monospace,Menlo,monospace;fill:rgba(255,255,255,.55);pointer-events:none;text-anchor:middle}
.lbl.dk{fill:rgba(0,0,0,.55)}
.edge1{fill:rgba(255,255,255,.75);pointer-events:none}
.tray{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:12px;display:flex;flex-direction:column;gap:10px}
.tray h2,.ctl h2{font-size:13px;margin:0;color:var(--muted);font-weight:500;text-transform:uppercase;letter-spacing:.08em}
.items{display:flex;flex-wrap:wrap;gap:10px;align-items:center;min-height:56px}
.item{width:56px;height:52px;touch-action:none;cursor:grab}
.item svg{width:56px;height:52px;display:block}
.empty{color:var(--muted);font-size:14px}
button{font:500 14px/1 system-ui,sans-serif;color:var(--fg);background:#1f242d;border:1px solid var(--line);border-radius:999px;padding:10px 14px;cursor:pointer}
button.on{background:var(--accent);color:#0d0f13;border-color:var(--accent)}
button:disabled{opacity:.4;cursor:default}
.ctl{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:12px;display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;align-items:end}
.ctl h2{grid-column:1/-1}
label{display:flex;flex-direction:column;gap:6px;font-size:13px;color:var(--muted)}
input[type=range]{width:100%;accent-color:var(--accent)}
input[type=text],input[type=password],input:not([type]){width:100%;height:40px;border:1px solid var(--line);border-radius:8px;background:#0d0f13;color:var(--fg);padding:0 10px;font:15px system-ui,sans-serif}
select{width:100%;height:40px;border:1px solid var(--line);border-radius:8px;background:#0d0f13;color:var(--fg);padding:0 8px;font:15px system-ui,sans-serif}
.pins{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}
summary{cursor:pointer;list-style:none}
summary::-webkit-details-marker{display:none}
summary h2::after{content:" ▾"}
details[open] summary h2::after{content:" ▴"}
input[type=color]{width:100%;height:40px;border:1px solid var(--line);border-radius:8px;background:none;padding:2px}
.row{display:flex;gap:8px;flex-wrap:wrap}
.toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:#232935;border:1px solid var(--line);padding:10px 14px;border-radius:10px;font-size:14px;opacity:0;transition:opacity .2s;pointer-events:none}
.toast.show{opacity:1}
</style></head><body>
<div class="wrap">
  <header><h1>Trilumag</h1><span class="status" id="status">verbinde …</span></header>
  <div class="tray" id="wifiBox" hidden>
    <h2>WLAN einrichten</h2>
    <p class="empty">Trilumag ist noch mit keinem WLAN verbunden. Wähle dein Netz, danach startet das Hauptpanel neu und ist unter http://trilumag.local erreichbar.</p>
    <label>WLAN<input id="ssid" list="nets" autocomplete="off" placeholder="Name deines WLANs"><datalist id="nets"></datalist></label>
    <label>Passwort<input id="pass" type="password" autocomplete="off"></label>
    <div class="row"><button id="scanBtn">Netze suchen</button><button id="wifiBtn" class="on">Speichern und verbinden</button></div>
  </div>
  <div class="wall">
    <svg id="wall" aria-label="Wand mit Panels"></svg>
    <div class="hint" id="hint">Panel aus der Ablage an eine freie Kante ziehen</div>
  </div>
  <div class="tray" id="trayBox">
    <h2>Ablage · abgeklipste Panels</h2>
    <div class="items" id="tray"></div>
    <div class="row"><button id="newBtn">Neues Panel</button></div>
  </div>
  <div class="ctl">
    <h2 id="selTitle">Kein Panel ausgewählt</h2>
    <label>Farbe<input type="color" id="col" value="#3d6dff"></label>
    <label>Weißanteil <span id="wv">0</span><input type="range" id="w" min="0" max="255" value="0"></label>
    <label>Helligkeit <span id="bv">180</span><input type="range" id="bri" min="1" max="255" value="180"></label>
    <div class="row"><button id="onBtn">Ein / Aus</button><button id="allBtn">Für alle</button></div>
  </div>
  <details class="tray" id="setBox">
    <summary><h2 style="display:inline">Einstellungen</h2></summary>
    <p class="empty" id="setInfo"></p>
    <label>Betriebsart<select id="mode"><option value="sim">Simulation: Panels in der App anklipsen</option><option value="bus">Bus: echte Panels über RS-485</option></select></label>
    <label>Board<select id="board"></select></label>
    <div class="pins">
      <label>RS-485 RX (RO)<select id="p_rx"></select></label>
      <label>RS-485 TX (DI)<select id="p_tx"></select></label>
      <label>RS-485 DE + /RE<select id="p_de"></select></label>
      <label>LED-Daten<select id="p_led"></select></label>
      <label>SNS rechts (Kante 2)<select id="p_snsR"></select></label>
      <label>SNS links (Kante 3)<select id="p_snsL"></select></label>
    </div>
    <label>Farbreihenfolge der LEDs<select id="order"></select></label>
    <div class="row"><span class="empty">Farbtest:</span><button data-t="0">Rot</button><button data-t="1">Grün</button><button data-t="2">Blau</button><button data-t="3">Weiß</button><button data-t="-1">Ende</button></div>
    <p class="empty">Leuchtet bei „Rot“ etwas anderes als Rot, stimmt die Reihenfolge nicht. Dann eine andere wählen und speichern.</p>
    <h2>Home Assistant (MQTT)</h2>
    <div class="pins">
      <label>Broker-Adresse<input id="m_host" placeholder="z. B. 192.168.1.10"></label>
      <label>Port<input id="m_port" inputmode="numeric" value="1883"></label>
      <label>Benutzer<input id="m_user" autocomplete="off"></label>
      <label>Passwort<input id="m_pass" type="password" autocomplete="off" placeholder="unverändert"></label>
    </div>
    <div class="row"><button id="saveBtn" class="on">Speichern und neu starten</button></div>
    <p class="empty" id="setErr"></p>
  </details>
</div>
<div class="toast" id="toast"></div>
<script>
const S=60,H=S*Math.sqrt(3)/2,SNAP=S*0.6,NS='http://www.w3.org/2000/svg';
const $=id=>document.getElementById(id);
const svg=$('wall'),tray=$('tray');
let st=null,sel=null,drag=null,vbFix=null,applyAll=false;

function geom(x,y,up){const cx=x*S/2,t=y*H;const p=up?[[cx-S/2,t+H],[cx+S/2,t+H],[cx,t]]:[[cx-S/2,t],[cx+S/2,t],[cx,t+H]];return{p,c:[cx,up?t+2*H/3:t+H/3]};}
function shrink(g,k){return g.p.map(q=>[g.c[0]+(q[0]-g.c[0])*k,g.c[1]+(q[1]-g.c[1])*k]);}
function pts(a){return a.map(q=>q[0].toFixed(1)+','+q[1].toFixed(1)).join(' ');}
function el(n,a){const e=document.createElementNS(NS,n);for(const k in a)e.setAttribute(k,a[k]);return e;}
function shade(p){if(!p.on)return'#22252b';const k=.22+.78*p.bri/255,f=c=>Math.min(255,Math.round((c+p.w*.85)*k));return`rgb(${f(p.r)},${f(p.g)},${f(p.b)})`;}
function light(p){if(!p.on||p.state!==2)return false;return(p.r+p.g+p.b+p.w*1.5)*p.bri/255>330;}
function edge1Mid(p){const g=geom(p.x,p.y,p.up);const L=p.up?['B','R','L']:['T','L','R'];const d=L[(p.rot)%3];const [a,b,c]=g.p;
  let m;if(p.up)m=d==='B'?[a,b]:d==='R'?[b,c]:[a,c];else m=d==='T'?[a,b]:d==='L'?[a,c]:[b,c];
  const mx=(m[0][0]+m[1][0])/2,my=(m[0][1]+m[1][1])/2;return[mx+(g.c[0]-mx)*.28,my+(g.c[1]-my)*.28];}
function toast(t){const e=$('toast');e.textContent=t;e.classList.add('show');clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('show'),2200);}

async function api(path,body){
  const r=await fetch(path,body?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}:{});
  const j=await r.json();if(!r.ok){toast(j.error||'Fehler');return null;}st=j;return j;}
async function poll(){if(drag)return;try{await api('/api/state');render();}catch(e){$('status').textContent='keine Verbindung zum ESP32';}}

function fitViewBox(){
  const xs=[],ys=[];const add=(x,y,up)=>{geom(x,y,up).p.forEach(q=>{xs.push(q[0]);ys.push(q[1]);});};
  st.panels.forEach(p=>add(p.x,p.y,p.up));st.ghosts.forEach(g=>add(g.x,g.y,g.up));
  let x0=Math.min(...xs)-S*.8,x1=Math.max(...xs)+S*.8,y0=Math.min(...ys)-S*.6,y1=Math.max(...ys)+S*.6;
  const r=svg.clientWidth/svg.clientHeight||1.6;let w=Math.max(x1-x0,S*3.4),h=Math.max(y1-y0,S*2.2);
  if(w/h<r)w=h*r;else h=w/r;const cx=(x0+x1)/2,cy=(y0+y1)/2;
  return[cx-w/2,cy-h/2,w,h];}

function render(){
  if(!st)return;
  const n=st.panels.length;
  $('status').textContent=`${n} von ${st.max} Panels · ${st.sim?'Simulation':'Bus'} · MQTT ${st.mqtt?'verbunden':'aus'}`;
  if(!drag){vbFix=fitViewBox();}
  svg.setAttribute('viewBox',vbFix.map(v=>v.toFixed(1)).join(' '));
  svg.innerHTML='';
  const gG=el('g',{}),gP=el('g',{}),gO=el('g',{id:'ov'});svg.append(gG,gP,gO);
  if(drag){st.ghosts.forEach((g,i)=>{const gg=geom(g.x,g.y,g.up);gG.append(el('polygon',{points:pts(shrink(gg,.94)),class:'ghost'+(drag.snap===i?' hot':'')}));});}
  st.panels.forEach(p=>{
    const g=geom(p.x,p.y,p.up);
    const cls=['tri'];if(p.main)cls.push('main');if(sel===p.id)cls.push('sel');
    let fill=null;if(p.state===0)cls.push('dark');else if(p.state===1)cls.push('pulse');else fill=shade(p);
    const poly=el('polygon',{points:pts(shrink(g,.94)),class:cls.join(' ')});if(fill)poly.setAttribute('fill',fill);
    poly.dataset.id=p.id;gP.append(poly);
    if(!p.main){const m=edge1Mid(p);gP.append(el('circle',{cx:m[0],cy:m[1],r:2.4,class:'edge1'}));}
    else{const a=g.p[0],b=g.p[1];gP.append(el('line',{x1:a[0]+8,y1:a[1]+5,x2:b[0]-8,y2:b[1]+5,stroke:'#7d8796','stroke-width':3,'stroke-linecap':'round'}));}
    const t=el('text',{x:g.c[0],y:g.c[1]+3,class:'lbl'+(light(p)?' dk':'')});t.textContent=p.main?'Haupt':p.id.slice(4);gP.append(t);
  });
  drawDrag();
  tray.innerHTML='';
  if(!st.loose.length){const e=document.createElement('span');e.className='empty';e.textContent='Leer. Mit „Neues Panel“ eins dazunehmen oder ein Panel von der Wand hierher ziehen.';tray.append(e);}
  st.loose.forEach(id=>{
    const d=document.createElement('div');d.className='item';d.dataset.id=id;
    const s=el('svg',{viewBox:`${-S/2-4} ${-4} ${S+8} ${H+8}`});
    s.append(el('polygon',{points:pts([[-S/2,H],[S/2,H],[0,0]]),fill:'#1b1e24',stroke:'#3a4250','stroke-width':1.5}));
    const t=el('text',{x:0,y:H*.68,class:'lbl'});t.textContent=id.slice(4);s.append(t);
    d.append(s);tray.append(d);d.addEventListener('pointerdown',e=>startDrag(e,id,'tray'));
  });
  $('newBtn').hidden=!st.sim;
  $('wifiBox').hidden=!st.ap;
  $('hint').textContent=!st.sim?'Echte Panels: anklipsen, und sie erscheinen hier':st.loose.length?'Panel aus der Ablage an eine freie Kante ziehen':'Panel antippen zum Einstellen, wegziehen zum Abklipsen';
  updateCtl();
}

function drawDrag(){
  const ov=$('ov');if(!ov||!drag||!drag.moved)return;ov.innerHTML='';
  let poly;
  if(drag.snap!=null){const g=st.ghosts[drag.snap];poly=shrink(geom(g.x,g.y,g.up),.94);}
  else{const c=drag.pt;poly=[[c.x-S/2,c.y+H/3],[c.x+S/2,c.y+H/3],[c.x,c.y-2*H/3]].map(q=>[q[0],q[1]]);}
  ov.append(el('polygon',{points:pts(poly),fill:drag.snap!=null?'rgba(110,139,255,.35)':'rgba(40,46,58,.9)',stroke:drag.snap!=null?'#6e8bff':'#7d8796','stroke-width':2}));
}

function toSvg(e){const p=svg.createSVGPoint();p.x=e.clientX;p.y=e.clientY;return p.matrixTransform(svg.getScreenCTM().inverse());}

function startDrag(e,id,from){
  if(!st||!st.sim&&from==='tray')return;
  e.preventDefault();
  drag={id,from,x0:e.clientX,y0:e.clientY,moved:from==='tray',pt:toSvg(e),snap:null,detached:from==='tray'};
  window.addEventListener('pointermove',onMove);window.addEventListener('pointerup',onUp,{once:true});
  if(drag.moved)render();
}
async function onMove(e){
  if(!drag)return;
  if(!drag.moved&&Math.hypot(e.clientX-drag.x0,e.clientY-drag.y0)>6){
    drag.moved=true;
    if(drag.from==='wall'&&!drag.detached){drag.detached=true;sel=null;await api('/api/sim/detach',{id:drag.id});toast('abgeklipst');}
  }
  if(!drag||!drag.moved)return;
  drag.pt=toSvg(e);
  let best=null,bd=SNAP;
  st.ghosts.forEach((g,i)=>{const c=geom(g.x,g.y,g.up).c;const d=Math.hypot(c[0]-drag.pt.x,c[1]-drag.pt.y);if(d<bd){bd=d;best=i;}});
  drag.snap=best;render();
}
async function onUp(){
  window.removeEventListener('pointermove',onMove);
  const d=drag;if(!d)return;
  if(!d.moved){drag=null;sel=d.id;render();return;}
  const g=d.snap!=null?st.ghosts[d.snap]:null;drag=null;
  if(g){const r=await api('/api/sim/attach',{id:d.id,parent:g.parent,edge:g.edge});if(r){sel=d.id;toast('angeklipst, wird erkannt …');}}
  render();
}
svg.addEventListener('pointerdown',e=>{
  const id=e.target.dataset&&e.target.dataset.id;if(!id)return;
  const p=st.panels.find(q=>q.id===id);if(!p)return;
  if(p.main||!st.sim){sel=id;render();return;}
  startDrag(e,id,'wall');
});

// ----- Einstellungen -----
function cur(){return st&&sel?st.panels.find(p=>p.id===sel):null;}
function updateCtl(){
  const p=cur();const has=!!p||applyAll;
  $('selTitle').textContent=applyAll?'Alle Panels':p?(p.main?'Hauptpanel':'Panel '+p.id.slice(4))+(p.state===1?' · wartet auf erste Farbe':''):'Panel antippen zum Einstellen';
  ['col','w','bri','onBtn'].forEach(k=>$(k).disabled=!has);
  $('allBtn').classList.toggle('on',applyAll);
  if(p&&!applyAll&&document.activeElement.tagName!=='INPUT'){
    const h=v=>v.toString(16).padStart(2,'0');$('col').value='#'+h(p.r)+h(p.g)+h(p.b);
    $('w').value=p.w;$('wv').textContent=p.w;$('bri').value=p.bri;$('bv').textContent=p.bri;
    $('onBtn').classList.toggle('on',p.on);}
}
let tSend=null;
function send(extra){
  const target=applyAll?'alle':sel;if(!target)return;
  const c=$('col').value;const body={id:target,color:{r:parseInt(c.slice(1,3),16),g:parseInt(c.slice(3,5),16),b:parseInt(c.slice(5,7),16),w:+$('w').value},brightness:+$('bri').value,state:'ON',...extra};
  clearTimeout(tSend);tSend=setTimeout(async()=>{await api('/api/set',body);render();},120);
}
$('col').addEventListener('input',()=>send());
$('w').addEventListener('input',()=>{$('wv').textContent=$('w').value;send();});
$('bri').addEventListener('input',()=>{$('bv').textContent=$('bri').value;send();});
$('onBtn').addEventListener('click',async()=>{const p=cur();const on=applyAll?true:!(p&&p.on);await api('/api/set',{id:applyAll?'alle':sel,state:on?'ON':'OFF'});render();});
$('allBtn').addEventListener('click',()=>{applyAll=!applyAll;updateCtl();});
$('newBtn').addEventListener('click',async()=>{if(await api('/api/sim/new',{}))render();});
$('scanBtn').addEventListener('click',async()=>{$('scanBtn').disabled=true;$('scanBtn').textContent='Suche …';
  try{const r=await fetch('/api/wifi/scan');const j=await r.json();$('nets').innerHTML='';j.networks.forEach(n=>{const o=document.createElement('option');o.value=n;$('nets').append(o);});toast(j.networks.length+' Netze gefunden');}catch(e){toast('Suche fehlgeschlagen');}
  $('scanBtn').disabled=false;$('scanBtn').textContent='Netze suchen';});
$('wifiBtn').addEventListener('click',async()=>{const ssid=$('ssid').value.trim();if(!ssid){toast('Bitte WLAN-Namen eingeben');return;}
  await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pass:$('pass').value})}).catch(()=>{});
  toast('Gespeichert. Trilumag startet neu und verbindet sich mit '+ssid);});
// ----- Einstellungen -----
let cfg=null;
const PIN_KEYS=['rx','tx','de','led','snsR','snsL'];
async function loadCfg(){
  try{cfg=await (await fetch('/api/config')).json();}catch(e){return;}
  $('setInfo').textContent=`${cfg.chip} · Firmware ${cfg.ver}`;
  $('mode').value=cfg.mode;
  const b=$('board');b.innerHTML='';
  cfg.boards.forEach(x=>{const o=document.createElement('option');o.value=x.id;o.textContent=x.name;b.append(o);});
  const c=document.createElement('option');c.value='custom';c.textContent='Eigene Belegung';b.append(c);
  b.value=cfg.boards.some(x=>x.id===cfg.board)?cfg.board:'custom';
  PIN_KEYS.forEach(k=>{const s=$('p_'+k);s.innerHTML='';cfg.validPins.forEach(p=>{const o=document.createElement('option');o.value=p;o.textContent='GPIO '+p;s.append(o);});s.value=cfg.pins[k];});
  $('order').innerHTML='';cfg.orders.forEach(o=>{const e=document.createElement('option');e.value=o;e.textContent=o;$('order').append(e);});$('order').value=cfg.order;
  $('m_host').value=cfg.mqtt.host;$('m_port').value=cfg.mqtt.port;$('m_user').value=cfg.mqtt.user;
}
$('board').addEventListener('change',()=>{const x=cfg.boards.find(b=>b.id===$('board').value);if(x)PIN_KEYS.forEach(k=>$('p_'+k).value=x.pins[k]);});
PIN_KEYS.forEach(k=>$('p_'+k).addEventListener('change',()=>{$('board').value='custom';}));
$('setBox').addEventListener('toggle',()=>{if($('setBox').open)loadCfg();});
document.querySelectorAll('[data-t]').forEach(b=>b.addEventListener('click',()=>fetch('/api/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ch:+b.dataset.t})})));
$('saveBtn').addEventListener('click',async()=>{
  const pins={};PIN_KEYS.forEach(k=>pins[k]=+$('p_'+k).value);
  const uniq=new Set(Object.values(pins));if(uniq.size<6){$('setErr').textContent='Ein Pin ist doppelt belegt.';return;}
  const body={mode:$('mode').value,board:$('board').value,pins,order:$('order').value,mqtt:{host:$('m_host').value.trim(),port:+$('m_port').value||1883,user:$('m_user').value.trim()}};
  if($('m_pass').value)body.mqtt.pass=$('m_pass').value;
  const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}).catch(()=>null);
  if(r&&!r.ok){const j=await r.json();$('setErr').textContent=j.error||'Speichern fehlgeschlagen';return;}
  $('setErr').textContent='';toast('Gespeichert. Trilumag startet neu …');setTimeout(()=>location.reload(),7000);
});
window.addEventListener('resize',()=>{if(!drag)render();});
poll();setInterval(poll,700);
</script></body></html>)HTML";
