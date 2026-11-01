/* Trilumag im Browser: ersetzt das Hauptpanel (ESP32), damit die echte App ohne Hardware läuft.
 * Wird vor die App geladen (siehe .github/scripts/sim_page.py) und fängt fetch('/api/…') und den WebSocket ab.
 * Die Effekte sind aus fxCompute() der Firmware übernommen, damit sie gleich aussehen. Alles bleibt im Browser,
 * der Zustand wird in localStorage gemerkt (Panelwand, Farben, Effekt, Presets, eigene Paletten). */
(function () {
'use strict';
const VERSION = window.TRILUMAG_SIM_VERSION || 'Simulation';
const SLOTS = 40, MAX_ATTACHED = 30, DETECT_MS = 1200, FX_JOIN_MS = 5 * 1600, PANEL_FW = 3, PRESET_MAX = 16, CPAL_MAX = 4;
const KEY = 'trilumag-sim-v1';
const NA = 'Im Simulator nicht verfügbar, dafür braucht es ein echtes Hauptpanel';

// ---------- Tabellen aus der Firmware ----------
const FX = [
  ['aus', 'Einfarbig', 0], ['regenbogen', 'Regenbogen', 0], ['welle', 'Regenbogenwelle', 0], ['atmen', 'Atmen', 1],
  ['farbwechsel', 'Farbwechsel', 0], ['funkeln', 'Funkeln', 1], ['ausbreiten', 'Ausbreiten', 1], ['feuer', 'Feuer', 0],
  ['polarlicht', 'Polarlicht', 0], ['lauflicht', 'Lauflicht', 1], ['spirale', 'Spirale', 0], ['gewitter', 'Gewitter', 0],
  ['kerzen', 'Kerzenlicht', 0], ['disco', 'Disco', 0], ['komet', 'Komet', 1], ['lava', 'Lava', 0], ['verlauf', 'Farbverlauf', 1],
  ['herz', 'Herzschlag', 1], ['plasma', 'Plasma', 0], ['regen', 'Regen', 0], ['sterne', 'Sternenhimmel', 0],
  ['feuerwerk', 'Feuerwerk', 0], ['matrix', 'Matrix', 0], ['wetter', 'Wetter', 0], ['raum-komet', 'Raum-Komet', 1],
  ['raum-regenbogen', 'Raum-Regenbogen', 0], ['raum-welle', 'Raum-Welle', 1], ['raum-feuerwerk', 'Raum-Feuerwerk', 0],
  ['raum-pingpong', 'Raum-Pingpong', 1], ['raum-atmen', 'Raum-Atmen', 1]].map(([id, name, color]) => ({ id, name, color: !!color }));
const PALS = [
  ['standard', 'Standard', []], ['regenbogen', 'Regenbogen', [0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF]],
  ['effektfarbe', 'Effektfarbe', []], ['ozean', 'Ozean', [0x001E64, 0x0050C8, 0x00B4E6, 0x50F0FF, 0x0064B4]],
  ['lava', 'Lava', [0x3C0000, 0xB40000, 0xFF3C00, 0xFFA000, 0xFF1E00]], ['wald', 'Wald', [0x003C00, 0x1E7814, 0x64B41E, 0xA0D23C, 0x145A28]],
  ['sonnenuntergang', 'Sonnenuntergang', [0x3C0A64, 0xA01E64, 0xF03C32, 0xFFA028, 0xFFD264]],
  ['party', 'Party', [0x5500AB, 0xFF0055, 0xFF9900, 0xFFFF00, 0x00FF99, 0x0066FF]],
  ['pastell', 'Pastell', [0xFFB3BA, 0xFFDFBA, 0xFFFFBA, 0xBAFFC9, 0xBAE1FF]], ['eis', 'Eis', [0xFFFFFF, 0xC8F0FF, 0x64C8FF, 0x1E78FF, 0xE6FAFF]]
].map(([id, name, c]) => ({ id, name, c }));
const PAL_COUNT = PALS.length;
const TAP_NAMES = ['nichts', 'Panel ein/aus', 'Panelwand ein/aus', 'nächstes Preset', 'nächster Effekt'];
const hex8 = c => (c >>> 0).toString(16).toUpperCase().padStart(8, '0');
const hex6 = c => '#' + (c & 0xFFFFFF).toString(16).toUpperCase().padStart(6, '0');
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const now = () => performance.now();

// ---------- Raster (wie in der Firmware) ----------
const BOTTOM = 0, RIGHT = 1, LEFT = 2, TOP = 3;
const UP_EDGES = [BOTTOM, RIGHT, LEFT], DOWN_EDGES = [TOP, LEFT, RIGHT];
const isUp = (x, y) => ((((x + y) % 2) + 2) % 2) === 0;
const slotEdges = up => up ? UP_EDGES : DOWN_EDGES;
const worldDir = (p, e) => slotEdges(isUp(p.x, p.y))[(e + p.rot) % 3];
const opposite = d => d === BOTTOM ? TOP : d === TOP ? BOTTOM : d === RIGHT ? LEFT : RIGHT;
const neighbor = (x, y, d) => d === BOTTOM ? [x, y + 1] : d === TOP ? [x, y - 1] : d === RIGHT ? [x + 1, y] : [x - 1, y];
const hasConnector = (i, e) => !(i === 0 && e === 0);

// ---------- Zustand ----------
function newPanel() {
  return { used: false, attached: false, chip: 0, x: 0, y: 0, rot: 0, parent: -1, depth: 0, state: 0, since: 0, joinAt: 0,
    hasColor: false, on: true, r: 0, g: 0, b: 0, w: 0, bri: 255, edges: false, clips: 0, identUntil: 0, lit: 0 };
}
let P, fx, fxC2, fxDir, fxSpin, master, masterOn, curPreset, presets, cpal, favs, cfg, sleepEnd, name, view;
function defaults() {
  P = Array.from({ length: SLOTS }, newPanel);
  fx = { id: 2, speed: 50, inten: 128, pal: 0, r: 255, g: 120, b: 30, w: 0 };
  fxC2 = [0, 80, 255, 0]; fxDir = 0; fxSpin = 0; master = 200; masterOn = true; curPreset = -1;
  presets = []; cpal = Array(CPAL_MAX).fill(null); favs = ['welle', 'polarlicht', 'kerzen'];
  cfg = { transMs: 700, onAnim: 2, daylight: false, pwrMax: 0, pwrCh: 12, touchOn: true, touchSens: 5, tapA1: 1, tapA2: 2, touchWave: true,
    bootMode: 0, bootPreset: -1, fault: true, guard: true, panelAuto: true, order: 'RGBW', board: 's3-devkitc',
    mqtt: { on: false, host: '', port: 1883, user: '' } };
  sleepEnd = 0; name = 'Trilumag Simulation'; view = { rot: 0, mir: false };
  const rnd = () => (Math.random() * 0xFFFFFFFF) >>> 0 || 1;
  const m = P[0]; Object.assign(m, { used: true, attached: true, chip: rnd(), state: 2, hasColor: true, w: 200 });
  // Beispielwand: Hauptpanel und fünf Panels, dazu drei in der Ablage
  const cols = [[255, 60, 40], [40, 120, 255], [30, 220, 140], [255, 170, 20], [190, 60, 255]];
  for (let k = 1; k <= 8; k++) Object.assign(P[k], { used: true, chip: rnd(), hasColor: k <= 5, clips: k <= 5 ? 1 : 0 },
    k <= 5 ? { r: cols[k - 1][0], g: cols[k - 1][1], b: cols[k - 1][2] } : {});
  const plan = [[1, 0, 1, 0], [2, 0, 2, 0], [3, 1, 1, 0], [4, 2, 2, 0], [5, 3, 2, 0]];
  for (const [i, par, e, own] of plan) if (placePanel(i, par, e, own)) { P[i].state = 2; P[i].since = -1e9; }
}
function save() {
  clearTimeout(save.t);
  save.t = setTimeout(() => {
    try {
      localStorage.setItem(KEY, JSON.stringify({ P: P.map(p => { const q = Object.assign({}, p); q.identUntil = 0; q.joinAt = 0; return q; }),
        fx, fxC2, fxDir, fxSpin, master, masterOn, presets, cpal, favs, cfg, name, view }));
    } catch (e) {}
  }, 400);
}
function load() {
  defaults();
  try {
    const s = JSON.parse(localStorage.getItem(KEY) || 'null');
    if (!s || !Array.isArray(s.P) || s.P.length !== SLOTS) return;
    P = s.P.map(p => Object.assign(newPanel(), p, { since: -1e9, joinAt: 0, identUntil: 0 }));
    ({ fx, fxC2, fxDir, fxSpin, master, masterOn, presets, cpal, favs, name, view } = Object.assign({ view: { rot: 0, mir: false } }, s));
    Object.assign(cfg, s.cfg || {});
  } catch (e) { defaults(); }
}

// ---------- Wand ----------
const findAt = (x, y) => P.findIndex(p => p.used && p.attached && p.x === x && p.y === y);
const findChip = id => { const c = parseInt(id || '0', 16) >>> 0; return P.findIndex(p => p.used && p.chip === c); };
const countAttached = () => P.filter(p => p.used && p.attached).length;
const edgeFacing = (i, d) => { for (let e = 0; e < 3; e++) if (worldDir(P[i], e) === d) return e; return -1; };
function reconcile() {
  const seen = Array(SLOTS).fill(false), q = [0]; seen[0] = true; P[0].parent = -1; P[0].depth = 0;
  while (q.length) {
    const i = q.shift();
    for (let e = 0; e < 3; e++) {
      if (!hasConnector(i, e)) continue;
      const d = worldDir(P[i], e), [nx, ny] = neighbor(P[i].x, P[i].y, d), j = findAt(nx, ny);
      if (j < 0 || seen[j]) continue;
      const ej = edgeFacing(j, opposite(d));
      if (ej < 0 || !hasConnector(j, ej)) continue;
      seen[j] = true; P[j].parent = i; P[j].depth = P[i].depth + 1; q.push(j);
    }
  }
  for (let i = 1; i < SLOTS; i++) if (P[i].used && P[i].attached && !seen[i]) Object.assign(P[i], { attached: false, state: 0, parent: -1 });
}
function placePanel(i, parent, edge, own) {
  if (i <= 0 || i >= SLOTS || !P[i].used || P[i].attached) return false;
  if (parent < 0 || !P[parent].used || !P[parent].attached) return false;
  if (edge > 2 || own > 2 || !hasConnector(parent, edge) || countAttached() >= MAX_ATTACHED) return false;
  const d = worldDir(P[parent], edge), [nx, ny] = neighbor(P[parent].x, P[parent].y, d);
  if (findAt(nx, ny) >= 0) return false;
  const list = slotEdges(isUp(nx, ny)), back = opposite(d); let k = 0;
  for (let m = 0; m < 3; m++) if (list[m] === back) k = m;
  Object.assign(P[i], { x: nx, y: ny, rot: (k - own + 3) % 3, attached: true, since: now() });
  reconcile();
  return true;
}
function edgePos(i, e) {
  const p = P[i]; let dx = 0, dy = 0, dd = 0;
  if (e >= 0) {
    const w = worldDir(p, e);
    if (w === BOTTOM) dy = 0.289; else if (w === TOP) dy = -0.289;
    else { dx = w === RIGHT ? 0.25 : -0.25; dy = isUp(p.x, p.y) ? -0.144 : 0.144; }
    const [nx, ny] = neighbor(p.x, p.y, w);
    dd = (p.parent >= 0 && P[p.parent].x === nx && P[p.parent].y === ny) ? -0.33 : 0.17;
  }
  return [p.x * 0.5 + dx, p.y * 0.866 + dy, dd];
}

// ---------- Farben ----------
function hue(h, c) {
  h -= Math.floor(h); const x = h * 6, k = Math.floor(x), f = x - k, q = 1 - f; let r = 0, g = 0, b = 0;
  switch (k % 6) { case 0: r = 1; g = f; break; case 1: r = q; g = 1; break; case 2: g = 1; b = f; break;
    case 3: g = q; b = 1; break; case 4: r = f; b = 1; break; default: r = 1; b = q; }
  c[0] = r * 255; c[1] = g * 255; c[2] = b * 255; c[3] = 0;
}
const base = (k, c) => { c[0] = fx.r * k; c[1] = fx.g * k; c[2] = fx.b * k; c[3] = fx.w * k; };
const fireColor = (h, c) => { c[0] = 255 * h; c[1] = 85 * h * h; c[2] = 0; c[3] = 20 * h * h * h; };
const D_HUE = 0, D_COLOR = 1, D_FIRE = 2, D_AURORA = 3;
function palStops(k) {
  if (k >= PAL_COUNT) { const q = cpal[k - PAL_COUNT]; return q ? q.c : PALS[1].c; }
  return PALS[k].c;
}
function pcol(t, c, def) {
  t -= Math.floor(t);
  if (fx.pal === 0) {
    if (def === D_HUE) return hue(t, c);
    if (def === D_COLOR) return base(1, c);
    if (def === D_FIRE) return fireColor(0.35 + 0.65 * t, c);
    return hue(0.36 + 0.36 * (0.5 + 0.5 * Math.sin(t * 6.2831853)), c);
  }
  if (fx.pal === 1) return hue(t, c);
  let stops;
  if (fx.pal === 2) {
    const col = [fx.r, fx.g, fx.b, fx.w];
    stops = [col.map(v => v * 0.25), col, col.map((v, k) => v * 0.6 + (k < 3 ? 102 : 0))];
  } else stops = palStops(fx.pal).map(v => [(v >> 16) & 255, (v >> 8) & 255, v & 255, 0]);
  const n = stops.length, x = t * n, a = Math.floor(x) % n, b = (a + 1) % n; let f = x - Math.floor(x);
  f = f * f * (3 - 2 * f);
  for (let k = 0; k < 4; k++) c[k] = stops[a][k] + (stops[b][k] - stops[a][k]) * f;
}
const mul = (c, k) => { for (let i = 0; i < 4; i++) c[i] *= k; };
function kelvinRgbw(k) {
  const t = clamp(k, 1500, 10000) / 100;
  let r = t <= 66 ? 255 : 329.698727446 * Math.pow(t - 60, -0.1332047592);
  let g = t <= 66 ? 99.4708025861 * Math.log(t) - 161.1195681661 : 288.1221695283 * Math.pow(t - 60, -0.0755148492);
  let b = t >= 66 ? 255 : (t <= 19 ? 0 : 138.5177312231 * Math.log(t - 10) - 305.0447927307);
  r = clamp(r, 0, 255); g = clamp(g, 0, 255); b = clamp(b, 0, 255);
  const w = Math.min(r, g, b);
  return { r: Math.round(r - w), g: Math.round(g - w), b: Math.round(b - w), w: Math.round(w) };
}

// ---------- Effekte (aus fxCompute) ----------
const fxA = new Float32Array(SLOTS * 3), fxB = new Float32Array(SLOTS * 3), fxT = new Float32Array(SLOTS * 3);
let fxPhase = 0, fxLast = 0, fxSpinAng = 0, fxFlash = 0, fxFlashX = 0, fxFlashY = 0, fxHead = 0, fxHeadE = 0, fxPlan = [0, 0, 0],
  fxPlanN = 0, fxPlanP = 0, fxStep = 0, fxBeat = -1, fwX = 0, fwY = 0, fwT = 9, fwH = 0, fxAng = 0;
const TGT = Array.from({ length: SLOTS }, () => [[0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]]);
const CUR = Array.from({ length: SLOTS }, () => [[0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]]);
const frand = Math.random;
function fxReset() { for (let i = 0; i < SLOTS * 3; i++) { fxA[i] = frand(); fxB[i] = frand(); fxT[i] = frand(); } fxLast = now(); }
function viewXY(x, y) {
  const a = view.rot * 0.01745329, c = Math.cos(a), s = Math.sin(a); let rx = x * c - y * s; const ry = x * s + y * c;
  if (view.mir) rx = -rx; return [rx, ry];
}
function effXY(x, y) {
  const [vx, vy] = viewXY(x, y), a = fxAng * 0.01745329, c = Math.cos(a), s = Math.sin(a);
  return [vx * c + vy * s, -vx * s + vy * c];
}
function fxStart(id) {
  if (id < 0 || id >= FX.length) return;
  const was = fx.id; fx.id = id;
  if (id && id !== was) { fxReset(); fxPhase = 0; }
  transition();
}
const masterK = () => masterOn ? master : 0;
function fxCompute() {
  const t = now(); let dt = (t - fxLast) / 1000; fxLast = t; if (dt > 0.2) dt = 0.2;
  const rate = Math.pow(4, (fx.speed - 50) / 50);
  fxPhase += dt * rate; if (fxPhase > 100000) fxPhase = 0;
  if (fxSpin) { fxSpinAng += dt * (fxSpin === 1 ? 6 : 30); if (fxSpinAng >= 360) fxSpinAng -= 360; }
  fxAng = fxDir + fxSpinAng;
  let minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
  const att = [];
  for (let i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) {
    att.push(i); const [rx, ry] = effXY(P[i].x * 0.5, P[i].y * 0.866);
    minX = Math.min(minX, rx); maxX = Math.max(maxX, rx); minY = Math.min(minY, ry); maxY = Math.max(maxY, ry);
  }
  const spanX = Math.max(1, maxX - minX), cxW = (minX + maxX) / 2, cyW = (minY + maxY) / 2,
    maxR = Math.max(1, 0.5 * Math.hypot(maxX - minX, maxY - minY)), TAU = 6.2831853;
  const pick = () => att[Math.floor(frand() * att.length)] || 0;
  fxFlash = Math.max(0, fxFlash - dt * rate * 5);
  if (fx.id === 21 || fx.id === 27) {
    fwT += dt * rate;
    if (fwT > 1.6 - 0.8 * fx.inten / 255) { const k = pick(); [fwX, fwY] = effXY(P[k].x * 0.5, P[k].y * 0.866); fwT = 0; fwH = frand(); }
  }
  if (fx.id === 11 && frand() < dt * rate * (0.12 + 0.6 * fx.inten / 255)) {
    const k = pick(); [fxFlashX, fxFlashY] = effXY(P[k].x * 0.5, P[k].y * 0.866); fxFlash = 0.7 + 0.3 * frand();
  }
  if (fx.id === 14 || fx.id === 24) {                     // Komet (Raum-Komet läuft im Simulator nur über diese Wand)
    if (!P[fxHead].used || !P[fxHead].attached) { fxHead = 0; fxHeadE = 0; fxPlanN = 0; }
    if (!P[fxHead].edges) { fxHeadE = 0; fxPlanN = 0; }
    fxStep += dt * rate * 4;
    while (fxStep >= 1) {
      fxStep -= 1;
      const h = P[fxHead];
      if (h.edges && fxPlanP < fxPlanN) { fxHeadE = fxPlan[fxPlanP++]; fxA[fxHead * 3 + fxHeadE] = 1; continue; }
      const cj = [], ce = [];
      for (let e = 0; e < 3; e++) {
        if (h.edges && e !== fxHeadE) continue;
        const w = worldDir(h, e), [nx, ny] = neighbor(h.x, h.y, w), j = findAt(nx, ny);
        if (j < 0) continue;
        let ej = P[j].edges ? edgeFacing(j, opposite(w)) : 0; if (ej < 0) ej = 0;
        cj.push(j); ce.push(ej);
      }
      const fr = []; for (let k = 0; k < cj.length; k++) if (fxA[cj[k] * 3 + ce[k]] < 0.5) fr.push(k);
      const pk = fr.length ? fr[Math.floor(frand() * fr.length)] : cj.length ? Math.floor(frand() * cj.length) : -1;
      if (pk < 0) {
        if (h.edges) { fxPlan = [(fxHeadE + 1) % 3, (fxHeadE + 2) % 3, 0]; fxPlanN = 2; fxPlanP = 0; }
        fxA[fxHead * 3 + fxHeadE] = 1; continue;
      }
      fxHead = cj[pk]; fxHeadE = ce[pk]; fxA[fxHead * 3 + fxHeadE] = 1; fxPlanN = fxPlanP = 0;
      const q = P[fxHead];
      if (q.edges) {
        const nb = e => { const [nx, ny] = neighbor(q.x, q.y, worldDir(q, e)); return findAt(nx, ny) >= 0; };
        const e0 = fxHeadE, fw = (e0 + 2) % 3, bw = (e0 + 1) % 3, f = nb(fw), b = nb(bw);
        const dirF = f && b ? frand() < 0.5 : f ? true : b ? false : frand() < 0.5;
        fxPlan = [dirF ? (e0 + 1) % 3 : (e0 + 2) % 3, dirF ? fw : bw, 0]; fxPlanN = 2;
        if (!f && !b) fxPlan[fxPlanN++] = e0;
      }
    }
    const ik = 1 - fx.inten / 255, dec = dt * rate * (0.1 + 2.4 * ik * ik);
    for (let s = 0; s < SLOTS * 3; s++) if (s !== fxHead * 3 + fxHeadE) fxA[s] = Math.max(0, fxA[s] - dec);
  }
  const rT = t / 1000, S = 23;                            // Raum-Effekte: Uhr und Strecke entlang dieser einen Wand (cm)
  const beat = Math.floor(fxPhase * 2), newBeat = beat !== fxBeat; fxBeat = beat;
  const K = fx.inten / 255, Lc = spanX * S;
  for (let i = 0; i < SLOTS; i++) {
    const p = P[i];
    if (!p.used || !p.attached || p.state === 0) { TGT[i].forEach(a => a.fill(0)); continue; }
    const points = p.edges ? 3 : 1;
    for (let e = 0; e < points; e++) {
      const [px, py, dd] = edgePos(i, p.edges ? e : -1);
      const c = [0, 0, 0, 0], s = i * 3 + e;
      const [rx, ry] = effXY(px, py), u = (rx - minX) / spanX, v = ry, depth = p.depth + dd, at = (rx - minX) * S;
      switch (fx.id) {
        case 1: pcol(fxPhase * 0.1 + u * K * 0.6, c, D_HUE); break;
        case 2: pcol(fxPhase * 0.15 + u * (0.2 + 1.6 * K), c, D_HUE); break;
        case 3: { const lo = 0.02 + 0.45 * (1 - K); pcol(fxPhase * 0.05 + u * 0.3, c, D_COLOR);
          mul(c, lo + (1 - lo) * (0.5 + 0.5 * Math.cos(fxPhase * TAU / 4 - (p.edges ? u * 0.6 : 0)))); break; }
        case 4: { fxT[s] += dt * rate / 3.5;
          if (fxT[s] >= 1) { fxA[s] = fxB[s]; fxB[s] = fxA[s] + (frand() * 2 - 1) * (0.1 + 0.4 * K); fxT[s] = 0; }
          let d = fxB[s] - fxA[s]; d -= Math.floor(d + 0.5); const tt = fxT[s] * fxT[s] * (3 - 2 * fxT[s]);
          pcol(fxA[s] + d * tt, c, D_HUE); break; }
        case 5: { fxA[s] = Math.max(0, fxA[s] - dt * rate * 2.2);
          if (frand() < dt * rate * (0.05 + 0.9 * K) / points) fxA[s] = 1;
          const a = fxA[s] * fxA[s]; pcol(u * 0.5 + fxPhase * 0.03, c, D_COLOR); mul(c, 0.18 + 0.5 * a); c[3] = Math.min(255, c[3] + 255 * a); break; }
        case 6: { const w = 0.5 + 0.5 * Math.cos(TAU * (fxPhase * 0.5 - depth * 0.17));
          pcol(fxPhase * 0.1 - depth * 0.08, c, D_COLOR); mul(c, 0.05 + 0.95 * Math.pow(w, 1 + 7 * (1 - K))); break; }
        case 7: { fxA[s] += (frand() - fxA[s]) * Math.min(1, dt * rate * 9); fxB[s] += (fxA[s] - fxB[s]) * Math.min(1, dt * rate * 5);
          const h = 1 - (0.2 + 0.8 * K) * (1 - fxB[s]); if (fx.pal === 0) fireColor(h, c); else { pcol(h * 0.5, c, D_FIRE); mul(c, h); } break; }
        case 8: { const sv = 0.5 + 0.5 * Math.sin(u * 5 + fxPhase * 0.9 + v * 0.7) * Math.sin(u * 2.3 - fxPhase * 0.55);
          pcol(fxPhase * 0.04 + u * 0.48, c, D_AURORA); mul(c, 1 - (0.3 + 0.7 * K) * (1 - sv)); break; }
        case 9: { let d = u - (fxPhase * 0.2 - Math.floor(fxPhase * 0.2)); d -= Math.floor(d); d = 1 - d;
          pcol(fxPhase * 0.02, c, D_COLOR); mul(c, 0.03 + 0.97 * Math.exp(-d * (3 + 14 * K))); break; }
        case 10: { const a = Math.atan2(v - cyW, rx - cxW) / TAU, r = Math.hypot(rx - cxW, v - cyW) / maxR;
          pcol(a + fxPhase * 0.08 + r * (0.2 + 1.2 * K), c, D_HUE); break; }
        case 11: { const d2 = (rx - fxFlashX) ** 2 + (v - fxFlashY) ** 2, f = fxFlash * Math.exp(-d2 / (1.5 + 3 * K)) * (0.7 + 0.3 * frand());
          if (fx.pal) { pcol(0.1, c, D_COLOR); mul(c, 0.12); } else { c[0] = 2; c[1] = 6; c[2] = 34; }
          c[0] += 170 * f; c[1] += 170 * f; c[2] += 255 * f; c[3] += 255 * f; break; }
        case 12: { fxA[s] += (frand() - fxA[s]) * Math.min(1, dt * rate * 6); fxB[s] += (fxA[s] - fxB[s]) * Math.min(1, dt * rate * 3);
          const h = 1 - (0.15 + 0.5 * K) * (1 - fxB[s]);
          if (fx.pal === 0) { c[0] = 255 * h; c[1] = 105 * h * h; c[2] = 12 * h; c[3] = 70 * h * h; } else { pcol(fxB[s] * 0.3, c, D_FIRE); mul(c, h); } break; }
        case 13: if (newBeat && frand() < 0.3 + 0.7 * K) fxA[s] = frand(); pcol(fxA[s], c, D_HUE); break;
        case 14: case 24: { const b = fxA[s]; pcol(fx.id === 24 ? rT * 0.03 : fxPhase * 0.03, c, D_COLOR); mul(c, 0.02 + 0.98 * Math.pow(b, 1.6));
          if (i === fxHead && e === fxHeadE) c[3] = Math.min(255, c[3] + 120); break; }
        case 16: { const tt = u * (0.6 + 1.4 * K) + (fx.speed > 1 ? fxPhase * 0.02 : 0);
          if (fx.pal === 0) { let m = tt - Math.floor(tt); m = m < 0.5 ? m * 2 : 2 - m * 2; m = m * m * (3 - 2 * m);
            const a1 = [fx.r, fx.g, fx.b, fx.w]; for (let ch = 0; ch < 4; ch++) c[ch] = a1[ch] + (fxC2[ch] - a1[ch]) * m; }
          else pcol(tt * 0.5, c, D_COLOR); break; }
        case 15: { const sv = 0.5 + 0.5 * Math.sin(u * 4 + fxPhase * 0.35 + 1.5 * Math.sin(v * 1.8 - fxPhase * 0.25));
          pcol(sv * 0.5 + fxPhase * 0.02, c, D_FIRE); mul(c, 0.2 + 0.8 * Math.pow(sv, 1 + 2 * K)); break; }
        case 17: { let tt = fxPhase * 0.8 - depth * (0.03 + 0.08 * K); tt -= Math.floor(tt);
          const b = Math.exp(-(tt * tt) / 0.003) + 0.65 * Math.exp(-((tt - 0.2) ** 2) / 0.003);
          pcol(fxPhase * 0.02, c, D_COLOR); mul(c, 0.06 + 0.94 * Math.min(1, b)); break; }
        case 18: { const sv = Math.sin(u * (3 + 5 * K) + fxPhase * 0.6) + Math.sin(v * 2.2 - fxPhase * 0.45) + Math.sin((u + v) * 2.5 + fxPhase * 0.3);
          pcol(sv * 0.17 + fxPhase * 0.03, c, D_HUE); break; }
        case 19: { fxA[s] = Math.max(0, fxA[s] - dt * rate * 3.5); if (frand() < dt * rate * (0.15 + 1.6 * K) / points) fxA[s] = 1;
          const a = fxA[s] * fxA[s];
          if (fx.pal === 0) { c[0] = 4 + 40 * a; c[1] = 10 + 110 * a; c[2] = 40 + 215 * a; c[3] = 60 * a; }
          else { pcol(u * 0.4 + fxPhase * 0.02, c, D_COLOR); mul(c, 0.12 + 0.88 * a); } break; }
        case 20: { const tw = 0.5 + 0.5 * Math.sin(fxPhase * (0.6 + fxB[s]) + fxA[s] * TAU), star = fxB[s] > 0.55 - 0.35 * K ? Math.pow(tw, 6) : 0;
          if (fx.pal === 0) { c[0] = 2 + 60 * star; c[1] = 3 + 70 * star; c[2] = 18 + 90 * star; c[3] = 200 * star; }
          else { pcol(fxA[s], c, D_COLOR); mul(c, 0.05 + 0.95 * star); } break; }
        case 21: case 27: { const d = Math.hypot(rx - fwX, v - fwY), r = fwT * 2.6, life = Math.max(0, 1 - fwT / 1.4);
          const a = Math.exp(-(d - r) * (d - r) / 0.25) * life + (fwT < 0.12 && d < 0.4 ? 1 : 0);
          pcol(fwH + d * 0.08, c, D_HUE); mul(c, Math.min(1, a)); c[3] = Math.min(255, c[3] + (fwT < 0.15 ? 255 * a : 40 * a * life)); break; }
        case 22: { let off = Math.sin(Math.round(rx * 2) * 12.9898) * 43758.5; off -= Math.floor(off);
          let d = v * 0.35 - fxPhase * 0.35 + off; d -= Math.floor(d); const b = Math.pow(1 - d, 3 + 8 * K);
          if (fx.pal === 0) { c[0] = 0; c[1] = 20 + 235 * b; c[2] = 30 * b * b; c[3] = d > 0.96 ? 160 : 0; }
          else { pcol(off, c, D_COLOR); mul(c, 0.05 + 0.95 * b); } break; }
        case 23: { const tc = [0, 220, 140, 0], br = 0.55 + 0.15 * Math.sin(fxPhase * 0.5 - u * 2); for (let ch = 0; ch < 4; ch++) c[ch] = tc[ch] * br; break; }
        case 25: { const W = S * (3 + 25 * (1 - K)); pcol(at / W - (rT * rate * 0.15) % 1000, c, D_HUE); break; }
        case 26: { const W = S * (4 + 20 * (1 - K)), b = 0.5 + 0.5 * Math.sin(TAU * (at / W - (rT * rate * 0.25) % 1000));
          pcol(at / (W * 4), c, D_COLOR); mul(c, 0.05 + 0.95 * b * b); break; }
        case 28: { const L = Math.max(S, Lc); let pos = (rT * rate * S * 4) % (2 * L); if (pos > L) pos = 2 * L - pos;
          const d = Math.abs(at - pos) / S; pcol((rT * rate * 0.05) % 1, c, D_COLOR); mul(c, 0.02 + 0.98 * Math.exp(-d * d / (0.3 + 3 * K * K))); break; }
        case 29: { const L = Math.max(S, Lc), dist = Math.abs(at - L / 2);
          const b = 0.5 + 0.5 * Math.cos(TAU * ((rT * rate * 0.2) % 1000 - dist / (S * (6 + 20 * (1 - K)))));
          pcol((rT * rate * 0.01) % 1, c, D_COLOR); mul(c, 0.04 + 0.96 * b * b); break; }
      }
      const k = p.on ? masterK() / 255 : 0;
      for (let ch = 0; ch < 4; ch++) TGT[i][e][ch] = clamp(Math.round(c[ch] * k), 0, 255);
    }
    if (!p.edges) { TGT[i][1] = TGT[i][0].slice(); TGT[i][2] = TGT[i][0].slice(); }
  }
}
function computeTargets() {
  const t = now();
  if (fx.id) fxCompute();
  for (let i = 0; i < SLOTS; i++) {
    const p = P[i];
    if (!p.used || !p.attached || p.state === 0 || !masterOn) { TGT[i].forEach(a => a.fill(0)); continue; }
    const joining = fx.id && p.joinAt && t < p.joinAt;
    if (joining || (!fx.id && p.state === 1)) {
      let tt = (t - (joining ? p.joinAt - FX_JOIN_MS : p.since)) % 1600; tt = tt < 800 ? tt : 1600 - tt;
      const lv = Math.floor(6 + (tt * 26) / 800); TGT[i].forEach(a => { a[0] = 0; a[1] = 0; a[2] = lv; a[3] = 0; }); continue;
    }
    if (!fx.id) { const k = p.on ? p.bri * master / 65025 : 0; const c = [p.r * k, p.g * k, p.b * k, p.w * k].map(Math.round); TGT[i].forEach(a => c.forEach((v, j) => a[j] = v)); }
    if (p.identUntil) {
      if (t >= p.identUntil) p.identUntil = 0;
      else { const lit = (p.identUntil - t) % 333 > 166; TGT[i].forEach(a => { a[0] = a[1] = a[2] = 0; a[3] = lit ? 255 : 0; }); }
    }
  }
}

// ---------- Ausgabe: weicher Übergang wie die Firmware ----------
let transFrom = null, transAt = 0, transDur = 0;
function transition() { transFrom = CUR.map(p => p.map(a => a.slice())); transAt = now(); transDur = cfg.transMs; }
function output() {
  computeTargets();
  const t = now(), f = transFrom && transDur > 0 ? clamp((t - transAt) / transDur, 0, 1) : 1;
  for (let i = 0; i < SLOTS; i++) for (let e = 0; e < 3; e++) for (let ch = 0; ch < 4; ch++)
    CUR[i][e][ch] = f < 1 ? Math.round(transFrom[i][e][ch] + (TGT[i][e][ch] - transFrom[i][e][ch]) * f) : TGT[i][e][ch];
  if (f >= 1) transFrom = null;
  return f < 1;
}
const h2 = v => v.toString(16).toUpperCase().padStart(2, '0');
function liveJson(ov) {
  const c = {}, j = [], t = now();
  for (let i = 0; i < SLOTS; i++) {
    const p = P[i]; if (!p.used || !p.attached) continue;
    c[hex8(p.chip)] = [0, 1, 2].slice(0, p.edges ? 3 : 1).map(e => CUR[i][e].map(h2).join('')).join(',');
    if (p.joinAt && t < p.joinAt) j.push(hex8(p.chip));
  }
  const o = { fx: FX[fx.id].id, c, j }; if (ov) o.ov = 1; return o;
}
function pwrJson() {
  let s = 0; for (let i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) for (let e = 0; e < 3; e++) s += CUR[i][e].reduce((a, b) => a + b, 0) / 255;
  return { est: Math.round(s * cfg.pwrCh * 2), lim: cfg.pwrMax, scale: 100, sensor: false, sda: -1, scl: -1, shunt: 50 };
}

// ---------- JSON für die App ----------
const palId = k => k < PAL_COUNT ? PALS[k].id : 'eigen' + (k - PAL_COUNT + 1);
function palFind(key) {
  key = String(key).toLowerCase();
  for (let k = 0; k < PAL_COUNT; k++) if (PALS[k].id === key || PALS[k].name.toLowerCase() === key) return k;
  for (let k = 0; k < CPAL_MAX; k++) if (cpal[k] && (palId(PAL_COUNT + k) === key || cpal[k].name.toLowerCase() === key)) return PAL_COUNT + k;
  return -1;
}
const fxFind = key => { key = String(key).toLowerCase(); return FX.findIndex(f => f.id === key || f.name.toLowerCase() === key); };
const fxUsesColor = () => !!fx.id && (fx.pal === 2 || (fx.pal === 0 && FX[fx.id].color));
function stateJson(meta) {
  const t = now(), d = {
    sim: true, name, view: Object.assign({}, view), max: MAX_ATTACHED, mqtt: false, mqttSet: false, ap: false,
    ssid: 'Browser', rssi: -51, ip: '127.0.0.1', pwr: pwrJson(), trans: cfg.transMs, game: { on: false, level: 0, ph: 0, best: 0 },
    hue: { ip: '', paired: false, busy: false }, upd: '', ver: VERSION, chip: 'ESP32-S3',
    fx: { id: FX[fx.id].id, speed: fx.speed, inten: fx.inten, pal: palId(fx.pal), dir: fxDir, spin: fxSpin,
      c2: { r: fxC2[0], g: fxC2[1], b: fxC2[2], w: fxC2[3] }, r: fx.r, g: fx.g, b: fx.b, w: fx.w, usesColor: fxUsesColor() },
    master, on: masterOn,
    cpal: cpal.map((q, i) => q && { id: palId(PAL_COUNT + i), name: q.name, slot: i, c: q.c.map(hex6) }).filter(Boolean),
    pfw: PANEL_FW, pAuto: cfg.panelAuto, favs: favs.slice(), sync: { on: false, group: 1, run: false, lead: false, walls: [] },
    zb: { avail: false }, presets: presets.map((p, i) => p && { id: i, name: p.name }).filter(Boolean), preset: curPreset,
    panels: [], loose: [], ghosts: []
  };
  if (cfg.daylight) d.day = { ok: true, dim: 100, warm: 0 };
  if (sleepEnd) d.sleep = Math.max(1, Math.ceil((sleepEnd - t) / 1000));
  if (meta) {
    d.effects = FX.map(f => ({ id: f.id, name: f.name, color: f.color }));
    d.palettes = PALS.map(p => ({ id: p.id, name: p.name, c: p.c.map(hex6) }));
  }
  for (let i = 0; i < SLOTS; i++) {
    const p = P[i]; if (!p.used) continue;
    if (!p.attached) { d.loose.push(hex8(p.chip)); continue; }
    const o = { id: hex8(p.chip), main: i === 0, x: p.x, y: p.y, up: isUp(p.x, p.y), rot: p.rot, parent: p.parent >= 0 ? hex8(P[p.parent].chip) : '',
      state: p.state, on: p.on, edges: p.edges, fw: PANEL_FW, clips: p.clips, caps: 3, lit: Math.floor(p.lit / 60) * 60,
      r: p.r, g: p.g, b: p.b, w: p.w, bri: p.bri };
    if (p.identUntil) o.ident = true;
    d.panels.push(o);
  }
  if (countAttached() < MAX_ATTACHED) for (let i = 0; i < SLOTS; i++) {
    const p = P[i]; if (!p.used || !p.attached) continue;
    for (let e = 0; e < 3; e++) {
      if (!hasConnector(i, e)) continue;
      const [nx, ny] = neighbor(p.x, p.y, worldDir(p, e));
      if (findAt(nx, ny) >= 0 || d.ghosts.some(g => g.x === nx && g.y === ny)) continue;
      d.ghosts.push({ x: nx, y: ny, up: isUp(nx, ny), parent: hex8(p.chip), edge: e });
    }
  }
  return d;
}
function configJson() {
  return { chip: 'ESP32-S3', ver: VERSION, mode: 'sim', board: cfg.board,
    pins: { rx: 18, tx: 17, de: 8, led: 16, snsR: 4, snsL: 5 },
    boards: [{ id: 's3-devkitc', name: 'ESP32-S3-DevKitC / Trilumag-Hauptplatine', pins: { rx: 18, tx: 17, de: 8, led: 16, snsR: 4, snsL: 5 } }],
    validPins: [1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42, 47, 48], order: cfg.order,
    light: { trans: cfg.transMs, onAnim: cfg.onAnim, daylight: cfg.daylight, pwrMax: cfg.pwrMax, pwrCh: cfg.pwrCh, sda: -1, scl: -1, shunt: 50, sensor: false, defSda: 1, defScl: 2 },
    orders: ['RGBW', 'GRBW', 'BRGW', 'BGRW', 'WRGB', 'WGRB'], mqtt: Object.assign({ hasPass: false }, cfg.mqtt),
    boot: { mode: cfg.bootMode, preset: cfg.bootPreset }, fault: cfg.fault, guard: cfg.guard, why: 'Simulator im Browser', crashes: 0,
    touch: { on: cfg.touchOn, sens: cfg.touchSens, a1: cfg.tapA1, a2: cfg.tapA2, wave: cfg.touchWave, actions: TAP_NAMES } };
}
const otaJson = () => ({ cur: VERSION, chip: 'esp32s3', auto: false, latest: VERSION, newer: false, ago: -1, busy: false, p: -1, err: '',
  notice: '', bad: '', verifying: false, versions: [] });
function energyJson() {
  const w = pwrJson().est * 24 / 1000;
  return { time: true, w: Math.round(w * 10) / 10, meas: false, total: 0, today: 0, month: 0, year: 0, days: [], months: [], years: [] };
}
function diagJson() {
  return { bus: false, up: Math.floor(now() / 1000), frames: 0, crc: 0, timeouts: 0, discovers: 0, heap: 180000, boot: 'Simulator im Browser',
    crashes: 0, guard: cfg.guard, panels: P.slice(1).filter(p => p.used && p.attached).map(p => ({ id: hex8(p.chip), addr: 0, pings: 0, missed: 0,
      rtt: 0, rttMax: 0, fw: PANEL_FW, att: 1, depth: p.depth, clips: p.clips, caps: 3, lit: p.lit })),
    log: [{ ago: 0, m: 'Simulator im Browser gestartet, keine echte Hardware' }] };
}

// ---------- Befehle (wie apiCall in der Firmware) ----------
function setColor(p, c) { if (!c) return; ['r', 'g', 'b', 'w'].forEach(k => { if (c[k] != null) p[k] = clamp(+c[k] | 0, 0, 255); }); p.hasColor = true; }
function applyCommand(i, cmd) {
  const p = P[i]; if (!p.used || !p.attached) return;
  curPreset = -1;
  if (cmd.state === 'OFF') p.on = false;
  if (cmd.state === 'ON') { p.on = true; masterOn = true; }
  if (Number.isInteger(cmd.brightness)) p.bri = clamp(cmd.brightness, 0, 255);
  if (typeof cmd.edges === 'boolean') { p.edges = cmd.edges; if (cmd.color == null && cmd.state == null) return; }
  if (cmd.color) setColor(p, cmd.color);
  if (!p.hasColor && p.on) { p.w = 255; p.hasColor = true; }
  if (p.state !== 0) p.state = 2;
  if (fx.id && cmd.color) fxStart(0);
  transition();
}
function applyFx(cmd) {
  curPreset = -1;
  if (Number.isInteger(cmd.speed)) fx.speed = clamp(cmd.speed, 1, 100);
  if (Number.isInteger(cmd.intensity)) fx.inten = clamp(cmd.intensity, 0, 255);
  if (cmd.color2) ['r', 'g', 'b', 'w'].forEach((k, j) => { if (cmd.color2[k] != null) fxC2[j] = cmd.color2[k]; });
  if (Number.isInteger(cmd.direction)) fxDir = ((cmd.direction % 360) + 360) % 360;
  if (Number.isInteger(cmd.spin)) { fxSpin = clamp(cmd.spin, 0, 2); if (!fxSpin) fxSpinAng = 0; }
  if (cmd.palette != null) { const k = typeof cmd.palette === 'number' ? cmd.palette : palFind(cmd.palette); if (k >= 0 && (k < PAL_COUNT || cpal[k - PAL_COUNT])) fx.pal = k; }
  if (cmd.color) ['r', 'g', 'b', 'w'].forEach(k => { if (cmd.color[k] != null) fx[k] = cmd.color[k]; });
  let id = fx.id;
  if (typeof cmd.effect === 'string') id = fxFind(cmd.effect); else if (Number.isInteger(cmd.effect)) id = cmd.effect;
  if (id >= 0 && id !== fx.id) fxStart(id);
}
function applyAll(cmd) {
  curPreset = -1;
  if (cmd.state === 'OFF' && masterOn) { masterOn = false; transition(); }
  if (cmd.state === 'ON' && !masterOn) { masterOn = true; transition(); }
  if (cmd.state) sleepEnd = 0;
  if (Number.isInteger(cmd.brightness)) { master = clamp(cmd.brightness, 1, 255); transition(); }
  if (['effect', 'speed', 'palette', 'intensity', 'direction', 'spin', 'color2'].some(k => cmd[k] != null)) {
    const d = Object.assign({}, cmd); delete d.color; delete d.brightness; applyFx(d);
  }
  if (cmd.color) {
    if (fxUsesColor()) ['r', 'g', 'b', 'w'].forEach(k => { if (cmd.color[k] != null) fx[k] = cmd.color[k]; });
    else {
      if (fx.id) fxStart(0);
      P.forEach(p => { if (p.used && p.attached) { setColor(p, cmd.color); p.on = true; if (p.state !== 0) p.state = 2; } });
      transition();
    }
  }
}
function presetSnap(nm) {
  return { name: nm, fx: Object.assign({}, fx), fxC2: fxC2.slice(), fxDir, fxSpin, master, on: masterOn,
    cols: Object.fromEntries(P.filter(p => p.used).map(p => [hex8(p.chip), [p.r, p.g, p.b, p.w, p.bri, p.on ? 1 : 0, p.edges ? 1 : 0]])) };
}
function presetLoad(k) {
  const s = presets[k]; if (!s) return false;
  fx = Object.assign({}, s.fx); fxC2 = s.fxC2.slice(); fxDir = s.fxDir; fxSpin = s.fxSpin; master = s.master; masterOn = s.on;
  P.forEach(p => { const c = s.cols[hex8(p.chip)]; if (!p.used || !c) return;
    [p.r, p.g, p.b, p.w, p.bri] = c; p.on = !!c[5]; p.edges = !!c[6]; p.hasColor = true; if (p.attached && p.state) p.state = 2; });
  fxReset(); curPreset = k; transition(); return true;
}
function touchEvent(i, kind) {
  ws.forEach(s => s.push({ t: 'touch', d: { id: hex8(P[i].chip), k: kind } }));
  switch (kind === 2 ? cfg.tapA2 : cfg.tapA1) {
    case 1: applyCommand(i, { state: P[i].on ? 'OFF' : 'ON' }); break;
    case 2: applyAll({ state: masterOn ? 'OFF' : 'ON' }); break;
    case 3: { const ids = presets.map((p, k) => p ? k : -1).filter(k => k >= 0); if (ids.length) presetLoad(ids[(ids.indexOf(curPreset) + 1) % ids.length]); break; }
    case 4: fxStart((fx.id + 1) % FX.length); break;
  }
}
function apiCall(path, d) {
  d = d || {};
  if (d.kelvin >= 1000 || d.color_temp >= 1000) { d.color = kelvinRgbw(d.kelvin || d.color_temp); delete d.kelvin; delete d.color_temp; }
  switch (path) {
    case '/api/set':
      if (Number.isInteger(d.panelBri)) { const v = clamp(d.panelBri, 1, 255); P.forEach(p => { if (p.used) p.bri = v; }); curPreset = -1; transition(); return null; }
      if (Array.isArray(d.ids)) { d.ids.forEach(id => { const i = findChip(id); if (i >= 0) applyCommand(i, d); }); return null; }
      if (d.id === 'alle') { applyAll(d); return null; }
      { const i = findChip(d.id); if (i < 0) return 'Panel unbekannt'; applyCommand(i, d); return null; }
    case '/api/effect': if (typeof d.effect === 'string' && fxFind(d.effect) < 0) return 'Effekt unbekannt'; applyAll(d); return null;
    case '/api/presets': {
      const id = d.id == null ? -1 : d.id;
      if (d.action === 'save') {
        const nm = String(d.name || '').trim().slice(0, 24); if (!nm) return 'Name fehlt';
        let k = id >= 0 && id < PRESET_MAX ? id : presets.findIndex(p => p && p.name === nm);
        if (k < 0) for (k = 0; k < PRESET_MAX && presets[k]; k++);
        if (k >= PRESET_MAX) return 'Alle 16 Plätze belegt';
        presets[k] = presetSnap(nm); curPreset = k; return null;
      }
      if (d.action === 'load') return presetLoad(id) ? null : 'Preset unbekannt';
      if (d.action === 'delete') { presets[id] = null; if (curPreset === id) curPreset = -1; return null; }
      return 'Unbekannte Aktion';
    }
    case '/api/palette': {
      let sl = d.slot == null ? -1 : d.slot;
      if (d.action === 'save') {
        if (sl < 0) sl = cpal.findIndex(q => !q);
        if (sl < 0 || sl >= CPAL_MAX) return 'Alle 4 eigenen Paletten belegt';
        const cs = d.colors || []; if (cs.length < 2 || cs.length > 6) return 'Eine Palette braucht 2 bis 6 Farben';
        const nm = String(d.name || '').trim().slice(0, 24) || 'Eigene ' + (sl + 1);
        cpal[sl] = { name: nm, c: cs.map(h => parseInt(String(h).replace('#', ''), 16) & 0xFFFFFF) };
        if (d.use) fx.pal = PAL_COUNT + sl; return null;
      }
      if (d.action === 'delete') { if (sl >= 0 && sl < CPAL_MAX) { cpal[sl] = null; if (fx.pal === PAL_COUNT + sl) fx.pal = 0; } return null; }
      return 'Unbekannte Aktion';
    }
    case '/api/favs': { const k = fxFind(d.effect || ''); if (k < 0) return 'Effekt unbekannt'; const id = FX[k].id;
      favs = favs.filter(x => x !== id); if (d.on !== false) favs.push(id); return null; }
    case '/api/view':
      if (Number.isInteger(d.rot)) view.rot = Math.floor((((d.rot % 360) + 360) % 360) / 30) * 30;
      if (typeof d.mir === 'boolean') view.mir = d.mir; return null;
    case '/api/light':
      if (Number.isInteger(d.trans)) cfg.transMs = clamp(d.trans, 0, 10000);
      if (Number.isInteger(d.onAnim)) cfg.onAnim = clamp(d.onAnim, 0, 3);
      if (typeof d.daylight === 'boolean') cfg.daylight = d.daylight;
      if (Number.isInteger(d.pwrMax)) cfg.pwrMax = clamp(d.pwrMax, 0, 60000);
      if (Number.isInteger(d.pwrCh)) cfg.pwrCh = clamp(d.pwrCh, 1, 100);
      if (d.sda != null && d.sda >= 0) return 'Ein Stromsensor geht nur am echten Hauptpanel';
      return null;
    case '/api/touch':
      if (typeof d.on === 'boolean') cfg.touchOn = d.on; if (Number.isInteger(d.sens)) cfg.touchSens = clamp(d.sens, 1, 10);
      if (Number.isInteger(d.a1)) cfg.tapA1 = clamp(d.a1, 0, 4); if (Number.isInteger(d.a2)) cfg.tapA2 = clamp(d.a2, 0, 4);
      if (typeof d.wave === 'boolean') cfg.touchWave = d.wave; return null;
    case '/api/identify': { const i = findChip(d.id); if (i < 0 || !P[i].attached) return 'Panel unbekannt'; P[i].identUntil = now() + 3000; return null; }
    case '/api/sleep': { const m = d.min == null ? -1 : d.min; if (m < 0 || m > 720) return 'Zeit ungültig'; sleepEnd = m ? now() + m * 60000 : 0; return null; }
    case '/api/boot': { const m = d.mode == null ? -1 : d.mode; if (m < 0 || m > 3) return 'Unbekannte Einstellung';
      if (m === 3) { if (!presets[d.preset]) return 'Bitte ein Preset wählen'; cfg.bootPreset = d.preset; } cfg.bootMode = m; return null; }
    case '/api/fault': cfg.fault = d.on !== false; return null;
    case '/api/guard': cfg.guard = d.on !== false; return null;
    case '/api/name': { const n = String(d.name || '').trim().slice(0, 32); if (!n) return 'Name fehlt'; name = n; return null; }
    case '/api/swap': return d.stop ? null : NA;
    case '/api/panelfw': return d.action === 'auto' ? (cfg.panelAuto = d.on !== false, null) : 'Kein Panel braucht ein Update';
    case '/api/test': case '/api/diag': case '/api/signal': case '/api/progress': return null;
    case '/api/sim/tap': { const i = findChip(d.id); if (i < 0 || !P[i].attached) return 'Panel unbekannt'; touchEvent(i, d.double ? 2 : 1); return null; }
    case '/api/sim/new': { const i = P.findIndex((p, k) => k > 0 && !p.used); if (i < 0) return 'Ablage voll';
      P[i] = Object.assign(newPanel(), { used: true, chip: ((Math.random() * 0xFFFFFFFF) >>> 0) || 1 }); return null; }
    case '/api/sim/remove': {
      const all = !!d.all, one = all ? -1 : findChip(d.id);
      if (!all && (one <= 0 || P[one].attached)) return 'Panel nicht in der Ablage';
      for (let i = 1; i < SLOTS; i++) if (P[i].used && !P[i].attached && (all || i === one)) P[i] = newPanel();
      return null;
    }
    case '/api/sim/attach': {
      const i = findChip(d.id), par = findChip(d.parent);
      if (!placePanel(i, par, d.edge == null ? 9 : d.edge, Math.floor(Math.random() * 3))) return 'Anklipsen nicht möglich';
      P[i].state = 0; P[i].clips++; return null;
    }
    case '/api/sim/rotate': { const i = findChip(d.id); if (i <= 0 || !P[i].attached) return 'Panel unbekannt'; P[i].rot = (P[i].rot + 1) % 3; reconcile(); return null; }
    case '/api/sim/detach': { const i = findChip(d.id); if (i > 0 && P[i].attached) { P[i].attached = false; P[i].state = 0; reconcile(); } return null; }
    case '/api/config': return 'Im Simulator gibt es keine Pins und keinen Bus. Lade Trilumag auf einen ESP32, um echte Panels anzuschließen.';
    case '/api/room': case '/api/peers': return null;
    case '/api/restore': try { const s = typeof d === 'string' ? JSON.parse(d) : d; if (!s) return 'Datei nicht lesbar'; return null; } catch (e) { return 'Datei nicht lesbar'; }
    default: return NA;      // OTA, WLAN, MQTT, Hue, WLED, Wetter, Zigbee, Spiel, andere Panelwände
  }
}

// ---------- Ablauf: Erkennung, Sleep-Timer, Betriebszeit ----------
function tick() {
  const t = now(); let changed = false;
  for (let i = 1; i < SLOTS; i++) {
    const p = P[i];
    if (p.used && p.attached && p.state === 0 && t - p.since > DETECT_MS) {
      p.state = p.hasColor ? 2 : 1; p.joinAt = t + FX_JOIN_MS; changed = true; transition();
    }
  }
  if (sleepEnd && t >= sleepEnd) { sleepEnd = 0; masterOn = false; transition(); changed = true; }
  return changed;
}
setInterval(() => { P.forEach((p, i) => { if (p.used && p.attached && p.on && masterOn && p.state) p.lit++; }); }, 1000);

// ---------- WebSocket-Ersatz ----------
const ws = new Set();
let lastSent = '';
function pushState(force) {
  const d = stateJson(false), s = JSON.stringify(d);
  if (!force && s === lastSent) return; lastSent = s;
  ws.forEach(w => w.push({ t: 'state', d }));
}
class FakeSocket {
  constructor(url) {
    this.url = url; this.readyState = 0; this.onopen = this.onmessage = this.onclose = this.onerror = null;
    setTimeout(() => {
      this.readyState = 1; ws.add(this);
      if (this.onopen) this.onopen({});
      this.push({ t: 'state', d: stateJson(true) }); this.push({ t: 'ota', d: otaJson() });
    }, 60);
  }
  push(m) { if (this.readyState === 1 && this.onmessage) try { this.onmessage({ data: JSON.stringify(m) }); } catch (e) { console.error(e); } }
  send(msg) {
    let m; try { m = JSON.parse(msg); } catch (e) { return; }
    const err = apiCall(m.p, m.b);
    if (err) this.push({ t: 'err', m: err });
    save(); setTimeout(() => pushState(true), 0);
  }
  close() { this.readyState = 3; ws.delete(this); }
  addEventListener(ev, fn) { this['on' + ev] = fn; }
}
FakeSocket.CONNECTING = 0; FakeSocket.OPEN = 1; FakeSocket.CLOSING = 2; FakeSocket.CLOSED = 3;
const RealWS = window.WebSocket;
window.WebSocket = function (url, proto) { return /:81\/?$/.test(String(url)) ? new FakeSocket(url) : new RealWS(url, proto); };
Object.assign(window.WebSocket, { CONNECTING: 0, OPEN: 1, CLOSING: 2, CLOSED: 3 });

// ---------- fetch-Ersatz ----------
const realFetch = window.fetch.bind(window);
const reply = (obj, status) => new Response(JSON.stringify(obj), { status: status || 200, headers: { 'Content-Type': 'application/json' } });
window.fetch = async function (input, init) {
  const url = typeof input === 'string' ? input : input.url;
  const u = new URL(url, location.href);
  if (u.origin !== location.origin || !u.pathname.includes('/api/')) return realFetch(input, init);
  const path = u.pathname.slice(u.pathname.indexOf('/api/'));
  const method = (init && init.method) || 'GET';
  await new Promise(r => setTimeout(r, 20));
  if (method === 'GET') {
    switch (path) {
      case '/api/state': return reply(stateJson(!u.searchParams.has('slim')));
      case '/api/live': output(); return reply(liveJson());
      case '/api/config': return reply(configJson());
      case '/api/energy': return reply(energyJson());
      case '/api/diag': return reply(diagJson());
      case '/api/ota': return reply(otaJson());
      case '/api/peers': return reply({ busy: false, ago: -1, list: [], pushing: false, wled: [] });
      case '/api/room': return reply({ me: hex8(P[0].chip), layout: null });
      case '/api/hue': return reply({ busy: false, ip: '', paired: false, bridges: [], lights: [] });
      case '/api/wifi/scan': return reply({ networks: [] });
      case '/api/health': return reply({ ok: true });
      case '/api/backup': return reply({ sim: true, note: 'Sicherung aus dem Simulator' });
      default: return reply({ error: NA }, 400);
    }
  }
  let body = {};
  try { body = init && init.body ? JSON.parse(init.body) : {}; } catch (e) { body = init.body; }
  const err = apiCall(path, body);
  save(); setTimeout(() => pushState(true), 0);
  return err ? reply({ error: err }, 400) : reply(stateJson(false));
};

// ---------- Bildtakt: 25 Bilder pro Sekunde wie die Firmware ----------
load(); reconcile(); fxReset();
setInterval(() => {
  if (tick()) pushState(true);
  const fading = output();
  if (!ws.size) return;
  if (fx.id || fading) { const l = liveJson(!fx.id); ws.forEach(w => w.push({ t: 'live', d: l })); }
}, 40);
setInterval(() => { if (ws.size) ws.forEach(w => w.push({ t: 'pwr', d: pwrJson() })); pushState(false); }, 1000);

window.trilumagSim = { reset() { try { localStorage.removeItem(KEY); } catch (e) {} location.reload(); } };
})();
