#pragma once
// Web-App von Trilumag im Stil von WLED. Wird vom ESP32 unter "/" ausgeliefert.

const char INDEX_HTML[] = R"HTML(<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#111">
<title>Trilumag</title>
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 120 104'%3E%3Cpolygon points='4,100 60,4 116,100' fill='%2316203d' stroke='%236e8bff' stroke-width='6' stroke-linejoin='round'/%3E%3Cpolygon points='32,52 60,4 88,52' fill='%233d6dff' opacity='.85'/%3E%3Cpolygon points='4,100 32,52 60,100' fill='%23ff6b5c' opacity='.8'/%3E%3Cpolygon points='60,100 88,52 116,100' fill='%233fd69a' opacity='.8'/%3E%3C/svg%3E">
<style>
:root{--bg:#111;--card:#1c1c1c;--card2:#252525;--line:#333;--fg:#eee;--muted:#999;--acc:#6e8bff;--accfg:#111;color-scheme:dark}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
[hidden]{display:none!important}
html,body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
body{padding-bottom:calc(72px + env(safe-area-inset-bottom))}
button{font:inherit;color:inherit}
/* Kopfzeile */
.top{position:sticky;top:0;z-index:5;background:rgba(17,17,17,.94);backdrop-filter:blur(8px);border-bottom:1px solid var(--line);padding:10px 14px 8px}
.toprow{display:flex;align-items:center;gap:12px;max-width:720px;margin:0 auto}
.pwr{width:46px;height:46px;flex:none;border-radius:50%;border:2px solid var(--line);background:var(--card);display:grid;place-items:center;cursor:pointer;transition:box-shadow .2s,border-color .2s}
.pwr svg{width:22px;height:22px;stroke:var(--muted);stroke-width:2.4;fill:none;stroke-linecap:round}
.pwr.on{border-color:var(--acc);box-shadow:0 0 16px -2px var(--acc)}
.pwr.on svg{stroke:var(--acc)}
.bri{flex:1;display:flex;align-items:center;gap:10px}
.bri svg{width:20px;height:20px;flex:none;fill:var(--muted)}
.pct{flex:none;width:46px;text-align:right;font:13px ui-monospace,Menlo,monospace;color:var(--fg)}
.status{max-width:720px;margin:4px auto 0;font:11.5px ui-monospace,Menlo,monospace;color:var(--muted);display:flex;justify-content:space-between;gap:8px}
.status b{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:45%;color:var(--fg);font-weight:600;font-family:system-ui,sans-serif;letter-spacing:.02em}
/* Regler */
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:28px;background:transparent;margin:0}
input[type=range]::-webkit-slider-runnable-track{height:8px;border-radius:4px;background:linear-gradient(90deg,var(--acc) var(--p,50%),#3a3a3a var(--p,50%))}
input[type=range]::-moz-range-track{height:8px;border-radius:4px;background:linear-gradient(90deg,var(--acc) var(--p,50%),#3a3a3a var(--p,50%))}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:22px;border-radius:50%;background:#eee;border:0;margin-top:-7px;box-shadow:0 1px 4px rgba(0,0,0,.6)}
input[type=range]::-moz-range-thumb{width:22px;height:22px;border-radius:50%;background:#eee;border:0}
main{max-width:720px;margin:0 auto;padding:12px 14px;display:flex;flex-direction:column;gap:12px}
.tab{display:flex;flex-direction:column;gap:12px}
.card{background:var(--card);border-radius:14px;padding:14px;display:flex;flex-direction:column;gap:12px}
.card h3{margin:0;font-size:12px;font-weight:600;color:var(--muted);text-transform:uppercase;letter-spacing:.09em}
.card h3.fold{display:flex;align-items:center;justify-content:space-between;cursor:pointer;user-select:none;min-height:24px;margin:-4px 0}
.card h3.fold::after{content:"";width:8px;height:8px;border-right:2px solid var(--muted);border-bottom:2px solid var(--muted);transform:rotate(45deg);margin:0 4px 4px 0;transition:transform .15s}
.card.closed h3.fold::after{transform:rotate(-45deg);margin:4px 4px 0 0}
.card.closed>:not(h3){display:none!important}
#zbCard.na .wstat,#zbCard.na .tog,#zbCard.na>.note:not(#zbNa){opacity:.45}
#zbCard.na .tog{pointer-events:none}
.subf,.subb{display:flex;flex-direction:column;gap:12px}
.subf h3.fold{justify-content:flex-start}
.subf h3 small{margin-left:auto;margin-right:12px;text-transform:none;letter-spacing:0;font-weight:500;font-size:12px}
.subf.closed .subb{display:none}
.subf.closed h3.fold::after{transform:rotate(-45deg);margin:4px 4px 0 0}
.wstat{display:flex;gap:12px;align-items:center;background:#13261a;border:1px solid #2c5a3a;border-radius:12px;padding:12px}
.wstat.bad{background:#2b1a12;border-color:#5a3a2a}
.wstat .ic{width:30px;height:30px;border-radius:50%;background:#2fbf65;display:grid;place-items:center;flex:none;color:#08130c;font-weight:800}
.wstat.bad .ic{background:#e2894a}
.wstat b{display:block;font-size:15px}
.wstat span{font-size:13px;color:var(--muted)}
.sl{display:grid;grid-template-columns:96px 1fr 38px;align-items:center;gap:10px;font-size:14px;color:var(--muted)}
.sl output{text-align:right;font:12px ui-monospace,Menlo,monospace;color:var(--fg)}
.sl:has(#fdir){grid-template-columns:96px 1fr 52px}
.note{font-size:13px;color:var(--muted);margin:0}
.note:empty{display:none}
.banner{background:#2b2410;border:1px solid #5a4a1a;color:#f0d890;border-radius:12px;padding:12px;font-size:14px;display:flex;gap:10px;align-items:center;justify-content:space-between}
/* Wand */
.wall{background:#000;border-radius:14px;overflow:hidden;position:relative}
.wall svg{display:block;width:100%;touch-action:none;user-select:none;-webkit-user-select:none}
#mini{height:150px}
#big{height:min(48vh,440px)}
.hint{position:absolute;left:12px;bottom:8px;right:12px;font:11.5px ui-monospace,Menlo,monospace;color:var(--muted);pointer-events:none}
.tri{stroke:#000;stroke-width:1.6;transition:fill .12s linear;cursor:pointer}
.tri.main{stroke:#666;stroke-width:2}
.tri.sel{stroke:#fff;stroke-width:3}
.tri.tap{stroke:#fff;stroke-width:5;filter:brightness(1.6)}
.prow{display:flex;flex-direction:column;gap:6px;padding:12px 14px;border-bottom:1px solid var(--card)}
.prow:last-child{border-bottom:0}
.prow .top{display:flex;align-items:center;gap:10px;font-size:15px}
.prow .top span{white-space:nowrap}
.prow .top small{color:var(--muted);font-size:13px;flex:1}
.prow .top .btn{margin-left:auto}
.prow .prog{height:6px}
.tri.part{stroke-width:.8}
.selo{fill:none;stroke:#fff;stroke-width:3;pointer-events:none}
.dark{fill:#1a1a1a}
.pulse{animation:pl 1.6s ease-in-out infinite}
@keyframes pl{0%,100%{fill:#0f2350}50%{fill:#3d6dff}}
.ghost{fill:none;stroke:#444;stroke-width:1.2;stroke-dasharray:4 4}
.ghost.hot{stroke:var(--acc);stroke-width:2.2;fill:rgba(110,139,255,.15)}
.lbl{font:9px ui-monospace,Menlo,monospace;fill:rgba(255,255,255,.6);pointer-events:none;text-anchor:middle}
.lbl.dk{fill:rgba(0,0,0,.6)}
.edge1{fill:rgba(255,255,255,.75);pointer-events:none}
/* Auswahl-Chips */
.chips{display:flex;gap:8px;flex-wrap:wrap}
#fdiro{width:64px;white-space:nowrap}
#fspin .note{width:100%}
.moon{width:40px;height:40px;flex:none;border-radius:50%;border:2px solid var(--line);background:var(--card);display:grid;place-items:center;cursor:pointer;position:relative;padding:0}
.moon svg{width:18px;height:18px;fill:none;stroke:var(--muted);stroke-width:2;stroke-linejoin:round}
.moon.on{border-color:var(--acc)}
.moon.on svg{stroke:var(--acc);fill:var(--acc)}
.moon small{position:absolute;bottom:-8px;left:50%;transform:translateX(-50%);background:var(--acc);color:#111;font:600 10px/1 system-ui,sans-serif;padding:2px 4px;border-radius:6px;white-space:nowrap}
.pop{position:absolute;right:14px;top:62px;z-index:8;background:#232323;border:1px solid var(--line);border-radius:14px;padding:12px;display:flex;flex-direction:column;gap:10px;box-shadow:0 10px 30px rgba(0,0,0,.5);max-width:300px}
.pop b{font-size:14px}
.sw8{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.sw8 button{width:36px;height:36px;border-radius:50%;border:2px solid var(--line);cursor:pointer;padding:0;position:relative}
.sw8 button.on{border-color:#fff;box-shadow:0 0 0 2px var(--acc)}
.sw8 .off{background:#141414;color:var(--muted);font-size:18px}
.sw8 label{width:36px;height:36px;border-radius:50%;border:2px dashed var(--line);display:grid;place-items:center;cursor:pointer;overflow:hidden;position:relative;color:var(--muted)}
.sw8 label input{position:absolute;inset:0;opacity:0;cursor:pointer}
.wall.paint svg{cursor:crosshair}
.tri.find{animation:find .33s steps(2) infinite}
@keyframes find{50%{stroke:#fff;stroke-width:6}}
.estats{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.estats div{background:var(--card2);border-radius:10px;padding:8px;display:flex;flex-direction:column;min-width:0}
.estats b{font-size:15px;white-space:nowrap}
.estats span{font-size:11.5px;color:var(--muted)}
@media(max-width:420px){.estats{grid-template-columns:repeat(2,1fr)}}
#eChart{width:100%;height:150px;display:block}
#eChart .bar{fill:var(--acc)}
#eChart .bar.cur{fill:#9fb3ff}
#eChart text{fill:var(--muted);font:10px ui-monospace,Menlo,monospace}
#eChart line{stroke:#333;stroke-width:1}
#eTip{font:12.5px ui-monospace,Menlo,monospace;color:var(--fg);min-height:18px}
.chip{border:1px solid var(--line);background:var(--card2);border-radius:999px;padding:8px 14px;font-size:14px;cursor:pointer}
.chip.on{background:var(--acc);border-color:var(--acc);color:var(--accfg);font-weight:600}
/* Farbrad */
.wheelbox{display:flex;justify-content:center}
.wheel{position:relative;width:min(300px,78vw);aspect-ratio:1;touch-action:none}
.wheel canvas{width:100%;height:100%;border-radius:50%;display:block}
.knob{position:absolute;width:26px;height:26px;margin:-13px 0 0 -13px;border-radius:50%;border:3px solid #fff;box-shadow:0 0 0 1px rgba(0,0,0,.5),0 2px 6px rgba(0,0,0,.6);pointer-events:none}
.quick{display:grid;grid-template-columns:repeat(6,1fr);gap:8px}
.qc{aspect-ratio:1;border-radius:50%;border:2px solid rgba(255,255,255,.12);cursor:pointer;padding:0}
.qc.txt{border-radius:10px;aspect-ratio:auto;height:36px;font-size:12px;color:#111;font-weight:600;grid-column:span 3}
.hexrow{display:flex;gap:8px;align-items:center}
input[type=text],input[type=password],input:not([type]),select{width:100%;height:42px;border:1px solid var(--line);border-radius:10px;background:#0c0c0c;color:var(--fg);padding:0 12px;font:15px system-ui,sans-serif}
#hex{font-family:ui-monospace,Menlo,monospace;text-transform:uppercase;max-width:130px}
/* Listen */
.list{display:flex;flex-direction:column;border-radius:12px;overflow:hidden;background:var(--card2)}
.row{display:flex;align-items:center;gap:12px;padding:12px 14px;border:0;border-bottom:1px solid var(--card);cursor:pointer;font-size:15px;background:none;text-align:left;width:100%}
.row:last-child{border-bottom:0}
.row .dot{width:18px;height:18px;border-radius:50%;border:2px solid #555;flex:none}
.row.on{background:#2f2f2f}
.row.on .dot{border-color:var(--acc);background:radial-gradient(circle,var(--acc) 0 45%,transparent 50%)}
.row .grad{height:16px;border-radius:8px;flex:1;max-width:46%;margin-left:auto}
.row small{color:var(--muted);margin-left:auto;font-size:12px}
.row .nm{flex:1;min-width:0;display:flex;flex-direction:column;gap:1px}.row .nm small{margin:0;font-size:11px}
.row .star{flex:none;font-size:20px;line-height:1;color:#666;padding:2px 4px;margin:-4px -6px -4px 0}.row .star.on{color:var(--acc)}
.row svg.pv{width:58px;height:17px;flex:none;border-radius:4px;background:#0c0c0c}
.banner .bb{display:flex;gap:8px;flex:none}
.erow input{width:20px;height:20px;margin:0;accent-color:var(--acc);flex:none;cursor:pointer}
/* Presets */
.pgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:10px}
.pgrid:empty{display:none}
.preset{position:relative;background:var(--card2);border:1px solid var(--line);border-radius:12px;padding:16px 40px 16px 12px;min-height:64px;text-align:left;cursor:pointer;font-size:15px;font-weight:550;overflow-wrap:anywhere}
.preset.on{border-color:var(--acc);box-shadow:inset 0 0 0 1px var(--acc)}
.preset .del{position:absolute;top:6px;right:6px;min-width:30px;height:30px;border-radius:999px;border:0;background:transparent;color:var(--muted);font-size:17px;cursor:pointer}
.preset .del.ask{padding:0 10px;background:#5a1d1d;color:#ffb4b4;font-size:12px}
a.btn{text-decoration:none;color:var(--fg);display:inline-block}
.btn{border:1px solid var(--line);background:var(--card2);border-radius:999px;padding:10px 16px;cursor:pointer;font-size:14px;white-space:nowrap}
.btn.pri{background:var(--acc);border-color:var(--acc);color:var(--accfg);font-weight:600}
.btn:disabled{opacity:.4;cursor:default}
.btnrow{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.tray{display:flex;flex-wrap:wrap;gap:10px;align-items:center;min-height:56px}
.item{width:56px;height:52px;touch-action:none;cursor:grab}
.item svg{width:56px;height:52px;display:block}
label.f{display:flex;flex-direction:column;gap:6px;font-size:13px;color:var(--muted)}
.pins{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px}
.tog{display:flex;align-items:center;gap:12px;font-size:15px;cursor:pointer;user-select:none}
.tog input{position:absolute;opacity:0;width:0;height:0}
.tog .sw{width:46px;height:28px;border-radius:14px;background:#3a3a3a;position:relative;flex:none;transition:background .15s}
.tog .sw::after{content:"";position:absolute;left:3px;top:3px;width:22px;height:22px;border-radius:50%;background:#eee;transition:transform .15s}
.tog input:checked+.sw{background:var(--acc)}
.tog input:checked+.sw::after{transform:translateX(18px)}
.tog input:focus-visible+.sw{outline:2px solid var(--acc);outline-offset:2px}
.pins.off,#tBox.off,label.f.off{opacity:.45}
.nets{display:flex;flex-direction:column;border-radius:12px;overflow:hidden;background:var(--card2)}
.nets:empty{display:none}
.nets .row{gap:10px}
.bars{display:flex;align-items:flex-end;gap:2px;height:14px;flex:none}
.bars i{width:4px;background:#555;border-radius:1px}
.bars i.on{background:var(--fg)}
.card h3.sub{font-size:11px;margin-top:6px}
.dtab{overflow-x:auto}
.dtab table{width:100%;border-collapse:collapse;font:13px ui-monospace,Menlo,monospace}
.dtab th{text-align:left;color:var(--muted);font-weight:500;padding:4px 6px;border-bottom:1px solid var(--line)}
.dtab td{padding:5px 6px;border-bottom:1px solid #222}
.dtab td.warn{color:#f0b070}.dtab td.bad{color:#ff8a80}
.dlog{display:flex;flex-direction:column;gap:2px;max-height:220px;overflow-y:auto;font-size:13px}
.dlog div{display:grid;grid-template-columns:62px 1fr;gap:8px;padding:3px 0;border-bottom:1px solid #222}
.dlog span{color:var(--muted);font:12px ui-monospace,Menlo,monospace}
.prog{height:8px;border-radius:4px;background:#333;overflow:hidden}
.prog i{display:block;height:100%;width:0;background:var(--acc);transition:width .3s}
.row .tag{font-size:11px;padding:2px 8px;border-radius:999px;background:#333;color:var(--fg);flex:none}
.row .tag.new{background:var(--acc);color:var(--accfg)}
.row .vinfo{display:flex;flex-direction:column;gap:2px;min-width:0;flex:1}
.row .vinfo span{font-size:12px;color:var(--muted);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.row .ib{border:1px solid var(--line);background:#1c1c1c;border-radius:999px;padding:6px 12px;font-size:13px;cursor:pointer;flex:none}
.row .ib.ask{background:#5a1d1d;border-color:#5a1d1d;color:#ffb4b4}
.vbox{border-bottom:1px solid var(--card)}
.vbox:last-child{border-bottom:0}
.vbox .row{border-bottom:0}
.clog{padding:0 14px 14px;font-size:14px;color:#ccc}
.clog h4{margin:10px 0 4px;font-size:13px;color:var(--fg)}
.clog ul{margin:0;padding-left:18px}
.clog li{margin:3px 0}
.clog p{margin:6px 0}
.lnk{background:none;border:0;color:var(--acc);font-size:12px;padding:0;cursor:pointer;text-align:left;width:max-content}
details.more summary{cursor:pointer;color:var(--muted);font-size:14px}
details.more[open] summary{margin-bottom:10px}
code{font:12px ui-monospace,Menlo,monospace;background:#0c0c0c;padding:1px 5px;border-radius:4px}
.badge{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--acc);margin-left:4px;vertical-align:top}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 14px;font-size:14px}
.kv span:nth-child(odd){color:var(--muted)}
/* Tabs unten */
nav.tabs{position:fixed;left:0;right:0;bottom:0;z-index:6;background:rgba(24,24,24,.97);backdrop-filter:blur(8px);border-top:1px solid var(--line);display:flex;justify-content:center;padding-bottom:env(safe-area-inset-bottom)}
nav.tabs button{flex:1;max-width:144px;background:none;border:0;padding:9px 2px 8px;display:flex;flex-direction:column;align-items:center;gap:3px;font-size:11px;color:var(--muted);cursor:pointer}
nav.tabs svg{width:24px;height:24px;stroke:currentColor;stroke-width:1.9;fill:none;stroke-linecap:round;stroke-linejoin:round}
nav.tabs button.on{color:var(--acc)}
.toast{position:fixed;left:50%;bottom:calc(84px + env(safe-area-inset-bottom));transform:translateX(-50%);background:#2a2a2a;border:1px solid var(--line);padding:10px 14px;border-radius:10px;font-size:14px;opacity:0;transition:opacity .2s;pointer-events:none;z-index:9;max-width:90vw}
.toast.show{opacity:1}
</style></head><body>
<header class="top">
  <div class="toprow">
    <button class="pwr" id="pwr" aria-label="Wand ein/aus"><svg viewBox="0 0 24 24"><path d="M12 3v8"/><path d="M6.3 6.8a8 8 0 1 0 11.4 0"/></svg></button>
    <div class="bri"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="4.2"/><g stroke="#999" stroke-width="2" stroke-linecap="round"><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/></g></svg>
      <input type="range" id="master" min="1" max="100" value="100" aria-label="Gesamthelligkeit in Prozent"><output id="mastero" class="pct">100 %</output></div>
    <button class="moon" id="sleepBtn" aria-label="Sleep-Timer"><svg viewBox="0 0 24 24"><path d="M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z"/></svg><small id="sleepLeft" hidden></small></button>
  </div>
  <div class="pop" id="sleepPop" hidden>
    <b>Sleep-Timer</b><span class="note" id="sleepInfo">Die Wand blendet langsam aus und geht dann aus.</span>
    <div class="chips" id="sleepChips"><button class="chip" data-m="15">15 min</button><button class="chip" data-m="30">30 min</button><button class="chip" data-m="60">1 h</button><button class="chip" data-m="90">1,5 h</button><button class="chip" data-m="120">2 h</button></div>
    <button class="btn" id="sleepOff" hidden>Timer beenden</button>
  </div>
  <div class="status"><b id="wallName" data-nt>Trilumag</b><span id="status">verbinde …</span></div>
</header>

<main>
  <div class="banner" id="apBanner" hidden><span>Noch kein WLAN eingerichtet.</span><button class="btn pri" data-go="opt">Einrichten</button></div>
  <div class="banner" id="swapBanner" hidden><span id="swapT"></span><span class="bb"><button class="btn" id="swapNo">Nein</button><button class="btn pri" id="swapYes">Übernehmen</button></span></div>
  <div class="wall" id="miniBox"><svg id="mini" aria-label="Vorschau der Wand"></svg><div class="hint" id="miniHint"></div></div>

  <!-- Farben -->
  <section id="t-col" class="tab">
    <div class="card" id="colCard">
      <h3>Farbe</h3>
      <div class="chips"><button class="chip" id="tgtAll">Ganze Wand</button><button class="chip" id="tgtSel">Auswahl</button></div>
      <div class="chips" id="c12" hidden><button class="chip on" data-k="1">Farbe 1</button><button class="chip" data-k="2">Farbe 2</button></div>
      <p class="note" id="colNote"></p>
      <div class="wheelbox"><div class="wheel" id="wheel"><canvas id="wcv"></canvas><div class="knob" id="knob"></div></div></div>
      <div class="sl">Helligkeit<input type="range" id="cv" min="0" max="100" value="100"><output id="cvo">100 %</output></div>
      <div class="sl">Weißanteil<input type="range" id="cw" min="0" max="255" value="0"><output id="cwo">0</output></div>
      <div class="quick" id="quick"></div>
      <div class="hexrow"><input id="hex" maxlength="7" value="#FF7800" aria-label="Farbe als Hex"><button class="btn" id="rnd">Zufall</button></div>
    </div>
  </section>

  <!-- Effekte -->
  <section id="t-fx" class="tab" hidden>
    <div class="card" id="fxSet">
      <h3>Tempo und Intensität</h3>
      <div class="sl">Tempo<input type="range" id="fspeed" min="1" max="100" value="50"><output id="fspeedo">50</output></div>
      <div class="sl">Intensität<input type="range" id="finten" min="0" max="255" value="128"><output id="finteno">128</output></div>
      <div class="sl">Richtung<input type="range" id="fdir" min="0" max="345" step="15" value="0"><output id="fdiro">0°</output></div>
      <div class="chips" id="fspin"><span class="note">Richtung dreht sich:</span><button class="chip on" data-s="0">nein</button><button class="chip" data-s="1">langsam</button><button class="chip" data-s="2">schnell</button></div>
      <p class="note" id="fxNote"></p>
    </div>
    <div class="card">
      <h3>Effekte</h3>
      <input id="fxq" placeholder="Effekt suchen" autocomplete="off">
      <div class="list" id="fxList"></div>
    </div>
    <div class="card">
      <h3>Paletten</h3>
      <div class="list" id="palList"></div>
    </div>
  </section>

  <!-- Wand -->
  <section id="t-wall" class="tab" hidden>
    <div class="card" id="trayCard">
      <h3>Ablage · abgeklipste Panels</h3>
      <div class="tray" id="tray"></div>
      <div class="btnrow"><button class="btn" id="newBtn">Neues Panel</button></div>
    </div>
    <div class="card" id="paintCard">
      <h3>Malen</h3>
      <label class="tog"><input type="checkbox" id="paintOn"><span class="sw"></span>Mit dem Finger über die Panels wischen</label>
      <div class="sw8" id="swatches"></div>
    </div>
    <div class="wall" id="bigBox"><svg id="big" aria-label="Wand mit Panels"></svg><div class="hint" id="bigHint"></div></div>
    <div class="btnrow" id="viewRow"><span class="note">Ansicht wie an der Wand:</span><button class="btn" id="viewL" aria-label="nach links drehen">↺</button><button class="btn" id="viewR" aria-label="nach rechts drehen">↻</button><button class="btn" id="viewMir">Spiegeln</button><span class="note" id="viewT"></span></div>
    <div class="card" id="panelCard">
      <h3 id="pTitle">Panel</h3>
      <div class="kv" id="pInfo"></div>
      <div class="sl">Helligkeit<input type="range" id="pbri" min="1" max="100" value="71"><output id="pbrio">71 %</output></div>
      <div class="btnrow"><button class="btn" id="pOn">Ein / Aus</button><button class="btn pri" id="pCol">Farbe wählen</button><button class="btn" id="pFind">Finden</button></div>
      <div class="btnrow" id="pTapRow"><span class="note">Antippen ausprobieren:</span><button class="btn" id="pTap1">einmal</button><button class="btn" id="pTap2">doppelt</button></div>
      <label class="tog"><input type="checkbox" id="pEdges"><span class="sw"></span>Kanten einzeln</label>
      <p class="note">Effekte bekommen dann drei Farben pro Panel, eine je Kante. Feste Farben bleiben pro Panel.</p>
    </div>
    <div class="card" id="edgeCard">
      <h3>Kanten einzeln</h3>
      <p class="note">Angehakte Panels zeigen bei Effekten drei Farben, eine je Kante.</p>
      <div class="btnrow"><button class="btn" id="eAllOn">Alle an</button><button class="btn" id="eAllOff">Alle aus</button><span class="note" id="eCount"></span></div>
      <div class="list" id="eList"></div>
    </div>
  </section>

  <!-- Presets -->
  <section id="t-pre" class="tab" hidden>
    <div class="card">
      <h3>Presets</h3>
      <div class="pgrid" id="pgrid"></div>
      <p class="note" id="preEmpty">Noch keine Presets. Stell eine Szene ein und speichere sie unten.</p>
    </div>
    <div class="card">
      <h3>Aktuelle Szene speichern</h3>
      <div class="hexrow"><input id="pname" maxlength="24" placeholder="Name, z. B. Abend"><button class="btn pri" id="psave">Speichern</button></div>
      <p class="note">Ein Preset merkt sich Ein/Aus, Gesamthelligkeit, Effekt mit Tempo, Intensität und Palette und die Farbe jedes Panels. In Home Assistant erscheinen die Presets als Auswahl „Preset“. Gleicher Name überschreibt.</p>
    </div>
  </section>

  <!-- Optionen -->
  <section id="t-opt" class="tab" hidden>
    <div class="card" id="nameCard">
      <h3>Name</h3>
      <label class="f">Name der Wand<input id="wname" maxlength="32" placeholder="z. B. Wohnzimmer" autocomplete="off"></label>
      <p class="note">Steht oben in der App, in Home Assistant und in WLED-Programmen. Die Adresse bleibt trilumag.local.</p>
      <label class="f">Sprache<select id="lang"><option value="">Automatisch</option><option value="de">Deutsch</option><option value="en">English</option></select></label>
    </div>
    <div class="card" id="wifiCard">
      <h3>WLAN</h3>
      <div class="wstat" id="wifiStat"><div class="ic" id="wifiIc">✓</div><div><b id="wifiT"></b><span id="wifiS"></span></div></div>
      <p class="note" id="wifiInfo"></p>
      <div class="nets" id="netList"></div>
      <label class="f">WLAN<input id="ssid" autocomplete="off" placeholder="Name deines WLANs"></label>
      <label class="f">Passwort<input id="pass" type="password" autocomplete="off"></label>
      <div class="btnrow"><button class="btn" id="scanBtn">Netze suchen</button><button class="btn pri" id="wifiBtn">Speichern und verbinden</button></div>
      <label class="tog"><input type="checkbox" id="guardOn"><span class="sw"></span>WLAN-Wächter</label>
      <p class="note">Ist das WLAN 3 Minuten weg, verbindet sich Trilumag neu, nach 10 Minuten startet es neu (nicht, solange jemand im Setup-Netz ist). Hängt die Software eine Minute, startet es ebenfalls neu. trilumag.local wird alle 30 Minuten neu angekündigt.</p>
    </div>
    <div class="card" id="lightCard">
      <h3>Licht</h3>
      <div class="sl">Übergänge<input type="range" id="ltrans" min="0" max="50" value="7"><output id="ltranso">0,7 s</output></div>
      <p class="note">So lange blenden Farb-, Preset-, Effekt- und Ein/Aus-Wechsel weich über. 0 = sofort.</p>
      <label class="f">Einschalt-Animation<select id="onAnim"><option value="0">aus</option><option value="1">langsam</option><option value="2">mittel</option><option value="3">schnell</option></select></label>
      <p class="note">Beim Einschalten leuchten die Panels der Reihe nach auf, vom Hauptpanel nach außen.</p>
      <div class="pins">
        <label class="f">Nach Stromausfall<select id="bootMode"><option value="0">wie vorher</option><option value="1">aus</option><option value="2">an</option><option value="3">Preset …</option></select></label>
        <label class="f" id="bootPreBox" hidden>Preset<select id="bootPre"></select></label>
      </div>
      <p class="note">Gilt nur, wenn der Strom weg war. Nach Updates und Neustarts aus der App bleibt alles, wie es war.</p>
    </div>
    <div class="card" id="powerCard">
      <h3>Stromlimit</h3>
      <div class="wstat" id="pwrStat"><div class="ic" id="pwrIc">A</div><div><b id="pwrT">–</b><span id="pwrS"></span></div></div>
      <label class="tog"><input type="checkbox" id="lpwrOn"><span class="sw"></span>Stromlimit</label>
      <div class="pins" id="lpwrBox">
        <label class="f">Netzteil liefert höchstens (A)<input id="lpwrMax" inputmode="decimal" placeholder="z. B. 6"></label>
        <label class="f">mA pro Farbkanal und Segment<input id="lpwrCh" inputmode="numeric" placeholder="12"></label>
      </div>
      <p class="note">Wird es mehr, dimmt Trilumag alle Panels gleichmäßig. Ohne Sensor wird der Strom aus den Farben geschätzt.</p>
      <div class="subf closed" id="sensFold">
        <h3 class="sub fold" id="sensHead" tabindex="0" role="button" aria-expanded="false">Stromsensor INA226<small id="sensBadge"></small></h3>
        <div class="subb">
          <div class="pins">
            <label class="f">SDA<select id="p_sda"></select></label>
            <label class="f">SCL<select id="p_scl"></select></label>
            <label class="f">Shunt (mΩ)<input id="p_shunt" inputmode="decimal" placeholder="5"></label>
          </div>
          <p class="note" id="sensInfo"></p>
        </div>
      </div>
    </div>
    <div class="card" id="energyCard">
      <h3>Energie</h3>
      <div class="estats"><div><b id="eToday">–</b><span>heute</span></div><div><b id="eMonth">–</b><span>diesen Monat</span></div><div><b id="eYear">–</b><span>dieses Jahr</span></div><div><b id="eTotal">–</b><span>gesamt</span></div></div>
      <div class="chips" id="eTabs"><button class="chip on" data-r="days">Tage</button><button class="chip" data-r="months">Monate</button><button class="chip" data-r="years">Jahre</button></div>
      <svg id="eChart" viewBox="0 0 340 150" role="img" aria-label="Energieverbrauch"></svg>
      <div id="eTip"></div>
      <p class="note" id="eNote"></p>
      <div class="btnrow"><button class="btn" id="eReset">Zähler zurücksetzen</button></div>
    </div>
    <div class="card" id="sigCard">
      <h3>Signale und Fortschritt</h3>
      <div class="chips" id="sigCols"><button class="chip on" data-c="blau">Blau</button><button class="chip" data-c="grün">Grün</button><button class="chip" data-c="rot">Rot</button><button class="chip" data-c="gelb">Gelb</button><button class="chip" data-c="weiß">Weiß</button></div>
      <div class="btnrow"><button class="btn" id="sigTest">Signal testen (3-mal blinken)</button></div>
      <div class="sl">Fortschritt<input type="range" id="progR" min="0" max="100" value="0"><output id="progRo">aus</output></div>
      <p class="note">Gedacht für Home Assistant: Das Gerät „Signal“ lässt die Wand blinken, etwa mit der Nachricht „blau 3“ für die Türklingel. Danach läuft alles weiter wie vorher. „Fortschritt“ füllt die Wand vom Hauptpanel aus, 0 % schaltet es wieder aus. Hier kannst du beides ausprobieren.</p>
    </div>
    <div class="card" id="touchCard">
      <h3>Antippen</h3>
      <label class="tog"><input type="checkbox" id="tOn"><span class="sw"></span>Panels reagieren auf Antippen</label>
      <div id="tBox">
        <label class="tog"><input type="checkbox" id="tWave"><span class="sw"></span>Welle über die Wand beim Antippen</label>
        <div class="sl">Empfindlichkeit<input type="range" id="tSens" min="1" max="10" value="5"><output id="tSenso">5</output></div>
        <div class="pins">
          <label class="f">Einmal antippen<select id="tA1"></select></label>
          <label class="f">Doppelt antippen<select id="tA2"></select></label>
        </div>
      </div>
      <p class="note" id="tNote"></p>
    </div>
    <div class="card">
      <h3>Hardware</h3>
      <label class="f">Betriebsart<select id="mode"><option value="sim">Simulation: Panels in der App anklipsen</option><option value="bus">Bus: echte Panels über RS-485</option></select></label>
      <label class="f">Board<select id="board"></select></label>
      <div class="pins">
        <label class="f">RS-485 RX (RO)<select id="p_rx"></select></label>
        <label class="f">RS-485 TX (DI)<select id="p_tx"></select></label>
        <label class="f">RS-485 DE + /RE<select id="p_de"></select></label>
        <label class="f">LED-Daten<select id="p_led"></select></label>
        <label class="f">SNS rechts (Kante 2)<select id="p_snsR"></select></label>
        <label class="f">SNS links (Kante 3)<select id="p_snsL"></select></label>
      </div>
      <label class="f">Farbreihenfolge der LEDs<select id="order"></select></label>

      <div class="btnrow"><span class="note">Farbtest:</span><button class="btn" data-t="0">Rot</button><button class="btn" data-t="1">Grün</button><button class="btn" data-t="2">Blau</button><button class="btn" data-t="3">Weiß</button><button class="btn" data-t="-1">Ende</button></div>
      <p class="note">Leuchtet bei „Rot“ etwas anderes als Rot, stimmt die Reihenfolge nicht. Dann eine andere wählen und speichern.</p>
    </div>
    <div class="card" id="otaCard">
      <h3>Updates</h3>
      <div class="wstat" id="otaStat"><div class="ic" id="otaIc">✓</div><div><b id="otaT">Version …</b><span id="otaS"></span></div></div>
      <div class="prog" id="otaProg" hidden><i id="otaBar"></i></div>
      <label class="tog"><input type="checkbox" id="otaAuto"><span class="sw"></span>Automatisch aktualisieren</label>
      <p class="note">Ist das an, installiert das Hauptpanel neue Versionen von selbst (es sieht alle 6 Stunden nach). Sonst zeigt die App sie hier an, und du entscheidest.</p>
      <div class="btnrow"><button class="btn" id="otaCheck">Nach Updates suchen</button></div>
      <div class="list" id="otaList"></div>
      <details class="more"><summary>Firmware-Datei hochladen</summary>
        <p class="note">Für eigene Builds: die Datei <code>trilumag.ino.bin</code> bzw. <code>app.bin</code> für deinen Chip.</p>
        <div class="btnrow"><input type="file" id="otaFile" accept=".bin"><button class="btn" id="otaUp">Hochladen</button></div>
      </details>
    </div>
    <div class="card" id="pfwCard">
      <h3>Panel-Firmware</h3>
      <div class="wstat" id="pfwStat"><div class="ic" id="pfwIc">✓</div><div><b id="pfwT">–</b><span id="pfwS"></span></div></div>
      <label class="tog"><input type="checkbox" id="pfwAuto"><span class="sw"></span>Panels automatisch aktualisieren</label>
      <p class="note">Das Hauptpanel bringt die passende Firmware für die Panels mit und spielt sie über den Bus auf. Die Wand leuchtet dabei weiter, nur das Panel, das gerade dran ist, bleibt kurz stehen.</p>
      <div class="btnrow"><button class="btn" id="pfwAll">Alle aktualisieren</button></div>
      <div class="list" id="pfwList"></div>
    </div>
    <div class="card">
      <h3>Home Assistant (MQTT)</h3>
      <label class="tog"><input type="checkbox" id="m_on"><span class="sw"></span>MQTT aktiv</label>
      <div class="pins" id="mqttFields">
        <label class="f">Broker-Adresse<input id="m_host" placeholder="z. B. 192.168.1.10"></label>
        <label class="f">Port<input id="m_port" inputmode="numeric" value="1883"></label>
        <label class="f">Benutzer<input id="m_user" autocomplete="off"></label>
        <label class="f">Passwort<input id="m_pass" type="password" autocomplete="off" placeholder="unverändert"></label>
      </div>
      <div class="btnrow"><button class="btn pri" id="saveBtn">Speichern und neu starten</button></div>
      <p class="note" id="setErr"></p>
    </div>
    <div class="card" id="zbCard" hidden>
      <h3>Philips Hue (Zigbee)</h3>
      <div class="wstat" id="zbStat"><div class="ic" id="zbIc">✓</div><div><b id="zbT">–</b><span id="zbS"></span></div></div>
      <label class="tog"><input type="checkbox" id="zbOn"><span class="sw"></span>Mit der Hue Bridge verbinden</label>
      <p class="note">Trilumag erscheint in der Hue-App als Farblampe „Trilumag Wand“. Hue steuert Ein/Aus, Helligkeit und Farbe der ganzen Wand, auch in Szenen, Routinen und mit Hue-Schaltern. Effekte und einzelne Panels bleiben hier in der App. Ein- und Ausschalten startet das Hauptpanel neu.</p>
      <p class="note" id="zbNa" hidden>Zigbee braucht einen eigenen Funkteil, den nur der ESP32-C6 hat. Mit einem C6 als Hauptpanel lässt sich das hier einschalten.</p>
      <p class="note" id="zbHow">Koppeln: In der Hue-App unter Einstellungen → Lampen → „+“ → Suchen. Trilumag muss dabei laufen und nah genug an der Bridge oder einer Hue-Lampe sein.</p>
      <div class="btnrow"><button class="btn" id="zbPair">Neu koppeln</button></div>
    </div>
    <div class="card" id="syncCard">
      <h3>Mehrere Wände</h3>
      <label class="tog"><input type="checkbox" id="syncOn"><span class="sw"></span>Mit anderen Wänden im Gleichtakt</label>
      <label class="f">Gruppe<select id="syncGrp"></select></label>
      <div class="list" id="syncList"></div>
      <p class="note">Alle Wände derselben Gruppe im selben WLAN teilen Ein/Aus, Helligkeit und Effekt mit allen Einstellungen, egal an welcher Wand du etwas änderst. Die Effekte laufen im selben Takt. Feste Farben einzelner Panels bleiben pro Wand.</p>
    </div>
    <div class="card" id="diagCard">
      <h3>Bus-Diagnose</h3>
      <p class="note" id="diagSum">–</p>
      <div class="dtab"><table><thead><tr><th>Panel</th><th>Adr.</th><th title="Antwortzeit">Antw.</th><th title="verpasste Antworten">verp.</th><th>FW</th><th title="wie oft angeklipst">Angekl.</th><th title="Leuchtdauer in Stunden">Std.</th></tr></thead><tbody id="diagRows"></tbody></table></div>
      <div class="subf closed" id="logFold">
        <h3 class="sub fold" tabindex="0" role="button" aria-expanded="false">Ereignisse<small id="logBadge"></small></h3>
        <div class="subb"><div class="dlog" id="diagLog"></div></div>
      </div>
      <div class="btnrow"><button class="btn" id="diagReset">Zähler zurücksetzen</button></div>
    </div>
    <div class="card" id="backupCard">
      <h3>Sicherung</h3>
      <p class="note">Einstellungen, Presets, Farben und die simulierte Wand als Datei. Die WLAN-Zugangsdaten sind nicht dabei. Nach dem Einspielen startet Trilumag neu.</p>
      <div class="btnrow"><a class="btn" id="bkDown" href="/api/backup" download>Sicherung herunterladen</a></div>
      <div class="btnrow"><input type="file" id="bkFile" accept=".json,application/json"><button class="btn" id="bkUp">Einspielen</button></div>
    </div>
    <div class="card"><h3>Info</h3><div class="kv" id="info"></div></div>
  </section>
</main>

<nav class="tabs" id="tabs">
  <button data-tab="col" class="on"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><circle cx="8.5" cy="9" r="1.4"/><circle cx="13" cy="7.5" r="1.4"/><circle cx="16.5" cy="11" r="1.4"/><path d="M12 21a3 3 0 0 1 0-6h1.5a2.5 2.5 0 0 0 0-5"/></svg>Farben</button>
  <button data-tab="fx"><svg viewBox="0 0 24 24"><path d="M12 3l1.8 4.6L18.5 9l-4.7 1.6L12 15l-1.8-4.4L5.5 9l4.7-1.4z"/><path d="M18 15l.8 2 2 .8-2 .8-.8 2-.8-2-2-.8 2-.8z"/></svg>Effekte</button>
  <button data-tab="wall"><svg viewBox="0 0 24 24"><path d="M3 19h9L7.5 11z"/><path d="M12 19l4.5-8H7.5"/><path d="M12 19h9l-4.5-8"/></svg>Wand</button>
  <button data-tab="pre"><svg viewBox="0 0 24 24"><path d="M6 3h12v18l-6-4-6 4z"/></svg>Presets</button>
  <button data-tab="opt"><svg viewBox="0 0 24 24"><path d="M4 6h10M18 6h2M4 12h4M12 12h8M4 18h12M20 18h0"/><circle cx="16" cy="6" r="2"/><circle cx="10" cy="12" r="2"/><circle cx="18" cy="18" r="2"/></svg><span>Optionen<i class="badge" id="updBadge" hidden></i></span></button>
</nav>
<div class="toast" id="toast"></div>

<script>
// ---------- Sprache: Deutsch oder Englisch (automatisch nach Browser, umschaltbar) ----------
let LANG='';try{LANG=localStorage.getItem('lang')||'';}catch(e){}
if(LANG!=='de'&&LANG!=='en')LANG=/^de/i.test(navigator.language||'de')?'de':'en';
const LOC=LANG==='de'?'de-AT':'en-GB';document.documentElement.lang=LANG;
/*EN*/const EN={"Wand ein/aus":"Wall on/off","Gesamthelligkeit in Prozent":"Overall brightness in percent","Sleep-Timer":"Sleep timer","Die Wand blendet langsam aus und geht dann aus.":"The wall slowly fades out and then turns off.","Timer beenden":"Stop timer","verbinde …":"connecting …","Noch kein WLAN eingerichtet.":"No Wi-Fi set up yet.","Einrichten":"Set up","Nein":"No","Übernehmen":"Apply","Vorschau der Wand":"Wall preview","Farbe":"Color","Ganze Wand":"Whole wall","Auswahl":"Selection","Farbe 1":"Color 1","Farbe 2":"Color 2","Helligkeit":"Brightness","Weißanteil":"White","Farbe als Hex":"Color as hex","Zufall":"Random","Tempo und Intensität":"Speed and intensity","Tempo":"Speed","Intensität":"Intensity","Richtung":"Direction","Richtung dreht sich:":"Direction rotates:","nein":"no","langsam":"slow","schnell":"fast","Effekte":"Effects","Effekt suchen":"Search effects","Paletten":"Palettes","Ablage · abgeklipste Panels":"Tray · unclipped panels","Neues Panel":"New panel","Malen":"Paint","Mit dem Finger über die Panels wischen":"Swipe over the panels with your finger","Wand mit Panels":"Wall with panels","Ansicht wie an der Wand:":"View as on the wall:","nach links drehen":"rotate left","nach rechts drehen":"rotate right","Spiegeln":"Mirror","Ein / Aus":"On / Off","Farbe wählen":"Pick color","Finden":"Find","Antippen ausprobieren:":"Try tapping:","einmal":"once","doppelt":"twice","Kanten einzeln":"Separate edges","Effekte bekommen dann drei Farben pro Panel, eine je Kante. Feste Farben bleiben pro Panel.":"Effects then get three colors per panel, one per edge. Fixed colors stay per panel.","Angehakte Panels zeigen bei Effekten drei Farben, eine je Kante.":"Checked panels show three colors in effects, one per edge.","Alle an":"All on","Alle aus":"All off","Noch keine Presets. Stell eine Szene ein und speichere sie unten.":"No presets yet. Set up a scene and save it below.","Aktuelle Szene speichern":"Save current scene","Name, z. B. Abend":"Name, e.g. Evening","Speichern":"Save","Ein Preset merkt sich Ein/Aus, Gesamthelligkeit, Effekt mit Tempo, Intensität und Palette und die Farbe jedes Panels. In Home Assistant erscheinen die Presets als Auswahl „Preset“. Gleicher Name überschreibt.":"A preset remembers on/off, overall brightness, the effect with speed, intensity and palette, and the color of every panel. In Home Assistant the presets appear as the select “Preset”. Saving with the same name overwrites.","Name der Wand":"Wall name","z. B. Wohnzimmer":"e.g. Living room","Steht oben in der App, in Home Assistant und in WLED-Programmen. Die Adresse bleibt trilumag.local.":"Shown at the top of the app, in Home Assistant and in WLED apps. The address stays trilumag.local.","Sprache":"Language","Automatisch":"Automatic","WLAN":"Wi-Fi","Name deines WLANs":"Name of your Wi-Fi","Passwort":"Password","Netze suchen":"Scan networks","Speichern und verbinden":"Save and connect","WLAN-Wächter":"Wi-Fi watchdog","Ist das WLAN 3 Minuten weg, verbindet sich Trilumag neu, nach 10 Minuten startet es neu (nicht, solange jemand im Setup-Netz ist). Hängt die Software eine Minute, startet es ebenfalls neu. trilumag.local wird alle 30 Minuten neu angekündigt.":"If Wi-Fi is gone for 3 minutes, Trilumag reconnects; after 10 minutes it restarts (not while someone is on the setup network). If the software hangs for a minute, it restarts as well. trilumag.local is announced again every 30 minutes.","Licht":"Light","Übergänge":"Transitions","So lange blenden Farb-, Preset-, Effekt- und Ein/Aus-Wechsel weich über. 0 = sofort.":"How long color, preset, effect and on/off changes fade. 0 = instant.","Einschalt-Animation":"Power-on animation","aus":"off","an":"on","mittel":"medium","Beim Einschalten leuchten die Panels der Reihe nach auf, vom Hauptpanel nach außen.":"When switched on, the panels light up one after another, from the main panel outwards.","Nach Stromausfall":"After a power cut","wie vorher":"as before","Preset …":"Preset …","Gilt nur, wenn der Strom weg war. Nach Updates und Neustarts aus der App bleibt alles, wie es war.":"Only applies when power was lost. After updates and restarts from the app everything stays as it was.","Stromlimit":"Current limit","Netzteil liefert höchstens (A)":"Power supply delivers at most (A)","mA pro Farbkanal und Segment":"mA per color channel and segment","Wird es mehr, dimmt Trilumag alle Panels gleichmäßig. Ohne Sensor wird der Strom aus den Farben geschätzt.":"If it gets higher, Trilumag dims all panels evenly. Without a sensor the current is estimated from the colors.","Stromsensor INA226":"Current sensor INA226","Energie":"Energy","heute":"today","diesen Monat":"this month","dieses Jahr":"this year","gesamt":"total","Tage":"Days","Monate":"Months","Jahre":"Years","Energieverbrauch":"Energy use","Zähler zurücksetzen":"Reset counters","Signale und Fortschritt":"Signals and progress","Blau":"Blue","Grün":"Green","Rot":"Red","Gelb":"Yellow","Weiß":"White","Signal testen (3-mal blinken)":"Test signal (blink 3 times)","Fortschritt":"Progress","Gedacht für Home Assistant: Das Gerät „Signal“ lässt die Wand blinken, etwa mit der Nachricht „blau 3“ für die Türklingel. Danach läuft alles weiter wie vorher. „Fortschritt“ füllt die Wand vom Hauptpanel aus, 0 % schaltet es wieder aus. Hier kannst du beides ausprobieren.":"Meant for Home Assistant: the entity “Signal” makes the wall blink, e.g. with the message “blau 3” for the doorbell. Afterwards everything continues as before. “Progress” fills the wall from the main panel, 0 % turns it off again. You can try both here.","Antippen":"Tapping","Panels reagieren auf Antippen":"Panels react to taps","Welle über die Wand beim Antippen":"Ripple across the wall on tap","Empfindlichkeit":"Sensitivity","Einmal antippen":"Single tap","Doppelt antippen":"Double tap","Hardware":"Hardware","Betriebsart":"Mode","Simulation: Panels in der App anklipsen":"Simulation: clip panels on in the app","Bus: echte Panels über RS-485":"Bus: real panels via RS-485","SNS rechts (Kante 2)":"SNS right (edge 2)","SNS links (Kante 3)":"SNS left (edge 3)","LED-Daten":"LED data","Farbreihenfolge der LEDs":"LED color order","Farbtest:":"Color test:","Ende":"End","Leuchtet bei „Rot“ etwas anderes als Rot, stimmt die Reihenfolge nicht. Dann eine andere wählen und speichern.":"If “Red” lights up as something other than red, the order is wrong. Pick another one and save.","Version …":"Version …","Automatisch aktualisieren":"Update automatically","Ist das an, installiert das Hauptpanel neue Versionen von selbst (es sieht alle 6 Stunden nach). Sonst zeigt die App sie hier an, und du entscheidest.":"When on, the main panel installs new versions by itself (it checks every 6 hours). Otherwise the app shows them here and you decide.","Nach Updates suchen":"Check for updates","Firmware-Datei hochladen":"Upload firmware file","Für eigene Builds: die Datei":"For your own builds: the file","bzw.":"or","für deinen Chip.":"for your chip.","Hochladen":"Upload","Panel-Firmware":"Panel firmware","Panels automatisch aktualisieren":"Update panels automatically","Das Hauptpanel bringt die passende Firmware für die Panels mit und spielt sie über den Bus auf. Die Wand leuchtet dabei weiter, nur das Panel, das gerade dran ist, bleibt kurz stehen.":"The main panel carries the matching firmware for the panels and installs it over the bus. The wall keeps glowing; only the panel being updated pauses briefly.","Alle aktualisieren":"Update all","MQTT aktiv":"MQTT enabled","Broker-Adresse":"Broker address","Benutzer":"User","unverändert":"unchanged","Speichern und neu starten":"Save and restart","Mit der Hue Bridge verbinden":"Connect to the Hue Bridge","Trilumag erscheint in der Hue-App als Farblampe „Trilumag Wand“. Hue steuert Ein/Aus, Helligkeit und Farbe der ganzen Wand, auch in Szenen, Routinen und mit Hue-Schaltern. Effekte und einzelne Panels bleiben hier in der App. Ein- und Ausschalten startet das Hauptpanel neu.":"Trilumag appears in the Hue app as the color light “Trilumag Wand”. Hue controls on/off, brightness and color of the whole wall, also in scenes, routines and with Hue switches. Effects and single panels stay here in the app. Switching this on or off restarts the main panel.","Zigbee braucht einen eigenen Funkteil, den nur der ESP32-C6 hat. Mit einem C6 als Hauptpanel lässt sich das hier einschalten.":"Zigbee needs its own radio, which only the ESP32-C6 has. With a C6 as main panel you can switch this on here.","Koppeln: In der Hue-App unter Einstellungen → Lampen → „+“ → Suchen. Trilumag muss dabei laufen und nah genug an der Bridge oder einer Hue-Lampe sein.":"Pairing: in the Hue app go to Settings → Lights → “+” → Search. Trilumag must be running and close enough to the Bridge or a Hue light.","Neu koppeln":"Pair again","Mehrere Wände":"Multiple walls","Mit anderen Wänden im Gleichtakt":"In sync with other walls","Gruppe":"Group","Gruppe {}":"Group {}","Alle Wände derselben Gruppe im selben WLAN teilen Ein/Aus, Helligkeit und Effekt mit allen Einstellungen, egal an welcher Wand du etwas änderst. Die Effekte laufen im selben Takt. Feste Farben einzelner Panels bleiben pro Wand.":"All walls of the same group on the same Wi-Fi share on/off, brightness and the effect with all its settings, no matter on which wall you change something. Effects run in step. Fixed colors of single panels stay per wall.","Bus-Diagnose":"Bus diagnostics","Adr.":"Addr.","Antwortzeit":"Response time","Antw.":"Resp.","verpasste Antworten":"missed replies","verp.":"missed","wie oft angeklipst":"how often clipped on","Angekl.":"Clips","Leuchtdauer in Stunden":"Hours lit","Std.":"Hrs","Ereignisse":"Events","Sicherung":"Backup","Einstellungen, Presets, Farben und die simulierte Wand als Datei. Die WLAN-Zugangsdaten sind nicht dabei. Nach dem Einspielen startet Trilumag neu.":"Settings, presets, colors and the simulated wall as a file. The Wi-Fi credentials are not included. Trilumag restarts after restoring.","Sicherung herunterladen":"Download backup","Einspielen":"Restore","Farben":"Colors","Wand":"Wall","Optionen":"Options","Favorit":"Favorite","Hauptpanel":"Main panel","Haupt":"Main","Panel {}":"Panel {}","Version {} installiert, Trilumag startet neu …":"Version {} installed, Trilumag is restarting …","{} einmal angetippt":"{} tapped once","{} doppelt angetippt":"{} double-tapped","{} einmal angetippt: {}":"{} tapped once: {}","{} doppelt angetippt: {}":"{} double-tapped: {}","keine Verbindung zum ESP32":"no connection to the ESP32","Fehler":"Error","angeklipst, wird erkannt …":"clipped on, detecting …","abgeklipst":"unclipped","{}/{} Panels · {} · MQTT {} · {}":"{}/{} panels · {} · MQTT {} · {}","verbunden":"connected","getrennt":"disconnected","{} Panel ausgewählt":"{} panel selected","{} Panels ausgewählt":"{} panels selected","Panels antippen, um nur diese einzufärben":"Tap panels to color only those","Auswahl ({})":"Selection ({})","Der Farbverlauf geht von Farbe 1 zu Farbe 2 und zurück.":"The gradient runs from color 1 to color 2 and back.","Färbt den laufenden Effekt „{}“.":"Colors the running effect “{}”.","„{}“ hat eigene Farben. Eine Farbe wählen wechselt auf Einfarbig.":"“{}” has its own colors. Picking a color switches to Solid.","Eine Farbe für einzelne Panels beendet den Effekt.":"A color for single panels ends the effect.","Auf Einfarbig gewechselt":"Switched to Solid","Effekt beendet, feste Farben":"Effect ended, fixed colors","Bitte als #RRGGBB eingeben":"Please enter as #RRGGBB","Oben in der Vorschau Panels antippen":"Tap panels in the preview above","Warmweiß":"Warm white","Kaltweiß":"Cool white","nutzt Effektfarbe":"uses effect color","Die Effektfarbe stellst du im Tab Farben ein. {}":"Set the effect color in the Colors tab. {}","Die Richtung wirkt bei Effekten, die über die Wand laufen, etwa Regenbogenwelle, Lauflicht, Polarlicht, Lava und Spirale.":"Direction applies to effects that move across the wall, such as Rainbow wave, Chaser, Aurora, Lava and Spiral.","Einfarbig: jedes Panel leuchtet in seiner eigenen Farbe. Tempo, Intensität und Palette wirken erst mit einem Effekt.":"Solid: every panel shows its own color. Speed, intensity and palette only apply with an effect.","Panel {} sitzt dort, wo Panel {} war. Farbe, Helligkeit und Kanten übernehmen?":"Panel {} sits where panel {} used to be. Take over its color, brightness and edges?","Einstellungen übernommen":"Settings taken over","Leer. Mit „Neues Panel“ eins dazunehmen oder ein Panel von der Wand hierher ziehen.":"Empty. Add one with “New panel” or drag a panel here from the wall.","Abgeklipste Panels erscheinen hier.":"Unclipped panels appear here.","Echte Panels: anklipsen, und sie erscheinen hier":"Real panels: clip them on and they appear here","Panel aus der Ablage an eine freie Kante ziehen":"Drag a panel from the tray to a free edge","Panel antippen für Details, wegziehen zum Abklipsen":"Tap a panel for details, drag it away to unclip","Chip-ID":"Chip ID","Zustand":"State","dunkel, wird erkannt":"dark, being detected","pulsiert blau, wartet auf erste Farbe":"pulsing blue, waiting for its first color","leuchtet":"lit","Bus":"Bus","Adresse {} · Antwort {} ms · {} % verpasst · Firmware {}":"Address {} · reply {} ms · {} % missed · firmware {}","Position":"Position","{} · Spitze oben":"{} · tip up","{} · Spitze unten":"{} · tip down","Hängt an":"Attached to","{} + Weiß {}":"{} + white {}","Angeklipst":"Clipped on","{}-mal":"{} times","{} · Update läuft {} %":"{} · updating {} %","{} · Update auf {} verfügbar":"{} · update to {} available","Sensor vorhanden":"sensor present","kein Sensor":"no sensor","Leuchtdauer":"Time lit","Ein":"On","Aus":"Off","{} von {} an":"{} of {} on","Kanten einzeln: alle an":"Separate edges: all on","Kanten einzeln: alle aus":"Separate edges: all off","Kanten einzeln an: wirkt bei Effekten":"Separate edges on: applies to effects","Kanten einzeln aus":"Separate edges off","Löschen?":"Delete?","Preset löschen":"Delete preset","„{}“ gelöscht":"Deleted “{}”","„{}“ gespeichert":"Saved “{}”","Bitte einen Namen eingeben":"Please enter a name","Eigene Belegung":"Custom pins","★ = Vorgabe für dein Board: SDA GPIO {}, SCL GPIO {}. {}":"★ = default for your board: SDA GPIO {}, SCL GPIO {}. {}","Letzter Neustart":"Last restart","Abstürze":"Crashes","WLAN-Wächter an":"Wi-Fi watchdog on","WLAN-Wächter aus":"Wi-Fi watchdog off","Ein Pin ist doppelt belegt.":"A pin is used twice.","Speichern fehlgeschlagen":"Saving failed","Gespeichert. Trilumag startet neu …":"Saved. Trilumag is restarting …","Gespeichert":"Saved","Suche …":"Scanning …","Signal {} von 4":"Signal {} of 4","gesichert":"secured","offen":"open","{} Netze gefunden":"{} networks found","Keine Netze gefunden, bitte noch einmal suchen":"No networks found, please scan again","Suche fehlgeschlagen":"Scan failed","Bitte WLAN-Namen eingeben":"Please enter the Wi-Fi name","Gespeichert. Trilumag startet neu und verbindet sich mit {}":"Saved. Trilumag restarts and connects to {}","Verbinde mit „{}“ …":"Connecting to “{}” …","Trilumag startet neu. Lade diese Seite in etwa 20 Sekunden neu, im neuen WLAN unter http://trilumag.local.":"Trilumag is restarting. Reload this page in about 20 seconds, on the new Wi-Fi at http://trilumag.local.","{} · {} W gemessen":"{} · {} W measured","{} · {} W geschätzt":"{} · {} W estimated","{} V · {}":"{} V · {}","Stromlimit greift: gedimmt auf {} %":"Current limit active: dimmed to {} %","unter dem Limit von {}":"below the limit of {}","kein Stromlimit":"no current limit","gefunden":"found","nicht gefunden":"not found","keiner":"none","Sensor gefunden: Trilumag misst den echten Strom und regelt danach.":"Sensor found: Trilumag measures the real current and regulates by it.","Sensor nicht gefunden. Verkabelung prüfen (SDA, SCL, 3,3 V, GND), dann hier die Pins neu wählen.":"Sensor not found. Check the wiring (SDA, SCL, 3.3 V, GND), then pick the pins here again.","Kein Sensor eingestellt. Der Strom wird aus den Farben geschätzt.":"No sensor set. The current is estimated from the colors.","Bitte beide Pins wählen oder bei beiden „kein Sensor“.":"Please pick both pins or “no sensor” for both.","In der Simulation probierst du es unter Wand aus: Panel antippen, dann „einmal“ oder „doppelt“. {}":"In the simulation, try it under Wall: tap a panel, then “once” or “twice”. {}","{} von {} Panels haben einen Bewegungssensor. {}":"{} of {} panels have a motion sensor. {}","In Home Assistant gibt es dazu das Ereignis „Antippen“ für eigene Automationen.":"Home Assistant gets the event “Antippen” (tap) for your own automations.","Panel {} wird aktualisiert":"Panel {} is being updated","{} Panel mit älterer Firmware":"{} panel with older firmware","{} Panels mit älterer Firmware":"{} panels with older firmware","Alle Panels sind aktuell":"All panels are up to date","Aktuelle Panel-Firmware: {}":"Current panel firmware: {}","Aktuelle Panel-Firmware: {} · {} Panel an der Wand":"Current panel firmware: {} · {} panel on the wall","Aktuelle Panel-Firmware: {} · {} Panels an der Wand":"Current panel firmware: {} · {} panels on the wall","wartet":"waiting","fehlgeschlagen, wird erneut versucht":"failed, retrying","Firmware {} · ohne Bootloader, nur mit Programmieradapter":"Firmware {} · no bootloader, programmer only","Nochmal":"Retry","Aktualisieren":"Update","Panels werden automatisch aktualisiert":"Panels are updated automatically","Panel-Updates nur noch per Knopf":"Panel updates by button only","Nur mit ESP32-C6":"Only with ESP32-C6","Dein Hauptpanel hat einen {} ohne Zigbee-Funk":"Your main panel has no Zigbee radio ({})","Zigbee ist aus":"Zigbee is off","Zigbee läuft nicht":"Zigbee is not running","Mit der Hue Bridge verbunden":"Connected to the Hue Bridge","Wartet auf die Hue Bridge":"Waiting for the Hue Bridge","Zum Koppeln mit Hue einschalten":"Switch on to pair with Hue","Zigbee-Kanal {}":"Zigbee channel {}","Jetzt in der Hue-App nach neuen Lampen suchen":"Now search for new lights in the Hue app","Zigbee an, Trilumag startet neu …":"Zigbee on, Trilumag is restarting …","Zigbee aus, Trilumag startet neu …":"Zigbee off, Trilumag is restarting …","Wirklich? Trilumag verlässt das Hue-Netz":"Really? Trilumag leaves the Hue network","Zigbee wird zurückgesetzt, Trilumag startet neu …":"Resetting Zigbee, Trilumag is restarting …","Name gespeichert":"Name saved","Das Panel blinkt jetzt 3 Sekunden weiß":"The panel now blinks white for 3 seconds","Eigene Farbe":"Custom color","Farbe wählen und über die Panels wischen":"Pick a color and swipe over the panels","Die Wand geht in {} aus. Neu wählen oder beenden:":"The wall turns off in {}. Choose again or stop:","Die Wand geht in {} aus":"The wall turns off in {}","Sleep-Timer beendet":"Sleep timer stopped","Zuerst unter Presets eins speichern":"Save one under Presets first","Gemessen mit dem Stromsensor":"Measured with the current sensor","Geschätzt aus den Farben (ohne Stromsensor)":"Estimated from the colors (no current sensor)","{} · gerade {} W":"{} · now {} W","{} · Datum noch unbekannt (kein Internet?), gezählt wird trotzdem":"{} · date not known yet (no internet?), counting anyway","Noch keine Werte":"No values yet","Wirklich alle Werte löschen?":"Really delete all values?","Energiezähler zurückgesetzt":"Energy counters reset","{} (diese Wand)":"{} (this wall)","gibt den Takt vor":"sets the pace","{} · Gruppe {}, läuft nicht mit":"{} · group {}, not in sync","Noch keine andere Wand gefunden":"No other wall found yet","Sie muss im selben WLAN sein und den Gleichtakt eingeschaltet haben":"It must be on the same Wi-Fi and have sync switched on","Gleichtakt an: andere Wände der Gruppe laufen mit":"Sync on: other walls of the group follow","Gleichtakt aus":"Sync off","{}° · gespiegelt":"{}° · mirrored","vor {} s":"{} s ago","vor {} min":"{} min ago","vor {} h":"{} h ago","Laufzeit {} h {} min · {} Bilder gesendet · {} Erkennungsrunden · {} verpasste Antworten · {} gestörte Übertragungen":"Uptime {} h {} min · {} frames sent · {} discovery rounds · {} missed replies · {} corrupted transfers","Simulation: Antwortzeiten und Fehler gibt es erst im Busbetrieb. Das Ereignisprotokoll läuft trotzdem.":"Simulation: response times and errors only exist in bus mode. The event log runs anyway.","{} Eintrag":"{} entry","{} Einträge":"{} entries","keine":"none","Noch keine Ereignisse.":"No events yet.","Bitte zuerst eine Sicherungsdatei wählen":"Please pick a backup file first","Sicherung eingespielt, Trilumag startet neu …":"Backup restored, Trilumag is restarting …","Einspielen fehlgeschlagen":"Restore failed","sehr gut":"very good","gut":"good","schwach":"weak","Verbunden mit „{}“":"Connected to “{}”","Nicht mit einem WLAN verbunden":"Not connected to a Wi-Fi","Empfang {} ({} dBm) · IP {} · trilumag.local":"Signal {} ({} dBm) · IP {} · trilumag.local","Das Setup-Netz „Trilumag-Setup“ ist offen. Wähle unten dein WLAN.":"The setup network “Trilumag-Setup” is open. Pick your Wi-Fi below.","Wähle unten dein WLAN.":"Pick your Wi-Fi below.","Anderes WLAN wählen:":"Choose another Wi-Fi:","Version {} bewährt sich noch":"Version {} is still on probation","In den ersten 45 Sekunden springt Trilumag bei einem Absturz automatisch auf die vorige Version zurück.":"During the first 45 seconds Trilumag automatically falls back to the previous version if it crashes.","Installiere … {} %":"Installing … {} %","Nicht ausschalten. Danach startet Trilumag neu.":"Do not switch off. Trilumag restarts afterwards.","Zurück auf die vorige Version":"Back on the previous version","Version {} ist verfügbar":"Version {} is available","Installiert ist {}.":"Installed: {}.","Suche nach Updates …":"Checking for updates …","Noch nicht nach Updates gesucht.":"Not checked for updates yet.","Aktuell · zuletzt geprüft vor {} Min.":"Up to date · last checked {} min ago","Änderungen ausblenden":"Hide changes","Änderungen anzeigen":"Show changes","Der Versionsverlauf lässt sich gerade nicht laden (kein Internet?). Er steht auch auf":"The version history can't be loaded right now (no internet?). It is also on","Keine Beschreibung vorhanden.":"No description.","Lade …":"Loading …","installiert":"installed","startete nicht":"did not start","neuer":"newer","älter":"older","Wirklich?":"Really?","Wirklich zurück?":"Really go back?","Installieren":"Install","Zurück":"Back","Update läuft, die Seite lädt danach neu":"Updating, the page reloads afterwards","Automatische Updates an":"Automatic updates on","Automatische Updates aus":"Automatic updates off","Bitte zuerst eine .bin-Datei wählen":"Please pick a .bin file first","Lade hoch … {} %":"Uploading … {} %","Installiert, Trilumag startet neu …":"Installed, Trilumag is restarting …","Hochladen fehlgeschlagen":"Upload failed","Version {} ist nicht richtig gestartet. Trilumag läuft wieder mit {}.":"Version {} did not start properly. Trilumag is running {} again.","Update auf {} erfolgreich.":"Update to {} successful.","Update fehlgeschlagen: {}":"Update failed: {}","Update-Server nicht erreichbar":"Update server not reachable","Versionsliste unlesbar":"Version list unreadable","Einfarbig":"Solid","Regenbogen":"Rainbow","Regenbogenwelle":"Rainbow wave","Atmen":"Breathe","Farbwechsel":"Color change","Funkeln":"Twinkle","Ausbreiten":"Spread","Feuer":"Fire","Polarlicht":"Aurora","Lauflicht":"Chaser","Spirale":"Spiral","Gewitter":"Thunderstorm","Kerzenlicht":"Candlelight","Komet":"Comet","Farbverlauf":"Gradient","Effektfarbe":"Effect color","Ozean":"Ocean","Wald":"Forest","Sonnenuntergang":"Sunset","Pastell":"Pastel","Eis":"Ice","nichts":"nothing","Panel ein/aus":"Panel on/off","nächstes Preset":"next preset","nächster Effekt":"next effect","Strom eingeschaltet oder Stromausfall":"power switched on or power cut","Spannung zu niedrig (Netzteil prüfen)":"voltage too low (check power supply)","Absturz":"crash","hing, vom Watchdog neu gestartet":"hung, restarted by the watchdog","Reset-Taster":"reset button","nach einem Update":"after an update","aus der App (Einstellungen, Sicherung oder WLAN)":"from the app (settings, backup or Wi-Fi)","vom Wächter: WLAN war 10 Minuten weg":"by the watchdog: Wi-Fi was gone for 10 minutes","vom Wächter: Hauptschleife hing":"by the watchdog: main loop hung","Neustart (Software)":"restart (software)","unbekannt":"unknown","Alle 16 Plätze belegt":"All 16 slots in use","Bitte ein Preset wählen":"Please pick a preset","Das ist keine Trilumag-Sicherung":"This is not a Trilumag backup","Effekt unbekannt":"Unknown effect","Farbe unbekannt (z. B. rot, grün, blau, gelb, weiß oder #RRGGBB)":"Unknown color (e.g. rot, grün, blau, gelb, weiß or #RRGGBB)","Farbe unbekannt":"Unknown color","Für den Stromsensor beide Pins wählen oder keinen":"Pick both pins for the current sensor or none","Kein WLAN":"No Wi-Fi","Name fehlt":"Name missing","Nichts zu übernehmen":"Nothing to take over","Panel unbekannt":"Unknown panel","Pin ist schon für den Bus oder die LEDs belegt":"Pin is already used by the bus or the LEDs","Pin des Stromsensors ist schon belegt":"Current sensor pin is already in use","Pins für den Stromsensor ungültig":"Invalid pins for the current sensor","Preset unbekannt":"Unknown preset","Unbekannte Aktion":"Unknown action","Unbekannte Einstellung":"Unknown setting","Unbekannter Befehl":"Unknown command","Unbekannte Farbreihenfolge":"Unknown color order","Version unbekannt, bitte zuerst nach Updates suchen":"Unknown version, please check for updates first","Wert fehlt (0 bis 100)":"Value missing (0 to 100)","Zeit ungültig":"Invalid time","Zigbee gibt es nur mit dem ESP32-C6":"Zigbee is only available with the ESP32-C6","Zigbee ist nicht eingeschaltet":"Zigbee is not switched on","nur in der Simulation":"simulation only","Ablage voll":"Tray full","Datei unlesbar":"File unreadable","Datei passt nicht oder ist beschädigt":"File does not fit or is damaged","JSON fehlt":"JSON missing","WLAN-Name fehlt":"Wi-Fi name missing","Speicheraufteilung ohne Zigbee-Bereich: einmal mit dem Webinstaller flashen":"Partition table without Zigbee area: flash once with the web installer","Zigbee ließ sich nicht starten":"Zigbee could not be started","Die Wand heißt jetzt „{}“":"The wall is now called “{}”","Gestartet: {}":"Started: {}","Gleichtakt mit anderen Wänden an (Gruppe {})":"Sync with other walls on (group {})","Gleichtakt mit anderen Wänden aus":"Sync with other walls off","Panel {} abgeklipst":"Panel {} unclipped","Panel {} an Kante {} von {} (eigene Kante {}), Adresse {}":"Panel {} on edge {} of {} (own edge {}), address {}","Panel {} an Kante {} von {} angeklipst":"Panel {} clipped onto edge {} of {}","Panel {} antwortet nicht mehr, gilt als abgeklipst":"Panel {} stopped answering, treated as unclipped","Panel {} getrennt (hing an einem abgeklipsten Panel)":"Panel {} disconnected (was attached to an unclipped panel)","Panel {} hat die Adresse nicht bestätigt":"Panel {} did not confirm its address","Panel {} hat jetzt Firmware {}":"Panel {} now has firmware {}","Panel {} hängt im Bootloader, Firmware wird neu aufgespielt":"Panel {} is stuck in the bootloader, reinstalling firmware","Panel {} lässt sich nicht einordnen":"Panel {} cannot be placed","Panel {} meldet Kontakt, Nachbar nicht gefunden (SNS prüfen)":"Panel {} reports contact, neighbor not found (check SNS)","Panel {} übernimmt die Einstellungen von {}":"Panel {} takes over the settings of {}","Panel {}: Update auf Firmware {} beginnt":"Panel {}: update to firmware {} starting","Signal: {}-mal blinken":"Signal: blink {} times","Sleep-Timer: Wand aus":"Sleep timer: wall off","Sleep-Timer: Wand geht in {} min aus":"Sleep timer: wall turns off in {} min","Trilumag {} gestartet ({}, {})":"Trilumag {} started ({}, {})","Update von Panel {} fehlgeschlagen: {}":"Update of panel {} failed: {}","Wand „{}“ gefunden (Gruppe {})":"Found wall “{}” (group {})","Wächter: WLAN seit 10 Minuten weg, starte neu":"Watchdog: Wi-Fi gone for 10 minutes, restarting","Wächter: WLAN seit 3 Minuten weg, verbinde neu":"Watchdog: Wi-Fi gone for 3 minutes, reconnecting","Zigbee {}, Hauptpanel startet neu":"Zigbee {}, main panel restarting","Zigbee abgeschaltet: Das Hauptpanel ist damit dreimal nicht richtig gestartet":"Zigbee switched off: the main panel failed to start properly three times with it","Zigbee läuft: in der Hue-App als „Trilumag Wand“ suchen":"Zigbee running: search for “Trilumag Wand” in the Hue app","Zigbee wird zurückgesetzt und ist danach wieder koppelbar":"Resetting Zigbee, it can be paired again afterwards","Zigbee: {}":"Zigbee: {}"};/*EN*/
const ENP=Object.keys(EN).filter(k=>k.includes('{}')).sort((a,b)=>b.length-a.length)
  .map(k=>[new RegExp('^'+k.replace(/[.*+?^$()|[\]\\]/g,'\\$&').replace(/\{\}/g,'(.+?)')+'$'),EN[k]]);
function tr(s,dep){if(LANG==='de'||!s)return s;const t=s.trim();if(!t)return s;let r=EN[t];
  if(r===undefined&&(dep||0)<2)for(const[re,v]of ENP){const m=t.match(re);if(m){let i=1;r=v.replace(/\{\}/g,()=>tr(m[i++],(dep||0)+1));break;}}
  return r===undefined?s:s.replace(t,()=>r);}
const TRA=['placeholder','title','aria-label'];
function trAttr(n,a){const v=n.getAttribute(a);if(v){const w=tr(v);if(w!==v)n.setAttribute(a,w);}}
function trNode(n){
  if(n.nodeType===3){const p=n.parentElement;if(!p||p.closest('[data-nt],script,style'))return;const v=tr(n.nodeValue);if(v!==n.nodeValue)n.nodeValue=v;return;}
  if(n.nodeType!==1||n.closest('[data-nt],script,style'))return;
  TRA.forEach(a=>trAttr(n,a));n.childNodes.forEach(trNode);}
function trStart(){if(LANG==='de')return;trNode(document.body);
  new MutationObserver(ms=>ms.forEach(m=>{if(m.type==='childList')m.addedNodes.forEach(trNode);else if(m.type==='characterData')trNode(m.target);
    else if(!m.target.closest('[data-nt]'))trAttr(m.target,m.attributeName);}))
    .observe(document.body,{subtree:true,childList:true,characterData:true,attributes:true,attributeFilter:TRA});}

const S=60,H=S*Math.sqrt(3)/2,SNAP=S*0.6,NS='http://www.w3.org/2000/svg';
const $=id=>document.getElementById(id);
let st=null,tab='col',sel=new Set(),focus=null,drag=null,vbFix=null,live={},joining=[],inflight=false,lastState=0;

// ---------- Hilfen ----------
function el(n,a){const e=document.createElementNS(NS,n);for(const k in a)e.setAttribute(k,a[k]);return e;}
function toast(t){const e=$('toast');e.textContent=t;e.classList.add('show');clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('show'),2400);}
function hx(v){return Math.round(v).toString(16).padStart(2,'0');}
function rgbHex(r,g,b){return('#'+hx(r)+hx(g)+hx(b)).toUpperCase();}
function hsv2rgb(h,s,v){const f=n=>{const k=(n+h*6)%6;return v*(1-s*Math.max(0,Math.min(k,4-k,1)));};return[f(5)*255,f(3)*255,f(1)*255];}
function rgb2hsv(r,g,b){r/=255;g/=255;b/=255;const M=Math.max(r,g,b),m=Math.min(r,g,b),d=M-m;let h=0;
  if(d){h=M===r?((g-b)/d)%6:M===g?(b-r)/d+2:(r-g)/d+4;h/=6;if(h<0)h+=1;}return[h,M?d/M:0,M];}
function setFill(r){r.style.setProperty('--p',((r.value-r.min)/(r.max-r.min)*100)+'%');}
document.querySelectorAll('input[type=range]').forEach(r=>{setFill(r);r.addEventListener('input',()=>setFill(r));});
function setRange(id,v,unit){const r=$(id);if(document.activeElement===r)return;r.value=v;setFill(r);const o=$(id+'o');if(o)o.textContent=v+(unit||'');}
// Akzentfarbe folgt der aktuellen Farbe, wie bei WLED
function accent(r,g,b){if(r+g+b<60){r=110;g=139;b=255;}const root=document.documentElement.style;root.setProperty('--acc',`rgb(${r|0},${g|0},${b|0})`);
  root.setProperty('--accfg',(0.3*r+0.59*g+0.11*b)>150?'#111':'#fff');}

// Neuer Zustand vom ESP32; die festen Listen (Effekte, Paletten) kommen nur beim ersten Mal mit
function takeState(j){if(st&&!j.effects){j.effects=st.effects;j.palettes=st.palettes;}st=j;}

// ----- Feste Verbindung (WebSocket, wie bei WLED) -----
// Das Hauptpanel schickt Zustand und Effektbild von selbst, Befehle gehen über dieselbe Verbindung.
// Ist sie weg, fragt die App wie früher per HTTP nach, bis sie wieder steht.
let sock=null,wsOk=false;
function wsConnect(){
  try{sock=new WebSocket(`ws://${location.hostname}:81/`);}catch(e){setTimeout(wsConnect,3000);return;}
  sock.onopen=()=>{wsOk=true;render();};
  sock.onmessage=e=>{let m;try{m=JSON.parse(e.data);}catch(x){return;}
    if(m.t==='state'){takeState(m.d);if(!drag)render();}
    else if(m.t==='live'){if(m.d.fx!=='aus'||m.d.ov){live=m.d.c;joining=m.d.j||[];
      if(m.d.ov&&!fxOn()){const was=Date.now()<liveOvUntil;liveOvUntil=Date.now()+400;clearTimeout(ovTimer);ovTimer=setTimeout(()=>{live={};if(!drag)render();},450);if(!was&&!drag){render();return;}}
      if(!drag)paintLive();}}
    else if(m.t==='ota'){otaInfo=m.d;renderOta();}
    else if(m.t==='otap'){if(otaInfo){otaInfo.busy=true;otaInfo.p=m.p;renderOta();}}
    else if(m.t==='otadone'){toast('Version '+m.v+' installiert, Trilumag startet neu …');setTimeout(()=>location.reload(),9000);}
    else if(m.t==='touch')onTouch(m.d);
    else if(m.t==='err')toast(m.m);};
  sock.onclose=()=>{wsOk=false;sock=null;render();setTimeout(wsConnect,2000);};
  sock.onerror=()=>{try{sock.close();}catch(x){}};
}

// Ein Panel wurde angetippt: kurz aufleuchten lassen
const tapUntil={};
function onTouch(d){
  tapUntil[d.id]=Date.now()+700;
  const p=st&&st.panels.find(q=>q.id===d.id);
  const a=cfg&&cfg.touch?cfg.touch.actions[d.k===2?cfg.touch.a2:cfg.touch.a1]:'';
  toast(`${p&&p.main?'Hauptpanel':'Panel '+d.id.slice(4)} ${d.k===2?'doppelt':'einmal'} angetippt${a&&a!=='nichts'?': '+a:''}`);
  if(!drag)render();setTimeout(()=>{if(!drag)render();},750);
}
async function api(path,body){
  if(body&&wsOk){sock.send(JSON.stringify({p:path,b:body}));return st;}   // Antwort kommt als neuer Zustand
  const r=await fetch(path,body?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}:{});
  const j=await r.json();if(!r.ok){toast(j.error||'Fehler');return null;}if(j.panels)takeState(j);return j;}
// Ohne WebSocket: Abfragen nie gleichzeitig, Klicks gehen vor
async function poll(){if(wsOk||drag||inflight)return;if(fxOn()&&Date.now()-lastState<2000)return;
  inflight=true;try{await api('/api/state');lastState=Date.now();render();}catch(e){$('status').textContent='keine Verbindung zum ESP32';}inflight=false;}
function fxOn(){return st&&st.fx&&st.fx.id!=='aus';}
let liveOvUntil=0,ovTimer=0;                            // Wellen und Einschalt-Animation kommen auch ohne Effekt als Live-Bild
function liveOn(){return fxOn()||Date.now()<liveOvUntil;}
async function pollLive(){
  if(wsOk||!fxOn()||drag||document.hidden||inflight)return;inflight=true;
  try{const j=await (await fetch('/api/live')).json();if(j.fx!=='aus'){live=j.c;joining=j.j||[];paintLive();}}catch(e){}
  inflight=false;}
const tSend={};function later(k,fn,ms){clearTimeout(tSend[k]);tSend[k]=setTimeout(()=>{tSend[k]=null;fn();},ms||110);}

// ---------- Tabs ----------
function showTab(t){tab=t;document.querySelectorAll('.tab').forEach(s=>s.hidden=s.id!=='t-'+t);
  document.querySelectorAll('#tabs button').forEach(b=>b.classList.toggle('on',b.dataset.tab===t));
  $('miniBox').hidden=(t==='wall'||t==='opt');
  if(t==='opt')loadCfg();if(t==='col')requestAnimationFrame(()=>{drawWheel();syncColUi();});
  vbFix=null;render();scrollTo(0,0);}
document.querySelectorAll('#tabs button').forEach(b=>b.addEventListener('click',()=>showTab(b.dataset.tab)));
document.querySelectorAll('[data-go]').forEach(b=>b.addEventListener('click',()=>showTab(b.dataset.go)));

// ---------- Wand ----------
// Ansicht: so hängt die Wand wirklich (Drehung in 30°-Schritten, Spiegeln); gilt auch für die Effekte
function vw(q){const v=st&&st.view;if(!v||(!v.rot&&!v.mir))return q;const a=v.rot*Math.PI/180,c=Math.cos(a),s=Math.sin(a);
  let x=q[0]*c-q[1]*s;const y=q[0]*s+q[1]*c;if(v.mir)x=-x;return[x,y];}
function geom(x,y,up){const cx=x*S/2,t=y*H;const p=up?[[cx-S/2,t+H],[cx+S/2,t+H],[cx,t]]:[[cx-S/2,t],[cx+S/2,t],[cx,t+H]];return{p:p.map(vw),c:vw([cx,up?t+2*H/3:t+H/3])};}
function shrink(g,k){return g.p.map(q=>[g.c[0]+(q[0]-g.c[0])*k,g.c[1]+(q[1]-g.c[1])*k]);}
function pts(a){return a.map(q=>q[0].toFixed(1)+','+q[1].toFixed(1)).join(' ');}
function edge1Mid(p){const g=geom(p.x,p.y,p.up);const L=p.up?['B','R','L']:['T','L','R'];const d=L[(p.rot)%3];const [a,b,c]=g.p;
  let m;if(p.up)m=d==='B'?[a,b]:d==='R'?[b,c]:[a,c];else m=d==='T'?[a,b]:d==='L'?[a,c]:[b,c];
  const mx=(m[0][0]+m[1][0])/2,my=(m[0][1]+m[1][1])/2;return[mx+(g.c[0]-mx)*.28,my+(g.c[1]-my)*.28];}
// Anzeige: dunkle LED-Werte etwas anheben, damit man sie am Bildschirm sieht
function disp(r,g,b,w){const f=c=>Math.round(255*Math.pow(Math.min(1,(c+w*.85)/255),.55));return`rgb(${f(r)},${f(g)},${f(b)})`;}
function staticCol(p){const k=(st.on?st.master:0)/255*p.bri/255;if(!p.on||!k)return'#141414';return disp(p.r*k,p.g*k,p.b*k,p.w*k);}
function liveCol(h){const v=[0,2,4,6].map(i=>parseInt(h.slice(i,i+2),16));return disp(...v);}
function lightish(p){if(fxOn())return false;return st.on&&p.on&&p.state===2&&(p.r+p.g+p.b+p.w*1.5)*p.bri*st.master/65025>330;}
function syncPulse(p){p.style.animationDelay=(-(Date.now()%1600))+'ms';}

function fit(svg,big){
  const xs=[],ys=[];const add=(x,y,up)=>{geom(x,y,up).p.forEach(q=>{xs.push(q[0]);ys.push(q[1]);});};
  st.panels.forEach(p=>add(p.x,p.y,p.up));if(big)st.ghosts.forEach(g=>add(g.x,g.y,g.up));
  const pad=big?.8:.25;let x0=Math.min(...xs)-S*pad,x1=Math.max(...xs)+S*pad,y0=Math.min(...ys)-S*pad,y1=Math.max(...ys)+S*(big?pad:.6);   // unten Platz für den Hinweis
  const r=svg.clientWidth/svg.clientHeight||2;let w=Math.max(x1-x0,S*(big?3.4:1.6)),h=Math.max(y1-y0,S*(big?2.2:1));
  if(w/h<r)w=h*r;else h=w/r;const cx=(x0+x1)/2,cy=(y0+y1)/2;return[cx-w/2,cy-h/2,w,h];}

function drawWall(svg,big){
  const vb=big&&drag&&vbFix?vbFix:fit(svg,big);if(big)vbFix=vb;
  svg.setAttribute('viewBox',vb.map(v=>v.toFixed(1)).join(' '));svg.innerHTML='';
  const gG=el('g',{}),gP=el('g',{}),gO=el('g',{class:'ov'});svg.append(gG,gP,gO);
  if(big&&drag)st.ghosts.forEach((g,i)=>{gG.append(el('polygon',{points:pts(shrink(geom(g.x,g.y,g.up),.94)),class:'ghost'+(drag.snap===i?' hot':'')}));});
  st.panels.forEach(p=>{
    const g=geom(p.x,p.y,p.up);const cls=['tri'];if(p.main)cls.push('main');
    if(big?focus===p.id:(tab==='col'&&sel.has(p.id)))cls.push('sel');
    if(tapUntil[p.id]>Date.now())cls.push('tap');
    if(p.ident)cls.push('find');
    let fill=null;
    if(p.state===0)cls.push('dark');
    else if(fxOn()&&joining.includes(p.id))cls.push('pulse');
    else if(liveOn()&&live[p.id])fill=liveCol(live[p.id]);
    else if(p.state===1&&st.on)cls.push('pulse');
    else fill=staticCol(p);
    const lv=liveOn()&&live[p.id]&&!joining.includes(p.id)&&live[p.id].includes(',')?live[p.id].split(','):null;
    if(lv&&p.state!==0){                                  // Kanten einzeln: drei Teildreiecke
      const sh=shrink(g,.94);fan(p,sh,g.c).forEach((tri,e)=>{const q=el('polygon',{points:pts(tri),class:cls.filter(c=>c!=='sel').join(' ')+' part'});
        q.style.fill=st.on?liveCol(lv[e]):'#141414';q.dataset.id=p.id;q.dataset.e=e;gP.append(q);});
      if(cls.includes('sel'))gP.append(el('polygon',{points:pts(sh),class:'selo'}));
    }else{
    const poly=el('polygon',{points:pts(shrink(g,.94)),class:cls.join(' ')});if(fill)poly.style.fill=fill;
    poly.dataset.id=p.id;if(cls.includes('pulse'))syncPulse(poly);gP.append(poly);}
    if(big){
      if(!p.main){const m=edge1Mid(p);gP.append(el('circle',{cx:m[0],cy:m[1],r:2.4,class:'edge1'}));}
      else{const a=g.p[0],b=g.p[1],mx=(a[0]+b[0])/2,my=(a[1]+b[1])/2;let ox=mx-g.c[0],oy=my-g.c[1];const L=Math.hypot(ox,oy)||1;ox=ox/L*5;oy=oy/L*5;   // Strich außen an der Unterkante des Hauptpanels
        const tx=(b[0]-a[0])*.12,ty=(b[1]-a[1])*.12;gP.append(el('line',{x1:a[0]+tx+ox,y1:a[1]+ty+oy,x2:b[0]-tx+ox,y2:b[1]-ty+oy,stroke:'#777','stroke-width':3,'stroke-linecap':'round'}));}
      const t=el('text',{x:g.c[0],y:g.c[1]+3,class:'lbl'+(lightish(p)?' dk':'')});t.textContent=p.main?'Haupt':p.id.slice(4);gP.append(t);
    }
  });
  if(big)drawDrag(svg);
}
function paintLive(){
  let rebuild=false;
  ['mini','big'].forEach(id=>$(id).querySelectorAll('polygon[data-id]').forEach(p=>{const i=p.dataset.id,h=live[i];
    if(h&&(h.includes(',')!==(p.dataset.e!==undefined))){rebuild=true;return;}   // Kanten ein-/ausgeschaltet: neu zeichnen
    if(joining.includes(i)){if(!p.classList.contains('pulse')){p.style.fill='';p.classList.add('pulse');syncPulse(p);}}
    else if(h){const c=h.split(',')[p.dataset.e||0];p.classList.remove('pulse');p.style.fill=st.on?liveCol(c):'#141414';}}));
  if(rebuild&&!drag)render();
}

// Teildreiecke für Kanten einzeln: Kante e (eigene Zählung) → Seite des Dreiecks in der Welt
function fan(p,sh,c){const L=p.up?['B','R','L']:['T','L','R'];const [a,b,t]=sh;
  const side=d=>p.up?(d==='B'?[a,b]:d==='R'?[b,t]:[a,t]):(d==='T'?[a,b]:d==='L'?[a,t]:[b,t]);
  return [0,1,2].map(e=>{const s2=side(L[(e+p.rot)%3]);return[c,s2[0],s2[1]];});}

// Vorschau oben: Panels antippen wählt sie für die Farbe aus
// beim Antippen auswerten: die Vorschau wird laufend neu gezeichnet, ein "click" ginge dabei verloren
$('mini').addEventListener('pointerdown',e=>{const id=e.target.dataset&&e.target.dataset.id;if(!id)return;
  if(sel.has(id))sel.delete(id);else sel.add(id);if(tab!=='col'&&sel.size)showTab('col');else render();});

// ---------- Ziehen und Anklipsen (Simulation) ----------
function drawDrag(svg){const ov=svg.querySelector('.ov');if(!ov||!drag||!drag.moved)return;ov.innerHTML='';
  let poly;if(drag.snap!=null){const g=st.ghosts[drag.snap];poly=shrink(geom(g.x,g.y,g.up),.94);}
  else{const c=drag.pt;poly=[[c.x-S/2,c.y+H/3],[c.x+S/2,c.y+H/3],[c.x,c.y-2*H/3]];}
  ov.append(el('polygon',{points:pts(poly),fill:drag.snap!=null?'rgba(110,139,255,.35)':'rgba(40,40,40,.9)',stroke:drag.snap!=null?'#6e8bff':'#888','stroke-width':2}));}
function toSvg(e){const svg=$('big');const p=svg.createSVGPoint();p.x=e.clientX;p.y=e.clientY;return p.matrixTransform(svg.getScreenCTM().inverse());}
function startDrag(e,id,from){
  if(!st||(!st.sim&&from==='tray'))return;e.preventDefault();
  if(from==='tray'){const r=$('big').getBoundingClientRect();if(r.top<0||r.bottom>innerHeight)$('big').scrollIntoView({block:'nearest'});}   // Wand sichtbar machen, damit man sieht, wohin
  drag={id,from,x0:e.clientX,y0:e.clientY,moved:from==='tray',pt:toSvg(e),snap:null,detached:from==='tray'};
  window.addEventListener('pointermove',onMove);window.addEventListener('pointerup',onUp,{once:true});if(drag.moved)render();}
async function onMove(e){
  if(!drag)return;
  if(!drag.moved&&Math.hypot(e.clientX-drag.x0,e.clientY-drag.y0)>6){drag.moved=true;
    if(drag.from==='wall'&&!drag.detached){drag.detached=true;focus=null;sel.delete(drag.id);await api('/api/sim/detach',{id:drag.id});toast('abgeklipst');}}
  if(!drag||!drag.moved)return;drag.pt=toSvg(e);
  let best=null,bd=SNAP;st.ghosts.forEach((g,i)=>{const c=geom(g.x,g.y,g.up).c;const d=Math.hypot(c[0]-drag.pt.x,c[1]-drag.pt.y);if(d<bd){bd=d;best=i;}});
  drag.snap=best;render();}
async function onUp(){
  window.removeEventListener('pointermove',onMove);const d=drag;if(!d)return;
  if(!d.moved){drag=null;focus=d.id;render();return;}
  const g=d.snap!=null?st.ghosts[d.snap]:null;drag=null;
  if(g){const r=await api('/api/sim/attach',{id:d.id,parent:g.parent,edge:g.edge});if(r){focus=d.id;toast('angeklipst, wird erkannt …');}}
  render();}
$('big').addEventListener('pointerdown',e=>{if(paintOn()){paintStart(e);return;}const id=e.target.dataset&&e.target.dataset.id;if(!id)return;
  const p=st.panels.find(q=>q.id===id);if(!p)return;if(p.main||!st.sim){focus=id;render();return;}startDrag(e,id,'wall');});

// ---------- Darstellung ----------
function render(){
  if(!st)return;
  if(st.name){$('wallName').textContent=st.name;document.title=st.name;
    if(document.activeElement!==$('wname')&&!tSend.name)$('wname').value=st.name;}
  renderSleep();
  $('status').textContent=`${st.panels.length}/${st.max} Panels · ${st.sim?'Simulation':'Bus'} · MQTT ${st.mqttSet?(st.mqtt?'verbunden':'getrennt'):'aus'} · ${wsOk?'live':'HTTP'}`;
  $('pwr').classList.toggle('on',st.on);
  if(document.activeElement!==$('master')){const p=pct(st.master);setRange('master',p);$('mastero').textContent=p+' %';}
  $('apBanner').hidden=!st.ap||tab==='opt';
  renderSwap();
  $('updBadge').hidden=!st.upd;
  if(tab==='opt'){renderWifi();renderPower();renderPfw();renderTouch();renderZb();renderSync();renderBoot();renderSig();}
  [...sel].forEach(id=>{if(!st.panels.some(p=>p.id===id))sel.delete(id);});     // abgeklipste Panels aus der Auswahl nehmen
  if(tab!=='wall'&&tab!=='opt'){drawWall($('mini'),false);
    $('miniHint').textContent=tab==='col'?(sel.size?`${sel.size} Panel${sel.size>1?'s':''} ausgewählt`:'Panels antippen, um nur diese einzufärben'):'';}
  if(tab==='wall')renderWall();
  if(tab==='col')renderCol();
  if(tab==='fx')renderFx();
  if(tab==='pre')renderPre();
  const k=colK;colK=1;const c=curColor();colK=k;accent(c[0],c[1],c[2]);
}

// ---------- Tab Farben ----------
let hsv=[0.08,1,1],cw=0,wheelBusy=false;
function target(){return sel.size?'sel':'all';}
let colK=1;
function c2On(){return!!st&&target()==='all'&&st.fx.id==='verlauf'&&st.fx.pal==='standard';}
function curColor(){
  if(!st)return[110,139,255,0];
  if(colK===2&&c2On()&&st.fx.c2){const c=st.fx.c2;return[c.r,c.g,c.b,c.w];}
  if(target()==='all'&&st.fx.usesColor)return[st.fx.r,st.fx.g,st.fx.b,st.fx.w];
  const id=sel.size?[...sel][0]:null;const p=st.panels.find(q=>q.id===id)||st.panels.find(q=>q.main);
  return p?[p.r,p.g,p.b,p.w]:[255,120,0,0];}
function renderCol(){
  $('tgtAll').classList.toggle('on',!sel.size);$('tgtSel').classList.toggle('on',!!sel.size);
  $('tgtSel').textContent=sel.size?`Auswahl (${sel.size})`:'Auswahl';
  if(!c2On())colK=1;
  $('c12').hidden=!c2On();$('c12').querySelectorAll('button').forEach(b=>b.classList.toggle('on',+b.dataset.k===colK));
  const fxName=(st.effects.find(e=>e.id===st.fx.id)||{}).name;
  $('colNote').textContent=!fxOn()?'':c2On()?'Der Farbverlauf geht von Farbe 1 zu Farbe 2 und zurück.':target()==='all'?(st.fx.usesColor?`Färbt den laufenden Effekt „${fxName}“.`:`„${fxName}“ hat eigene Farben. Eine Farbe wählen wechselt auf Einfarbig.`):'Eine Farbe für einzelne Panels beendet den Effekt.';
  if(!wheelBusy&&!tSend.col){const c=curColor();hsv=rgb2hsv(c[0],c[1],c[2]);cw=c[3];syncColUi();}
}
function syncColUi(){
  const w=$('wheel').clientWidth,r=w/2,a=hsv[0]*2*Math.PI;
  $('knob').style.left=(r+Math.cos(a)*hsv[1]*r)+'px';$('knob').style.top=(r+Math.sin(a)*hsv[1]*r)+'px';
  $('knob').style.background=rgbHex(...hsv2rgb(hsv[0],hsv[1],1));
  setRange('cv',Math.round(hsv[2]*100),' %');setRange('cw',cw);
  if(document.activeElement!==$('hex'))$('hex').value=rgbHex(...hsv2rgb(...hsv));
}
function drawWheel(){
  const cv=$('wcv'),n=Math.round($('wheel').clientWidth*(devicePixelRatio||1));if(!n||cv.width===n)return;cv.width=cv.height=n;
  const ctx=cv.getContext('2d'),img=ctx.createImageData(n,n),R=n/2;
  for(let y=0;y<n;y++)for(let x=0;x<n;x++){const dx=x-R+.5,dy=y-R+.5,d=Math.hypot(dx,dy);const i=(y*n+x)*4;
    if(d>R){img.data[i+3]=0;continue;}let h=Math.atan2(dy,dx)/(2*Math.PI);if(h<0)h+=1;const c=hsv2rgb(h,Math.min(1,d/R),1);
    img.data[i]=c[0];img.data[i+1]=c[1];img.data[i+2]=c[2];img.data[i+3]=d>R-1?255*(R-d):255;}
  ctx.putImageData(img,0,0);}
function sendColor(){
  const c=hsv2rgb(...hsv).map(Math.round);const color={r:c[0],g:c[1],b:c[2],w:+cw};
  if(!(colK===2&&c2On()))accent(c[0],c[1],c[2]);
  later('col',async()=>{
    const wasFx=fxOn();
    if(colK===2&&c2On()){st.fx.c2=color;await api('/api/effect',{color2:color});render();return;}
    if(target()==='all')await api('/api/set',{id:'alle',color,state:'ON'});
    else await api('/api/set',{ids:[...sel],color,state:'ON'});
    if(wasFx&&!fxOn()){live={};toast(target()==='all'?'Auf Einfarbig gewechselt':'Effekt beendet, feste Farben');}
    render();},120);}
function wheelPick(e){const b=$('wheel').getBoundingClientRect(),r=b.width/2;const dx=e.clientX-b.left-r,dy=e.clientY-b.top-r;
  let h=Math.atan2(dy,dx)/(2*Math.PI);if(h<0)h+=1;hsv[0]=h;hsv[1]=Math.min(1,Math.hypot(dx,dy)/r);if(hsv[2]<.05)hsv[2]=1;syncColUi();sendColor();}
$('wheel').addEventListener('pointerdown',e=>{wheelBusy=true;$('wheel').setPointerCapture(e.pointerId);wheelPick(e);});
$('wheel').addEventListener('pointermove',e=>{if(wheelBusy)wheelPick(e);});
$('wheel').addEventListener('pointerup',()=>{wheelBusy=false;});
$('cv').addEventListener('input',()=>{hsv[2]=$('cv').value/100;$('cvo').textContent=$('cv').value+' %';sendColor();});
$('cw').addEventListener('input',()=>{cw=+$('cw').value;$('cwo').textContent=cw;sendColor();});
$('hex').addEventListener('change',()=>{const m=$('hex').value.trim().replace('#','');if(!/^[0-9a-f]{6}$/i.test(m)){toast('Bitte als #RRGGBB eingeben');return;}
  hsv=rgb2hsv(parseInt(m.slice(0,2),16),parseInt(m.slice(2,4),16),parseInt(m.slice(4,6),16));syncColUi();sendColor();});
$('rnd').addEventListener('click',()=>{hsv=[Math.random(),.75+Math.random()*.25,1];cw=0;syncColUi();sendColor();});
$('tgtAll').addEventListener('click',()=>{sel.clear();render();});
$('c12').querySelectorAll('button').forEach(b=>b.addEventListener('click',()=>{colK=+b.dataset.k;tSend.col=0;renderCol();}));
$('tgtSel').addEventListener('click',()=>{if(!sel.size)toast('Oben in der Vorschau Panels antippen');});
const QUICK=[['#FF0000'],['#FF5000'],['#FFC800'],['#00FF00'],['#00FFC8'],['#00A0FF'],['#0000FF'],['#7800FF'],['#FF00C8'],['#FF0050'],['#FFFFFF'],['#000000'],['Warmweiß','#FFB46E',220],['Kaltweiß','#C8DCFF',255]];
QUICK.forEach(q=>{const b=document.createElement('button');const txt=q.length>1;const col=txt?q[1]:q[0];
  b.className='qc'+(txt?' txt':'');b.style.background=col;if(txt)b.textContent=q[0];b.setAttribute('aria-label',txt?q[0]:col);
  b.addEventListener('click',()=>{const m=col.slice(1);hsv=rgb2hsv(parseInt(m.slice(0,2),16),parseInt(m.slice(2,4),16),parseInt(m.slice(4,6),16));
    if(txt){cw=q[2];hsv[2]=.25;}else cw=0;syncColUi();sendColor();});$('quick').append(b);});

// ---------- Tab Effekte ----------
let fxBuilt=false;
function palGrad(p){
  if(p.id==='standard')return'linear-gradient(90deg,#444,#888,#444)';
  if(p.id==='effektfarbe'){const c=[st.fx.r,st.fx.g,st.fx.b];const d=c.map(v=>v*.25|0),l=c.map(v=>v*.6+102|0);return`linear-gradient(90deg,rgb(${d}),rgb(${c}),rgb(${l}),rgb(${d}))`;}
  return'linear-gradient(90deg,'+p.c.concat([p.c[0]]).join(',')+')';}
function renderFx(){
  if(!fxBuilt){fxBuilt=true;
    st.effects.forEach(e=>{const b=document.createElement('div');b.className='row';b.dataset.fx=e.id;b.tabIndex=0;b.setAttribute('role','button');
      b.innerHTML='<span class="dot"></span><svg class="pv" viewBox="0 0 64 18" aria-hidden="true"></svg><span class="nm"><span class="nt"></span>'+(e.color?'<small>nutzt Effektfarbe</small>':'')+'</span><span class="star" role="button" tabindex="0" title="Favorit">☆</span>';
      b.querySelector('.nt').textContent=e.name;pvBuild(b.querySelector('.pv'));
      const star=b.querySelector('.star');
      const fav=ev=>{ev.stopPropagation();const on=!(st.favs||[]).includes(e.id);st.favs=(st.favs||[]).filter(x=>x!==e.id).concat(on?[e.id]:[]);renderFx();api('/api/favs',{effect:e.id,on});};
      star.addEventListener('click',fav);star.addEventListener('keydown',ev=>{if(ev.key==='Enter'||ev.key===' '){ev.preventDefault();fav(ev);}});
      b.addEventListener('click',()=>setFx({effect:e.id}));b.addEventListener('keydown',ev=>{if(ev.target===b&&(ev.key==='Enter'||ev.key===' ')){ev.preventDefault();setFx({effect:e.id});}});
      $('fxList').append(b);});
    st.palettes.forEach(p=>{const b=document.createElement('button');b.className='row';b.dataset.pal=p.id;
      b.innerHTML='<span class="dot"></span><span></span><span class="grad"></span>';b.children[1].textContent=p.name;
      b.addEventListener('click',()=>setFx({palette:p.id}));$('palList').append(b);});}
  const q=$('fxq').value.trim().toLowerCase();
  const favs=st.favs||[];
  $('fxList').querySelectorAll('.row').forEach(r=>{r.classList.toggle('on',r.dataset.fx===st.fx.id);r.hidden=!!q&&!r.querySelector('.nt').textContent.toLowerCase().includes(q);
    const f=favs.includes(r.dataset.fx),s=r.querySelector('.star');s.classList.toggle('on',f);s.textContent=f?'★':'☆';s.setAttribute('aria-pressed',f);
    r.style.order=r.dataset.fx==='aus'?-2:f?-1:0;});
  pvStart();
  $('palList').querySelectorAll('.row').forEach(r=>{r.classList.toggle('on',r.dataset.pal===st.fx.pal);
    r.querySelector('.grad').style.background=palGrad(st.palettes.find(p=>p.id===r.dataset.pal));});
  setRange('fspeed',st.fx.speed);setRange('finten',st.fx.inten);
  if(document.activeElement!==$('fdir')&&!tSend.fdir){setRange('fdir',st.fx.dir||0);$('fdiro').textContent=dirText(st.fx.dir||0);}
  $('fspin').querySelectorAll('button').forEach(b=>b.classList.toggle('on',+b.dataset.s===(st.fx.spin||0)));
  $('fxNote').textContent=fxOn()?(st.fx.usesColor?'Die Effektfarbe stellst du im Tab Farben ein. ':'')+'Die Richtung wirkt bei Effekten, die über die Wand laufen, etwa Regenbogenwelle, Lauflicht, Polarlicht, Lava und Spirale.':'Einfarbig: jedes Panel leuchtet in seiner eigenen Farbe. Tempo, Intensität und Palette wirken erst mit einem Effekt.';
}
async function setFx(body){
  if(body.effect){st.fx.id=body.effect;live={};joining=[];}
  if(body.palette)st.fx.pal=body.palette;
  renderFx();                                        // sofort umschalten, nicht erst nach der Antwort
  const r=await api('/api/effect',body);if(r){render();pollLive();}}
$('fxq').addEventListener('input',renderFx);
// Vorschau: sieben Dreiecke pro Effekt, mit Tempo, Intensität, Palette und Farben der Wand
const PVN=7;let pvOn=false,pvLast=0,pvPh=0,pvSt={};
function pvBuild(svg){for(let i=0;i<PVN;i++){const up=i%2===0,x=4+i*8;svg.append(el('polygon',{points:up?`${x-8},16 ${x+8},16 ${x},2`:`${x-8},2 ${x+8},2 ${x},16`,stroke:'#0c0c0c','stroke-width':1}));}}
function pvHex(c){return[parseInt(c.slice(1,3),16),parseInt(c.slice(3,5),16),parseInt(c.slice(5,7),16),0];}
function pvHue(h){h-=Math.floor(h);const x=h*6,k=Math.floor(x),f=x-k,q=1-f;return[[1,f,0],[q,1,0],[0,1,f],[0,q,1],[f,0,1],[1,0,q]][k%6].map(v=>v*255).concat(0);}
function pvCol(t,def){t-=Math.floor(t);const F=st.fx,pal=F.pal;
  if(pal==='standard'){if(def==='h')return pvHue(t);if(def==='c')return[F.r,F.g,F.b,F.w];
    if(def==='f'){const h=.35+.65*t;return[255*h,85*h*h,0,20*h*h*h];}return pvHue(.36+.36*(.5+.5*Math.sin(t*6.2832)));}
  if(pal==='regenbogen')return pvHue(t);
  let S;if(pal==='effektfarbe'){const c=[F.r,F.g,F.b,F.w];S=[c.map(v=>v*.25),c,c.map((v,k)=>v*.6+(k<3?102:0))];}
  else S=((st.palettes.find(p=>p.id===pal)||{}).c||["#ff0000","#0000ff"]).map(pvHex);
  const n=S.length,x=t*n,a=Math.floor(x)%n,b=(a+1)%n;let f=x-Math.floor(x);f=f*f*(3-2*f);return S[a].map((v,k)=>v+(S[b][k]-v)*f);}
function pvPoint(id,i,ph,dt,s){const u=i/(PVN-1),K=st.fx.inten/255,dep=Math.abs(i-3),mul=(c,k)=>c.map(v=>v*k),R=Math.random;
  const A=s.a,B=s.b,T=s.t;
  switch(id){
    case'regenbogen':return pvCol(ph*.1+u*K*.6,'h');
    case'welle':return pvCol(ph*.15+u*(.2+1.6*K),'h');
    case'atmen':{const lo=.02+.45*(1-K);return mul(pvCol(ph*.05+u*.3,'c'),lo+(1-lo)*(.5+.5*Math.cos(ph*6.2832/4)));}
    case'farbwechsel':{T[i]+=dt/3.5;if(T[i]>=1){A[i]=B[i];B[i]=A[i]+(R()*2-1)*(.1+.4*K);T[i]=0;}let d=B[i]-A[i];d-=Math.floor(d+.5);const t=T[i]*T[i]*(3-2*T[i]);return pvCol(A[i]+d*t,'h');}
    case'funkeln':{A[i]=Math.max(0,A[i]-dt*2.2);if(R()<dt*(.05+.9*K))A[i]=1;const a=A[i]*A[i],c=mul(pvCol(u*.5+ph*.03,'c'),.18+.5*a);c[3]=Math.min(255,c[3]+255*a);return c;}
    case'ausbreiten':{const w=.5+.5*Math.cos(6.2832*(ph*.5-dep*.17));return mul(pvCol(ph*.1-dep*.08,'c'),.05+.95*Math.pow(w,1+7*(1-K)));}
    case'feuer':{A[i]+=(R()-A[i])*Math.min(1,dt*9);B[i]+=(A[i]-B[i])*Math.min(1,dt*5);const h=1-(.2+.8*K)*(1-B[i]);return st.fx.pal==='standard'?pvCol((h-.35)/.65,'f'):mul(pvCol(h*.5,'f'),h);}
    case'polarlicht':{const sv=.5+.5*Math.sin(u*5+ph*.9)*Math.sin(u*2.3-ph*.55);return mul(pvCol(ph*.04+u*.48,'a'),1-(.3+.7*K)*(1-sv));}
    case'lauflicht':{let d=u-(ph*.2-Math.floor(ph*.2));d-=Math.floor(d);d=1-d;return mul(pvCol(ph*.02,'c'),.03+.97*Math.exp(-d*(3+14*K)));}
    case'spirale':{const a=Math.atan2(i%2?-.3:.3,(i-3)*.5)/6.2832,r=Math.abs(i-3)/3;return pvCol(a+ph*.08+r*(.2+1.2*K),'h');}
    case'gewitter':{if(i===0){s.fl=Math.max(0,(s.fl||0)-dt*5);if(R()<dt*(.12+.6*K)){s.fl=.7+.3*R();s.fx=R()*PVN;}}const f=(s.fl||0)*Math.exp(-((i-s.fx)**2)/(1.5+3*K)*.5);
      const c=st.fx.pal!=='standard'?mul(pvCol(.1,'c'),.12):[2,6,34,0];return[c[0]+170*f,c[1]+170*f,c[2]+255*f,c[3]+255*f];}
    case'kerzen':{A[i]+=(R()-A[i])*Math.min(1,dt*6);B[i]+=(A[i]-B[i])*Math.min(1,dt*3);const h=1-(.15+.5*K)*(1-B[i]);return st.fx.pal==='standard'?[255*h,105*h*h,12*h,70*h*h]:mul(pvCol(B[i]*.3,'f'),h);}
    case'disco':{if(s.nb&&R()<.3+.7*K)A[i]=R();return pvCol(A[i],'h');}
    case'komet':{if(i===0){s.st=(s.st||0)+dt*4;while(s.st>=1){s.st--;s.hd=((s.hd||0)+1)%PVN;A[s.hd]=1;}}if(i!==s.hd)A[i]=Math.max(0,A[i]-dt*(2.2-1.6*K));const c=mul(pvCol(ph*.03,'c'),.02+.98*A[i]*A[i]);if(i===s.hd)c[3]=Math.min(255,c[3]+120);return c;}
    case'lava':{const sv=.5+.5*Math.sin(u*4+ph*.35+1.5*Math.sin(-ph*.25));return mul(pvCol(sv*.5+ph*.02,'f'),.2+.8*Math.pow(sv,1+2*K));}
    case'verlauf':{const t=u*(.6+1.4*K)+(st.fx.speed>1?ph*.02:0);if(st.fx.pal!=='standard')return pvCol(t*.5,'c');
      let m=t-Math.floor(t);m=m<.5?m*2:2-m*2;m=m*m*(3-2*m);const a=[st.fx.r,st.fx.g,st.fx.b,st.fx.w],c2=st.fx.c2||{r:0,g:80,b:255,w:0},b=[c2.r,c2.g,c2.b,c2.w];return a.map((v,k)=>v+(b[k]-v)*m);}
    default:return[st.fx.r,st.fx.g,st.fx.b,st.fx.w];}}
function pvFrame(now){
  if(!pvOn)return;if(tab!=='fx'||document.hidden||!st){pvOn=false;return;}
  requestAnimationFrame(pvFrame);if(now-pvLast<80)return;
  const rate=Math.pow(4,(st.fx.speed-50)/50),dt=Math.min(.2,(now-(pvLast||now))/1000)*rate;pvLast=now;pvPh+=dt;
  const beat=Math.floor(pvPh*2);
  $('fxList').querySelectorAll('.row').forEach(r=>{if(r.hidden)return;const id=r.dataset.fx;
    const s=pvSt[id]||(pvSt[id]={a:[...Array(PVN)].map(Math.random),b:[...Array(PVN)].map(Math.random),t:[...Array(PVN)].map(Math.random)});
    s.nb=beat!==s.bt;s.bt=beat;
    r.querySelectorAll('.pv polygon').forEach((p,i)=>{const c=pvPoint(id,i,pvPh,dt,s);const w=(c[3]||0)*.6;
      p.setAttribute('fill',`rgb(${[0,1,2].map(k=>Math.round(Math.max(0,Math.min(255,c[k]+w)))).join(',')})`);});});}
function pvStart(){if(pvOn||tab!=='fx')return;pvOn=true;pvLast=0;requestAnimationFrame(pvFrame);}
document.addEventListener('visibilitychange',()=>{if(!document.hidden&&tab==='fx')pvStart();});
$('fspeed').addEventListener('input',()=>{$('fspeedo').textContent=$('fspeed').value;later('fx',()=>api('/api/effect',{speed:+$('fspeed').value}));});
function dirText(d){return d+'° '+['→','↘','↓','↙','←','↖','↑','↗'][Math.round(d/45)%8];}
$('fdir').addEventListener('input',()=>{const d=+$('fdir').value;$('fdiro').textContent=dirText(d);later('fdir',()=>api('/api/effect',{direction:d}));});
$('fspin').querySelectorAll('button').forEach(b=>b.addEventListener('click',()=>{st.fx.spin=+b.dataset.s;renderFx();api('/api/effect',{spin:+b.dataset.s});}));
$('finten').addEventListener('input',()=>{$('finteno').textContent=$('finten').value;later('fx',()=>api('/api/effect',{intensity:+$('finten').value}));});

// ---------- Kopfzeile ----------
$('pwr').addEventListener('click',async()=>{if(!st)return;st.on=!st.on;render();await api('/api/set',{id:'alle',state:st.on?'ON':'OFF'});render();});
// Helligkeit in Prozent anzeigen, intern 1 bis 255
function pct(v){return Math.max(1,Math.round(v*100/255));}
function fromPct(p){return Math.max(1,Math.round(p*255/100));}
$('master').addEventListener('input',()=>{if(!st)return;const p=+$('master').value;$('mastero').textContent=p+' %';st.master=fromPct(p);
  later('m',async()=>{await api('/api/set',{id:'alle',brightness:st.master});render();},60);});

// ---------- Panel tauschen ----------
let swapSkip=new Set();
function renderSwap(){const p=st.panels.find(q=>q.swap&&!swapSkip.has(q.id));$('swapBanner').hidden=!p;if(!p)return;
  $('swapBanner').dataset.id=p.id;$('swapT').textContent=`Panel ${p.id.slice(4)} sitzt dort, wo Panel ${p.swap.slice(4)} war. Farbe, Helligkeit und Kanten übernehmen?`;}
async function swapDo(take){const id=$('swapBanner').dataset.id;swapSkip.add(id);renderSwap();
  if(await api('/api/swap',{id,take})&&take)toast('Einstellungen übernommen');render();}
$('swapYes').addEventListener('click',()=>swapDo(true));$('swapNo').addEventListener('click',()=>swapDo(false));

// ---------- Tab Wand ----------
function renderWall(){
  drawWall($('big'),true);renderEdges();renderView();
  const tray=$('tray');tray.innerHTML='';
  if(!st.loose.length){const e=document.createElement('span');e.className='note';e.textContent=st.sim?'Leer. Mit „Neues Panel“ eins dazunehmen oder ein Panel von der Wand hierher ziehen.':'Abgeklipste Panels erscheinen hier.';tray.append(e);}
  st.loose.forEach(id=>{const d=document.createElement('div');d.className='item';
    const s=el('svg',{viewBox:`${-S/2-4} ${-4} ${S+8} ${H+8}`});
    s.append(el('polygon',{points:pts([[-S/2,H],[S/2,H],[0,0]]),fill:'#1a1a1a',stroke:'#444','stroke-width':1.5}));
    const t=el('text',{x:0,y:H*.68,class:'lbl'});t.textContent=id.slice(4);s.append(t);d.append(s);tray.append(d);
    d.addEventListener('pointerdown',e=>startDrag(e,id,'tray'));});
  $('newBtn').hidden=!st.sim;
  $('bigHint').textContent=!st.sim?'Echte Panels: anklipsen, und sie erscheinen hier':st.loose.length?'Panel aus der Ablage an eine freie Kante ziehen':'Panel antippen für Details, wegziehen zum Abklipsen';
  const p=st.panels.find(q=>q.id===focus);$('panelCard').hidden=!p;
  if(p){
    $('pTitle').textContent=p.main?'Hauptpanel':'Panel '+p.id.slice(4);
    const stx=['dunkel, wird erkannt','pulsiert blau, wartet auf erste Farbe','leuchtet'][p.state];
    const par=st.panels.find(q=>q.id===p.parent);
    $('pInfo').innerHTML='';[['Chip-ID',p.id],['Zustand',stx],...(()=>{const q=diagData&&diagData.panels.find(x=>x.id===p.id);if(!q||!diagData.bus||p.main)return[];const pct=q.pings?Math.round(q.missed*1000/q.pings)/10:0;
      return[['Bus',`Adresse ${q.addr} · Antwort ${(q.rtt/1000).toFixed(2)} ms · ${pct} % verpasst · Firmware ${q.fw}`]];})(),['Position',`${p.x} / ${p.y} · Spitze ${p.up?'oben':'unten'}`],['Hängt an',p.main?'–':par?(par.main?'Hauptpanel':'Panel '+par.id.slice(4)):'–'],['Farbe',rgbHex(p.r,p.g,p.b)+(p.w?' + Weiß '+p.w:'')],
      ...(p.main?[]:[['Angeklipst',(p.clips||0)===1?'einmal':(p.clips||0)+'-mal'],['Firmware',p.fw?p.fw+(p.upd===2?` · Update läuft ${p.pct||0} %`:p.fw<st.pfw?` · Update auf ${st.pfw} verfügbar`:''):'–'],
        ['Antippen',!p.fw?'–':(p.caps&1)?'Sensor vorhanden':'kein Sensor']]),['Leuchtdauer',fmtDur(p.lit||0)]]
      .forEach(([k,v])=>{const a=document.createElement('span');a.textContent=k;const b=document.createElement('span');b.textContent=v;$('pInfo').append(a,b);});
    if(document.activeElement!==$('pbri')){setRange('pbri',pct(p.bri));$('pbrio').textContent=pct(p.bri)+' %';}
    $('pOn').classList.toggle('pri',p.on);$('pOn').textContent=p.on?'Ein':'Aus';
    if(document.activeElement!==$('pEdges'))$('pEdges').checked=!!p.edges;
    $('pTapRow').hidden=!st.sim||p.main;
  }
}
$('newBtn').addEventListener('click',async()=>{if(await api('/api/sim/new',{}))render();});
// Kanten einzeln: Liste zum An- und Abhaken, oder alle auf einmal
function renderEdges(){
  const ps=st.panels.slice().sort((a,b)=>(b.main-a.main)||a.id.localeCompare(b.id));
  $('edgeCard').hidden=!ps.length;
  const l=$('eList'),key=ps.map(p=>p.id).join();
  if(l.dataset.key!==key){l.dataset.key=key;l.innerHTML='';
    ps.forEach(p=>{const r=document.createElement('label');r.className='row erow';r.dataset.id=p.id;
      const c=document.createElement('input');c.type='checkbox';
      c.addEventListener('change',async()=>{focus=p.id;render();await api('/api/set',{id:p.id,edges:c.checked});render();});   // angehaktes Panel auf der Wand markieren
      const n=document.createElement('span');n.textContent=p.main?'Hauptpanel':'Panel '+p.id.slice(4);
      r.append(c,n);l.append(r);});}
  let on=0;l.querySelectorAll('.erow').forEach(r=>{const p=ps.find(q=>q.id===r.dataset.id);const c=r.firstChild;
    if(document.activeElement!==c)c.checked=!!p.edges;if(p.edges)on++;});
  $('eCount').textContent=on+' von '+ps.length+' an';
  $('eAllOn').disabled=on===ps.length;$('eAllOff').disabled=!on;
}
async function edgesAll(v){if(await api('/api/set',{ids:st.panels.map(p=>p.id),edges:v})){toast(v?'Kanten einzeln: alle an':'Kanten einzeln: alle aus');render();}}
$('eAllOn').addEventListener('click',()=>edgesAll(true));
$('eAllOff').addEventListener('click',()=>edgesAll(false));
$('pbri').addEventListener('input',()=>{$('pbrio').textContent=$('pbri').value+' %';const id=focus;later('pb',async()=>{await api('/api/set',{id,brightness:fromPct(+$('pbri').value)});render();});});
$('pOn').addEventListener('click',async()=>{const p=st.panels.find(q=>q.id===focus);if(!p)return;await api('/api/set',{id:p.id,state:p.on?'OFF':'ON'});render();});
$('pEdges').addEventListener('change',async()=>{if(!focus)return;await api('/api/set',{id:focus,edges:$('pEdges').checked});
  toast($('pEdges').checked?'Kanten einzeln an: wirkt bei Effekten':'Kanten einzeln aus');render();});
$('pTap1').addEventListener('click',()=>{if(focus)api('/api/sim/tap',{id:focus});});
$('pTap2').addEventListener('click',()=>{if(focus)api('/api/sim/tap',{id:focus,double:true});});
$('pCol').addEventListener('click',()=>{if(!focus)return;sel=new Set([focus]);showTab('col');});

// ---------- Tab Presets ----------
let askDel=null;
function renderPre(){
  const g=$('pgrid');g.innerHTML='';$('preEmpty').hidden=st.presets.length>0;
  st.presets.forEach(p=>{const b=document.createElement('div');b.className='preset'+(st.preset===p.id?' on':'');b.setAttribute('role','button');b.tabIndex=0;
    const n=document.createElement('span');n.textContent=p.name;n.dataset.nt='';b.append(n);
    const d=document.createElement('button');d.className='del'+(askDel===p.id?' ask':'');d.textContent=askDel===p.id?'Löschen?':'×';d.setAttribute('aria-label','Preset löschen');
    d.addEventListener('click',async e=>{e.stopPropagation();if(askDel!==p.id){askDel=p.id;renderPre();setTimeout(()=>{if(askDel===p.id){askDel=null;renderPre();}},3000);return;}
      askDel=null;await api('/api/presets',{action:'delete',id:p.id});toast('„'+p.name+'“ gelöscht');render();});
    b.append(d);
    b.addEventListener('click',async()=>{st.preset=p.id;renderPre();const r=await api('/api/presets',{action:'load',id:p.id});if(r){live={};render();pollLive();toast('„'+p.name+'“');}});
    g.append(b);});
}
$('psave').addEventListener('click',async()=>{const name=$('pname').value.trim();if(!name){toast('Bitte einen Namen eingeben');return;}
  const r=await api('/api/presets',{action:'save',name});if(r){$('pname').value='';toast('„'+name+'“ gespeichert');render();}});
$('pname').addEventListener('keydown',e=>{if(e.key==='Enter')$('psave').click();});

// ---------- Tab Optionen ----------
let cfg=null;const PIN_KEYS=['rx','tx','de','led','snsR','snsL'];
async function loadCfg(){
  renderWifi();loadOta();loadDiag();loadEnergy();
  try{cfg=await (await fetch('/api/config')).json();}catch(e){return;}
  if(st&&(st.ap||!st.ssid)&&!$('netList').children.length)$('scanBtn').click();   // ohne WLAN gleich nach Netzen suchen
  $('mode').value=cfg.mode;
  const b=$('board');b.innerHTML='';cfg.boards.forEach(x=>{const o=document.createElement('option');o.value=x.id;o.textContent=x.name;b.append(o);});
  const c=document.createElement('option');c.value='custom';c.textContent='Eigene Belegung';b.append(c);
  b.value=cfg.boards.some(x=>x.id===cfg.board)?cfg.board:'custom';
  PIN_KEYS.forEach(k=>{const s=$('p_'+k);s.innerHTML='';cfg.validPins.forEach(p=>{const o=document.createElement('option');o.value=p;o.textContent='GPIO '+p;s.append(o);});s.value=cfg.pins[k];});
  $('order').innerHTML='';cfg.orders.forEach(o=>{const e=document.createElement('option');e.value=o;e.textContent=o;$('order').append(e);});$('order').value=cfg.order;
  const L=cfg.light||{};
  $('onAnim').value=L.onAnim??2;
  $('lpwrOn').checked=L.pwrMax>0;$('lpwrBox').classList.toggle('off',!(L.pwrMax>0));
  $('lpwrMax').value=L.pwrMax?(L.pwrMax/1000).toLocaleString(LOC):'';$('lpwrCh').value=L.pwrCh||12;
  ['sda','scl'].forEach(k=>{const s=$('p_'+k);s.innerHTML='<option value="-1">kein Sensor</option>';const def=k==='sda'?L.defSda:L.defScl;
    cfg.validPins.forEach(p=>{const o=document.createElement('option');o.value=p;o.textContent='GPIO '+p+(p===def?' ★':'');s.append(o);});s.value=L[k]??-1;});
  $('p_shunt').value=((L.shunt||50)/10).toLocaleString(LOC);
  sensDef=L.defSda>=0?`★ = Vorgabe für dein Board: SDA GPIO ${L.defSda}, SCL GPIO ${L.defScl}. `:'';
  renderSensor(L.sda,L.sensor);
  renderBoot();
  if(cfg.touch){const T=cfg.touch;$('tOn').checked=T.on;$('tWave').checked=T.wave!==false;setRange('tSens',T.sens);$('tSenso').textContent=T.sens;
    ['tA1','tA2'].forEach((k,n)=>{const s=$(k);s.innerHTML='';T.actions.forEach((a,i)=>{const o=document.createElement('option');o.value=i;o.textContent=a;s.append(o);});s.value=n?T.a2:T.a1;});
    renderTouch();}
  $('guardOn').checked=cfg.guard!==false;
  $('m_on').checked=!!cfg.mqtt.on;$('mqttFields').classList.toggle('off',!cfg.mqtt.on);$('m_host').value=cfg.mqtt.host;$('m_port').value=cfg.mqtt.port;$('m_user').value=cfg.mqtt.user;
  $('info').innerHTML='';[['Chip',cfg.chip],['Firmware',cfg.ver],['Betriebsart',cfg.mode==='bus'?'Bus':'Simulation'],['WLAN',st&&st.ssid||'–'],['Letzter Neustart',cfg.why||'–'],['Abstürze',String(cfg.crashes||0)]]
    .forEach(([k,v])=>{const a=document.createElement('span');a.textContent=k;const x=document.createElement('span');x.textContent=v;$('info').append(a,x);});
}
$('guardOn').addEventListener('change',async()=>{const on=$('guardOn').checked;if(await api('/api/guard',{on}))toast(on?'WLAN-Wächter an':'WLAN-Wächter aus');});
$('board').addEventListener('change',()=>{const x=cfg.boards.find(b=>b.id===$('board').value);if(x)PIN_KEYS.forEach(k=>$('p_'+k).value=x.pins[k]);});
PIN_KEYS.forEach(k=>$('p_'+k).addEventListener('change',()=>{$('board').value='custom';}));
document.querySelectorAll('[data-t]').forEach(b=>b.addEventListener('click',()=>fetch('/api/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ch:+b.dataset.t})})));
$('saveBtn').addEventListener('click',async()=>{
  const pins={};PIN_KEYS.forEach(k=>pins[k]=+$('p_'+k).value);
  if(new Set(Object.values(pins)).size<6){$('setErr').textContent='Ein Pin ist doppelt belegt.';return;}
  const body={mode:$('mode').value,board:$('board').value,pins,order:$('order').value,mqtt:{on:$('m_on').checked,host:$('m_host').value.trim(),port:+$('m_port').value||1883,user:$('m_user').value.trim()}};
  if($('m_pass').value)body.mqtt.pass=$('m_pass').value;
  const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}).catch(()=>null);
  if(r&&!r.ok){const j=await r.json();$('setErr').textContent=j.error||'Speichern fehlgeschlagen';return;}
  $('setErr').textContent='';toast('Gespeichert. Trilumag startet neu …');setTimeout(()=>location.reload(),7000);});
$('m_on').addEventListener('change',()=>{$('mqttFields').classList.toggle('off',!$('m_on').checked);
  if($('m_on').checked&&!$('m_host').value.trim())$('m_host').focus();});
// Gefundene Netze als Liste zum Antippen, stärkstes zuerst
$('scanBtn').addEventListener('click',async()=>{$('scanBtn').disabled=true;$('scanBtn').textContent='Suche …';
  try{const r=await fetch('/api/wifi/scan');const j=await r.json();const box=$('netList');box.innerHTML='';
    const list=(j.list||j.networks.map(n=>({ssid:n,rssi:-70,lock:true}))).sort((a,b)=>b.rssi-a.rssi);
    list.forEach(n=>{const b=document.createElement('button');b.className='row';
      const lv=n.rssi>-55?4:n.rssi>-65?3:n.rssi>-75?2:1;
      const bars=document.createElement('span');bars.className='bars';bars.setAttribute('aria-label','Signal '+lv+' von 4');
      for(let k=1;k<=4;k++){const i=document.createElement('i');i.style.height=(k*3+2)+'px';if(k<=lv)i.className='on';bars.append(i);}
      const t=document.createElement('span');t.textContent=n.ssid;t.dataset.nt='';
      const sm=document.createElement('small');sm.textContent=n.lock?'gesichert':'offen';
      b.append(bars,t,sm);
      b.addEventListener('click',()=>{$('ssid').value=n.ssid;box.querySelectorAll('.row').forEach(x=>x.classList.toggle('on',x===b));$('pass').value='';$('pass').focus();});
      box.append(b);});
    toast(list.length?list.length+' Netze gefunden':'Keine Netze gefunden, bitte noch einmal suchen');}catch(e){toast('Suche fehlgeschlagen');}
  $('scanBtn').disabled=false;$('scanBtn').textContent='Netze suchen';});
$('wifiBtn').addEventListener('click',async()=>{const ssid=$('ssid').value.trim();if(!ssid){toast('Bitte WLAN-Namen eingeben');return;}
  await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pass:$('pass').value})}).catch(()=>{});
  toast('Gespeichert. Trilumag startet neu und verbindet sich mit '+ssid);
  $('wifiStat').classList.add('bad');$('wifiIc').textContent='…';$('wifiT').textContent=`Verbinde mit „${ssid}“ …`;
  $('wifiS').textContent='Trilumag startet neu. Lade diese Seite in etwa 20 Sekunden neu, im neuen WLAN unter http://trilumag.local.';});

// ---------- Licht: Übergänge und Stromlimit ----------
function fmtA(ma){return (ma/1000).toLocaleString(LOC,{minimumFractionDigits:1,maximumFractionDigits:1})+' A';}
function renderPower(){
  if(!st||!st.pwr)return;const p=st.pwr,meas=p.ma!=null;
  const ma=meas?p.ma:p.est,v=meas?p.v:24;
  $('pwrT').textContent=`${fmtA(ma)} · ${Math.round(ma/1000*v)} W ${meas?'gemessen':'geschätzt'}`;
  $('pwrS').textContent=(meas?`${String(p.v).replace('.',',')} V · `:'')+(p.lim?(p.scale<100?`Stromlimit greift: gedimmt auf ${p.scale} %`:`unter dem Limit von ${fmtA(p.lim)}`):'kein Stromlimit');
  $('pwrStat').classList.toggle('bad',!!p.lim&&p.scale<100);
  if(document.activeElement!==$('ltrans')){const t=Math.round(st.trans/100);setRange('ltrans',t);$('ltranso').textContent=(t/10).toLocaleString(LOC)+' s';}
}
$('ltrans').addEventListener('input',()=>{const t=+$('ltrans').value;$('ltranso').textContent=(t/10).toLocaleString(LOC)+' s';later('lt',()=>api('/api/light',{trans:t*100}));});
function sendPwr(){const on=$('lpwrOn').checked;$('lpwrBox').classList.toggle('off',!on);
  const a=parseFloat(($('lpwrMax').value||'').replace(',','.'));const ch=parseInt($('lpwrCh').value);
  const body={pwrMax:on&&a>0?Math.round(a*1000):0};if(ch>0)body.pwrCh=ch;
  if(on&&!(a>0)){$('lpwrMax').focus();return;}
  api('/api/light',body);}
$('lpwrOn').addEventListener('change',sendPwr);
$('lpwrMax').addEventListener('change',sendPwr);$('lpwrCh').addEventListener('change',sendPwr);

let sensDef='';
// Unterbereiche (Stromsensor, Ereignisse) auf- und zuklappen: zu, bis man sie einmal öffnet
[['sensFold','zu:sens'],['logFold','zu:log']].forEach(([id,key])=>{const f=$(id),h=f.querySelector('h3');
  try{if(localStorage.getItem(key)==='0')f.classList.remove('closed');}catch(e){}
  h.setAttribute('aria-expanded',!f.classList.contains('closed'));
  const t=()=>{f.classList.toggle('closed');const open=!f.classList.contains('closed');h.setAttribute('aria-expanded',open);try{localStorage.setItem(key,open?'0':'1');}catch(e){}};
  h.addEventListener('click',t);h.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();t();}});});
function renderSensor(sda,found){$('sensBadge').textContent=sda>=0?(found?'gefunden':'nicht gefunden'):'keiner';$('sensInfo').textContent=sensDef+(sda>=0?(found?'Sensor gefunden: Trilumag misst den echten Strom und regelt danach.':'Sensor nicht gefunden. Verkabelung prüfen (SDA, SCL, 3,3 V, GND), dann hier die Pins neu wählen.'):'Kein Sensor eingestellt. Der Strom wird aus den Farben geschätzt.');}
async function sendSensor(){
  const sda=+$('p_sda').value,scl=+$('p_scl').value;
  if((sda<0)!==(scl<0)){$('sensInfo').textContent='Bitte beide Pins wählen oder bei beiden „kein Sensor“.';return;}
  const shunt=Math.round(parseFloat(($('p_shunt').value||'5').replace(',','.'))*10);
  const r=await api('/api/light',{sda,scl,shunt});
  setTimeout(async()=>{try{const s2=await (await fetch('/api/state')).json();renderSensor(s2.pwr.sda,s2.pwr.sensor);}catch(e){}},600);}
['p_sda','p_scl','p_shunt'].forEach(k=>$(k).addEventListener('change',sendSensor));

// ---------- Bus-Diagnose ----------
let diagData=null;
// ---------- Antippen ----------
function renderTouch(){
  if(!st||!cfg||!cfg.touch)return;
  $('tBox').classList.toggle('off',!$('tOn').checked);
  const ps=st.panels.filter(p=>!p.main&&p.fw),n=ps.filter(p=>p.caps&1).length;
  $('tNote').textContent=(st.sim?'In der Simulation probierst du es unter Wand aus: Panel antippen, dann „einmal“ oder „doppelt“. ':
    ps.length?`${n} von ${ps.length} Panels haben einen Bewegungssensor. `:'')+'In Home Assistant gibt es dazu das Ereignis „Antippen“ für eigene Automationen.';
}
function saveTouch(){renderTouch();later('touch',()=>api('/api/touch',{on:$('tOn').checked,wave:$('tWave').checked,sens:+$('tSens').value,a1:+$('tA1').value,a2:+$('tA2').value}).then(()=>{
  if(cfg&&cfg.touch)Object.assign(cfg.touch,{on:$('tOn').checked,sens:+$('tSens').value,a1:+$('tA1').value,a2:+$('tA2').value});}));}
$('tOn').addEventListener('change',saveTouch);$('tWave').addEventListener('change',saveTouch);
$('tSens').addEventListener('input',()=>{$('tSenso').textContent=$('tSens').value;saveTouch();});
['tA1','tA2'].forEach(k=>$(k).addEventListener('change',saveTouch));

// ---------- Panel-Firmware ----------
function renderPfw(){
  if(!st)return;
  const ps=st.panels.filter(p=>!p.main&&p.fw);
  const old=ps.filter(p=>p.fw<st.pfw||p.upd);
  const busy=ps.find(p=>p.upd===2);
  $('pfwStat').classList.toggle('bad',old.length>0);
  $('pfwIc').textContent=old.length?'↑':'✓';
  $('pfwT').textContent=busy?`Panel ${busy.id.slice(4)} wird aktualisiert`:old.length?`${old.length} Panel${old.length>1?'s':''} mit älterer Firmware`:'Alle Panels sind aktuell';
  $('pfwS').textContent=`Aktuelle Panel-Firmware: ${st.pfw}`+(ps.length?` · ${ps.length} Panel${ps.length>1?'s':''} an der Wand`:'');
  if(document.activeElement!==$('pfwAuto'))$('pfwAuto').checked=!!st.pAuto;
  $('pfwAll').hidden=!old.some(p=>!p.upd||p.upd===3)||!old.some(p=>st.sim||(p.caps&2));
  const l=$('pfwList');l.innerHTML='';
  old.forEach(p=>{const r=document.createElement('div');r.className='prow';
    const t=document.createElement('div');t.className='top';
    const n=document.createElement('span');n.textContent='Panel '+p.id.slice(4);
    const sm=document.createElement('small');
    sm.textContent=p.upd===2?`Firmware ${p.fw} → ${st.pfw} · ${p.pct||0} %`:p.upd===1?'wartet':p.upd===3?'fehlgeschlagen, wird erneut versucht':
      (!st.sim&&!(p.caps&2))?`Firmware ${p.fw} · ohne Bootloader, nur mit Programmieradapter`:`Firmware ${p.fw} → ${st.pfw}`;
    t.append(n,sm);
    if(p.upd!==2&&p.upd!==1&&(st.sim||(p.caps&2))){const b=document.createElement('button');b.className='btn';b.textContent=p.upd===3?'Nochmal':'Aktualisieren';
      b.addEventListener('click',()=>api('/api/panelfw',{action:'one',id:p.id}));t.append(b);}
    r.append(t);
    if(p.upd===2){const g=document.createElement('div');g.className='prog';const i=document.createElement('i');i.style.width=(p.pct||0)+'%';g.append(i);r.append(g);}
    l.append(r);});
  l.hidden=!old.length;
}
$('pfwAuto').addEventListener('change',async()=>{await api('/api/panelfw',{action:'auto',on:$('pfwAuto').checked});
  toast($('pfwAuto').checked?'Panels werden automatisch aktualisiert':'Panel-Updates nur noch per Knopf');});
$('pfwAll').addEventListener('click',()=>api('/api/panelfw',{action:'all'}));

// ---------- Philips Hue (Zigbee, nur ESP32-C6) ----------
function renderZb(){
  const z=st&&st.zb;$('zbCard').hidden=!z;if(!z)return;
  const na=z.avail===false;                             // anderer Chip als ESP32-C6: sichtbar, aber ausgegraut
  $('zbCard').classList.toggle('na',na);$('zbOn').disabled=na;$('zbNa').hidden=!na;
  if(na){$('zbOn').checked=false;$('zbStat').classList.add('bad');$('zbIc').textContent='–';$('zbT').textContent='Nur mit ESP32-C6';
    $('zbS').textContent=`Dein Hauptpanel hat einen ${st.chip||'anderen Chip'} ohne Zigbee-Funk`;$('zbHow').hidden=true;$('zbPair').hidden=true;return;}
  if(document.activeElement!==$('zbOn'))$('zbOn').checked=z.on;
  const ok=z.run&&z.join;
  $('zbStat').classList.toggle('bad',z.on&&!ok);
  $('zbIc').textContent=!z.on?'–':ok?'✓':'…';
  $('zbT').textContent=!z.on?'Zigbee ist aus':z.err?'Zigbee läuft nicht':ok?'Mit der Hue Bridge verbunden':'Wartet auf die Hue Bridge';
  $('zbS').textContent=!z.on?'Zum Koppeln mit Hue einschalten':z.err?z.err:ok?`Zigbee-Kanal ${z.ch}`:'Jetzt in der Hue-App nach neuen Lampen suchen';
  $('zbHow').hidden=!z.on||ok;$('zbPair').hidden=!z.run;
}
$('zbOn').addEventListener('change',async()=>{const v=$('zbOn').checked;
  const r=await api('/api/zigbee',{action:'on',value:v});if(r){toast(v?'Zigbee an, Trilumag startet neu …':'Zigbee aus, Trilumag startet neu …');setTimeout(()=>location.reload(),9000);}});
$('zbPair').addEventListener('click',async()=>{const b=$('zbPair');
  if(!b.dataset.ask){b.dataset.ask=1;b.textContent='Wirklich? Trilumag verlässt das Hue-Netz';setTimeout(()=>{delete b.dataset.ask;b.textContent='Neu koppeln';},4000);return;}
  delete b.dataset.ask;b.textContent='Neu koppeln';
  const r=await api('/api/zigbee',{action:'pair'});if(r){toast('Zigbee wird zurückgesetzt, Trilumag startet neu …');setTimeout(()=>location.reload(),9000);}});

// ---------- Name der Wand ----------
$('wname').addEventListener('input',()=>{const n=$('wname').value.trim();if(!n)return;$('wallName').textContent=n;document.title=n;
  later('name',async()=>{if(await api('/api/name',{name:n}))toast('Name gespeichert');},700);});
$('wname').addEventListener('blur',()=>{if(!$('wname').value.trim()&&st)$('wname').value=st.name;});

// ---------- Dauer lesbar ----------
function fmtDur(s){const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h>=100?h+' h':h?`${h} h ${m} min`:`${m} min`;}

// ---------- Panel finden ----------
$('pFind').addEventListener('click',()=>{if(!focus)return;api('/api/identify',{id:focus});toast('Das Panel blinkt jetzt 3 Sekunden weiß');});

// ---------- Malen ----------
const SW=[['#ff2a1a',[255,0,0,0]],['#ff7a1a',[255,90,0,0]],['#ffd21a',[255,190,0,0]],['#34d058',[0,255,40,0]],['#1ad8ff',[0,220,255,0]],['#2a5bff',[0,40,255,0]],
  ['#9b3cff',[140,0,255,0]],['#ff3ca8',[255,0,140,0]],['#ffe6c0',[0,0,0,255]],['#ffffff',[255,255,255,255]]];
let paintCol=SW[0][1],painted=null;
function paintOn(){return $('paintOn').checked;}
(()=>{const w=$('swatches');
  SW.forEach(([hx,c],k)=>{const b=document.createElement('button');b.style.background=hx;b.title=k===8?'Warmweiß':k===9?'Kaltweiß':hx;b.setAttribute('aria-label',b.title);
    if(!k)b.classList.add('on');b.addEventListener('click',()=>{paintCol=c;mark(b);});w.append(b);});
  const off=document.createElement('button');off.className='off';off.textContent='×';off.title='Aus';off.setAttribute('aria-label','Aus');
  off.addEventListener('click',()=>{paintCol=null;mark(off);});w.append(off);
  const l=document.createElement('label');l.title='Eigene Farbe';l.innerHTML='＋<input type="color" value="#ff9a3c" aria-label="Eigene Farbe">';
  l.querySelector('input').addEventListener('input',e=>{const v=e.target.value;paintCol=[parseInt(v.slice(1,3),16),parseInt(v.slice(3,5),16),parseInt(v.slice(5,7),16),0];l.style.background=v;mark(l);});w.append(l);
  function mark(el){w.querySelectorAll('.on').forEach(x=>x.classList.remove('on'));el.classList.add('on');}})();
$('paintOn').addEventListener('change',()=>{$('bigBox').classList.toggle('paint',paintOn());focus=null;render();
  if(paintOn())toast('Farbe wählen und über die Panels wischen');});
function paintAt(x,y){const el=document.elementFromPoint(x,y);const id=el&&el.dataset&&el.dataset.id;if(!id||painted.has(id))return;painted.add(id);
  const p=st.panels.find(q=>q.id===id);if(!p)return;
  if(paintCol){if(st.fx.id!=='aus'){st.fx.id='aus';live={};}Object.assign(p,{r:paintCol[0],g:paintCol[1],b:paintCol[2],w:paintCol[3],on:true,state:2});
    api('/api/set',{id,state:'ON',color:{r:paintCol[0],g:paintCol[1],b:paintCol[2],w:paintCol[3]}});}
  else{p.on=false;api('/api/set',{id,state:'OFF'});}
  drawWall($('big'),true);}
function paintStart(e){e.preventDefault();painted=new Set();const svg=$('big');svg.setPointerCapture(e.pointerId);paintAt(e.clientX,e.clientY);
  const mv=ev=>paintAt(ev.clientX,ev.clientY);
  const up=()=>{svg.removeEventListener('pointermove',mv);svg.removeEventListener('pointerup',up);svg.removeEventListener('pointercancel',up);painted=null;};
  svg.addEventListener('pointermove',mv);svg.addEventListener('pointerup',up);svg.addEventListener('pointercancel',up);}

// ---------- Sleep-Timer ----------
function renderSleep(){const s=st&&st.sleep;$('sleepBtn').classList.toggle('on',!!s);$('sleepLeft').hidden=!s;
  if(s){const m=Math.ceil(s/60);$('sleepLeft').textContent=m>=60?Math.floor(m/60)+':'+String(m%60).padStart(2,'0'):m+'′';}
  $('sleepOff').hidden=!s;$('sleepInfo').textContent=s?`Die Wand geht in ${fmtDur(s)} aus. Neu wählen oder beenden:`:'Die Wand blendet langsam aus und geht dann aus.';}
$('sleepBtn').addEventListener('click',e=>{e.stopPropagation();$('sleepPop').hidden=!$('sleepPop').hidden;});
document.addEventListener('click',e=>{if(!$('sleepPop').hidden&&!$('sleepPop').contains(e.target))$('sleepPop').hidden=true;});
$('sleepChips').querySelectorAll('button').forEach(b=>b.addEventListener('click',async()=>{const m=+b.dataset.m;$('sleepPop').hidden=true;
  if(await api('/api/sleep',{min:m}))toast(`Die Wand geht in ${m>=60?(m/60).toLocaleString(LOC)+' h':m+' min'} aus`);}));
$('sleepOff').addEventListener('click',async()=>{$('sleepPop').hidden=true;if(await api('/api/sleep',{min:0}))toast('Sleep-Timer beendet');});

// ---------- Nach Stromausfall ----------
function renderBoot(){if(!cfg||!cfg.boot||!st)return;
  const sel=$('bootPre');if(document.activeElement!==sel){sel.innerHTML='';st.presets.forEach(p=>{const o=document.createElement('option');o.value=p.id;o.textContent=p.name;sel.append(o);});
    if(cfg.boot.preset>=0)sel.value=cfg.boot.preset;}
  if(document.activeElement!==$('bootMode'))$('bootMode').value=cfg.boot.mode;
  $('bootPreBox').hidden=$('bootMode').value!=='3';}
async function saveBoot(){const m=+$('bootMode').value;$('bootPreBox').hidden=m!==3;
  if(m===3&&!st.presets.length){toast('Zuerst unter Presets eins speichern');$('bootMode').value=cfg.boot.mode;$('bootPreBox').hidden=cfg.boot.mode!==3;return;}
  const body={mode:m};if(m===3)body.preset=+($('bootPre').value||st.presets[0].id);
  if(await api('/api/boot',body)){cfg.boot.mode=m;if(m===3)cfg.boot.preset=body.preset;toast('Gespeichert');}}
$('bootMode').addEventListener('change',saveBoot);$('bootPre').addEventListener('change',saveBoot);

// ---------- Energie ----------
let enData=null,enRange='days';
function fmtWh(wh){return wh>=1000?(wh/1000).toLocaleString(LOC,{maximumFractionDigits:wh>=100000?0:2})+' kWh':wh.toLocaleString(LOC,{maximumFractionDigits:wh<10?1:0})+' Wh';}
async function loadEnergy(){try{enData=await (await fetch('/api/energy')).json();renderEnergy();}catch(e){}}
function keyLabel(k,r,long){const s=String(k);const M=LANG==='de'?['Jän','Feb','Mär','Apr','Mai','Jun','Jul','Aug','Sep','Okt','Nov','Dez']:['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  if(r==='days')return long?`${+s.slice(6)}. ${M[+s.slice(4,6)-1]} ${s.slice(0,4)}`:String(+s.slice(6));
  if(r==='months')return long?`${M[+s.slice(4)-1]} ${s.slice(0,4)}`:M[+s.slice(4)-1];return s;}
function renderEnergy(){const d=enData;if(!d)return;
  $('eToday').textContent=d.time?fmtWh(d.today):'–';$('eMonth').textContent=d.time?fmtWh(d.month):'–';$('eYear').textContent=d.time?fmtWh(d.year):'–';$('eTotal').textContent=fmtWh(d.total);
  $('eNote').textContent=(d.meas?'Gemessen mit dem Stromsensor':'Geschätzt aus den Farben (ohne Stromsensor)')+` · gerade ${d.w.toLocaleString(LOC)} W`+(d.time?'':' · Datum noch unbekannt (kein Internet?), gezählt wird trotzdem');
  const rows=(d[enRange]||[]).slice().reverse();         // älteste links
  const svg=$('eChart'),W=340,H=150,top=10,bot=20,left=4;svg.innerHTML='';$('eTip').textContent='';
  if(!rows.length){svg.innerHTML='<text x="170" y="75" text-anchor="middle">Noch keine Werte</text>';return;}
  const max=Math.max(...rows.map(r=>r[1]),1),n=enRange==='days'?31:enRange==='months'?24:10,bw=(W-left)/n;
  const ns='http://www.w3.org/2000/svg';const mk=(t,a)=>{const e=document.createElementNS(ns,t);for(const k in a)e.setAttribute(k,a[k]);return e;};
  svg.append(mk('line',{x1:0,x2:W,y1:H-bot+.5,y2:H-bot+.5}));
  rows.forEach((r,i)=>{const x=left+(n-rows.length+i)*bw,h=Math.max(r[1]>0?2:0,(H-top-bot)*r[1]/max);
    const b=mk('path',{class:'bar'+(i===rows.length-1?' cur':''),d:`M${x+1} ${H-bot}V${H-bot-h+Math.min(4,h)}q0 -${Math.min(4,h)} ${Math.min(4,h)} -${Math.min(4,h)}H${x+bw-1-Math.min(4,h)}q${Math.min(4,h)} 0 ${Math.min(4,h)} ${Math.min(4,h)}V${H-bot}Z`});
    const hit=mk('rect',{x:x,y:top,width:bw,height:H-top,fill:'transparent'});
    const tip=()=>{$('eTip').textContent=`${keyLabel(r[0],enRange,true)}: ${fmtWh(r[1])}`;};
    hit.addEventListener('pointerenter',tip);hit.addEventListener('click',tip);
    svg.append(b,hit);
    const every=enRange==='days'?5:enRange==='months'?3:1;
    if((rows.length-1-i)%every===0){const t=mk('text',{x:x+bw/2,y:H-6,'text-anchor':'middle'});t.textContent=keyLabel(r[0],enRange,false);svg.append(t);}});
  const mt=mk('text',{x:W-2,y:top+2,'text-anchor':'end'});mt.textContent='max '+fmtWh(max);svg.append(mt);}
$('eTabs').querySelectorAll('button').forEach(b=>b.addEventListener('click',()=>{enRange=b.dataset.r;$('eTabs').querySelectorAll('button').forEach(x=>x.classList.toggle('on',x===b));renderEnergy();}));
$('eReset').addEventListener('click',async()=>{const b=$('eReset');
  if(!b.dataset.ask){b.dataset.ask=1;b.textContent='Wirklich alle Werte löschen?';setTimeout(()=>{delete b.dataset.ask;b.textContent='Zähler zurücksetzen';},4000);return;}
  delete b.dataset.ask;b.textContent='Zähler zurücksetzen';if(await api('/api/energy',{action:'reset'})){toast('Energiezähler zurückgesetzt');loadEnergy();}});
setInterval(()=>{if(!document.hidden&&tab==='opt'&&!$('energyCard').classList.contains('closed'))loadEnergy();},10000);

// ---------- Mehrere Wände ----------
(()=>{const s=$('syncGrp');for(let g=1;g<=9;g++){const o=document.createElement('option');o.value=g;o.textContent='Gruppe '+g;s.append(o);}})();
function renderSync(){const y=st&&st.sync;if(!y)return;
  if(document.activeElement!==$('syncOn'))$('syncOn').checked=y.on;if(document.activeElement!==$('syncGrp'))$('syncGrp').value=y.group;
  const l=$('syncList');l.innerHTML='';
  const rows=y.on?[[st.name+' (diese Wand)',y.lead?'gibt den Takt vor':'',true],...y.walls.map(w=>[w.name,w.ip+(w.group!==y.group?` · Gruppe ${w.group}, läuft nicht mit`:''),w.group===y.group])]:[];
  if(y.on&&!y.walls.length)rows.push(['Noch keine andere Wand gefunden','Sie muss im selben WLAN sein und den Gleichtakt eingeschaltet haben',false]);
  rows.forEach(([a,b,ok])=>{const r=document.createElement('div');r.className='prow';const t=document.createElement('div');t.className='top';
    const n=document.createElement('span');n.textContent=a;const m=document.createElement('small');m.textContent=b;if(!ok)n.style.color='var(--muted)';t.append(n,m);r.append(t);l.append(r);});
  l.hidden=!rows.length;$('syncGrp').closest('label').classList.toggle('off',!y.on);}
$('syncOn').addEventListener('change',async()=>{const v=$('syncOn').checked;if(await api('/api/sync',{on:v,group:+$('syncGrp').value}))toast(v?'Gleichtakt an: andere Wände der Gruppe laufen mit':'Gleichtakt aus');});
$('syncGrp').addEventListener('change',()=>api('/api/sync',{group:+$('syncGrp').value}));

$('onAnim').addEventListener('change',async()=>{if(await api('/api/light',{onAnim:+$('onAnim').value})){if(cfg&&cfg.light)cfg.light.onAnim=+$('onAnim').value;toast('Gespeichert');}});

// ---------- Ansicht drehen und spiegeln ----------
function renderView(){const v=st&&st.view;if(!v)return;$('viewT').textContent=`${v.rot}°${v.mir?' · gespiegelt':''}`;$('viewMir').classList.toggle('pri',!!v.mir);}
async function setView(b){Object.assign(st.view,b);render();await api('/api/view',b);}
$('viewL').addEventListener('click',()=>setView({rot:(st.view.rot+330)%360}));
$('viewR').addEventListener('click',()=>setView({rot:(st.view.rot+30)%360}));
$('viewMir').addEventListener('click',()=>setView({mir:!st.view.mir}));

// ---------- Signale und Fortschritt ----------
let sigC='blau';
$('sigCols').querySelectorAll('button').forEach(b=>b.addEventListener('click',()=>{sigC=b.dataset.c;$('sigCols').querySelectorAll('button').forEach(x=>x.classList.toggle('on',x===b));}));
$('sigTest').addEventListener('click',()=>api('/api/signal',{color:sigC,blink:3}));
$('progR').addEventListener('input',()=>{const v=+$('progR').value;$('progRo').textContent=v?v+' %':'aus';later('prog',()=>api('/api/progress',{value:v,color:sigC}));});
function renderSig(){if(!st||document.activeElement===$('progR')||tSend.prog)return;const v=Math.round(st.prog||0);setRange('progR',v);$('progRo').textContent=v?v+' %':'aus';}

function ago(s){return s<60?`vor ${s} s`:s<3600?`vor ${Math.round(s/60)} min`:`vor ${Math.round(s/3600)} h`;}
async function loadDiag(){try{diagData=await (await fetch('/api/diag')).json();renderDiag();if(tab==='wall')renderWall();}catch(e){}}
function renderDiag(){
  const d=diagData;if(!d)return;
  $('diagSum').textContent=d.bus?`Laufzeit ${Math.floor(d.up/3600)} h ${Math.floor(d.up%3600/60)} min · ${d.frames} Bilder gesendet · ${d.discovers} Erkennungsrunden · ${d.timeouts} verpasste Antworten · ${d.crc} gestörte Übertragungen`:
    'Simulation: Antwortzeiten und Fehler gibt es erst im Busbetrieb. Das Ereignisprotokoll läuft trotzdem.';
  const tb=$('diagRows');tb.innerHTML='';
  d.panels.forEach(p=>{const tr=document.createElement('tr');const pct=p.pings?Math.round(p.missed*1000/p.pings)/10:0;
    const cells=[p.id.slice(4),p.addr||'–',p.rtt?(p.rtt/1000).toFixed(2)+' ms':'–',p.pings?pct+' %':'–',p.fw||'–',(p.clips||0)+'×',p.lit!=null?(p.lit/3600).toLocaleString(LOC,{minimumFractionDigits:1,maximumFractionDigits:1}):'–'];
    cells.forEach((c,i)=>{const td=document.createElement('td');td.textContent=c;if(i===3&&pct>=5)td.className=pct>=20?'bad':'warn';tr.append(td);});tb.append(tr);});
  $('logBadge').textContent=d.log.length?d.log.length+(d.log.length===1?' Eintrag':' Einträge'):'keine';
  const lg=$('diagLog');lg.innerHTML='';
  if(!d.log.length){const x=document.createElement('p');x.className='note';x.textContent='Noch keine Ereignisse.';lg.append(x);}
  d.log.forEach(e=>{const r=document.createElement('div');const t=document.createElement('span');t.textContent=ago(e.ago);const m=document.createElement('p');m.style.margin='0';m.textContent=e.m;r.append(t,m);lg.append(r);});
}
$('diagReset').addEventListener('click',async()=>{await api('/api/diag',{});loadDiag();});
setInterval(()=>{if(document.hidden)return;if((tab==='opt'&&!$('diagCard').classList.contains('closed'))||(tab==='wall'&&focus))loadDiag();},3000);

// ---------- Sicherung ----------
$('bkUp').addEventListener('click',async()=>{const f=$('bkFile').files[0];if(!f){toast('Bitte zuerst eine Sicherungsdatei wählen');return;}
  const txt=await f.text();
  const r=await fetch('/api/restore',{method:'POST',headers:{'Content-Type':'application/json'},body:txt}).catch(()=>null);
  if(r&&r.ok){toast('Sicherung eingespielt, Trilumag startet neu …');setTimeout(()=>location.reload(),9000);}
  else{let m='Einspielen fehlgeschlagen';try{m=(await r.json()).error||m;}catch(e){}toast(m);}});

// ---------- WLAN-Anzeige ----------
function quality(r){return r>=-55?'sehr gut':r>=-65?'gut':r>=-75?'mittel':'schwach';}
function renderWifi(){
  if(!st)return;const ok=!!st.ssid;
  $('wifiStat').classList.toggle('bad',!ok);$('wifiIc').textContent=ok?'✓':'!';
  $('wifiT').textContent=ok?`Verbunden mit „${st.ssid}“`:'Nicht mit einem WLAN verbunden';
  $('wifiS').textContent=ok?`Empfang ${quality(st.rssi)} (${st.rssi} dBm) · IP ${st.ip} · trilumag.local`:(st.ap?'Das Setup-Netz „Trilumag-Setup“ ist offen. Wähle unten dein WLAN.':'Wähle unten dein WLAN.');
  $('wifiInfo').textContent=ok?'Anderes WLAN wählen:':'';
}

// ---------- Updates ----------
let otaInfo=null,askVer=null;
async function loadOta(){if(wsOk&&otaInfo){renderOta();return;}try{otaInfo=await (await fetch('/api/ota')).json();renderOta();}catch(e){}}
function cmpV(a,b){const x=a.split('.').map(Number),y=b.split('.').map(Number);for(let i=0;i<Math.max(x.length,y.length);i++){const d=(x[i]||0)-(y[i]||0);if(d)return d<0?-1:1;}return 0;}
function renderOta(){
  const o=otaInfo;if(!o)return;
  const installing=o.busy&&o.p>=0;
  $('otaProg').hidden=!installing;$('otaBar').style.width=(installing?o.p:0)+'%';
  const bad=!!o.err&&!installing;$('otaStat').classList.toggle('bad',bad);
  if(!installing&&o.verifying){$('otaStat').classList.remove('bad');$('otaIc').textContent='…';$('otaT').textContent=`Version ${o.cur} bewährt sich noch`;$('otaS').textContent='In den ersten 45 Sekunden springt Trilumag bei einem Absturz automatisch auf die vorige Version zurück.';}
  else if(installing){$('otaIc').textContent='↓';$('otaT').textContent=`Installiere … ${o.p} %`;$('otaS').textContent='Nicht ausschalten. Danach startet Trilumag neu.';}
  else if(bad){$('otaIc').textContent='!';$('otaT').textContent=`Version ${o.cur}`;$('otaS').textContent=o.err;}
  else if(o.notice&&o.notice.includes('nicht richtig')){$('otaStat').classList.add('bad');$('otaIc').textContent='↺';$('otaT').textContent='Zurück auf die vorige Version';$('otaS').textContent=o.notice;}
  else if(o.newer){$('otaIc').textContent='↑';$('otaT').textContent=`Version ${o.latest} ist verfügbar`;$('otaS').textContent=`Installiert ist ${o.cur}.`;}
  else{$('otaIc').textContent='✓';$('otaT').textContent=`Version ${o.cur}`;
    $('otaS').textContent=o.busy?'Suche nach Updates …':o.ago<0?'Noch nicht nach Updates gesucht.':`Aktuell · zuletzt geprüft vor ${o.ago<90?'1':Math.round(o.ago/60)} Min.`;}
  if(document.activeElement!==$('otaAuto'))$('otaAuto').checked=!!o.auto;
  $('otaCheck').disabled=!!o.busy;
  const L=$('otaList');L.innerHTML='';
  o.versions.forEach(v=>{const c=cmpV(v.v,o.cur);const box=document.createElement('div');box.className='vbox';
    const r=document.createElement('div');r.className='row';box.append(r);
    const info=document.createElement('div');info.className='vinfo';const b=document.createElement('b');b.textContent=v.v;
    const sp=document.createElement('span');sp.textContent=[v.date,v.notes].filter(Boolean).join(' · ');
    const tg=document.createElement('button');tg.className='lnk';const open=openLog.has(v.v);
    tg.textContent=open?'Änderungen ausblenden':'Änderungen anzeigen';tg.setAttribute('aria-expanded',open);
    tg.addEventListener('click',()=>{if(openLog.has(v.v))openLog.delete(v.v);else{openLog.add(v.v);loadLog();}renderOta();});
    info.append(b,sp,tg);
    if(open){const cl=document.createElement('div');cl.className='clog';
      if(logData&&logData[v.v])cl.innerHTML=md(logData[v.v]);
      else if(logData===false)cl.innerHTML='<p>Der Versionsverlauf lässt sich gerade nicht laden (kein Internet?). Er steht auch auf <a href="https://github.com/mariofritzer/Trilumag/blob/main/CHANGELOG.md" target="_blank" rel="noopener">GitHub</a>.</p>';
      else if(logData)cl.textContent=v.notes||'Keine Beschreibung vorhanden.';
      else cl.textContent='Lade …';
      box.append(cl);}
    const tag=document.createElement('span');tag.className='tag'+(c>0?' new':'');tag.textContent=c===0?'installiert':v.v===o.bad?'startete nicht':c>0?'neuer':'älter';
    r.append(info,tag);
    if(c!==0){const btn=document.createElement('button');btn.className='ib'+(askVer===v.v?' ask':'');btn.disabled=!!o.busy;
      btn.textContent=askVer===v.v?(c>0?'Wirklich?':'Wirklich zurück?'):(c>0?'Installieren':'Zurück');
      btn.addEventListener('click',async()=>{if(askVer!==v.v){askVer=v.v;renderOta();setTimeout(()=>{if(askVer===v.v){askVer=null;renderOta();}},4000);return;}
        askVer=null;otaInfo.busy=true;otaInfo.p=0;renderOta();
        const r2=await api('/api/ota',{action:'install',version:v.v});if(!wsOk&&r2){toast('Update läuft, die Seite lädt danach neu');setTimeout(()=>location.reload(),60000);}});
      r.append(btn);}
    L.append(box);});
}
$('otaAuto').addEventListener('change',async()=>{otaInfo&&(otaInfo.auto=$('otaAuto').checked);await api('/api/ota',{action:'auto',on:$('otaAuto').checked});
  toast($('otaAuto').checked?'Automatische Updates an':'Automatische Updates aus');if(!wsOk)loadOta();});
$('otaCheck').addEventListener('click',async()=>{if(otaInfo){otaInfo.busy=true;renderOta();}await api('/api/ota',{action:'check'});if(!wsOk)setTimeout(loadOta,4000);});
$('otaUp').addEventListener('click',()=>{const f=$('otaFile').files[0];if(!f){toast('Bitte zuerst eine .bin-Datei wählen');return;}
  const fd=new FormData();fd.append('firmware',f,f.name);const x=new XMLHttpRequest();x.open('POST','/update');
  $('otaProg').hidden=false;x.upload.onprogress=e=>{if(e.lengthComputable){$('otaBar').style.width=(e.loaded/e.total*100)+'%';$('otaT').textContent=`Lade hoch … ${Math.round(e.loaded/e.total*100)} %`;}};
  x.onload=()=>{if(x.status===200){toast('Installiert, Trilumag startet neu …');setTimeout(()=>location.reload(),9000);}else{let m='Hochladen fehlgeschlagen';try{m=JSON.parse(x.responseText).error||m;}catch(e){}toast(m);$('otaProg').hidden=true;}};
  x.onerror=()=>{toast('Hochladen fehlgeschlagen');$('otaProg').hidden=true;};x.send(fd);});

// Versionsverlauf: kommt direkt von der Installer-Seite (die App braucht dafür Internet, das Hauptpanel nicht)
let logData=null,openLog=new Set();
async function loadLog(){if(logData)return;
  try{const r=await fetch('https://mariofritzer.github.io/Trilumag/changelog.json',{cache:'no-cache'});logData=await r.json();}catch(e){logData=false;}
  renderOta();}
// kleiner Markdown-Leser für den Versionsverlauf: **Überschrift**, - Liste, `Code`
function md(t){const esc=x=>x.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
  const inl=x=>esc(x).replace(/\*\*(.+?)\*\*/g,'<b>$1</b>').replace(/`(.+?)`/g,'<code>$1</code>').replace(/\[(.+?)\]\((https?:[^)]+)\)/g,'<a href="$2" target="_blank" rel="noopener">$1</a>').replace(/\*(.+?)\*/g,'<i>$1</i>');
  let h='',ul=false;
  t.split('\n').forEach(l=>{l=l.trimEnd();
    if(/^- /.test(l)){if(!ul){h+='<ul>';ul=true;}h+='<li>'+inl(l.slice(2))+'</li>';return;}
    if(ul){h+='</ul>';ul=false;}
    if(!l.trim())return;
    const m=l.match(/^\*\*(.+)\*\*$/);h+=m?'<h4>'+esc(m[1])+'</h4>':'<p>'+inl(l)+'</p>';});
  if(ul)h+='</ul>';return h;}

// ---------- Ausklappbare Kästchen ----------
document.querySelectorAll('.card').forEach(c=>{const h=c.querySelector(':scope>h3');if(!h)return;
  h.classList.add('fold');h.tabIndex=0;h.setAttribute('role','button');const key='zu:'+(c.id||h.textContent);
  try{if(localStorage.getItem(key)==='1')c.classList.add('closed');}catch(e){}
  h.setAttribute('aria-expanded',!c.classList.contains('closed'));
  const t=()=>{c.classList.toggle('closed');const open=!c.classList.contains('closed');h.setAttribute('aria-expanded',open);
    try{localStorage.setItem(key,open?'0':'1');}catch(e){}
    if(open&&c.id==='colCard')requestAnimationFrame(()=>{drawWheel();syncColUi();});};
  h.addEventListener('click',t);h.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();t();}});});

// ---------- Start ----------
try{$('lang').value=localStorage.getItem('lang')||'';}catch(e){}
$('lang').addEventListener('change',()=>{try{if($('lang').value)localStorage.setItem('lang',$('lang').value);else localStorage.removeItem('lang');}catch(e){}location.reload();});
trStart();
window.addEventListener('resize',()=>{if(drag)return;vbFix=null;drawWheel();syncColUi();render();});
drawWheel();syncColUi();
poll();wsConnect();setInterval(poll,700);setInterval(pollLive,120);
</script></body></html>)HTML";
