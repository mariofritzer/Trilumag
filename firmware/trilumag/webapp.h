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
.status b{color:var(--fg);font-weight:600;font-family:system-ui,sans-serif;letter-spacing:.02em}
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
.wstat{display:flex;gap:12px;align-items:center;background:#13261a;border:1px solid #2c5a3a;border-radius:12px;padding:12px}
.wstat.bad{background:#2b1a12;border-color:#5a3a2a}
.wstat .ic{width:30px;height:30px;border-radius:50%;background:#2fbf65;display:grid;place-items:center;flex:none;color:#08130c;font-weight:800}
.wstat.bad .ic{background:#e2894a}
.wstat b{display:block;font-size:15px}
.wstat span{font-size:13px;color:var(--muted)}
.sl{display:grid;grid-template-columns:96px 1fr 38px;align-items:center;gap:10px;font-size:14px;color:var(--muted)}
.sl output{text-align:right;font:12px ui-monospace,Menlo,monospace;color:var(--fg)}
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
.pins.off{opacity:.45}
.nets{display:flex;flex-direction:column;border-radius:12px;overflow:hidden;background:var(--card2)}
.nets:empty{display:none}
.nets .row{gap:10px}
.bars{display:flex;align-items:flex-end;gap:2px;height:14px;flex:none}
.bars i{width:4px;background:#555;border-radius:1px}
.bars i.on{background:var(--fg)}
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
  </div>
  <div class="status"><b>Trilumag</b><span id="status">verbinde …</span></div>
</header>

<main>
  <div class="banner" id="apBanner" hidden><span>Noch kein WLAN eingerichtet.</span><button class="btn pri" data-go="opt">Einrichten</button></div>
  <div class="wall" id="miniBox"><svg id="mini" aria-label="Vorschau der Wand"></svg><div class="hint" id="miniHint"></div></div>

  <!-- Farben -->
  <section id="t-col" class="tab">
    <div class="card" id="colCard">
      <h3>Farbe</h3>
      <div class="chips"><button class="chip" id="tgtAll">Ganze Wand</button><button class="chip" id="tgtSel">Auswahl</button></div>
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
    <div class="wall"><svg id="big" aria-label="Wand mit Panels"></svg><div class="hint" id="bigHint"></div></div>
    <div class="card" id="panelCard">
      <h3 id="pTitle">Panel</h3>
      <div class="kv" id="pInfo"></div>
      <div class="sl">Helligkeit<input type="range" id="pbri" min="1" max="100" value="71"><output id="pbrio">71 %</output></div>
      <div class="btnrow"><button class="btn" id="pOn">Ein / Aus</button><button class="btn pri" id="pCol">Farbe wählen</button></div>
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
    <div class="card" id="wifiCard">
      <h3>WLAN</h3>
      <div class="wstat" id="wifiStat"><div class="ic" id="wifiIc">✓</div><div><b id="wifiT"></b><span id="wifiS"></span></div></div>
      <p class="note" id="wifiInfo"></p>
      <div class="nets" id="netList"></div>
      <label class="f">WLAN<input id="ssid" autocomplete="off" placeholder="Name deines WLANs"></label>
      <label class="f">Passwort<input id="pass" type="password" autocomplete="off"></label>
      <div class="btnrow"><button class="btn" id="scanBtn">Netze suchen</button><button class="btn pri" id="wifiBtn">Speichern und verbinden</button></div>
    </div>
    <div class="card" id="lightCard">
      <h3>Licht</h3>
      <div class="sl">Übergänge<input type="range" id="ltrans" min="0" max="50" value="7"><output id="ltranso">0,7 s</output></div>
      <p class="note">So lange blenden Farb-, Preset-, Effekt- und Ein/Aus-Wechsel weich über. 0 = sofort.</p>
      <label class="tog"><input type="checkbox" id="lpwrOn"><span class="sw"></span>Stromlimit</label>
      <div class="pins" id="lpwrBox">
        <label class="f">Netzteil liefert höchstens (A)<input id="lpwrMax" inputmode="decimal" placeholder="z. B. 6"></label>
        <label class="f">mA pro Farbkanal und Segment<input id="lpwrCh" inputmode="numeric" placeholder="12"></label>
      </div>
      <div class="wstat" id="pwrStat"><div class="ic" id="pwrIc">A</div><div><b id="pwrT">–</b><span id="pwrS"></span></div></div>
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
      <div class="pins">
        <label class="f">Stromsensor INA226 SDA<select id="p_sda"></select></label>
        <label class="f">Stromsensor INA226 SCL<select id="p_scl"></select></label>
        <label class="f">Shunt (mΩ)<input id="p_shunt" inputmode="decimal" placeholder="5"></label>
      </div>
      <p class="note" id="sensInfo">Optional. Ohne Sensor schätzt Trilumag den Strom aus den Farben.</p>
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
    else if(m.t==='live'){if(m.d.fx!=='aus'){live=m.d.c;joining=m.d.j||[];if(!drag)paintLive();}}
    else if(m.t==='ota'){otaInfo=m.d;renderOta();}
    else if(m.t==='otap'){if(otaInfo){otaInfo.busy=true;otaInfo.p=m.p;renderOta();}}
    else if(m.t==='otadone'){toast('Version '+m.v+' installiert, Trilumag startet neu …');setTimeout(()=>location.reload(),9000);}
    else if(m.t==='err')toast(m.m);};
  sock.onclose=()=>{wsOk=false;sock=null;render();setTimeout(wsConnect,2000);};
  sock.onerror=()=>{try{sock.close();}catch(x){}};
}

async function api(path,body){
  if(body&&wsOk){sock.send(JSON.stringify({p:path,b:body}));return st;}   // Antwort kommt als neuer Zustand
  const r=await fetch(path,body?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}:{});
  const j=await r.json();if(!r.ok){toast(j.error||'Fehler');return null;}if(j.panels)takeState(j);return j;}
// Ohne WebSocket: Abfragen nie gleichzeitig, Klicks gehen vor
async function poll(){if(wsOk||drag||inflight)return;if(fxOn()&&Date.now()-lastState<2000)return;
  inflight=true;try{await api('/api/state');lastState=Date.now();render();}catch(e){$('status').textContent='keine Verbindung zum ESP32';}inflight=false;}
function fxOn(){return st&&st.fx&&st.fx.id!=='aus';}
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
function geom(x,y,up){const cx=x*S/2,t=y*H;const p=up?[[cx-S/2,t+H],[cx+S/2,t+H],[cx,t]]:[[cx-S/2,t],[cx+S/2,t],[cx,t+H]];return{p,c:[cx,up?t+2*H/3:t+H/3]};}
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
    let fill=null;
    if(p.state===0)cls.push('dark');
    else if(fxOn()&&joining.includes(p.id))cls.push('pulse');
    else if(fxOn()&&live[p.id])fill=st.on?liveCol(live[p.id]):'#141414';
    else if(p.state===1&&st.on)cls.push('pulse');
    else fill=staticCol(p);
    const poly=el('polygon',{points:pts(shrink(g,.94)),class:cls.join(' ')});if(fill)poly.style.fill=fill;
    poly.dataset.id=p.id;if(cls.includes('pulse'))syncPulse(poly);gP.append(poly);
    if(big){
      if(!p.main){const m=edge1Mid(p);gP.append(el('circle',{cx:m[0],cy:m[1],r:2.4,class:'edge1'}));}
      else{const a=g.p[0],b=g.p[1];gP.append(el('line',{x1:a[0]+8,y1:a[1]+5,x2:b[0]-8,y2:b[1]+5,stroke:'#777','stroke-width':3,'stroke-linecap':'round'}));}
      const t=el('text',{x:g.c[0],y:g.c[1]+3,class:'lbl'+(lightish(p)?' dk':'')});t.textContent=p.main?'Haupt':p.id.slice(4);gP.append(t);
    }
  });
  if(big)drawDrag(svg);
}
function paintLive(){
  ['mini','big'].forEach(id=>$(id).querySelectorAll('polygon[data-id]').forEach(p=>{const i=p.dataset.id,h=live[i];
    if(joining.includes(i)){if(!p.classList.contains('pulse')){p.style.fill='';p.classList.add('pulse');syncPulse(p);}}
    else if(h){p.classList.remove('pulse');p.style.fill=st.on?liveCol(h):'#141414';}}));
}

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
$('big').addEventListener('pointerdown',e=>{const id=e.target.dataset&&e.target.dataset.id;if(!id)return;
  const p=st.panels.find(q=>q.id===id);if(!p)return;if(p.main||!st.sim){focus=id;render();return;}startDrag(e,id,'wall');});

