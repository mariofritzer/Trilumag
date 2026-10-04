/*
  Trilumag – Firmware für das Hauptpanel, Version 0.3
  ---------------------------------------------------
  Läuft auf jedem ESP32 (klassischer ESP32, ESP32-S3, ESP32-C3, ESP32-C6).
  Am einfachsten über den Webinstaller: https://mariofritzer.github.io/Trilumag/

  Was sie kann:
   - WLAN-Einrichtung im Webinstaller (Improv) oder über das Setup-Netz "Trilumag-Setup"
   - Web-App unter http://trilumag.local
   - zwei Betriebsarten, umschaltbar in der App:
       Simulation: Panels werden in der App angeklipst
       Bus:        echte Panels über RS-485, Erkennung über die SNS-Leitungen (docs/protokoll.md)
   - Pinbelegung pro Board in der App einstellbar, mit Vorlagen und Prüfung
   - eigener WS2814-Strip des Hauptpanels
   - Effekte für die ganze Wand (Regenbogen, Atmen, Feuer …), über App, API und Home Assistant
   - Farben bleiben pro Panel (Chip-ID) gespeichert, auch über einen Neustart
   - HTTP-API und Home Assistant über MQTT mit Auto-Discovery, Broker in der App einstellbar

  Selbst kompilieren mit der Arduino IDE:
   - Boardverwalter: "esp32" von Espressif
   - Bibliotheken: ArduinoJson (ab 7), PubSubClient, Adafruit NeoPixel
   - ESP32-S3/C3/C6: "USB CDC On Boot" auf "Enabled" stellen
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <stdarg.h>
#include <math.h>
#include "webapp.h"
#include "improv.h"
#include "pins.h"

// ================= Voreinstellungen =================
// Alles hier lässt sich später in der App ändern. Diese Werte gelten nur, solange nichts gespeichert ist.
const char* WLAN_SSID = "";
const char* WLAN_PASS = "";
const uint8_t  MAX_ATTACHED = 30;     // höchstens so viele Panels an der Wand (inkl. Hauptpanel)
const uint32_t DETECT_MS    = 1200;   // simulierte Erkennungszeit nach dem Anklipsen
const uint32_t FX_JOIN_MS   = 5 * 1600; // bei laufendem Effekt pulsieren neue Panels erst 5-mal blau
// ====================================================

const char* FW_NAME    = "Trilumag";
const char* FW_VERSION = "0.3.1";
const char* HOSTNAME   = "trilumag";
const char* SETUP_SSID = "Trilumag-Setup";
const char* SETUP_PASS = "trilumag";

#if CONFIG_IDF_TARGET_ESP32S3
const char* CHIP_FAMILY = "ESP32-S3";
#elif CONFIG_IDF_TARGET_ESP32C3
const char* CHIP_FAMILY = "ESP32-C3";
#elif CONFIG_IDF_TARGET_ESP32S2
const char* CHIP_FAMILY = "ESP32-S2";
#elif CONFIG_IDF_TARGET_ESP32C6
const char* CHIP_FAMILY = "ESP32-C6";
#else
const char* CHIP_FAMILY = "ESP32";
#endif

// Zweiter Konsolenanschluss nur beim S3 (dort hat das DevKit eine eigene UART-Buchse)
#if ARDUINO_USB_CDC_ON_BOOT && CONFIG_IDF_TARGET_ESP32S3
#define HAS_UART_CONSOLE 1
#else
#define HAS_UART_CONSOLE 0
#endif

const uint8_t SLOTS = 40;             // Wand + Ablage

enum Dir : uint8_t { BOTTOM, RIGHT, LEFT, TOP };
// Kanten jedes Panels gegen den Uhrzeigersinn: Kante 1 unten, Kante 2 rechts, Kante 3 links (Spitze oben)
const Dir UP_EDGES[3]   = {BOTTOM, RIGHT, LEFT};
const Dir DOWN_EDGES[3] = {TOP, LEFT, RIGHT};

enum State : uint8_t { DARK = 0, PULSE = 1, ACTIVE = 2 };

struct Panel {
  bool used = false;
  bool attached = false;
  uint32_t chip = 0;
  int8_t x = 0, y = 0;
  uint8_t rot = 0;        // welche Weltkante die eigene Kante 1 ist
  int8_t parent = -1;
  uint8_t depth = 0;      // Schritte bis zum Hauptpanel
  uint8_t addr = 0;       // Kurzadresse auf dem Bus (nur Busbetrieb)
  uint8_t miss = 0;       // verpasste Antworten in Folge
  uint8_t state = DARK;
  uint32_t since = 0;
  uint32_t joinAt = 0;    // bei laufendem Effekt: bis hierher noch blau pulsieren (0 = nicht)
  bool hasColor = false;
  bool on = true;
  uint8_t r = 0, g = 0, b = 0, w = 0, bri = 180;
};

struct Config {
  bool bus = false;
  String board;
  PinSet pins;
  String order = "RGBW";
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser, mqttPass;
};

// Effekt für die ganze Wand (Liste FX[] weiter unten)
struct FxCfg { uint8_t id = 0, speed = 50, bri = 180, r = 255, g = 120, b = 30, w = 0; };

Panel P[SLOTS];
Config cfg;
FxCfg fx;
// Der WebServer bedient nur eine Verbindung nach der anderen und wartet auf eine leere Verbindung
// bis zu 5 s. Browser öffnen solche Verbindungen gern auf Vorrat, dann hängt die App.
// Hier wird eine Verbindung, die nach 200 ms noch nichts geschickt hat, sofort freigegeben.
class AppServer : public WebServer {
public:
  using WebServer::WebServer;
  void handleClient() override {
    if (_currentStatus == HC_WAIT_READ && !_currentClient.available() && millis() - _statusChange > 200) {
      _currentClient.stop();
      _currentClient = NetworkClient();
      _currentStatus = HC_NONE;
    }
    WebServer::handleClient();
  }
};
AppServer server(80);
WiFiClient netClient;
PubSubClient mqtt(netClient);
Preferences prefs;
Adafruit_NeoPixel strip(3, -1, NEO_RGBW + NEO_KHZ800);
improv::Parser improvMain;
#if HAS_UART_CONSOLE
improv::Parser improvUart;
#endif
uint32_t lastMqttTry = 0, lastPoll = 0, lastDiscover = 0, colorsDirtyAt = 0;
bool wlanOk = false, apMode = false, colorsDirty = false;
uint8_t discoverRound = 0;

// ---------- Ausgabe ----------
void logf(const char* fmt, ...) {
  char b[320];
  va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
  Serial.print(b);
#if HAS_UART_CONSOLE
  Serial0.print(b);
#endif
}

void saveWifi(const String& ss, const String& pw);
bool connectWifi(const String& ss, const String& pw, uint32_t timeout);
void startSetupAp();
void publishDiscovery(int i);
void publishAvail(int i, bool online);
void publishState(int i);
void publishFx();

// ---------- Raster ----------
bool isUp(int x, int y) { return ((((x + y) % 2) + 2) % 2) == 0; }
const Dir* slotEdges(bool up) { return up ? UP_EDGES : DOWN_EDGES; }
Dir worldDir(const Panel& p, uint8_t e) { return slotEdges(isUp(p.x, p.y))[(e + p.rot) % 3]; }
Dir opposite(Dir d) {
  switch (d) { case BOTTOM: return TOP; case TOP: return BOTTOM; case RIGHT: return LEFT; default: return RIGHT; }
}
void neighbor(int x, int y, Dir d, int& nx, int& ny) {
  nx = x; ny = y;
  if (d == BOTTOM) ny++; else if (d == TOP) ny--; else if (d == RIGHT) nx++; else nx--;
}
bool hasConnector(int i, uint8_t e) { return !(i == 0 && e == 0); }   // Hauptpanel: unten flach
int findAt(int x, int y) {
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached && P[i].x == x && P[i].y == y) return i;
  return -1;
}
int findChip(uint32_t c) {
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].chip == c) return i;
  return -1;
}
int findAddr(uint8_t a) {
  for (int i = 1; i < SLOTS; i++) if (P[i].used && P[i].attached && P[i].addr == a) return i;
  return -1;
}
int edgeFacing(int i, Dir d) {
  for (uint8_t e = 0; e < 3; e++) if (worldDir(P[i], e) == d) return e;
  return -1;
}
int countAttached() { int n = 0; for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) n++; return n; }
String hex(uint32_t c) { char b[9]; snprintf(b, sizeof b, "%08X", c); return String(b); }
uint32_t parseHex(const char* s) { return (uint32_t)strtoul(s, nullptr, 16); }

// ---------- Farben dauerhaft merken ----------
void loadColor(int i) {
  uint8_t c[6];
  if (prefs.getBytes(("c" + hex(P[i].chip)).c_str(), c, 6) == 6) {
    P[i].r = c[0]; P[i].g = c[1]; P[i].b = c[2]; P[i].w = c[3]; P[i].bri = c[4]; P[i].on = c[5];
    P[i].hasColor = true;
  }
}
void saveColors() {
  for (int i = 0; i < SLOTS; i++) {
    if (!P[i].used || !P[i].hasColor) continue;
    uint8_t c[6] = {P[i].r, P[i].g, P[i].b, P[i].w, P[i].bri, (uint8_t)P[i].on}, o[6];
    String k = "c" + hex(P[i].chip);
    if (prefs.getBytes(k.c_str(), o, 6) == 6 && !memcmp(c, o, 6)) continue;
    prefs.putBytes(k.c_str(), c, 6);
  }
  colorsDirty = false;
}

// ---------- Farbreihenfolge der LEDs ----------
const char* ORDERS[] = {"RGBW", "GRBW", "BRGW", "BGRW", "WRGB", "WGRB"};
uint16_t neoType(const String& o) {
  if (o == "GRBW") return NEO_GRBW;
  if (o == "BRGW") return NEO_BRGW;
  if (o == "BGRW") return NEO_BGRW;
  if (o == "WRGB") return NEO_WRGB;
  if (o == "WGRB") return NEO_WGRB;
  return NEO_RGBW;
}
void orderBytes(const String& o, uint8_t* out) {
  for (int k = 0; k < 4; k++) {
    char c = k < (int)o.length() ? o[k] : 'W';
    out[k] = c == 'R' ? 0 : c == 'G' ? 1 : c == 'B' ? 2 : 3;
  }
}

// ---------- Bus ----------
namespace bus {
const uint8_t SYNC = 0xA5, ALL = 0x00, BYID = 0x7F, REPLY = 0x80;
enum { C_PING = 0x01, C_COLOR = 0x02, C_EDGES = 0x03, C_FRAME = 0x04, C_PULSE = 0x05, C_ORDER = 0x06,
       C_BEACON = 0x10, C_DISCOVER = 0x11, C_PROBE = 0x12, C_ASSIGN = 0x14, C_RESET = 0x15 };

HardwareSerial& port = Serial1;

uint8_t crc8(uint8_t c, uint8_t b) {
  c ^= b;
  for (int i = 0; i < 8; i++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
  return c;
}

void send(uint8_t addr, uint8_t cmd, const uint8_t* d = nullptr, uint8_t len = 0) {
  while (port.available()) port.read();
  uint8_t buf[210]; uint8_t n = 0, c = 0;
  buf[n++] = SYNC; buf[n++] = addr; buf[n++] = cmd; buf[n++] = len;
  if (len) { memcpy(buf + n, d, len); n += len; }
  for (int i = 1; i < n; i++) c = crc8(c, buf[i]);
  buf[n++] = c;
  port.write(buf, n);
  port.flush();
}

void sendById(uint32_t chip, uint8_t cmd, uint8_t extra) {
  uint8_t d[5] = {(uint8_t)chip, (uint8_t)(chip >> 8), (uint8_t)(chip >> 16), (uint8_t)(chip >> 24), extra};
  send(BYID, cmd, d, 5);
}

struct Reply { uint8_t addr, cmd, len; uint8_t data[200]; };

// Wartet auf eine gültige Antwort; true, wenn eine kam
bool receive(Reply& r, uint32_t timeoutMs) {
  uint8_t f[206]; uint16_t pos = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (port.available()) {
      uint8_t b = port.read();
      if (pos == 0 && b != SYNC) continue;
      f[pos++] = b;
      if (pos == 4 && f[3] > 200) { pos = 0; continue; }
      if (pos >= 4 && pos == 5 + f[3]) {
        uint8_t c = 0;
        for (int i = 1; i < pos - 1; i++) c = crc8(c, f[i]);
        if (c == f[pos - 1] && (f[1] & REPLY)) {
          r.addr = f[1] & 0x7F; r.cmd = f[2]; r.len = f[3]; memcpy(r.data, f + 4, f[3]);
          return true;
        }
        pos = 0;
      }
    }
    delay(0);
  }
  return false;
}

// Eigene SNS-Pins des Hauptpanels: Leuchtfeuer (Low) oder Fühler (Pull-up)
void ownBeacon(bool on) {
  int8_t pins[2] = {cfg.pins.snsR, cfg.pins.snsL};
  for (int8_t p : pins) {
    if (on) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
    else pinMode(p, INPUT_PULLUP);
  }
}

void beacons(bool on) {
  uint8_t v = on;
  send(ALL, C_BEACON, &v, 1);
  ownBeacon(on);
}

void sendColor(int i) {
  const Panel& p = P[i];
  if (!p.addr) return;
  if (p.state == ::PULSE) { send(p.addr, C_PULSE); return; }
  uint8_t c[4] = {0, 0, 0, 0};
  if (p.on && p.state == ::ACTIVE) {
    c[0] = (uint16_t)p.r * p.bri / 255; c[1] = (uint16_t)p.g * p.bri / 255;
    c[2] = (uint16_t)p.b * p.bri / 255; c[3] = (uint16_t)p.w * p.bri / 255;
  }
  send(p.addr, C_COLOR, c, 4);
}

void sendOrder(uint8_t addr) {
  uint8_t o[4]; orderBytes(cfg.order, o);
  send(addr, C_ORDER, o, 4);
}

void begin() {
  port.begin(250000, SERIAL_8N1, cfg.pins.rx, cfg.pins.tx);
  port.setPins(cfg.pins.rx, cfg.pins.tx, -1, cfg.pins.de);
  port.setMode(UART_MODE_RS485_HALF_DUPLEX);
  delay(50);
  send(ALL, C_RESET);
  delay(5);
  sendOrder(ALL);
  beacons(true);
  logf("[BUS] gestartet: RX %d, TX %d, DE %d, SNS rechts %d, SNS links %d\n",
       cfg.pins.rx, cfg.pins.tx, cfg.pins.de, cfg.pins.snsR, cfg.pins.snsL);
}
}  // namespace bus

void reconcile();
bool placePanel(int i, int parent, uint8_t edge, uint8_t own);

// Neues Panel gefunden: Nachbarn orten, Adresse vergeben, Farbe oder Pulsieren
void busHandleNew(uint32_t chip, uint8_t mask) {
  int i = findChip(chip);
  if (i >= 0 && P[i].attached) { P[i].attached = false; P[i].addr = 0; }   // Panel wurde neu gestartet
  if (i < 0) {
    for (int k = 1; k < SLOTS; k++) if (!P[k].used) { i = k; break; }
    if (i < 0) { logf("[BUS] kein Platz mehr für %s\n", hex(chip).c_str()); return; }
    P[i] = Panel(); P[i].used = true; P[i].chip = chip;
    loadColor(i);
  }
  if (countAttached() >= MAX_ATTACHED) { logf("[BUS] Höchstzahl erreicht, %s wird ignoriert\n", hex(chip).c_str()); return; }

  int parent = -1, parentEdge = -1, ownEdge = -1;
  bus::beacons(false);
  delay(2);
  for (uint8_t k = 0; k < 3 && parent < 0; k++) {
    if (!(mask & (1 << k))) continue;
    bus::sendById(chip, bus::C_PROBE, k);
    bus::Reply r;
    if (!bus::receive(r, 5)) continue;
    delay(1);
    if (digitalRead(cfg.pins.snsR) == LOW) { parent = 0; parentEdge = 1; ownEdge = k; break; }
    if (digitalRead(cfg.pins.snsL) == LOW) { parent = 0; parentEdge = 2; ownEdge = k; break; }
    for (int j = 1; j < SLOTS && parent < 0; j++) {
      if (!P[j].used || !P[j].attached || !P[j].addr) continue;
      bus::send(P[j].addr, bus::C_PING);
      if (bus::receive(r, 4) && r.addr == P[j].addr && r.len >= 2 && r.data[1]) {
        for (uint8_t e = 0; e < 3; e++) if (r.data[1] & (1 << e)) { parent = j; parentEdge = e; ownEdge = k; break; }
      }
    }
  }
  bus::sendById(chip, bus::C_PROBE, 0xFF);
  { bus::Reply r; bus::receive(r, 5); }
  bus::beacons(true);

  if (parent < 0) { logf("[BUS] %s meldet Kontakt, Nachbar aber nicht gefunden\n", hex(chip).c_str()); return; }

  uint8_t addr = 0;
  for (uint8_t a = 1; a <= 0x3E; a++) if (findAddr(a) < 0) { addr = a; break; }
  if (!addr || !placePanel(i, parent, parentEdge, ownEdge)) {
    logf("[BUS] %s lässt sich nicht einordnen\n", hex(chip).c_str()); return;
  }
  bus::sendById(chip, bus::C_ASSIGN, addr);
  bus::Reply r;
  if (!bus::receive(r, 5) || r.addr != addr) {
    logf("[BUS] %s hat die Adresse nicht bestätigt\n", hex(chip).c_str());
    P[i].attached = false; reconcile(); return;
  }
  P[i].addr = addr; P[i].miss = 0;
  P[i].joinAt = (millis() + FX_JOIN_MS) | 1;
  bus::sendOrder(addr);
  P[i].state = P[i].hasColor ? ACTIVE : PULSE;
  bus::sendColor(i);
  logf("[BUS] %s an Kante %d von %s, eigene Kante %d, Adresse %u, %s\n", hex(chip).c_str(), parentEdge + 1,
       hex(P[parent].chip).c_str(), ownEdge + 1, addr, P[i].hasColor ? "alte Farbe" : "pulsiert blau");
  publishDiscovery(i); publishAvail(i, true); publishState(i);
}

void busLoop() {
  uint32_t now = millis();
  if (now - lastPoll > 150) {
    lastPoll = now;
    bool lost = false;
    for (int i = 1; i < SLOTS; i++) {
      Panel& p = P[i];
      if (!p.used || !p.attached || !p.addr) continue;
      bus::send(p.addr, bus::C_PING);
      bus::Reply r;
      if (bus::receive(r, 4) && r.addr == p.addr) {
        p.miss = 0;
        if (!fx.id && r.len >= 1 && r.data[0] == DARK && p.state != DARK) bus::sendColor(i);   // Panel hat Zustand verloren
      } else if (++p.miss >= 3) {
        logf("[BUS] %s antwortet nicht mehr, gilt als abgeklipst\n", hex(p.chip).c_str());
        p.attached = false; p.addr = 0; p.state = DARK; publishAvail(i, false); lost = true;
      }
    }
    if (lost) reconcile();
  }
  if (now - lastDiscover > 400 && countAttached() < MAX_ATTACHED) {
    lastDiscover = now;
    const uint8_t slots = 8;
    uint8_t d[2] = {++discoverRound, slots};
    bus::send(bus::ALL, bus::C_DISCOVER, d, 2);
    uint32_t found[8]; uint8_t masks[8]; uint8_t n = 0;
    uint32_t t0 = millis();
    bus::Reply r;
    while (n < 8) {
      uint32_t el = millis() - t0;
      if (el >= (uint32_t)slots + 4) break;
      if (bus::receive(r, slots + 4 - el) && r.cmd == bus::C_DISCOVER && r.len >= 5) {
        found[n] = (uint32_t)r.data[0] | ((uint32_t)r.data[1] << 8) | ((uint32_t)r.data[2] << 16) | ((uint32_t)r.data[3] << 24);
        masks[n++] = r.data[4];
      }
    }
    for (uint8_t k = 0; k < n; k++) busHandleNew(found[k], masks[k]);
  }
}

// ---------- LEDs des Hauptpanels ----------
void showMain() {
  if (cfg.pins.led < 0) return;
  if (fx.id) return;               // Effekt läuft, fxSend kümmert sich um den Strip
  const Panel& p = P[0];
  uint32_t c = 0;
  if (p.on) c = strip.Color((uint16_t)p.r * p.bri / 255, (uint16_t)p.g * p.bri / 255,
                            (uint16_t)p.b * p.bri / 255, (uint16_t)p.w * p.bri / 255);
  for (int k = 0; k < 3; k++) strip.setPixelColor(k, c);
  strip.show();
}

// ---------- Effekte ----------
// Das Hauptpanel rechnet jeden Effekt selbst und schickt etwa 25 Bilder pro Sekunde
// mit einem einzigen FRAME-Befehl an alle Panels. Die Panel-Firmware braucht dafür nichts Neues.
struct FxDef { const char* id; const char* name; bool color; };
const FxDef FX[] = {
  {"aus",         "Kein Effekt",     false},
  {"regenbogen",  "Regenbogen",      false},
  {"welle",       "Regenbogenwelle", false},
  {"atmen",       "Atmen",           true},
  {"farbwechsel", "Farbwechsel",     false},
  {"funkeln",     "Funkeln",         true},
  {"ausbreiten",  "Ausbreiten",      true},
  {"feuer",       "Feuer",           false},
  {"polarlicht",  "Polarlicht",      false},
};
const uint8_t FX_COUNT = sizeof(FX) / sizeof(FX[0]);
const uint32_t FX_FRAME_MS = 40;

float fxPhase = 0, fxA[SLOTS], fxB[SLOTS], fxT[SLOTS];
uint32_t fxLast = 0, fxDirtyAt = 0, fxLastFrame = 0;
bool fxDirty = false;
uint8_t OUT[SLOTS][4];          // aktuelles Effektbild pro Panel, Helligkeit schon eingerechnet

int fxFind(const char* key) {
  for (uint8_t k = 0; k < FX_COUNT; k++) if (!strcasecmp(key, FX[k].id) || !strcasecmp(key, FX[k].name)) return k;
  return -1;
}
float frand() { return (esp_random() & 0xFFFFFF) / 16777216.0f; }

// Farbton 0..1 → RGB 0..255
void hue(float h, float* c) {
  h = h - floorf(h);
  float x = h * 6; int k = (int)x; float f = x - k;
  float q = 1 - f;
  float r = 0, g = 0, b = 0;
  switch (k % 6) {
    case 0: r = 1; g = f; break;   case 1: r = q; g = 1; break;
    case 2: g = 1; b = f; break;   case 3: g = q; b = 1; break;
    case 4: r = f; b = 1; break;   default: r = 1; b = q;
  }
  c[0] = r * 255; c[1] = g * 255; c[2] = b * 255; c[3] = 0;
}
void base(float k, float* c) { c[0] = fx.r * k; c[1] = fx.g * k; c[2] = fx.b * k; c[3] = fx.w * k; }

void fxReset() {
  for (int i = 0; i < SLOTS; i++) { fxA[i] = frand(); fxB[i] = frand(); fxT[i] = frand(); }
  fxLast = millis();
}

void fxCompute() {
  uint32_t now = millis();
  float dt = (now - fxLast) / 1000.0f; fxLast = now;
  if (dt > 0.2f) dt = 0.2f;
  float rate = powf(4.0f, (fx.speed - 50) / 50.0f);   // Tempo 1..100 → 0,25- bis 4-fach
  fxPhase += dt * rate;
  if (fxPhase > 100000) fxPhase = 0;

  float minX = 1e9, maxX = -1e9;
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) { minX = fminf(minX, P[i].x * 0.5f); maxX = fmaxf(maxX, P[i].x * 0.5f); }
  float spanX = fmaxf(1.0f, maxX - minX);
  const float TAU = 6.2831853f;

  for (int i = 0; i < SLOTS; i++) {
    float c[4] = {0, 0, 0, 0};
    const Panel& p = P[i];
    if (!p.used || !p.attached || p.state == DARK) { memset(OUT[i], 0, 4); continue; }
    if (p.joinAt && (int32_t)(now - p.joinAt) < 0) {      // gerade angeklipst: so pulsieren wie das Panel selbst
      uint32_t t = (now - (p.joinAt - FX_JOIN_MS)) % 1600;
      t = t < 800 ? t : 1600 - t;
      OUT[i][0] = 0; OUT[i][1] = 0; OUT[i][2] = (uint8_t)(6 + (t * 26) / 800); OUT[i][3] = 0;
      continue;
    }
    float u = (p.x * 0.5f - minX) / spanX;                 // 0 links … 1 rechts
    float v = p.y * 0.866f;
    switch (fx.id) {
      case 1: hue(fxPhase * 0.1f, c); break;                                  // ganze Wand im Farbkreis
      case 2: hue(fxPhase * 0.15f + u * 0.85f, c); break;                     // Farbkreis wandert von links nach rechts
      case 3: base(0.06f + 0.94f * (0.5f + 0.5f * cosf(fxPhase * TAU / 4)), c); break;   // startet hell
      case 4: {                                                               // jedes Panel blendet zu eigener Zufallsfarbe
        fxT[i] += dt * rate / 3.5f;
        if (fxT[i] >= 1) { fxA[i] = fxB[i]; fxB[i] = frand(); fxT[i] = 0; }
        float d = fxB[i] - fxA[i]; if (d > 0.5f) d -= 1; if (d < -0.5f) d += 1;
        float t = fxT[i] * fxT[i] * (3 - 2 * fxT[i]);
        hue(fxA[i] + d * t, c); break;
      }
      case 5: {                                                               // Grundfarbe, einzelne Panels blitzen weiß auf
        fxA[i] = fmaxf(0, fxA[i] - dt * rate * 2.2f);
        if (frand() < dt * rate * 0.35f) fxA[i] = 1;
        float a = fxA[i] * fxA[i];
        base(0.18f + 0.5f * a, c); c[3] = fminf(255, c[3] + 255 * a); break;
      }
      case 6: {                                                               // Wellen laufen vom Hauptpanel nach außen
        float w = 0.5f + 0.5f * cosf(TAU * (fxPhase * 0.5f - p.depth * 0.17f));
        base(0.05f + 0.95f * w * w * w, c); break;
      }
      case 7: {                                                               // flackernde Glut
        fxA[i] += (frand() - fxA[i]) * fminf(1, dt * rate * 9);
        fxB[i] += (fxA[i] - fxB[i]) * fminf(1, dt * rate * 5);
        float h = 0.3f + 0.7f * fxB[i];
        c[0] = 255 * h; c[1] = 85 * h * h; c[2] = 0; c[3] = 20 * h * h * h; break;
      }
      case 8: {                                                               // Polarlicht: grün bis violett, langsam ziehend
        float s = 0.5f + 0.5f * sinf(u * 5.0f + fxPhase * 0.9f + v * 0.7f) * sinf(u * 2.3f - fxPhase * 0.55f);
        hue(0.36f + 0.36f * (0.5f + 0.5f * sinf(fxPhase * 0.25f + u * 3.0f)), c);
        for (int k = 0; k < 3; k++) c[k] *= 0.15f + 0.85f * s; break;
      }
    }
    float k = p.on ? fx.bri / 255.0f : 0;
    for (int ch = 0; ch < 4; ch++) OUT[i][ch] = (uint8_t)fminf(255, fmaxf(0, c[ch] * k));
  }
}

void fxSend() {
  if (cfg.pins.led >= 0) {
    uint32_t c = strip.Color(OUT[0][0], OUT[0][1], OUT[0][2], OUT[0][3]);
    for (int k = 0; k < 3; k++) strip.setPixelColor(k, c);
    strip.show();
  }
  if (!cfg.bus) return;
  uint8_t lo = 0xFF, hi = 0;
  for (int i = 1; i < SLOTS; i++) if (P[i].used && P[i].attached && P[i].addr) { if (P[i].addr < lo) lo = P[i].addr; if (P[i].addr > hi) hi = P[i].addr; }
  if (lo > hi) return;
  // höchstens 48 Panels pro Rahmen (Datenlänge bis 200 Byte)
  for (uint8_t first = lo; first <= hi; first += 48) {
    uint8_t n = (hi - first + 1) > 48 ? 48 : (hi - first + 1);
    uint8_t d[2 + 48 * 4] = {first, n};
    for (int i = 1; i < SLOTS; i++) {
      const Panel& p = P[i];
      if (!p.used || !p.attached || p.addr < first || p.addr >= first + n) continue;
      memcpy(d + 2 + 4 * (p.addr - first), OUT[i], 4);
    }
    bus::send(bus::ALL, bus::C_FRAME, d, 2 + 4 * n);
  }
}

void fxLoop() {
  if (!fx.id || millis() - fxLastFrame < FX_FRAME_MS) return;
  fxLastFrame = millis();
  fxCompute();
  fxSend();
}

void sendToPanel(int i);
void fxStart(int id) {
  if (id < 0 || id >= FX_COUNT) return;
  bool was = fx.id;
  fx.id = id;
  fxDirty = true; fxDirtyAt = millis();
  if (id && id != was) { fxReset(); fxPhase = 0; }   // jeder Effekt beginnt von vorn, nicht mitten in einer dunklen Phase
  fxLastFrame = 0;                                      // nächstes Bild sofort, nicht erst nach 40 ms
  if (!id && was) for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) sendToPanel(i);   // zurück zu den festen Farben
  logf("[FX] %s\n", FX[fx.id].name);
  publishFx();
}

void fxSave() {
  prefs.putBytes("fx", &fx, sizeof fx);
  fxDirty = false;
}
void fxLoad() {
  FxCfg f;
  if (prefs.getBytes("fx", &f, sizeof f) == sizeof f && f.id < FX_COUNT) fx = f;
  fxReset();
}

// Befehl {"effect":"regenbogen","speed":50,"brightness":180,"color":{...}}
void applyFx(JsonVariantConst cmd) {
  if (cmd["speed"].is<int>()) fx.speed = constrain(cmd["speed"].as<int>(), 1, 100);
  if (cmd["brightness"].is<int>()) fx.bri = constrain(cmd["brightness"].as<int>(), 1, 255);
  JsonVariantConst c = cmd["color"];
  if (!c.isNull()) { fx.r = c["r"] | fx.r; fx.g = c["g"] | fx.g; fx.b = c["b"] | fx.b; fx.w = c["w"] | fx.w; }
  int id = fx.id;
  if (cmd["effect"].is<const char*>()) id = fxFind(cmd["effect"].as<const char*>());
  else if (cmd["effect"].is<int>()) id = cmd["effect"].as<int>();
  if (id >= 0 && id != fx.id) fxStart(id);
  else { fxDirty = true; fxDirtyAt = millis(); fxLastFrame = 0; publishFx(); }
}

// ---------- Ausgabe an ein Panel ----------
void sendToPanel(int i) {
  const Panel& p = P[i];
  if (i == 0) { showMain(); return; }
  if (cfg.bus) bus::sendColor(i);
  else logf("[SIM] %s  Zustand=%u  an=%d  RGBW=%u,%u,%u,%u  Hell=%u\n", hex(p.chip).c_str(), p.state, p.on, p.r, p.g, p.b, p.w, p.bri);
}

// ---------- MQTT / Home Assistant ----------
String tBase(int i) { return "trilumag/" + hex(P[i].chip); }

void publishState(int i) {
  if (!mqtt.connected()) return;
  const Panel& p = P[i];
  JsonDocument d;
  d["state"] = (p.on && p.state != DARK) ? "ON" : "OFF";
  d["brightness"] = p.bri;
  d["color_mode"] = "rgbw";
  JsonObject c = d["color"].to<JsonObject>();
  c["r"] = p.r; c["g"] = p.g; c["b"] = p.b; c["w"] = p.w;
  char buf[192]; size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish((tBase(i) + "/state").c_str(), (const uint8_t*)buf, n, true);
}

void publishAvail(int i, bool online) {
  if (!mqtt.connected()) return;
  mqtt.publish((tBase(i) + "/avail").c_str(), online ? "online" : "offline", true);
}

void publishDiscovery(int i) {
  if (!mqtt.connected()) return;
  String id = hex(P[i].chip);
  JsonDocument d;
  d["name"] = (i == 0) ? String("Hauptpanel") : "Panel " + id.substring(4);
  d["unique_id"] = "trilumag_" + id;
  d["schema"] = "json";
  d["command_topic"] = tBase(i) + "/set";
  d["state_topic"] = tBase(i) + "/state";
  d["brightness"] = true;
  d["supported_color_modes"].to<JsonArray>().add("rgbw");
  JsonArray av = d["availability"].to<JsonArray>();
  av.add<JsonObject>()["topic"] = "trilumag/bridge/avail";
  av.add<JsonObject>()["topic"] = tBase(i) + "/avail";
  d["availability_mode"] = "all";
  JsonObject dev = d["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add("trilumag_" + hex(P[0].chip));
  dev["name"] = "Trilumag";
  dev["manufacturer"] = "DIY";
  dev["model"] = "Panel v0.1";
  dev["sw_version"] = FW_VERSION;
  char buf[900]; size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish(("homeassistant/light/trilumag_" + id + "/config").c_str(), (const uint8_t*)buf, n, true);
}

void publishAllLight() {
  JsonDocument d;
  d["name"] = "Alle Panels";
  d["unique_id"] = "trilumag_alle_" + hex(P[0].chip);
  d["schema"] = "json";
  d["command_topic"] = "trilumag/alle/set";
  d["brightness"] = true;
  d["supported_color_modes"].to<JsonArray>().add("rgbw");
  d["state_topic"] = "trilumag/alle/state";
  d["effect"] = true;
  JsonArray el = d["effect_list"].to<JsonArray>();
  for (uint8_t k = 0; k < FX_COUNT; k++) el.add(FX[k].name);
  d["availability_topic"] = "trilumag/bridge/avail";
  JsonObject dev = d["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add("trilumag_" + hex(P[0].chip));
  dev["name"] = "Trilumag";
  char buf[900]; size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish(("homeassistant/light/trilumag_alle_" + hex(P[0].chip) + "/config").c_str(), (const uint8_t*)buf, n, true);

  // Tempo der Effekte als Schieberegler in Home Assistant
  JsonDocument t;
  t["name"] = "Effekt-Tempo";
  t["unique_id"] = "trilumag_tempo_" + hex(P[0].chip);
  t["command_topic"] = "trilumag/tempo/set";
  t["state_topic"] = "trilumag/tempo/state";
  t["min"] = 1; t["max"] = 100; t["step"] = 1; t["mode"] = "slider"; t["icon"] = "mdi:speedometer";
  t["availability_topic"] = "trilumag/bridge/avail";
  JsonObject td = t["device"].to<JsonObject>();
  td["identifiers"].to<JsonArray>().add("trilumag_" + hex(P[0].chip));
  td["name"] = "Trilumag";
  n = serializeJson(t, buf, sizeof buf);
  mqtt.publish(("homeassistant/number/trilumag_tempo_" + hex(P[0].chip) + "/config").c_str(), (const uint8_t*)buf, n, true);
}

// Zustand von "Alle Panels" und Effekt-Tempo
void publishFx() {
  if (!mqtt.connected()) return;
  bool any = false;
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached && P[i].on && P[i].state != DARK) any = true;
  JsonDocument d;
  d["state"] = any ? "ON" : "OFF";
  d["effect"] = FX[fx.id].name;
  d["brightness"] = fx.id ? fx.bri : P[0].bri;
  d["color_mode"] = "rgbw";
  JsonObject c = d["color"].to<JsonObject>();
  if (fx.id) { c["r"] = fx.r; c["g"] = fx.g; c["b"] = fx.b; c["w"] = fx.w; }
  else { c["r"] = P[0].r; c["g"] = P[0].g; c["b"] = P[0].b; c["w"] = P[0].w; }
  char buf[200]; size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish("trilumag/alle/state", (const uint8_t*)buf, n, true);
  String sp(fx.speed);
  mqtt.publish("trilumag/tempo/state", sp.c_str(), true);
}

// ---------- Farbe setzen (gemeinsam für App, API, MQTT) ----------
void applyCommand(int i, JsonVariantConst cmd) {
  Panel& p = P[i];
  if (!p.used || !p.attached) return;
  const char* st = cmd["state"] | "";
  if (!strcmp(st, "OFF")) p.on = false;
  if (!strcmp(st, "ON")) p.on = true;
  if (cmd["brightness"].is<int>()) p.bri = constrain(cmd["brightness"].as<int>(), 0, 255);
  JsonVariantConst c = cmd["color"];
  if (!c.isNull()) {
    p.r = c["r"] | p.r; p.g = c["g"] | p.g; p.b = c["b"] | p.b; p.w = c["w"] | p.w;
    p.hasColor = true;
  }
  if (!p.hasColor && p.on) { p.w = 255; p.hasColor = true; }   // nur "Ein" ohne Farbe: weiß
  if (p.state != DARK) p.state = ACTIVE;                         // erste Farbzuordnung beendet das Pulsieren
  colorsDirty = true; colorsDirtyAt = millis();
  if (fx.id && !c.isNull()) fxStart(0);   // eine feste Farbe beendet den Effekt
  sendToPanel(i);
  publishState(i);
}

void applyAll(JsonVariantConst cmd) {
  // Effekt starten, ändern oder (mit "Kein Effekt") beenden
  if (!cmd["effect"].isNull() || (fx.id && cmd["color"].isNull())) {
    const char* st = cmd["state"] | "";
    bool on = strcmp(st, "OFF") != 0;
    if (*st || !cmd["effect"].isNull())
      for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) { P[i].on = on; colorsDirty = true; colorsDirtyAt = millis(); }
    if (fx.id || !cmd["effect"].isNull()) {
      applyFx(cmd);
      if (fx.id) { for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) publishState(i); return; }
    }
  }
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) applyCommand(i, cmd);
  publishFx();
}

// ---------- Topologie ----------
// Prüft von Hauptpanel aus, welche Panels noch verbunden sind. Nicht erreichbare gelten als abgeklipst.
void reconcile() {
  int8_t q[SLOTS]; bool seen[SLOTS] = {false};
  int head = 0, tail = 0;
  q[tail++] = 0; seen[0] = true; P[0].parent = -1; P[0].depth = 0;
  while (head < tail) {
    int i = q[head++];
    for (uint8_t e = 0; e < 3; e++) {
      if (!hasConnector(i, e)) continue;
      Dir d = worldDir(P[i], e); int nx, ny; neighbor(P[i].x, P[i].y, d, nx, ny);
      int j = findAt(nx, ny);
      if (j < 0 || seen[j]) continue;
      int ej = edgeFacing(j, opposite(d));
      if (ej < 0 || !hasConnector(j, ej)) continue;
      seen[j] = true; P[j].parent = i; P[j].depth = P[i].depth + 1; q[tail++] = j;
    }
  }
  for (int i = 1; i < SLOTS; i++) {
    if (P[i].used && P[i].attached && !seen[i]) {
      P[i].attached = false; P[i].state = DARK; P[i].parent = -1; P[i].addr = 0;
      logf("[TOPO] %s getrennt\n", hex(P[i].chip).c_str());
      publishAvail(i, false);
    }
  }
}

// Setzt Panel i an Kante "edge" von "parent"; "own" ist die eigene Kante, die den Kontakt hat
bool placePanel(int i, int parent, uint8_t edge, uint8_t own) {
  if (i <= 0 || i >= SLOTS || !P[i].used || P[i].attached) return false;
  if (parent < 0 || parent >= SLOTS || !P[parent].used || !P[parent].attached) return false;
  if (edge > 2 || own > 2 || !hasConnector(parent, edge)) return false;
  if (countAttached() >= MAX_ATTACHED) return false;
  Dir d = worldDir(P[parent], edge); int nx, ny; neighbor(P[parent].x, P[parent].y, d, nx, ny);
  if (findAt(nx, ny) >= 0) return false;
  const Dir* list = slotEdges(isUp(nx, ny));
  Dir back = opposite(d); uint8_t k = 0;
  for (uint8_t m = 0; m < 3; m++) if (list[m] == back) k = m;
  P[i].x = nx; P[i].y = ny; P[i].rot = (k - own + 3) % 3;
  P[i].attached = true; P[i].since = millis();
  reconcile();
  return true;
}

// Simulation: Panel anklipsen, die eigene Kante hängt davon ab, wie man es hält (zufällig)
bool simAttach(int i, int parent, uint8_t edge) {
  if (!placePanel(i, parent, edge, esp_random() % 3)) return false;
  P[i].state = DARK;
  logf("[SIM] %s an Kante %u von %s angeklipst\n", hex(P[i].chip).c_str(), edge + 1, hex(P[parent].chip).c_str());
  publishDiscovery(i); publishAvail(i, true);
  return true;
}

void simDetach(int i) {
  if (i <= 0 || i >= SLOTS || !P[i].used || !P[i].attached) return;
  P[i].attached = false; P[i].state = DARK;
  publishAvail(i, false);
  logf("[SIM] %s abgeklipst\n", hex(P[i].chip).c_str());
  reconcile();
}

int simNewPanel() {
  for (int i = 1; i < SLOTS; i++) if (!P[i].used) {
    P[i] = Panel(); P[i].used = true;
    do { P[i].chip = esp_random(); } while (P[i].chip == 0 || findChip(P[i].chip) != i);
    return i;
  }
  return -1;
}

// ---------- Einstellungen ----------
void loadConfig() {
  const BoardPreset* def = defaultBoard(CHIP_FAMILY);
  cfg.bus = prefs.getBool("bus", false);
  cfg.board = prefs.getString("board", def->id);
  cfg.pins = def->pins;
  if (prefs.getBytes("pins", &cfg.pins, sizeof cfg.pins) != sizeof cfg.pins || checkPins(cfg.pins)) cfg.pins = def->pins;
  cfg.order = prefs.getString("order", "RGBW");
  cfg.mqttHost = prefs.getString("mqttHost", "");
  cfg.mqttPort = prefs.getUShort("mqttPort", 1883);
  cfg.mqttUser = prefs.getString("mqttUser", "");
  cfg.mqttPass = prefs.getString("mqttPass", "");
}

String configJson() {
  JsonDocument d;
  d["chip"] = CHIP_FAMILY; d["ver"] = FW_VERSION;
  d["mode"] = cfg.bus ? "bus" : "sim";
  d["board"] = cfg.board;
  JsonObject p = d["pins"].to<JsonObject>();
  p["rx"] = cfg.pins.rx; p["tx"] = cfg.pins.tx; p["de"] = cfg.pins.de;
  p["led"] = cfg.pins.led; p["snsR"] = cfg.pins.snsR; p["snsL"] = cfg.pins.snsL;
  JsonArray bs = d["boards"].to<JsonArray>();
  for (size_t i = 0; i < BOARD_COUNT; i++) {
    if (strcmp(BOARDS[i].chip, CHIP_FAMILY)) continue;
    JsonObject b = bs.add<JsonObject>();
    b["id"] = BOARDS[i].id; b["name"] = BOARDS[i].name;
    JsonObject bp = b["pins"].to<JsonObject>();
    bp["rx"] = BOARDS[i].pins.rx; bp["tx"] = BOARDS[i].pins.tx; bp["de"] = BOARDS[i].pins.de;
    bp["led"] = BOARDS[i].pins.led; bp["snsR"] = BOARDS[i].pins.snsR; bp["snsL"] = BOARDS[i].pins.snsL;
  }
  JsonArray vp = d["validPins"].to<JsonArray>();
  for (size_t i = 0; i < VALID_COUNT; i++) vp.add(VALID_PINS[i]);
  d["order"] = cfg.order;
  JsonArray os = d["orders"].to<JsonArray>();
  for (const char* o : ORDERS) os.add(o);
  JsonObject m = d["mqtt"].to<JsonObject>();
  m["host"] = cfg.mqttHost; m["port"] = cfg.mqttPort; m["user"] = cfg.mqttUser; m["hasPass"] = cfg.mqttPass.length() > 0;
  String out; serializeJson(d, out); return out;
}

// ---------- JSON für die App ----------
String stateJson() {
  JsonDocument d;
  d["sim"] = !cfg.bus;
  d["max"] = MAX_ATTACHED;
  d["mqtt"] = mqtt.connected();
  d["mqttSet"] = cfg.mqttHost.length() > 0;
  d["ap"] = apMode;
  d["ver"] = FW_VERSION;
  d["chip"] = CHIP_FAMILY;
  JsonObject f = d["fx"].to<JsonObject>();
  f["id"] = FX[fx.id].id; f["speed"] = fx.speed; f["bri"] = fx.bri;
  f["r"] = fx.r; f["g"] = fx.g; f["b"] = fx.b; f["w"] = fx.w;
  JsonArray fl = d["effects"].to<JsonArray>();
  for (uint8_t k = 0; k < FX_COUNT; k++) { JsonObject e = fl.add<JsonObject>(); e["id"] = FX[k].id; e["name"] = FX[k].name; e["color"] = FX[k].color; }
  JsonArray pa = d["panels"].to<JsonArray>();
  JsonArray lo = d["loose"].to<JsonArray>();
  for (int i = 0; i < SLOTS; i++) {
    const Panel& p = P[i];
    if (!p.used) continue;
    if (!p.attached) { lo.add(hex(p.chip)); continue; }
    JsonObject o = pa.add<JsonObject>();
    o["id"] = hex(p.chip); o["main"] = (i == 0);
    o["x"] = p.x; o["y"] = p.y; o["up"] = isUp(p.x, p.y); o["rot"] = p.rot;
    o["parent"] = p.parent >= 0 ? hex(P[p.parent].chip) : String();
    o["state"] = p.state; o["on"] = p.on;
    o["r"] = p.r; o["g"] = p.g; o["b"] = p.b; o["w"] = p.w; o["bri"] = p.bri;
  }
  JsonArray gh = d["ghosts"].to<JsonArray>();
  if (!cfg.bus && countAttached() < MAX_ATTACHED) {
    for (int i = 0; i < SLOTS; i++) {
      if (!P[i].used || !P[i].attached) continue;
      for (uint8_t e = 0; e < 3; e++) {
        if (!hasConnector(i, e)) continue;
        int nx, ny; neighbor(P[i].x, P[i].y, worldDir(P[i], e), nx, ny);
        if (findAt(nx, ny) >= 0) continue;
        bool dup = false;
        for (JsonObject g : gh) if (g["x"] == nx && g["y"] == ny) { dup = true; break; }
        if (dup) continue;
        JsonObject g = gh.add<JsonObject>();
        g["x"] = nx; g["y"] = ny; g["up"] = isUp(nx, ny); g["parent"] = hex(P[i].chip); g["edge"] = e;
      }
    }
  }
  String out; serializeJson(d, out); return out;
}

// ---------- Webserver ----------
bool readBody(JsonDocument& d) {
  if (!server.hasArg("plain")) return false;
  return !deserializeJson(d, server.arg("plain"));
}
void replyState() { server.send(200, "application/json", stateJson()); }
void replyError(const char* m) { server.send(400, "application/json", String("{\"error\":\"") + m + "\"}"); }

void setupWeb() {
  server.on("/", HTTP_GET, [] { server.send(200, "text/html; charset=utf-8", INDEX_HTML); });
  server.on("/api/state", HTTP_GET, replyState);
  server.on("/api/set", HTTP_POST, [] {
    JsonDocument d; if (!readBody(d)) return replyError("JSON fehlt");
    const char* id = d["id"] | "";
    if (!strcmp(id, "alle")) applyAll(d.as<JsonVariantConst>());
    else { int i = findChip(parseHex(id)); if (i < 0) return replyError("Panel unbekannt"); applyCommand(i, d.as<JsonVariantConst>()); }
    replyState();
  });
  server.on("/api/effect", HTTP_POST, [] {
    JsonDocument d; if (!readBody(d)) return replyError("JSON fehlt");
    if (d["effect"].is<const char*>() && fxFind(d["effect"].as<const char*>()) < 0) return replyError("Effekt unbekannt");
    if (d["effect"].is<const char*>() && fxFind(d["effect"].as<const char*>()) > 0)
      for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached && !P[i].on) { P[i].on = true; colorsDirty = true; colorsDirtyAt = millis(); }
    applyFx(d.as<JsonVariantConst>());
    replyState();
  });
  // aktuelles Effektbild, damit die App mitleuchtet
  server.on("/api/live", HTTP_GET, [] {
    String out = "{\"fx\":\""; out += FX[fx.id].id; out += "\",\"c\":{";
    bool first = true;
    for (int i = 0; i < SLOTS; i++) {
      if (!P[i].used || !P[i].attached) continue;
      char b[32]; snprintf(b, sizeof b, "%s\"%08X\":\"%02X%02X%02X%02X\"", first ? "" : ",", (unsigned)P[i].chip, OUT[i][0], OUT[i][1], OUT[i][2], OUT[i][3]);
      out += b; first = false;
    }
    out += "},\"j\":[";
    first = true;
    for (int i = 0; i < SLOTS; i++) {
      if (!P[i].used || !P[i].attached || !P[i].joinAt || (int32_t)(millis() - P[i].joinAt) >= 0) continue;
      char b[16]; snprintf(b, sizeof b, "%s\"%08X\"", first ? "" : ",", (unsigned)P[i].chip);
      out += b; first = false;
    }
    out += "]}";
    server.send(200, "application/json", out);
  });
  server.on("/api/test", HTTP_POST, [] {
    JsonDocument d; if (!readBody(d)) return replyError("JSON fehlt");
    int ch = d["ch"] | -1;
    uint8_t c[4] = {0, 0, 0, 0};
    if (ch >= 0 && ch < 4) c[ch] = 70;
    if (cfg.bus) bus::send(bus::ALL, bus::C_COLOR, c, 4);
    if (cfg.pins.led >= 0) { for (int k = 0; k < 3; k++) strip.setPixelColor(k, strip.Color(c[0], c[1], c[2], c[3])); strip.show(); }
    if (ch < 0) for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) sendToPanel(i);   // Test beenden
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/sim/new", HTTP_POST, [] {
    if (cfg.bus) return replyError("nur in der Simulation");
    if (simNewPanel() < 0) return replyError("Ablage voll");
    replyState();
  });
  server.on("/api/sim/attach", HTTP_POST, [] {
    if (cfg.bus) return replyError("nur in der Simulation");
    JsonDocument d; if (!readBody(d)) return replyError("JSON fehlt");
    int i = findChip(parseHex(d["id"] | "0")), par = findChip(parseHex(d["parent"] | "0"));
    if (!simAttach(i, par, d["edge"] | 9)) return replyError("Anklipsen nicht möglich");
    replyState();
  });
  server.on("/api/sim/detach", HTTP_POST, [] {
    if (cfg.bus) return replyError("nur in der Simulation");
    JsonDocument d; if (!readBody(d)) return replyError("JSON fehlt");
    simDetach(findChip(parseHex(d["id"] | "0")));
    replyState();
  });
  server.on("/api/config", HTTP_GET, [] { server.send(200, "application/json", configJson()); });
  server.on("/api/config", HTTP_POST, [] {
    JsonDocument d; if (!readBody(d)) return replyError("JSON fehlt");
    PinSet ps = cfg.pins;
    JsonObject p = d["pins"];
    if (!p.isNull()) {
      ps.rx = p["rx"] | ps.rx; ps.tx = p["tx"] | ps.tx; ps.de = p["de"] | ps.de;
      ps.led = p["led"] | ps.led; ps.snsR = p["snsR"] | ps.snsR; ps.snsL = p["snsL"] | ps.snsL;
    }
    if (const char* err = checkPins(ps)) return replyError(err);
    String order = d["order"] | cfg.order.c_str();
    bool okOrder = false; for (const char* o : ORDERS) if (order == o) okOrder = true;
    if (!okOrder) return replyError("Unbekannte Farbreihenfolge");
    prefs.putBytes("pins", &ps, sizeof ps);
    prefs.putString("board", (const char*)(d["board"] | cfg.board.c_str()));
    prefs.putBool("bus", !strcmp(d["mode"] | (cfg.bus ? "bus" : "sim"), "bus"));
    prefs.putString("order", order);
    JsonObject m = d["mqtt"];
    if (!m.isNull()) {
      prefs.putString("mqttHost", (const char*)(m["host"] | ""));
      prefs.putUShort("mqttPort", m["port"] | 1883);
      prefs.putString("mqttUser", (const char*)(m["user"] | ""));
      if (m["pass"].is<const char*>()) prefs.putString("mqttPass", (const char*)m["pass"]);
    }
    saveColors(); fxSave();
    server.send(200, "application/json", "{\"ok\":true,\"restart\":true}");
    logf("Einstellungen gespeichert, starte neu\n");
    delay(600);
    ESP.restart();
  });
  server.on("/api/wifi/scan", HTTP_GET, [] {
    int n = WiFi.scanNetworks();
    JsonDocument d; JsonArray a = d["networks"].to<JsonArray>();
    for (int k = 0; k < n && k < 20; k++) {
      String ss = WiFi.SSID(k);
      if (!ss.length()) continue;
      bool dup = false; for (JsonVariant v : a) if (v.as<String>() == ss) dup = true;
      if (!dup) a.add(ss);
    }
    WiFi.scanDelete();
    String out; serializeJson(d, out); server.send(200, "application/json", out);
  });
  server.on("/api/wifi", HTTP_POST, [] {
    JsonDocument d; if (!readBody(d)) return replyError("JSON fehlt");
    String ss = d["ssid"] | "", pw = d["pass"] | "";
    if (!ss.length()) return replyError("WLAN-Name fehlt");
    saveWifi(ss, pw);
    server.send(200, "application/json", "{\"ok\":true}");
    delay(800);
    ESP.restart();
  });
  server.onNotFound([] { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();
}

// ---------- MQTT-Empfang ----------
void onMqtt(char* topic, byte* payload, unsigned int len) {
  if (!strcmp(topic, "trilumag/tempo/set")) {
    char b[8]; unsigned n = len < 7 ? len : 7; memcpy(b, payload, n); b[n] = 0;
    fx.speed = constrain(atoi(b), 1, 100); fxDirty = true; fxDirtyAt = millis(); publishFx();
    return;
  }
  JsonDocument d;
  if (deserializeJson(d, payload, len)) return;
  String t(topic);                       // trilumag/<ID>/set
  String id = t.substring(9, t.lastIndexOf('/'));
  if (id == "alle") applyAll(d.as<JsonVariantConst>());
  else { int i = findChip(parseHex(id.c_str())); if (i >= 0) applyCommand(i, d.as<JsonVariantConst>()); }
}

void mqttLoop() {
  if (!cfg.mqttHost.length() || !wlanOk) return;
  if (mqtt.connected()) { mqtt.loop(); return; }
  if (millis() - lastMqttTry < 5000) return;
  lastMqttTry = millis();
  String cid = "trilumag-" + hex(P[0].chip);
  const char* u = cfg.mqttUser.length() ? cfg.mqttUser.c_str() : nullptr;
  const char* pw = cfg.mqttPass.length() ? cfg.mqttPass.c_str() : nullptr;
  if (mqtt.connect(cid.c_str(), u, pw, "trilumag/bridge/avail", 0, true, "offline")) {
    logf("[MQTT] verbunden\n");
    mqtt.publish("trilumag/bridge/avail", "online", true);
    mqtt.subscribe("trilumag/+/set");
    publishAllLight();
    publishFx();
    for (int i = 0; i < SLOTS; i++) if (P[i].used) {
      publishDiscovery(i); publishAvail(i, P[i].attached);
      if (P[i].attached) publishState(i);
    }
  } else {
    logf("[MQTT] keine Verbindung zu %s (Fehler %d), neuer Versuch in 5 s\n", cfg.mqttHost.c_str(), mqtt.state());
  }
}

// ---------- WLAN ----------
void saveWifi(const String& ss, const String& pw) {
  prefs.putString("ssid", ss);
  prefs.putString("pass", pw);
  logf("[WLAN] Zugangsdaten für '%s' gespeichert\n", ss.c_str());
}

bool connectWifi(const String& ss, const String& pw, uint32_t timeout) {
  WiFi.mode(apMode ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);            // ohne Funk-Sparmodus antwortet die App sofort
  WiFi.begin(ss.c_str(), pw.c_str());
  logf("[WLAN] verbinde mit '%s'", ss.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeout) {
    delay(250);
    if (apMode) server.handleClient();
    logf(".");
  }
  logf("\n");
  wlanOk = WiFi.status() == WL_CONNECTED;
  if (wlanOk && apMode) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apMode = false;
    MDNS.end(); MDNS.begin(HOSTNAME); MDNS.addService("http", "tcp", 80);
  }
  if (!wlanOk) logf("[WLAN] keine Verbindung\n");
  return wlanOk;
}

void startSetupAp() {
  apMode = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SETUP_SSID, SETUP_PASS);
  logf("Kein WLAN eingerichtet. Setup-Netz '%s' (Passwort %s), dann http://%s öffnen\n",
       SETUP_SSID, SETUP_PASS, WiFi.softAPIP().toString().c_str());
}

// ---------- Improv (WLAN aus dem Webinstaller) ----------
void improvSendUrl(Stream& s, uint8_t cmd) {
  String url = "http://" + WiFi.localIP().toString();
  const char* a[1] = {url.c_str()};
  improv::sendResult(s, cmd, a, 1);
}

void improvRpc(Stream& s, const improv::Parser& p) {
  switch (p.command()) {
    case improv::GET_CURRENT_STATE:
      improv::sendState(s, wlanOk ? improv::PROVISIONED : improv::AUTHORIZED);
      if (wlanOk) improvSendUrl(s, improv::GET_CURRENT_STATE);
      break;
    case improv::GET_DEVICE_INFO: {
      const char* a[4] = {FW_NAME, FW_VERSION, CHIP_FAMILY, "Trilumag Hauptpanel"};
      improv::sendResult(s, improv::GET_DEVICE_INFO, a, 4);
      break;
    }
    case improv::GET_WIFI_NETWORKS: {
      int n = WiFi.scanNetworks();
      for (int k = 0; k < n && k < 20; k++) {
        String ss = WiFi.SSID(k), rssi = String(WiFi.RSSI(k));
        if (!ss.length()) continue;
        const char* a[3] = {ss.c_str(), rssi.c_str(), WiFi.encryptionType(k) == WIFI_AUTH_OPEN ? "NO" : "YES"};
        improv::sendResult(s, improv::GET_WIFI_NETWORKS, a, 3);
      }
      improv::sendResult(s, improv::GET_WIFI_NETWORKS, nullptr, 0);
      WiFi.scanDelete();
      break;
    }
    case improv::WIFI_SETTINGS: {
      const uint8_t* d = p.payload();
      uint8_t sl = d[0];
      if (1 + sl + 1 > p.payloadLen()) { improv::sendError(s, improv::INVALID_RPC); break; }
      uint8_t pl = d[1 + sl];
      if (2 + sl + pl > p.payloadLen()) { improv::sendError(s, improv::INVALID_RPC); break; }
      char sb[65], pb[65];
      uint8_t sn = sl > 64 ? 64 : sl, pn = pl > 64 ? 64 : pl;
      memcpy(sb, d + 1, sn); sb[sn] = 0;
      memcpy(pb, d + 2 + sl, pn); pb[pn] = 0;
      String ss(sb), pw(pb);
      improv::sendState(s, improv::PROVISIONING);
      if (connectWifi(ss, pw, 20000)) {
        saveWifi(ss, pw);
        improv::sendState(s, improv::PROVISIONED);
        improvSendUrl(s, improv::WIFI_SETTINGS);
        logf("Verbunden. App: http://%s  oder  http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
      } else {
        improv::sendError(s, improv::UNABLE_TO_CONNECT);
        improv::sendState(s, improv::AUTHORIZED);
        if (!apMode) startSetupAp();
      }
      break;
    }
    default:
      improv::sendError(s, improv::UNKNOWN_RPC);
  }
}

void improvLoop() {
  while (Serial.available()) if (improvMain.feed((uint8_t)Serial.read())) improvRpc(Serial, improvMain);
#if HAS_UART_CONSOLE
  while (Serial0.available()) if (improvUart.feed((uint8_t)Serial0.read())) improvRpc(Serial0, improvUart);
#endif
}

// ---------- Start ----------
void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
#if HAS_UART_CONSOLE
  Serial0.begin(115200);
#endif
  delay(300);
  logf("\n%s %s auf %s\n", FW_NAME, FW_VERSION, CHIP_FAMILY);

  prefs.begin("trilumag", false);
  loadConfig();
  logf("Betriebsart: %s, Board: %s\n", cfg.bus ? "Bus" : "Simulation", cfg.board.c_str());

  P[0].used = true; P[0].attached = true;
  P[0].chip = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF);
  P[0].x = 0; P[0].y = 0; P[0].rot = 0;
  P[0].state = ACTIVE; P[0].hasColor = true; P[0].w = 200;
  loadColor(0);
  fxLoad();
  if (fx.id) logf("[FX] Effekt %s läuft weiter\n", FX[fx.id].name);

  strip.updateType(neoType(cfg.order) + NEO_KHZ800);
  strip.setPin(cfg.pins.led);
  strip.begin();
  showMain();

  String ss = prefs.getString("ssid", WLAN_SSID), pw = prefs.getString("pass", WLAN_PASS);
  WiFi.setHostname(HOSTNAME);
  if (ss.length() && connectWifi(ss, pw, 15000)) {
    logf("Verbunden. App: http://%s  oder  http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
  } else {
    startSetupAp();
  }
  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);

  mqtt.setServer(cfg.mqttHost.c_str(), cfg.mqttPort);
  mqtt.setBufferSize(1024);
  mqtt.setCallback(onMqtt);

  if (cfg.bus) bus::begin();
  else for (int k = 0; k < 3; k++) simNewPanel();   // drei Panels liegen in der Ablage bereit
  setupWeb();
}

void loop() {
  server.handleClient();
  improvLoop();
  mqttLoop();
  if (cfg.bus) busLoop();
  else {
    // simulierte Erkennung: erst dunkel, dann pulsieren oder gespeicherte Farbe
    for (int i = 1; i < SLOTS; i++) {
      Panel& p = P[i];
      if (p.used && p.attached && p.state == DARK && millis() - p.since > DETECT_MS) {
        p.state = p.hasColor ? ACTIVE : PULSE;
        p.joinAt = (millis() + FX_JOIN_MS) | 1;
        logf("[SIM] %s eingegliedert: %s\n", hex(p.chip).c_str(), p.hasColor ? "alte Farbe" : "pulsiert blau");
        sendToPanel(i); publishState(i);
      }
    }
  }
  fxLoop();
  if (colorsDirty && millis() - colorsDirtyAt > 5000) saveColors();
  if (fxDirty && millis() - fxDirtyAt > 5000) fxSave();
}