// ---------- Darstellung ----------
function render(){
  if(!st)return;
  $('status').textContent=`${st.panels.length}/${st.max} Panels · ${st.sim?'Simulation':'Bus'} · MQTT ${st.mqttSet?(st.mqtt?'verbunden':'getrennt'):'aus'} · ${wsOk?'live':'HTTP'}`;
  $('pwr').classList.toggle('on',st.on);
  if(document.activeElement!==$('master')){const p=pct(st.master);setRange('master',p);$('mastero').textContent=p+' %';}
  $('apBanner').hidden=!st.ap||tab==='opt';
  $('updBadge').hidden=!st.upd;
  if(tab==='opt'){renderWifi();renderPower();}
  [...sel].forEach(id=>{if(!st.panels.some(p=>p.id===id))sel.delete(id);});     // abgeklipste Panels aus der Auswahl nehmen
  if(tab!=='wall'&&tab!=='opt'){drawWall($('mini'),false);
    $('miniHint').textContent=tab==='col'?(sel.size?`${sel.size} Panel${sel.size>1?'s':''} ausgewählt`:'Panels antippen, um nur diese einzufärben'):'';}
  if(tab==='wall')renderWall();
  if(tab==='col')renderCol();
  if(tab==='fx')renderFx();
  if(tab==='pre')renderPre();
  const c=curColor();accent(c[0],c[1],c[2]);
}

// ---------- Tab Farben ----------
let hsv=[0.08,1,1],cw=0,wheelBusy=false;
function target(){return sel.size?'sel':'all';}
function curColor(){
  if(!st)return[110,139,255,0];
  if(target()==='all'&&st.fx.usesColor)return[st.fx.r,st.fx.g,st.fx.b,st.fx.w];
  const id=sel.size?[...sel][0]:null;const p=st.panels.find(q=>q.id===id)||st.panels.find(q=>q.main);
  return p?[p.r,p.g,p.b,p.w]:[255,120,0,0];}
function renderCol(){
  $('tgtAll').classList.toggle('on',!sel.size);$('tgtSel').classList.toggle('on',!!sel.size);
  $('tgtSel').textContent=sel.size?`Auswahl (${sel.size})`:'Auswahl';
  const fxName=(st.effects.find(e=>e.id===st.fx.id)||{}).name;
  $('colNote').textContent=!fxOn()?'':target()==='all'?(st.fx.usesColor?`Färbt den laufenden Effekt „${fxName}“.`:`„${fxName}“ hat eigene Farben. Eine Farbe wählen wechselt auf Einfarbig.`):'Eine Farbe für einzelne Panels beendet den Effekt.';
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
  accent(c[0],c[1],c[2]);
  later('col',async()=>{
    const wasFx=fxOn();
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
    st.effects.forEach(e=>{const b=document.createElement('button');b.className='row';b.dataset.fx=e.id;
      b.innerHTML='<span class="dot"></span><span></span>'+(e.color?'<small>nutzt Effektfarbe</small>':'');b.children[1].textContent=e.name;
      b.addEventListener('click',()=>setFx({effect:e.id}));$('fxList').append(b);});
    st.palettes.forEach(p=>{const b=document.createElement('button');b.className='row';b.dataset.pal=p.id;
      b.innerHTML='<span class="dot"></span><span></span><span class="grad"></span>';b.children[1].textContent=p.name;
      b.addEventListener('click',()=>setFx({palette:p.id}));$('palList').append(b);});}
  const q=$('fxq').value.trim().toLowerCase();
  $('fxList').querySelectorAll('.row').forEach(r=>{r.classList.toggle('on',r.dataset.fx===st.fx.id);r.hidden=!!q&&!r.textContent.toLowerCase().includes(q);});
  $('palList').querySelectorAll('.row').forEach(r=>{r.classList.toggle('on',r.dataset.pal===st.fx.pal);
    r.querySelector('.grad').style.background=palGrad(st.palettes.find(p=>p.id===r.dataset.pal));});
  setRange('fspeed',st.fx.speed);setRange('finten',st.fx.inten);
  $('fxNote').textContent=fxOn()?(st.fx.usesColor?'Die Effektfarbe stellst du im Tab Farben ein.':''):'Einfarbig: jedes Panel leuchtet in seiner eigenen Farbe. Tempo, Intensität und Palette wirken erst mit einem Effekt.';
}
async function setFx(body){
  if(body.effect){st.fx.id=body.effect;live={};joining=[];}
  if(body.palette)st.fx.pal=body.palette;
  renderFx();                                        // sofort umschalten, nicht erst nach der Antwort
  const r=await api('/api/effect',body);if(r){render();pollLive();}}
$('fxq').addEventListener('input',renderFx);
$('fspeed').addEventListener('input',()=>{$('fspeedo').textContent=$('fspeed').value;later('fx',()=>api('/api/effect',{speed:+$('fspeed').value}));});
$('finten').addEventListener('input',()=>{$('finteno').textContent=$('finten').value;later('fx',()=>api('/api/effect',{intensity:+$('finten').value}));});

// ---------- Kopfzeile ----------
$('pwr').addEventListener('click',async()=>{if(!st)return;st.on=!st.on;render();await api('/api/set',{id:'alle',state:st.on?'ON':'OFF'});render();});
// Helligkeit in Prozent anzeigen, intern 1 bis 255
function pct(v){return Math.max(1,Math.round(v*100/255));}
function fromPct(p){return Math.max(1,Math.round(p*255/100));}
$('master').addEventListener('input',()=>{if(!st)return;const p=+$('master').value;$('mastero').textContent=p+' %';st.master=fromPct(p);
  later('m',async()=>{await api('/api/set',{id:'alle',brightness:st.master});render();},60);});

// ---------- Tab Wand ----------
function renderWall(){
  drawWall($('big'),true);
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
    $('pInfo').innerHTML='';[['Chip-ID',p.id],['Zustand',stx],['Position',`${p.x} / ${p.y} · Spitze ${p.up?'oben':'unten'}`],['Hängt an',p.main?'–':par?(par.main?'Hauptpanel':'Panel '+par.id.slice(4)):'–'],['Farbe',rgbHex(p.r,p.g,p.b)+(p.w?' + Weiß '+p.w:'')]]
      .forEach(([k,v])=>{const a=document.createElement('span');a.textContent=k;const b=document.createElement('span');b.textContent=v;$('pInfo').append(a,b);});
    if(document.activeElement!==$('pbri')){setRange('pbri',pct(p.bri));$('pbrio').textContent=pct(p.bri)+' %';}
    $('pOn').classList.toggle('pri',p.on);$('pOn').textContent=p.on?'Ein':'Aus';
  }
}
$('newBtn').addEventListener('click',async()=>{if(await api('/api/sim/new',{}))render();});
$('pbri').addEventListener('input',()=>{$('pbrio').textContent=$('pbri').value+' %';const id=focus;later('pb',async()=>{await api('/api/set',{id,brightness:fromPct(+$('pbri').value)});render();});});
$('pOn').addEventListener('click',async()=>{const p=st.panels.find(q=>q.id===focus);if(!p)return;await api('/api/set',{id:p.id,state:p.on?'OFF':'ON'});render();});
$('pCol').addEventListener('click',()=>{if(!focus)return;sel=new Set([focus]);showTab('col');});

// ---------- Tab Presets ----------
let askDel=null;
function renderPre(){
  const g=$('pgrid');g.innerHTML='';$('preEmpty').hidden=st.presets.length>0;
  st.presets.forEach(p=>{const b=document.createElement('div');b.className='preset'+(st.preset===p.id?' on':'');b.setAttribute('role','button');b.tabIndex=0;
    const n=document.createElement('span');n.textContent=p.name;b.append(n);
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
  renderWifi();loadOta();
  try{cfg=await (await fetch('/api/config')).json();}catch(e){return;}
  if(st&&(st.ap||!st.ssid)&&!$('netList').children.length)$('scanBtn').click();   // ohne WLAN gleich nach Netzen suchen
  $('mode').value=cfg.mode;
  const b=$('board');b.innerHTML='';cfg.boards.forEach(x=>{const o=document.createElement('option');o.value=x.id;o.textContent=x.name;b.append(o);});
  const c=document.createElement('option');c.value='custom';c.textContent='Eigene Belegung';b.append(c);
  b.value=cfg.boards.some(x=>x.id===cfg.board)?cfg.board:'custom';
  PIN_KEYS.forEach(k=>{const s=$('p_'+k);s.innerHTML='';cfg.validPins.forEach(p=>{const o=document.createElement('option');o.value=p;o.textContent='GPIO '+p;s.append(o);});s.value=cfg.pins[k];});
  $('order').innerHTML='';cfg.orders.forEach(o=>{const e=document.createElement('option');e.value=o;e.textContent=o;$('order').append(e);});$('order').value=cfg.order;
  const L=cfg.light||{};
  $('lpwrOn').checked=L.pwrMax>0;$('lpwrBox').classList.toggle('off',!(L.pwrMax>0));
  $('lpwrMax').value=L.pwrMax?(L.pwrMax/1000).toLocaleString('de-AT'):'';$('lpwrCh').value=L.pwrCh||12;
  ['sda','scl'].forEach(k=>{const s=$('p_'+k);s.innerHTML='<option value="-1">aus</option>';cfg.validPins.forEach(p=>{const o=document.createElement('option');o.value=p;o.textContent='GPIO '+p;s.append(o);});s.value=L[k]??-1;});
  $('p_shunt').value=((L.shunt||50)/10).toLocaleString('de-AT');
  $('sensInfo').textContent=L.sda>=0?(L.sensor?'Stromsensor gefunden, Trilumag misst den echten Strom.':'Stromsensor nicht gefunden. Verkabelung und Pins prüfen.'):'Optional. Ohne Sensor schätzt Trilumag den Strom aus den Farben.';
  $('m_on').checked=!!cfg.mqtt.on;$('mqttFields').classList.toggle('off',!cfg.mqtt.on);$('m_host').value=cfg.mqtt.host;$('m_port').value=cfg.mqtt.port;$('m_user').value=cfg.mqtt.user;
  $('info').innerHTML='';[['Chip',cfg.chip],['Firmware',cfg.ver],['Betriebsart',cfg.mode==='bus'?'Bus':'Simulation'],['WLAN',st&&st.ssid||'–']]
    .forEach(([k,v])=>{const a=document.createElement('span');a.textContent=k;const x=document.createElement('span');x.textContent=v;$('info').append(a,x);});
}
$('board').addEventListener('change',()=>{const x=cfg.boards.find(b=>b.id===$('board').value);if(x)PIN_KEYS.forEach(k=>$('p_'+k).value=x.pins[k]);});
PIN_KEYS.forEach(k=>$('p_'+k).addEventListener('change',()=>{$('board').value='custom';}));
document.querySelectorAll('[data-t]').forEach(b=>b.addEventListener('click',()=>fetch('/api/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ch:+b.dataset.t})})));
$('saveBtn').addEventListener('click',async()=>{
  const pins={};PIN_KEYS.forEach(k=>pins[k]=+$('p_'+k).value);
  if(new Set(Object.values(pins)).size<6){$('setErr').textContent='Ein Pin ist doppelt belegt.';return;}
  const sda=+$('p_sda').value,scl=+$('p_scl').value;
  if((sda<0)!==(scl<0)){$('setErr').textContent='Für den Stromsensor beide Pins wählen oder keinen.';return;}
  if(sda>=0&&(Object.values(pins).includes(sda)||Object.values(pins).includes(scl)||sda===scl)){$('setErr').textContent='Ein Pin des Stromsensors ist schon belegt.';return;}
  const body={light:{sda,scl,shunt:Math.round(parseFloat(($('p_shunt').value||'5').replace(',','.'))*10)},mode:$('mode').value,board:$('board').value,pins,order:$('order').value,mqtt:{on:$('m_on').checked,host:$('m_host').value.trim(),port:+$('m_port').value||1883,user:$('m_user').value.trim()}};
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
      const t=document.createElement('span');t.textContent=n.ssid;
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
function fmtA(ma){return (ma/1000).toLocaleString('de-AT',{minimumFractionDigits:1,maximumFractionDigits:1})+' A';}
function renderPower(){
  if(!st||!st.pwr)return;const p=st.pwr,meas=p.ma!=null;
  const ma=meas?p.ma:p.est,v=meas?p.v:24;
  $('pwrT').textContent=`${fmtA(ma)} · ${Math.round(ma/1000*v)} W ${meas?'gemessen':'geschätzt'}`;
  $('pwrS').textContent=(meas?`${String(p.v).replace('.',',')} V · `:'')+(p.lim?(p.scale<100?`Stromlimit greift: gedimmt auf ${p.scale} %`:`unter dem Limit von ${fmtA(p.lim)}`):'kein Stromlimit');
  $('pwrStat').classList.toggle('bad',!!p.lim&&p.scale<100);
  if(document.activeElement!==$('ltrans')){const t=Math.round(st.trans/100);setRange('ltrans',t);$('ltranso').textContent=(t/10).toLocaleString('de-AT')+' s';}
}
$('ltrans').addEventListener('input',()=>{const t=+$('ltrans').value;$('ltranso').textContent=(t/10).toLocaleString('de-AT')+' s';later('lt',()=>api('/api/light',{trans:t*100}));});
function sendPwr(){const on=$('lpwrOn').checked;$('lpwrBox').classList.toggle('off',!on);
  const a=parseFloat(($('lpwrMax').value||'').replace(',','.'));const ch=parseInt($('lpwrCh').value);
  const body={pwrMax:on&&a>0?Math.round(a*1000):0};if(ch>0)body.pwrCh=ch;
  if(on&&!(a>0)){$('lpwrMax').focus();return;}
  api('/api/light',body);}
$('lpwrOn').addEventListener('change',sendPwr);
$('lpwrMax').addEventListener('change',sendPwr);$('lpwrCh').addEventListener('change',sendPwr);

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
window.addEventListener('resize',()=>{if(drag)return;vbFix=null;drawWheel();syncColUi();render();});
drawWheel();syncColUi();
poll();wsConnect();setInterval(poll,700);setInterval(pollLive,120);
</script></body></html>)HTML";
