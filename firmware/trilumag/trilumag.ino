/*
  Trilumag – Firmware für das Hauptpanel, Version 0.4
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
   - Effekte für die ganze Wand (Regenbogen, Atmen, Feuer …) mit Paletten und Intensität
   - Gesamthelligkeit und Ein/Aus für die ganze Wand, Presets (gespeicherte Szenen)
   - Web-App im Stil von WLED
   - Online-Updates von GitHub, auf Wunsch automatisch, auch Downgrades; Datei-Upload
   - Update-Absicherung: eine neue Version muss sich nach dem Start bewähren, sonst zurück zur alten
   - weiche Übergänge, Stromlimit (geschätzt oder mit Stromsensor INA226), Sichern und Wiederherstellen
   - Farben bleiben pro Panel (Chip-ID) gespeichert, auch über einen Neustart
   - HTTP-API und Home Assistant über MQTT mit Auto-Discovery, Broker in der App einstellbar

  Selbst kompilieren mit der Arduino IDE:
   - Boardverwalter: "esp32" von Espressif
   - Bibliotheken: ArduinoJson (ab 7), PubSubClient, Adafruit NeoPixel
   - ESP32-S3/C3/C6: "USB CDC On Boot" auf "Enabled" stellen
   - Partition Scheme: "Minimal SPIFFS (1.9MB APP with OTA)", sonst passen Online-Updates nicht
*/

#include <sys/time.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <stdarg.h>
#include <math.h>
#include "webapp_gz.h"                  // die App (Quelle: webapp.h), gepackt
#include "improv.h"
#include "pins.h"
#include "ws.h"
#include "ota.h"
#include "zigbee.h"     // Philips Hue über Zigbee, nur ESP32-C6
#include "panel_fw.h"
#include "energy_types.h" // eigene Typen in einer Datei, sonst stolpern die automatischen Arduino-Prototypen   // aktuelle Panel-Firmware für Updates über den Bus (erzeugt beim Build)
#include <Wire.h>
#include <WiFiUdp.h>
#include <time.h>
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include <HTTPClient.h>

// ================= Voreinstellungen =================
// Alles hier lässt sich später in der App ändern. Diese Werte gelten nur, solange nichts gespeichert ist.
const char* WLAN_SSID = "";
const char* WLAN_PASS = "";
const uint8_t  MAX_ATTACHED = 30;     // höchstens so viele Panels an der Wand (inkl. Hauptpanel)
const uint32_t DETECT_MS    = 1200;   // simulierte Erkennungszeit nach dem Anklipsen
const uint32_t FX_JOIN_MS   = 5 * 1600; // bei laufendem Effekt pulsieren neue Panels erst 5-mal blau
// ====================================================

const char* FW_NAME    = "Trilumag";
const char* FW_VERSION = "0.7.0";   // die dritte Stelle setzt der Build
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
  // Diagnose
  uint32_t pings = 0, missed = 0;   // Abfragen und verpasste Antworten seit dem Anklipsen
  uint16_t rtt = 0, rttMax = 0;     // Antwortzeit in µs (zuletzt, höchste)
  uint8_t fw = 0;                   // Firmware-Version des Panels (aus PING)
  uint16_t attaches = 0;            // wie oft seit dem Start erkannt
  uint32_t clips = 0;               // wie oft insgesamt angeklipst (dauerhaft gespeichert)
  uint8_t caps = 0;                 // vom Panel gemeldet: 1 = Touch-Sensor, 2 = Bootloader
  uint8_t touchSeq = 0; bool touchKnown = false;
  uint8_t upd = 0, updPct = 0, updFails = 0;   // Panel-Update: 0 nichts, 1 wartet, 2 läuft, 3 fehlgeschlagen
  uint32_t identUntil = 0;          // Panel finden: blinkt bis hierher weiß
  uint32_t litSec = 0, litSaved = 0;   // Betriebsstunden: so lange hat das Panel insgesamt geleuchtet (s)
  bool edges = false;               // Kanten einzeln: Effekte bekommen drei Farben pro Panel
  uint8_t state = DARK;
  uint32_t since = 0;
  uint32_t joinAt = 0;    // bei laufendem Effekt: bis hierher noch blau pulsieren (0 = nicht)
  bool hasColor = false;
  bool on = true;
  uint8_t r = 0, g = 0, b = 0, w = 0, bri = 255;
};

struct Config {
  bool bus = false;
  String board;
  PinSet pins;
  String order = "RGBW";
  bool autoUpdate = false; // Häkchen "Automatisch aktualisieren"
  uint16_t transMs = 700;   // Dauer weicher Übergänge
  uint16_t pwrMax = 0;      // Stromlimit in mA (0 = aus)
  uint8_t pwrCh = 12;       // mA pro Farbkanal und LED-Segment bei voller Helligkeit (WS2814 24V)
  int8_t i2cSda = -1, i2cScl = -1;   // Stromsensor INA226 (optional)
  uint16_t shuntUo = 50;    // Shunt in 0,1 mΩ (50 = 5 mΩ)
  bool mqttOn = false;     // Häkchen "MQTT aktiv"; Zugangsdaten bleiben auch ausgeschaltet gespeichert
  bool touchOn = true;      // Antippen der Panels (Beschleunigungssensor)
  uint8_t touchSens = 5;    // Empfindlichkeit 1 bis 10
  uint8_t tapA1 = 1, tapA2 = 2;   // Aktion für einmal und doppelt antippen (TapAction)
  bool panelAuto = true;    // Panels mit älterer Firmware automatisch aktualisieren
  bool zbOn = false;        // Zigbee für die Hue Bridge (nur ESP32-C6)
  String name = "Trilumag"; // Name der Wand: App, Home Assistant, WLED-Programme
  uint8_t bootMode = 0;     // nach Stromausfall: 0 wie vorher, 1 aus, 2 an, 3 Preset
  int8_t bootPreset = -1;
  bool syncOn = false;      // mit anderen Wänden im selben WLAN im Gleichtakt
  bool guard = true;        // Wächter: WLAN weg oder Hauptschleife hängt → neu verbinden bzw. neu starten
  uint8_t onAnim = 2;       // Einschalt-Animation: 0 aus, 1 langsam, 2 mittel, 3 schnell
  bool touchWave = true;    // Welle über die Wand beim Antippen
  bool daylight = false;    // Tageslicht-Kurve: abends wärmer und dunkler
  bool faultBlink = true;   // Störung (WLAN weg, Panel antwortet nicht): Hauptpanel blinkt kurz
  uint16_t viewRot = 0;     // so hängt die Wand: Drehung in Grad (Vielfache von 30), wirkt in App und Effekten
  bool viewMir = false;
  uint8_t syncGroup = 1;    // nur Wände derselben Gruppe (1 bis 9) laufen zusammen
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser, mqttPass;
};

// Effekt für die ganze Wand (Liste FX[] weiter unten)
struct FxCfg { uint8_t id = 0, speed = 50, bri = 180, r = 255, g = 120, b = 30, w = 0, pal = 0, inten = 128; };

Panel P[SLOTS];
Config cfg;
FxCfg fx;
uint8_t master = 255;          // Gesamthelligkeit der Wand
bool masterOn = true;          // Ein/Aus der ganzen Wand
int8_t curPreset = -1;         // zuletzt geladenes Preset, -1 = keins oder verändert
uint8_t masterK() { return masterOn ? master : 0; }
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
// WLAN-Verbindung im Hintergrund (siehe wifiLoop)
enum WifiPhase : uint8_t { W_IDLE, W_CONNECTING };
WifiPhase wPhase = W_IDLE;
uint32_t wDeadline = 0;
String wSsid, wPass;
bool wImprov = false;          // Verbindungsversuch kam aus dem Webinstaller
bool wSaveLate = false;        // verbindet es sich doch noch, die Zugangsdaten trotzdem speichern
Stream* wImprovStream = nullptr;
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

// ---------- Diagnose: Zähler und Ereignisprotokoll ----------
struct DiagEvent { uint32_t t; char m[80]; };
const uint8_t DIAG_N = 40;
DiagEvent diagLogBuf[DIAG_N];
uint8_t diagHead = 0, diagCount = 0;
uint32_t busFramesSent = 0, busCrcErr = 0, busTimeouts = 0, busDiscovers = 0;
// schreibt ins serielle Log und ins Ereignisprotokoll der App
void diag(const char* fmt, ...) {
  char b[80];
  va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
  logf("%s\n", b);
  DiagEvent& e = diagLogBuf[diagHead];
  e.t = millis(); strncpy(e.m, b, sizeof e.m - 1); e.m[sizeof e.m - 1] = 0;
  diagHead = (diagHead + 1) % DIAG_N; if (diagCount < DIAG_N) diagCount++;
}

// Neustart-Grund: überlebt einen Software-Neustart im RTC-Speicher
RTC_NOINIT_ATTR uint32_t rstMagic, rstCode;
enum RstCode : uint8_t { R_NONE, R_UPDATE, R_APP, R_WLAN, R_HANG };
void restartWith(uint8_t code) { rstMagic = 0x7121A600; rstCode = code; ESP.restart(); }
String bootReason;                         // lesbar für Diagnose und Info
uint32_t loopBeat = 0;

void saveWifi(const String& ss, const String& pw);
void wifiStart(const String& ss, const String& pw, uint32_t timeout, bool fromImprov);
void startSetupAp();
void publishDiscovery(int i);
void publishAvail(int i, bool online);
void publishState(int i);
void publishFx();
void publishExtras();

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
// Mitte von Kante e des Panels i (Seitenlänge 1) und ob sie zum Elternpanel zeigt: dd -0,33 (Eingang) oder +0,17
void edgePos(int i, int e, float& x, float& y, float& dd) {
  const Panel& p = P[i]; float dx = 0, dy = 0; dd = 0;
  if (e >= 0) {
    Dir w = worldDir(p, e);
    if (w == BOTTOM) dy = 0.289f; else if (w == TOP) dy = -0.289f;
    else { dx = w == RIGHT ? 0.25f : -0.25f; dy = isUp(p.x, p.y) ? -0.144f : 0.144f; }
    int nx, ny; neighbor(p.x, p.y, w, nx, ny);
    dd = (p.parent >= 0 && P[p.parent].x == nx && P[p.parent].y == ny) ? -0.33f : 0.17f;
  }
  x = p.x * 0.5f + dx; y = p.y * 0.866f + dy;
}
int countAttached() { int n = 0; for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) n++; return n; }
String hex(uint32_t c) { char b[9]; snprintf(b, sizeof b, "%08X", c); return String(b); }
uint32_t parseHex(const char* s) { return (uint32_t)strtoul(s, nullptr, 16); }

// ---------- Farben dauerhaft merken ----------
void loadColor(int i) {
  P[i].edges = prefs.getBool(("e" + hex(P[i].chip)).c_str(), false);
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
enum { C_PING = 0x01, C_COLOR = 0x02, C_EDGES = 0x03, C_FRAME = 0x04, C_PULSE = 0x05, C_ORDER = 0x06, C_FRAME3 = 0x07,
       C_TOUCH = 0x08,
       C_BEACON = 0x10, C_DISCOVER = 0x11, C_PROBE = 0x12, C_ASSIGN = 0x14, C_RESET = 0x15,
       C_BOOT = 0x20, BL_INFO = 0x21, BL_WRITE = 0x22, BL_DONE = 0x23, BL_SCAN = 0x24, BL_RUN = 0x25 };
enum { CAP_TOUCH = 1, CAP_BOOT = 2 };

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
        if (c != f[pos - 1]) busCrcErr++;          // gestörte Übertragung
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
    uint32_t k = (uint32_t)p.bri * masterK();
    c[0] = p.r * k / 65025; c[1] = p.g * k / 65025; c[2] = p.b * k / 65025; c[3] = p.w * k / 65025;
  }
  send(p.addr, C_COLOR, c, 4);
}

void sendOrder(uint8_t addr) {
  uint8_t o[4]; orderBytes(cfg.order, o);
  send(addr, C_ORDER, o, 4);
}

// Empfindlichkeit 1..10 als Schwelle des Sensors (16 mg je Schritt), 0 = aus
uint8_t touchThreshold() { return cfg.touchOn ? 66 - 6 * constrain((int)cfg.touchSens, 1, 10) : 0; }
void sendTouch(uint8_t addr) { uint8_t t = touchThreshold(); send(addr, C_TOUCH, &t, 1); }

void begin() {
  port.begin(250000, SERIAL_8N1, cfg.pins.rx, cfg.pins.tx);
  port.setPins(cfg.pins.rx, cfg.pins.tx, -1, cfg.pins.de);
  port.setMode(UART_MODE_RS485_HALF_DUPLEX);
  delay(50);
  send(ALL, C_RESET);
  delay(5);
  sendOrder(ALL);
  sendTouch(ALL);
  beacons(true);
  logf("[BUS] gestartet: RX %d, TX %d, DE %d, SNS rechts %d, SNS links %d\n",
       cfg.pins.rx, cfg.pins.tx, cfg.pins.de, cfg.pins.snsR, cfg.pins.snsL);
}
}  // namespace bus

void reconcile();
void simChanged();
bool timeOk();
bool safeBoot = false;         // nach wiederholten Abstürzen beim Start: Netz-Nachahmen nicht starten
uint32_t faultPanelAt = 0;     // wann zuletzt ein Panel nicht mehr geantwortet hat
// Während Updates ruhen die Abfragen an Hue und WLED: zwei verschlüsselte Verbindungen gleichzeitig brauchen zu viel Speicher
volatile bool netQuiet = false, hueInReq = false, wledInReq = false;
volatile bool otaBusy = false;   // Installation läuft: alle Verbindungen nach außen getrennt (MQTT, Hue, WLED, Wetter, Suche)
void netQuietOn() { netQuiet = true; for (int i = 0; i < 600 && (hueInReq || wledInReq); i++) delay(10); }
namespace wx { extern String place, err; extern volatile bool busy; void json(JsonObject o); }
namespace mirror { extern String ip; void json(JsonObject o); }
namespace hueb { void json(JsonObject o); void closeConn(); }
extern bool outForce, testMode;
void transition();
bool placePanel(int i, int parent, uint8_t edge, uint8_t own);

// ---------- Anklips-Zähler (dauerhaft im NVS, je Chip-ID) ----------
String clipKey(uint32_t chip) { return "n" + hex(chip); }
void clipsLoad(int i) { P[i].clips = prefs.getUInt(clipKey(P[i].chip).c_str(), 0); }
void clipCount(int i) { P[i].clips++; prefs.putUInt(clipKey(P[i].chip).c_str(), P[i].clips); }
// Beim Einschalten der ganzen Wand melden sich alle Panels als frisch versorgt. Das ist kein Anklipsen:
// Bis 5 s nach dem letzten gefundenen Panel (mindestens 15 s nach dem Start) wird nicht gezählt.
uint32_t busQuietUntil = 15000;

namespace pupd { bool busy(uint32_t chip); void loop(); }
void touchEvent(int i, uint8_t kind);
void wsKick();
void litLoad(int i);
extern uint32_t sleepEnd; uint32_t sleepLeft();
namespace wallsync { void json(JsonObject o); extern uint32_t lastSig; }

// Neues Panel gefunden: Nachbarn orten, Adresse vergeben, Farbe oder Pulsieren
void busHandleNew(uint32_t chip, uint8_t mask) {
  if (pupd::busy(chip)) return;                // startet gerade nach einem Update neu, bekommt seine alte Adresse
  int i = findChip(chip);
  if (i >= 0 && P[i].attached) { P[i].attached = false; P[i].addr = 0; }   // Panel wurde neu gestartet
  if (i < 0) {
    for (int k = 1; k < SLOTS; k++) if (!P[k].used) { i = k; break; }
    if (i < 0) { logf("[BUS] kein Platz mehr für %s\n", hex(chip).c_str()); return; }
    P[i] = Panel(); P[i].used = true; P[i].chip = chip;
    loadColor(i); clipsLoad(i); litLoad(i);
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

  if (parent < 0) { diag("Panel %s meldet Kontakt, Nachbar nicht gefunden (SNS prüfen)", hex(chip).substring(4).c_str()); return; }

  uint8_t addr = 0;
  for (uint8_t a = 1; a <= 0x3E; a++) if (findAddr(a) < 0) { addr = a; break; }
  if (!addr || !placePanel(i, parent, parentEdge, ownEdge)) {
    diag("Panel %s lässt sich nicht einordnen", hex(chip).substring(4).c_str()); return;
  }
  bus::sendById(chip, bus::C_ASSIGN, addr);
  bus::Reply r;
  if (!bus::receive(r, 5) || r.addr != addr) {
    diag("Panel %s hat die Adresse nicht bestätigt", hex(chip).substring(4).c_str());
    P[i].attached = false; reconcile(); return;
  }
  bool fresh = r.len >= 1 ? r.data[0] : true;   // Panel-Firmware ab 3: 1 = gerade erst Strom bekommen
  P[i].addr = addr; P[i].miss = 0; P[i].touchKnown = false;
  P[i].joinAt = (millis() + FX_JOIN_MS) | 1;
  bus::sendOrder(addr);
  bus::sendTouch(addr);
  if ((int32_t)(millis() - busQuietUntil) < 0) busQuietUntil = millis() + 5000;
  else if (fresh) clipCount(i);
  P[i].state = P[i].hasColor ? ACTIVE : PULSE;
  P[i].since = millis();
  transition();                       // weich einblenden, das nächste Bild enthält das neue Panel
  diag("Panel %s an Kante %d von %s (eigene Kante %d), Adresse %u", hex(chip).substring(4).c_str(), parentEdge + 1,
       parent == 0 ? "Haupt" : hex(P[parent].chip).substring(4).c_str(), ownEdge + 1, addr);
  P[i].pings = P[i].missed = 0; P[i].rttMax = 0; P[i].attaches++;
  publishDiscovery(i); publishAvail(i, true); publishState(i);
}

void busLoop() {
  uint32_t now = millis();
  if (now - lastPoll > 150) {
    lastPoll = now;
    bool lost = false;
    for (int i = 1; i < SLOTS; i++) {
      Panel& p = P[i];
      if (!p.used || !p.attached || !p.addr || p.upd == 2) continue;   // während des Updates im Bootloader
      uint32_t t0 = micros();
      bus::send(p.addr, bus::C_PING);
      bus::Reply r;
      p.pings++;
      if (bus::receive(r, 4) && r.addr == p.addr) {
        p.miss = 0;
        uint32_t us = micros() - t0; p.rtt = us > 65535 ? 65535 : us; if (p.rtt > p.rttMax) p.rttMax = p.rtt;
        if (r.len >= 3) p.fw = r.data[2];
        if (r.len >= 5) {                       // ab Panel-Firmware 3: Antippen und Fähigkeiten
          p.caps = r.data[4];
          if (p.touchKnown && r.data[3] != p.touchSeq && (r.data[3] & 3)) touchEvent(i, r.data[3] & 3);
          p.touchSeq = r.data[3]; p.touchKnown = true;
        }
        if (r.len >= 1 && r.data[0] == DARK && p.state != DARK) outForce = true;   // Panel hat Zustand verloren: alles neu schicken   // Panel hat Zustand verloren
      } else if (p.missed++, busTimeouts++, ++p.miss >= 3) {
        diag("Panel %s antwortet nicht mehr, gilt als abgeklipst", hex(p.chip).substring(4).c_str());
        faultPanelAt = millis() | 1;
        bus::send(p.addr, bus::C_RESET);
        p.attached = false; p.addr = 0; p.state = DARK; publishAvail(i, false); lost = true;
      }
    }
    if (lost) reconcile();
  }
  if (now - lastDiscover > 400 && countAttached() < MAX_ATTACHED) {
    lastDiscover = now;
    const uint8_t slots = 8;
    uint8_t d[2] = {++discoverRound, slots};
    busDiscovers++;
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
  pupd::loop();
}

// ---------- Panel-Updates über den Bus ----------
// Das Hauptpanel trägt die aktuelle Panel-Firmware (panel_fw.h) in sich. Ein Panel mit älterer Version
// springt auf BOOT in seinen Bootloader, bekommt die Firmware Seite für Seite (64 Bytes) und startet neu.
// Danach bekommt es seine alte Adresse zurück, ohne Neuerkennung. Panels, die nach einem abgebrochenen Update
// im Bootloader hängen, melden sich auf BL_SCAN und werden automatisch neu bespielt.
namespace pupd {
enum Step : uint8_t { IDLE, ENTER, INFO, WRITE, DONE, ASSIGN, SIM };
struct Job { int8_t i = -1; uint32_t chip = 0; uint8_t addr = 0; Step step = IDLE; uint16_t page = 0; uint8_t tries = 0; uint32_t at = 0; bool rescue = false; };
Job job;
uint32_t lastScan = 0;
uint8_t scanRound = 0;

bool busy(uint32_t chip) { return job.step != IDLE && job.chip == chip; }
bool canUpdate(const Panel& p) {
  if (!p.used || !p.attached || !p.fw || p.fw >= PANEL_FW_VERSION) return false;
  return cfg.bus ? (p.caps & bus::CAP_BOOT) && p.addr : true;
}

void sendBl(uint32_t chip, uint8_t cmd, const uint8_t* extra, uint8_t n) {
  uint8_t d[4 + 65] = {(uint8_t)chip, (uint8_t)(chip >> 8), (uint8_t)(chip >> 16), (uint8_t)(chip >> 24)};
  if (n) memcpy(d + 4, extra, n);
  bus::send(bus::BYID, cmd, d, 4 + n);
}

void end(bool ok, const char* why) {
  int i = job.i;
  String name = hex(job.chip).substring(4);
  if (i >= 0) {
    P[i].upd = ok ? 0 : 3; P[i].updPct = ok ? 100 : 0; P[i].miss = 0;
    if (!ok) P[i].updFails++;
    if (ok && !cfg.bus) { P[i].fw = PANEL_FW_VERSION; prefs.putUChar(("f" + hex(P[i].chip)).c_str(), PANEL_FW_VERSION); }
  }
  if (ok) diag("Panel %s hat jetzt Firmware %u", name.c_str(), PANEL_FW_VERSION);
  else diag("Update von Panel %s fehlgeschlagen: %s", name.c_str(), why);
  job = Job();
  wsKick();
}

void start(int i) {
  job = Job(); job.i = i; job.chip = P[i].chip; job.addr = P[i].addr;
  job.step = cfg.bus ? ENTER : SIM; job.at = millis();
  P[i].upd = 2; P[i].updPct = 0;
  diag("Panel %s: Update auf Firmware %u beginnt", hex(P[i].chip).substring(4).c_str(), PANEL_FW_VERSION);
}

// Ein Schritt pro Aufruf, damit App und Effekte weiterlaufen
void step() {
  if ((int32_t)(millis() - job.at) < 0) return;
  bus::Reply r;
  switch (job.step) {
  case IDLE: return;
  case SIM:                                          // Simulation: gleiche Dauer wie am Bus, etwa 15 ms pro Seite
    job.at = millis() + 15;
    if (++job.page >= PANEL_FW_PAGES) end(true, nullptr);
    break;
  case ENTER:
    bus::send(job.addr, bus::C_BOOT);
    if (bus::receive(r, 5) && r.cmd == bus::C_BOOT && r.len >= 1) {
      if (!r.data[0]) { end(false, "kein Bootloader"); return; }
      job.step = INFO; job.tries = 0; job.at = millis() + 30;
    } else if (++job.tries > 5) end(false, "antwortet nicht");
    return;
  case INFO:
    sendBl(job.chip, bus::BL_INFO, nullptr, 0);
    if (bus::receive(r, 5) && r.cmd == bus::BL_INFO) { job.step = WRITE; job.page = 0; job.tries = 0; }
    else if (++job.tries > 50) end(false, "Bootloader meldet sich nicht");
    else job.at = millis() + 20;
    return;
  case WRITE: {
    uint8_t d[65]; d[0] = job.page;
    memcpy(d + 1, PANEL_FW + job.page * 64, 64);
    sendBl(job.chip, bus::BL_WRITE, d, 65);
    if (bus::receive(r, 30) && r.cmd == bus::BL_WRITE && r.len >= 1 && r.data[0]) { job.page++; job.tries = 0; }
    else if (++job.tries > 5) { end(false, "Schreiben fehlgeschlagen"); return; }
    if (job.page >= PANEL_FW_PAGES) { job.step = DONE; job.tries = 0; }
    break;
  }
  case DONE: {
    uint8_t d[4] = {(uint8_t)PANEL_FW_PAGES, (uint8_t)PANEL_FW_CRC, (uint8_t)(PANEL_FW_CRC >> 8), PANEL_FW_VERSION};
    sendBl(job.chip, bus::BL_DONE, d, 4);
    if (bus::receive(r, 60) && r.cmd == bus::BL_DONE && r.len >= 1 && r.data[0]) {
      if (job.rescue) { end(true, nullptr); return; }  // ohne bekannte Adresse: die normale Erkennung übernimmt
      job.step = ASSIGN; job.tries = 0; job.at = millis() + 30;
    } else if (++job.tries > 3) end(false, "Prüfsumme passt nicht");
    return;
  }
  case ASSIGN:                                       // neue Firmware läuft: alte Adresse zurück
    bus::sendById(job.chip, bus::C_ASSIGN, job.addr);
    if (bus::receive(r, 5) && r.cmd == bus::C_ASSIGN && r.addr == job.addr) {
      bus::sendOrder(job.addr); bus::sendTouch(job.addr);
      if (job.i >= 0) { P[job.i].touchKnown = false; P[job.i].fw = 0; }   // Version kommt mit dem nächsten PING
      outForce = true;
      end(true, nullptr);
    } else if (++job.tries > 50) end(false, "startet nach dem Update nicht");
    else job.at = millis() + 20;
    return;
  }
  if (job.i >= 0 && job.step != IDLE) P[job.i].updPct = job.page * 100 / PANEL_FW_PAGES;
}

void loop() {
  if (job.step != IDLE) { step(); return; }
  for (int i = 1; i < SLOTS; i++) if (P[i].upd == 1) {
    if (canUpdate(P[i])) { start(i); return; }
    P[i].upd = 0;
  }
  if (cfg.panelAuto)
    for (int i = 1; i < SLOTS; i++) if (P[i].upd == 0 && P[i].updFails < 2 && canUpdate(P[i])) { start(i); return; }
  if (!cfg.bus || millis() - lastScan < 5000) return;
  lastScan = millis();                               // hängt ein Panel nach abgebrochenem Update im Bootloader?
  uint8_t d[2] = {++scanRound, 4};
  bus::send(bus::ALL, bus::BL_SCAN, d, 2);
  bus::Reply r;
  if (bus::receive(r, 8) && r.cmd == bus::BL_SCAN && r.len >= 5) {
    uint32_t chip = (uint32_t)r.data[0] | ((uint32_t)r.data[1] << 8) | ((uint32_t)r.data[2] << 16) | ((uint32_t)r.data[3] << 24);
    job = Job(); job.chip = chip; job.i = findChip(chip); job.rescue = true; job.step = INFO; job.at = millis();
    if (job.i >= 0) {
      P[job.i].upd = 2; P[job.i].updPct = 0;
      if (P[job.i].attached && P[job.i].addr) { job.addr = P[job.i].addr; job.rescue = false; }   // steht noch an der Wand: alte Adresse zurück
    }
    diag("Panel %s hängt im Bootloader, Firmware wird neu aufgespielt", hex(chip).substring(4).c_str());
  }
}
}  // namespace pupd


// ---------- Effekte ----------
// Das Hauptpanel rechnet jeden Effekt selbst und schickt etwa 25 Bilder pro Sekunde
// mit einem einzigen FRAME-Befehl an alle Panels. Die Panel-Firmware braucht dafür nichts Neues.
// color: mit der Palette "Standard" benutzt der Effekt die gewählte Effektfarbe
struct FxDef { const char* id; const char* name; bool color; };
const FxDef FX[] = {
  {"aus",         "Einfarbig",       false},
  {"regenbogen",  "Regenbogen",      false},
  {"welle",       "Regenbogenwelle", false},
  {"atmen",       "Atmen",           true},
  {"farbwechsel", "Farbwechsel",     false},
  {"funkeln",     "Funkeln",         true},
  {"ausbreiten",  "Ausbreiten",      true},
  {"feuer",       "Feuer",           false},
  {"polarlicht",  "Polarlicht",      false},
  {"lauflicht",   "Lauflicht",       true},
  {"spirale",     "Spirale",         false},
  {"gewitter",    "Gewitter",        false},
  {"kerzen",      "Kerzenlicht",     false},
  {"disco",       "Disco",           false},
  {"komet",       "Komet",           true},
  {"lava",        "Lava",            false},
  {"verlauf",     "Farbverlauf",     true},
  {"herz",        "Herzschlag",      true},
  {"plasma",      "Plasma",          false},
  {"regen",       "Regen",           false},
  {"sterne",      "Sternenhimmel",   false},
  {"feuerwerk",   "Feuerwerk",       false},
  {"matrix",      "Matrix",          false},
  {"wetter",      "Wetter",          false},
  {"raum-komet",  "Raum-Komet",      true},     // laufen über alle Wände im Raum
  {"raum-regenbogen", "Raum-Regenbogen", false},
  {"raum-welle",  "Raum-Welle",      true},
};
const uint8_t FX_COUNT = sizeof(FX) / sizeof(FX[0]);

// Paletten wie bei WLED: Farbverläufe, aus denen die Effekte ihre Farben nehmen
struct PalDef { const char* id; const char* name; uint8_t n; uint32_t c[6]; };
const PalDef PALS[] = {
  {"standard",        "Standard",        0, {0}},       // jeder Effekt mit seinen eigenen Farben
  {"regenbogen",      "Regenbogen",      6, {0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF}},
  {"effektfarbe",     "Effektfarbe",     0, {0}},       // dunkel, hell und aufgehellt aus der Effektfarbe
  {"ozean",           "Ozean",           5, {0x001E64, 0x0050C8, 0x00B4E6, 0x50F0FF, 0x0064B4}},
  {"lava",            "Lava",            5, {0x3C0000, 0xB40000, 0xFF3C00, 0xFFA000, 0xFF1E00}},
  {"wald",            "Wald",            5, {0x003C00, 0x1E7814, 0x64B41E, 0xA0D23C, 0x145A28}},
  {"sonnenuntergang", "Sonnenuntergang", 5, {0x3C0A64, 0xA01E64, 0xF03C32, 0xFFA028, 0xFFD264}},
  {"party",           "Party",           6, {0x5500AB, 0xFF0055, 0xFF9900, 0xFFFF00, 0x00FF99, 0x0066FF}},
  {"pastell",         "Pastell",         5, {0xFFB3BA, 0xFFDFBA, 0xFFFFBA, 0xBAFFC9, 0xBAE1FF}},
  {"eis",             "Eis",             5, {0xFFFFFF, 0xC8F0FF, 0x64C8FF, 0x1E78FF, 0xE6FAFF}},
};
const uint8_t PAL_COUNT = sizeof(PALS) / sizeof(PALS[0]);
// Eigene Paletten: 4 feste Plätze (eigen1 … eigen4) mit 2 bis 6 Farben, Index nach den eingebauten
struct CPal { bool used; String name; uint8_t n; uint32_t c[6]; };
const uint8_t CPAL_MAX = 4, PAL_ALL = PAL_COUNT + CPAL_MAX;
CPal cpal[CPAL_MAX];
// Mitgenommene Palette: kommt mit den Farben von einer anderen Wand (Gleichtakt, Raum-Effekt), ohne die eigenen zu ändern
const uint8_t PAL_X = PAL_ALL;
CPal xpal;
bool palValid(uint8_t k) { return k < PAL_COUNT || (k < PAL_ALL && cpal[k - PAL_COUNT].used) || (k == PAL_X && xpal.used); }
String palId(uint8_t k) { return k < PAL_COUNT ? String(PALS[k].id) : k == PAL_X ? String("mitgenommen") : "eigen" + String(k - PAL_COUNT + 1); }
String palName(uint8_t k) { return k < PAL_COUNT ? String(PALS[k].name) : !palValid(k) ? String("Standard") : k == PAL_X ? xpal.name : cpal[k - PAL_COUNT].name; }
namespace pl { const CPal* custom(uint8_t k) { return k == PAL_X ? (xpal.used ? &xpal : nullptr) : (k >= PAL_COUNT && k < PAL_ALL && cpal[k - PAL_COUNT].used) ? &cpal[k - PAL_COUNT] : nullptr; } }   // im Namensraum: sonst setzt Arduino den Prototyp vor CPal
#define palCustom pl::custom
// Farben einer anderen Wand übernehmen: gibt es dieselben schon als eigene Palette, die nehmen, sonst als mitgenommene
void palAdopt(uint8_t n, const uint32_t* c, const String& name);
void cpalSave() {
  JsonDocument d; JsonArray a = d.to<JsonArray>();
  for (uint8_t i = 0; i < CPAL_MAX; i++) {
    if (!cpal[i].used) { a.add(nullptr); continue; }
    JsonObject o = a.add<JsonObject>(); o["n"] = cpal[i].name; JsonArray c = o["c"].to<JsonArray>();
    for (uint8_t k = 0; k < cpal[i].n; k++) c.add(cpal[i].c[k]);
  }
  String s; serializeJson(d, s); prefs.putString("cpal", s);
}
void palAdopt(uint8_t n, const uint32_t* c, const String& name) {
  if (n < 2) return; if (n > 6) n = 6;
  for (uint8_t i = 0; i < CPAL_MAX; i++) if (cpal[i].used && cpal[i].n == n && !memcmp(cpal[i].c, c, n * 4)) { fx.pal = PAL_COUNT + i; return; }
  if (!(xpal.used && xpal.n == n && !memcmp(xpal.c, c, n * 4) && xpal.name == name)) {
    xpal.used = true; xpal.n = n; memcpy(xpal.c, c, n * 4); xpal.name = name.length() ? name.substring(0, 24) : String("Mitgenommen");
    JsonDocument d; d["n"] = xpal.name; JsonArray a = d["c"].to<JsonArray>(); for (uint8_t k = 0; k < n; k++) a.add(c[k]);
    String s; serializeJson(d, s); prefs.putString("xpal", s);
  }
  fx.pal = PAL_X;
}
// Farben der aktuellen Palette als Liste (nur eigene und mitgenommene), damit andere Wände sie übernehmen können
bool palCarry(JsonObject o) {
  const CPal* q = palCustom(fx.pal); if (!q) return false;
  JsonArray a = o["palColors"].to<JsonArray>(); for (uint8_t k = 0; k < q->n; k++) a.add(q->c[k]);
  o["palName"] = q->name; return true;
}
void cpalLoad() {
  { JsonDocument d; String x = prefs.getString("xpal", "");
    if (x.length() && !deserializeJson(d, x)) { xpal.n = 0; xpal.name = (const char*)(d["n"] | "Mitgenommen");
      for (JsonVariant v : d["c"].as<JsonArray>()) if (xpal.n < 6) xpal.c[xpal.n++] = v.as<uint32_t>();
      xpal.used = xpal.n >= 2; } }
  String s = prefs.getString("cpal", "");
  if (!s.length()) {                                   // zum Start eine Vorlage: Rot, Gold, Weiß
    cpal[0].used = true; cpal[0].name = "Rot-Gold-Weiß"; cpal[0].n = 3; cpal[0].c[0] = 0xFF0000; cpal[0].c[1] = 0xFFB000; cpal[0].c[2] = 0xFFFFFF;
    return;
  }
  JsonDocument d; if (deserializeJson(d, s)) return;
  for (uint8_t i = 0; i < CPAL_MAX; i++) {
    JsonVariant o = d[i]; cpal[i].used = o.is<JsonObject>();
    if (!cpal[i].used) continue;
    cpal[i].name = (const char*)(o["n"] | "Eigene"); cpal[i].n = 0;
    for (JsonVariant v : o["c"].as<JsonArray>()) if (cpal[i].n < 6) cpal[i].c[cpal[i].n++] = v.as<uint32_t>();
    if (cpal[i].n < 2) cpal[i].used = false;
  }
}
int palFind(const char* key) {
  for (uint8_t k = 0; k < PAL_COUNT; k++) if (!strcasecmp(key, PALS[k].id) || !strcasecmp(key, PALS[k].name)) return k;
  for (uint8_t k = PAL_COUNT; k < PAL_ALL; k++) if (palValid(k) && (!strcasecmp(key, palId(k).c_str()) || !strcasecmp(key, palName(k).c_str()))) return k;
  return -1;
}
bool fxUsesColor() { return fx.id && (fx.pal == 2 || (fx.pal == 0 && FX[fx.id].color)); }
const uint32_t FX_FRAME_MS = 40;

float fxPhase = 0, fxA[SLOTS * 3], fxB[SLOTS * 3], fxT[SLOTS * 3];   // Zustand pro Punkt (Panel oder Kante)
float fxFlash = 0, fxFlashX = 0, fxFlashY = 0;      // Gewitter: aktueller Blitz
int fxHead = 0, fxHeadE = 0; uint8_t fxPlan[3], fxPlanN = 0, fxPlanP = 0; float fxStep = 0; int fxBeat = -1;  // Komet: Kopf (Panel, Kante); Disco: Takt
float fwX = 0, fwY = 0, fwT = 9, fwH = 0;          // Feuerwerk: Mitte, Alter und Farbe der aktuellen Rakete
float wxTemp = 15; uint8_t wxKind = 0; bool wxOk = false;   // Wetter: Temperatur, 0 trocken, 1 Regen, 2 Schnee
uint16_t fxDir = 0;            // Richtung der Effekte in Grad (Lauflicht, Wellen, Lava …)
uint8_t fxC2[4] = {0, 80, 255, 0};   // zweite Farbe für den Farbverlauf
uint32_t fxFavs = 0;                 // Favoriten: Bit pro Effekt
uint8_t fxSpin = 0;            // Richtung dreht sich: 0 nein, 1 langsam, 2 schnell
float fxSpinAng = 0;
float fxAng = 0;               // aktueller Winkel (Richtung + Drehung), pro Bild berechnet
// Wandansicht: so hängt die Wand wirklich (Drehung, Spiegelung). Effekte laufen danach links/rechts, oben/unten.
void viewXY(float x, float y, float& rx, float& ry) {
  float a = cfg.viewRot * 0.01745329f, c = cosf(a), s = sinf(a);
  rx = x * c - y * s; ry = x * s + y * c;
  if (cfg.viewMir) rx = -rx;
}
// wie die Wand hängt, dann um die Effekt-Richtung gedreht: u läuft immer "in Effektrichtung"
void effXY(float x, float y, float& rx, float& ry) {
  float vx, vy; viewXY(x, y, vx, vy);
  float a = fxAng * 0.01745329f, c = cosf(a), s = sinf(a);
  rx = vx * c + vy * s; ry = -vx * s + vy * c;
}
uint32_t fxLast = 0, fxDirtyAt = 0, fxLastFrame = 0;
bool fxDirty = false;
uint8_t TGT[SLOTS][3][4];       // Ziel pro Panel und Kante (Effekt oder feste Farbe), Helligkeit schon eingerechnet
uint8_t CUR[SLOTS][3][4];       // was die Panels gerade zeigen (nach Übergang und Stromlimit)

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

enum PalDefault : uint8_t { D_HUE, D_COLOR, D_FIRE, D_AURORA };
void fireColor(float h, float* c) { c[0] = 255 * h; c[1] = 85 * h * h; c[2] = 0; c[3] = 20 * h * h * h; }

// Farbe an Stelle t (0..1, wiederholt sich) aus der gewählten Palette
void pcol(float t, float* c, uint8_t def) {   // def: PalDefault (als uint8_t, wegen der automatischen Arduino-Prototypen)
  t = t - floorf(t);
  if (fx.pal == 0) {
    switch (def) {
      case D_HUE: hue(t, c); return;
      case D_COLOR: base(1, c); return;
      case D_FIRE: fireColor(0.35f + 0.65f * t, c); return;
      default: hue(0.36f + 0.36f * (0.5f + 0.5f * sinf(t * 6.2831853f)), c); return;
    }
  }
  if (fx.pal == 1) { hue(t, c); return; }
  float st[3][4];
  const float* stops[6]; uint8_t n;
  float tmp[6][4];
  if (fx.pal == 2) {                                   // aus der Effektfarbe: dunkel → Farbe → aufgehellt
    float col[4] = {(float)fx.r, (float)fx.g, (float)fx.b, (float)fx.w};
    for (int k = 0; k < 4; k++) { st[0][k] = col[k] * 0.25f; st[1][k] = col[k]; st[2][k] = col[k] * 0.6f + (k < 3 ? 102 : 0); }
    for (int k = 0; k < 3; k++) stops[k] = st[k];
    n = 3;
  } else {
    const uint32_t* cs;
    if (const CPal* q = palCustom(fx.pal)) { n = q->n; cs = q->c; }
    else { const PalDef& p = PALS[fx.pal < PAL_COUNT ? fx.pal : 1]; n = p.n; cs = p.c; }
    for (int k = 0; k < n; k++) {
      tmp[k][0] = (cs[k] >> 16) & 0xFF; tmp[k][1] = (cs[k] >> 8) & 0xFF; tmp[k][2] = cs[k] & 0xFF; tmp[k][3] = 0;
      stops[k] = tmp[k];
    }
  }
  float x = t * n; int a = (int)x % n, b = (a + 1) % n; float f = x - floorf(x);
  f = f * f * (3 - 2 * f);
  for (int k = 0; k < 4; k++) c[k] = stops[a][k] + (stops[b][k] - stops[a][k]) * f;
}
// Temperatur in °C → Farbe: -10 tiefblau, 0 hellblau, 10 türkis, 20 gelb, 30 rot
void tempColor(float t, float* c) {
  static const float T[][4] = {{-10, 0, 30, 255}, {0, 0, 150, 255}, {10, 0, 220, 140}, {20, 255, 200, 0}, {30, 255, 30, 0}};
  if (t <= T[0][0]) { c[0] = T[0][1]; c[1] = T[0][2]; c[2] = T[0][3]; c[3] = 0; return; }
  for (int k = 0; k < 4; k++) if (t <= T[k + 1][0]) {
    float f = (t - T[k][0]) / 10;
    for (int j = 0; j < 3; j++) c[j] = T[k][j + 1] + (T[k + 1][j + 1] - T[k][j + 1]) * f;
    c[3] = 0; return;
  }
  c[0] = 255; c[1] = 30; c[2] = 0; c[3] = 0;
}
void mul(float* c, float k) { for (int i = 0; i < 4; i++) c[i] *= k; }


// ---------- Raum: mehrere Wände im Grundriss ----------
// Der Grundriss (Ecken von oben, in cm, im Uhrzeigersinn) und wo jede Wand hängt, liegt auf allen Wänden gleich.
// Raum-Effekte rechnen mit der Stelle im Raum (abgewickelt entlang der Raumwände) und der gemeinsamen Uhrzeit:
// so laufen sie über alle Wände, ohne dass dafür Bilder verschickt werden.
const char* apiCall(const char* path, JsonDocument& d);
namespace room {
const uint8_t MAXC = 16, MAXD = 12;
struct Dev { String id, name, ip; uint8_t w = 0; float x = 0, y = 0, bw = 0, bh = 0; uint32_t g = 0; };
float cx[MAXC], cy[MAXC], U[MAXC + 1]; uint8_t nC = 0;
Dev dev[MAXD]; uint8_t nD = 0;
float H = 250, S = 23, Ptot = 0;
double ver = 0;
String raw;                         // der Grundriss so, wie ihn die App geschickt hat
int me = -1;                        // diese Wand im Raum (-1: nicht aufgestellt)
// Abbildung "Stelle im Raum" → "Effekt-Strecke": lange Lücken zwischen den Wänden werden kurz
struct Seg { float u0, c0, len, k; };
Seg seg[2 * MAXD + 2]; uint8_t nS = 0; float Lc = 0, uStart = 0;
float ocx = 0, ocy = 0;             // Mitte der eigenen Panels (Wandansicht), pro Bild

// eigene Panels: Mitte der Mittelpunkte und Größe (Seitenlänge 1), wie die Wand hängt
void ownBox(float& mx, float& my, float& bw, float& bh) {
  float x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) {
    float vx, vy; viewXY(P[i].x * 0.5f, P[i].y * 0.866f, vx, vy);
    x0 = fminf(x0, vx); x1 = fmaxf(x1, vx); y0 = fminf(y0, vy); y1 = fmaxf(y1, vy);
  }
  if (x0 > x1) { mx = my = 0; bw = bh = 1; return; }
  mx = (x0 + x1) / 2; my = (y0 + y1) / 2; bw = x1 - x0 + 1; bh = y1 - y0 + 1.16f;
}
uint32_t geoSig() {
  uint32_t h = 2166136261u ^ cfg.viewRot ^ (cfg.viewMir << 9);
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) h = (h ^ (uint32_t)(P[i].x * 131 + P[i].y * 7 + P[i].rot * 3 + P[i].edges)) * 16777619u;
  return h ? h : 1;
}
// Geometrie dieser Wand für die App: Panels mit Mitte, Winkel der Spitze und aktueller Farbe
String geoJson() {
  JsonDocument d; float mx, my, bw, bh; ownBox(mx, my, bw, bh);
  d["id"] = hex(P[0].chip); d["n"] = cfg.name; d["ip"] = WiFi.localIP().toString(); d["ver"] = FW_VERSION; d["g"] = geoSig();
  d["bw"] = bw; d["bh"] = bh;
  JsonArray a = d["p"].to<JsonArray>();
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) {
    float vx, vy; viewXY(P[i].x * 0.5f, P[i].y * 0.866f, vx, vy);
    int ang = (isUp(P[i].x, P[i].y) ? 270 : 90) + cfg.viewRot; if (cfg.viewMir) ang = 180 - ang; ang = ((ang % 360) + 360) % 360;
    const uint8_t* c = CUR[i][0]; char b[8];
    auto cl = [](int v) { return v > 255 ? 255 : v; };
    snprintf(b, sizeof b, "#%02X%02X%02X", cl(c[0] + c[3]), cl(c[1] + c[3]), cl(c[2] + c[3]));
    JsonArray q = a.add<JsonArray>(); q.add(roundf((vx - mx) * 100) / 100); q.add(roundf((vy - my) * 100) / 100); q.add(ang); q.add(b); q.add(P[i].edges ? 1 : 0);
  }
  String s; serializeJson(d, s); return s;
}
void build() {
  // Wandlängen und wo jede Raumwand in der Abwicklung beginnt
  Ptot = 0;
  for (uint8_t i = 0; i < nC; i++) { U[i] = Ptot; uint8_t j = (i + 1) % nC; Ptot += hypotf(cx[j] - cx[i], cy[j] - cy[i]); }
  U[nC] = Ptot;
  // belegte Stücke, sortiert und zusammengefasst
  float a[MAXD], b[MAXD]; uint8_t n = 0;
  for (uint8_t i = 0; i < nD; i++) if (dev[i].w < nC) { a[n] = U[dev[i].w] + dev[i].x - dev[i].bw / 2; b[n] = a[n] + fmaxf(1, dev[i].bw); n++; }
  for (uint8_t i = 0; i < n; i++) for (uint8_t j = i + 1; j < n; j++) if (a[j] < a[i]) { float t = a[i]; a[i] = a[j]; a[j] = t; t = b[i]; b[i] = b[j]; b[j] = t; }
  uint8_t m = 0; for (uint8_t i = 0; i < n; i++) { if (m && a[i] <= b[m - 1]) b[m - 1] = fmaxf(b[m - 1], b[i]); else { a[m] = a[i]; b[m] = b[i]; m++; } }
  nS = 0; Lc = 0;
  if (!m || Ptot <= 0) return;
  uStart = a[0];
  float G = 1.5f * S;                                   // längere Lücken werden so kurz wie 1,5 Panels
  for (uint8_t i = 0; i < m; i++) {
    seg[nS++] = {a[i], Lc, b[i] - a[i], 1}; Lc += b[i] - a[i];
    float nx = i + 1 < m ? a[i + 1] : a[0] + Ptot, g = nx - b[i];
    if (g > 0) { float cg = fminf(g, G); seg[nS++] = {b[i], Lc, g, cg / g}; Lc += cg; }
  }
}
// Stelle im Raum (cm entlang der Wände) → Stelle auf der Effekt-Strecke (0 … Lc)
float cu(float u) {
  if (!nS) return u;
  while (u < uStart) u += Ptot; while (u >= uStart + Ptot) u -= Ptot;
  for (uint8_t i = 0; i < nS; i++) if (u < seg[i].u0 + seg[i].len || i == nS - 1) return seg[i].c0 + fminf(u - seg[i].u0, seg[i].len) * seg[i].k;
  return 0;
}
bool parse(const String& js) {
  JsonDocument d; if (deserializeJson(d, js)) return false;
  nC = 0; for (JsonVariant c : d["c"].as<JsonArray>()) if (nC < MAXC) { cx[nC] = c[0] | 0.0f; cy[nC] = c[1] | 0.0f; nC++; }
  H = d["h"] | 250.0f; S = constrain(d["s"] | 23.0f, 5.0f, 100.0f); ver = d["v"] | 0.0;
  nD = 0; me = -1; String my = hex(P[0].chip);
  for (JsonVariant v : d["d"].as<JsonArray>()) {
    if (nD >= MAXD) break;
    Dev& e = dev[nD]; e = Dev();
    e.id = (const char*)(v["id"] | ""); e.name = (const char*)(v["n"] | ""); e.ip = (const char*)(v["ip"] | "");
    e.w = v["w"] | 0; e.x = v["x"] | 0.0f; e.y = v["y"] | 0.0f; e.bw = v["bw"] | 0.0f; e.bh = v["bh"] | 0.0f; e.g = v["g"] | 0u;
    if (e.id == my) me = nD;
    nD++;
  }
  if (nC < 3) { nC = 0; nD = 0; me = -1; }
  build();
  return true;
}
void load() { size_t n = prefs.getBytesLength("room"); if (n && n < 16000) { char* b = (char*)malloc(n + 1); if (b) { prefs.getBytes("room", b, n); b[n] = 0; raw = b; free(b); parse(raw); } } }
void store() { prefs.putBytes("room", raw.c_str(), raw.length()); }

// an die anderen Wände im Raum schicken (der Reihe nach, im Hintergrund)
String qPath, qBody; volatile bool sending = false, again = false;
void sendTask(void*) {
  do {
    again = false;
    String path = qPath, body = qBody; String ips[MAXD]; uint8_t n = 0;
    for (uint8_t i = 0; i < nD; i++) if ((int)i != me && dev[i].ip.length() > 6) ips[n++] = dev[i].ip;
    for (uint8_t i = 0; i < n && !netQuiet && !otaBusy; i++) {
      HTTPClient h; h.setTimeout(2500);
      if (h.begin("http://" + ips[i] + path)) { h.addHeader("Content-Type", "application/json"); h.POST(body); }
      h.end();
    }
  } while (again);
  sending = false;
  vTaskDelete(nullptr);
}
void fanout(const char* path, const String& body) {
  qPath = path; qBody = body;
  if (sending) { again = true; return; }
  sending = true;
  if (xTaskCreate(sendTask, "raum", 6144, nullptr, 1, nullptr) != pdPASS) sending = false;
}
double nowV() { if (timeOk()) { struct timeval tv; gettimeofday(&tv, nullptr); return (double)tv.tv_sec * 1000 + tv.tv_usec / 1000; } return millis(); }
// neuen Grundriss übernehmen (nur, wenn er neuer ist) und auf Wunsch an die anderen weitergeben
const char* save(JsonVariantConst lay, bool fwd) {
  double v = lay["v"] | 0.0;
  if (v < ver) return nullptr;                          // älterer Stand: ignorieren
  String js; serializeJson(lay, js);
  if (js.length() > 15000) return "Raum zu groß";
  if (!parse(js)) return "Raum unlesbar";
  raw = js; store();
  if (fwd) { JsonDocument f; f["action"] = "save"; f["fwd"] = false; f["layout"] = lay; String b; serializeJson(f, b); fanout("/api/room", b); }
  return nullptr;
}
// Abgleich: jede Minute bei den anderen Wänden nachsehen, ob sie denselben Raum haben (z. B. nach einem Update
// oder wenn eine Wand beim Speichern aus war). Älteren schicken wir unseren, einen neueren übernehmen wir.
volatile bool healing = false; String healNew;
void healTask(void*) {
  String ips[MAXD]; uint8_t n = 0; double v0 = ver; String mine = raw;
  for (uint8_t i = 0; i < nD; i++) if ((int)i != me && dev[i].ip.length() > 6) ips[n++] = dev[i].ip;
  for (uint8_t i = 0; i < n && !netQuiet && !otaBusy; i++) {
    HTTPClient h; h.setTimeout(2500); String r;
    if (h.begin("http://" + ips[i] + "/api/room") && h.GET() == 200) r = h.getString();
    h.end();
    if (!r.length()) continue;
    JsonDocument f; f["layout"]["v"] = true; JsonDocument d;
    if (deserializeJson(d, r, DeserializationOption::Filter(f))) continue;
    double v = d["layout"]["v"] | 0.0;
    if (v < v0) {
      HTTPClient p; p.setTimeout(3000);
      if (p.begin("http://" + ips[i] + "/api/room")) { p.addHeader("Content-Type", "application/json"); p.POST("{\"action\":\"save\",\"fwd\":false,\"layout\":" + mine + "}"); }
      p.end();
    } else if (v > v0 && !healNew.length()) {
      JsonDocument full; if (!deserializeJson(full, r) && full["layout"].is<JsonObject>()) serializeJson(full["layout"], healNew);
    }
  }
  healing = false;
  vTaskDelete(nullptr);
}
// eigene Panels geändert (abgeklipst, gedreht …): den eigenen Eintrag im Raum nachziehen und weitergeben
void loop() {
  static uint32_t last = 0, lastHeal = 0;
  if (healNew.length() && !healing) { JsonDocument d; String js = healNew; healNew = ""; if (!deserializeJson(d, js)) save(d.as<JsonVariantConst>(), false); }
  if (me >= 0 && wlanOk && !healing && !sending && !netQuiet && !otaBusy && nD > 1 && (!lastHeal || millis() - lastHeal > 60000)) {
    lastHeal = millis() | 1; healing = true;
    if (xTaskCreate(healTask, "raumabgl", 6144, nullptr, 1, nullptr) != pdPASS) healing = false;
  }
  if (me < 0 || millis() - last < 5000) return;
  last = millis();
  uint32_t g = geoSig(); if (dev[me].g == g) return;
  JsonDocument d; if (deserializeJson(d, raw)) return;
  String my = hex(P[0].chip);
  for (JsonVariant v : d["d"].as<JsonArray>()) if (my == (const char*)(v["id"] | "")) {
    JsonDocument geo; deserializeJson(geo, geoJson());
    float mx, my2, bw, bh; ownBox(mx, my2, bw, bh);
    v["p"] = geo["p"]; v["bw"] = roundf(bw * S); v["bh"] = roundf(bh * S); v["g"] = g; v["ip"] = WiFi.localIP().toString(); v["n"] = cfg.name;
  }
  d["v"] = (uint64_t)fmax(ver + 1, nowV());           // als ganze Zahl: Millisekunden passen nicht genau in eine Kommazahl
  save(d.as<JsonVariantConst>(), true);
}
// Sekunden seit Mitternacht (gemeinsame Uhr über das Internet), ohne Uhrzeit die Laufzeit
double tsec() { if (timeOk()) { struct timeval tv; gettimeofday(&tv, nullptr); return (double)(tv.tv_sec % 86400) + tv.tv_usec / 1e6; } return millis() / 1000.0; }
// vor jedem Bild: Mitte der eigenen Panels; ohne Raum wirkt der Effekt nur auf dieser Wand
// ----- Raum-Komet: wie der Komet einer Wand, aber über alle Wände im Raum -----
// Alle Wände bauen aus demselben Raum dasselbe Netz und rechnen denselben Weg: Schritt n hängt nur vom Raum und
// von der Uhrzeit ab, darum gibt es genau einen Kopf, ohne dass etwas gesendet wird.
// Panels mit "Kanten einzeln": alle drei Kanten nacheinander, dann über die zuletzt leuchtende Kante hinüber.
const uint16_t MAXU = 160, MAXN = 480;
const int32_t BLK = 20000;
struct Unit { float u, h, a; uint16_t n0; bool ed; uint8_t dv; int16_t acU[3]; int8_t acS[3]; int16_t jmp[2]; };
Unit un[MAXU]; uint16_t nU = 0;
uint16_t nodeU[MAXN]; int8_t nodeS[MAXN]; int32_t lastV[MAXN]; uint16_t nN = 0;
int16_t ownU[SLOTS], ownE[SLOTS][3];
uint32_t gSig = 0; bool gRoom = false;
int32_t curStep = -1; uint16_t head = 0; uint8_t plan[3], planN = 0, planP = 0;
uint32_t mix(uint32_t a, uint32_t b) { uint32_t h = a * 2654435761u ^ (b + 0x9E3779B9u + (a << 6) + (a >> 2)); h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return h; }
// Kantenmitte s: der Schwerpunkt liegt 0,144 hinter der Rastermitte (weg von der Spitze), die Kantenmitten 0,289 um ihn herum
void sideMid(const Unit& q, int s, float& u, float& h) {
  float t = q.a * 0.01745329f, a = (q.a + 60 + 120 * s) * 0.01745329f;
  float x = -0.1443f * cosf(t) + 0.2887f * cosf(a), y = -0.1443f * sinf(t) + 0.2887f * sinf(a);
  u = q.u + x * S; h = q.h - y * S;
}
void addUnit(float u, float h, float a, bool ed, uint8_t dv) {
  if (nU >= MAXU || nN + (ed ? 3 : 1) > MAXN) return;
  Unit& q = un[nU]; q.u = u; q.h = h; q.a = a; q.ed = ed; q.dv = dv; q.n0 = nN;
  for (int s = 0; s < 3; s++) { q.acU[s] = -1; q.acS[s] = -1; } q.jmp[0] = q.jmp[1] = -1;
  for (int s = 0; s < (ed ? 3 : 1); s++) { nodeU[nN] = nU; nodeS[nN] = ed ? s : -1; nN++; }
  nU++;
}
// Nachbarn in einer Wand: zwei Panels teilen eine Kante, wenn die Kantenmitten aufeinander liegen
void linkWithin(uint16_t a0, uint16_t a1) {
  for (uint16_t i = a0; i < a1; i++) for (int s = 0; s < 3; s++) {
    float u1, h1; sideMid(un[i], s, u1, h1);
    for (uint16_t j = a0; j < a1; j++) if (j != i) for (int t = 0; t < 3; t++) {
      float u2, h2; sideMid(un[j], t, u2, h2);
      if (fabsf(u1 - u2) + fabsf(h1 - h2) < 0.15f * S) { un[i].acU[s] = j; un[i].acS[s] = t; }
    }
  }
}
void jump(uint16_t a, uint16_t b) {
  if (a == b) return;
  for (int k = 0; k < 2; k++) if (un[a].jmp[k] == (int16_t)b) return;
  for (int k = 0; k < 2; k++) if (un[a].jmp[k] < 0) { un[a].jmp[k] = b; break; }
  for (int k = 0; k < 2; k++) if (un[b].jmp[k] < 0) { un[b].jmp[k] = a; break; }
}
void addP(JsonArrayConst p, float u0, float h0, uint8_t dv) {
  for (JsonVariantConst q : p) addUnit(u0 + (q[0] | 0.0f) * S, h0 - (q[1] | 0.0f) * S, q[2] | 270.0f, (q[4] | 0) != 0, dv);
}
void graph() {
  nU = nN = 0; curStep = -1;
  for (int i = 0; i < SLOTS; i++) { ownU[i] = -1; ownE[i][0] = ownE[i][1] = ownE[i][2] = -1; }
  float mx, my, bw, bh; ownBox(mx, my, bw, bh);
  JsonDocument d; uint16_t first[MAXD], cnt[MAXD]; float lo[MAXD]; uint8_t k = 0;
  gRoom = me >= 0 && !deserializeJson(d, raw);
  float ou = 0, oh = 0;                                     // wo die eigene Wand im Raum liegt
  if (gRoom) {
    for (JsonVariant v : d["d"].as<JsonArray>()) {
      if (k >= nD) break;
      const Dev& e = dev[k]; first[k] = nU;
      if (e.w < nC) addP(v["p"].as<JsonArrayConst>(), U[e.w] + e.x, e.y, k);
      cnt[k] = nU - first[k]; lo[k] = 1e9; for (uint16_t q = first[k]; q < nU; q++) lo[k] = fminf(lo[k], un[q].u);
      linkWithin(first[k], nU); k++;
    }
    // die Wände der Reihe nach entlang der Raumwände verbinden: rechtes Ende der einen mit dem linken der nächsten
    uint8_t ord[MAXD], n = 0; for (uint8_t i = 0; i < k; i++) if (cnt[i]) ord[n++] = i;
    for (uint8_t i = 0; i < n; i++) for (uint8_t j = i + 1; j < n; j++) if (lo[ord[j]] < lo[ord[i]]) { uint8_t t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    for (uint8_t i = 0; n > 1 && i < n; i++) {
      uint8_t A = ord[i], B = ord[(i + 1) % n]; float wrap = i + 1 == n ? Ptot : 0;
      uint16_t a = first[A]; for (uint16_t q = first[A]; q < first[A] + cnt[A]; q++) if (un[q].u > un[a].u) a = q;
      uint16_t b = first[B]; float best = 1e9;
      for (uint16_t q = first[B]; q < first[B] + cnt[B]; q++) { float c = (un[q].u + wrap - lo[B]) + 0.5f * fabsf(un[q].h - un[a].h); if (c < best) { best = c; b = q; } }
      jump(a, b);
    }
    ou = U[dev[me].w] + dev[me].x; oh = dev[me].y;
  } else {                                                  // ohne Raum: nur die eigenen Panels
    JsonDocument g; deserializeJson(g, geoJson());
    addP(g["p"].as<JsonArrayConst>(), 0, 0, 0); linkWithin(0, nU);
  }
  // eigene Panels und Kanten den Knoten zuordnen (nächster Punkt)
  uint16_t a0 = gRoom ? first[me] : 0, a1 = gRoom ? first[me] + cnt[me] : nU;
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) {
    float vx, vy; viewXY(P[i].x * 0.5f, P[i].y * 0.866f, vx, vy);
    float u = ou + (vx - mx) * S, h = oh - (vy - my) * S, bd = 1e9; int bi = -1;
    for (uint16_t q = a0; q < a1; q++) { float dd = fabsf(un[q].u - u) + fabsf(un[q].h - h); if (dd < bd) { bd = dd; bi = q; } }
    if (bi < 0 || bd > 0.4f * S) continue;
    ownU[i] = bi;
    if (un[bi].ed) for (int e = 0; e < 3; e++) {
      float px, py, dd2; edgePos(i, e, px, py, dd2); viewXY(px, py, vx, vy);
      float eu = ou + (vx - mx) * S, eh = oh - (vy - my) * S, b2 = 1e9;
      for (int s = 0; s < 3; s++) { float su, sh; sideMid(un[bi], s, su, sh); float x = fabsf(su - eu) + fabsf(sh - eh); if (x < b2) { b2 = x; ownE[i][e] = un[bi].n0 + s; } }
    }
  }
  gSig = geoSig() ^ (uint32_t)ver;
}
int32_t seen(uint16_t q) { int32_t m = -1000000; const Unit& x = un[q]; for (int s = 0; s < (x.ed ? 3 : 1); s++) { if (lastV[x.n0 + s] > m) m = lastV[x.n0 + s]; } return m; }
bool onward(uint16_t q, int s) { return un[q].acU[s] >= 0 || un[q].jmp[0] >= 0; }
int32_t onwardSeen(uint16_t q, int s) { int16_t t = un[q].acU[s] >= 0 ? un[q].acU[s] : un[q].jmp[0]; return t >= 0 ? seen(t) : 0x7FFFFFFF; }
// ein Schritt des Kopfes (wie fxCompute beim Komet einer Wand)
void step() {
  uint16_t Q = nodeU[head]; const Unit& q = un[Q];
  if (q.ed && planP < planN) { head = q.n0 + plan[planP++]; return; }        // nächste Kante im Panel
  int16_t cu[5]; int8_t cs[5]; uint8_t n = 0;
  if (q.ed) { int s = nodeS[head]; if (q.acU[s] >= 0) { cu[n] = q.acU[s]; cs[n] = q.acS[s]; n++; } }
  else for (int s = 0; s < 3; s++) if (q.acU[s] >= 0) { cu[n] = q.acU[s]; cs[n] = q.acS[s]; n++; }
  for (int k = 0; k < 2; k++) if (q.jmp[k] >= 0) { cu[n] = q.jmp[k]; cs[n] = -1; n++; }      // hinüber zur nächsten Wand
  if (!n) {
    if (q.ed) { int s = nodeS[head]; plan[0] = (s + 1) % 3; plan[1] = (s + 2) % 3; planN = 2; planP = 0; return; }   // erst die anderen Kanten
    head = mix(curStep, 7) % nN; planN = planP = 0; return;
  }
  int pick = 0; int32_t best = 0x7FFFFFFF; uint32_t tie = 0;
  for (int k = 0; k < n; k++) { int32_t v = seen(cu[k]); uint32_t r = mix(curStep, cu[k]); if (v < best || (v == best && r > tie)) { best = v; tie = r; pick = k; } }
  uint16_t R = cu[pick]; const Unit& r = un[R]; int t = cs[pick];
  if (t < 0) {                                              // Sprung: Eingang ist die Kante, die zur alten Wand zeigt
    float bd = 1e9; for (int s = 0; s < 3; s++) { float su, sh; sideMid(r, s, su, sh); float x = fabsf(su - q.u) + fabsf(sh - q.h); if (x < bd) { bd = x; t = s; } }
  }
  planN = planP = 0;
  if (!r.ed) { head = r.n0; return; }
  head = r.n0 + t;
  int fwE = (t + 2) % 3, bwE = (t + 1) % 3;                 // vorwärts endet bei t+2, rückwärts bei t+1
  bool f = onward(R, fwE), b = onward(R, bwE), dirF;
  if (f && b) { int32_t sf = onwardSeen(R, fwE), sb = onwardSeen(R, bwE); dirF = sf != sb ? sf < sb : (mix(curStep, R) & 1); }
  else dirF = f ? true : b ? false : (mix(curStep, R) & 1);
  plan[0] = dirF ? (t + 1) % 3 : (t + 2) % 3; plan[1] = dirF ? fwE : bwE; planN = 2;
  if (!f && !b) plan[planN++] = t;                          // Sackgasse: zurück zur Eingangskante
}
void walkTo(int32_t t) {
  if (!nN) return;
  int32_t b0 = t - t % BLK;
  if (curStep < 0 || t < curStep || curStep < b0) {         // neu: ab Blockanfang durchrechnen
    for (uint16_t i = 0; i < nN; i++) lastV[i] = -1000000;
    head = un[mix(b0, nU) % nU].n0; planN = planP = 0; curStep = b0; lastV[head] = curStep;
  }
  while (curStep < t) { step(); curStep++; lastV[head] = curStep; }
}
float stepF = 0;
void frame() {
  float bw, bh; ownBox(ocx, ocy, bw, bh);
  if (me < 0) { Lc = fmaxf(1, bw * S); }
  if (fx.id == 24) {
    if (gSig != (geoSig() ^ (uint32_t)ver) || (me >= 0) != gRoom) graph();
    float rate = powf(4.0f, (fx.speed - 50) / 50.0f);
    stepF = (float)fmod(tsec() * 4 * rate, 2000000.0);
    walkTo((int32_t)stepF);
  }
}
// Helligkeit eines eigenen Panels oder einer Kante beim Raum-Kometen (1 = Kopf); tail = Schritte bis dunkel
float comet(int i, int e, float tail, bool& isHead) {
  isHead = false; int16_t q = ownU[i];
  if (q < 0 || !nN) return 0;
  int n = -1; int32_t lv;
  if (un[q].ed && e >= 0 && ownE[i][e] >= 0) { n = ownE[i][e]; lv = lastV[n]; }
  else { lv = seen(q); for (int s = 0; s < (un[q].ed ? 3 : 1); s++) if (un[q].n0 + s == head) n = head; if (n < 0) n = un[q].n0; }
  isHead = n == head;
  float age = stepF - lv;
  if (age < 1) return 1;
  return fmaxf(0, 1 - (age - 1) / tail);
}
// Stelle eines Punkts dieser Wand auf der Effekt-Strecke (cm)
float at(float px, float py) {
  float vx, vy; viewXY(px, py, vx, vy);
  if (me < 0) return (vx - ocx) * S + Lc / 2;
  return cu(U[dev[me].w] + dev[me].x + (vx - ocx) * S);
}
String json() { return "{\"me\":\"" + hex(P[0].chip) + "\",\"layout\":" + (raw.length() ? raw : String("null")) + "}"; }
}  // namespace room

void fxReset() {
  for (int i = 0; i < SLOTS * 3; i++) { fxA[i] = frand(); fxB[i] = frand(); fxT[i] = frand(); }
  fxLast = millis();
}

void fxCompute() {
  uint32_t now = millis();
  float dt = (now - fxLast) / 1000.0f; fxLast = now;
  if (dt > 0.2f) dt = 0.2f;
  float rate = powf(4.0f, (fx.speed - 50) / 50.0f);   // Tempo 1..100 → 0,25- bis 4-fach
  fxPhase += dt * rate;
  if (fxPhase > 100000) fxPhase = 0;

  if (fxSpin) { fxSpinAng += dt * (fxSpin == 1 ? 6 : 30); if (fxSpinAng >= 360) fxSpinAng -= 360; }
  fxAng = fxDir + fxSpinAng;
  float minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) {
    float rx, ry; effXY(P[i].x * 0.5f, P[i].y * 0.866f, rx, ry);
    minX = fminf(minX, rx); maxX = fmaxf(maxX, rx); minY = fminf(minY, ry); maxY = fmaxf(maxY, ry);
  }
  float spanX = fmaxf(1.0f, maxX - minX);
  float cxW = (minX + maxX) / 2, cyW = (minY + maxY) / 2, maxR = fmaxf(1.0f, 0.5f * hypotf(maxX - minX, maxY - minY));
  const float TAU = 6.2831853f;
  // Gewitter: Blitze kommen und klingen schnell ab
  fxFlash = fmaxf(0, fxFlash - dt * rate * 5);
  // Feuerwerk: alle 1,5 s (bei Tempo 50) eine neue Rakete an einem zufälligen Panel
  if (fx.id == 21) {
    fwT += dt * rate;
    if (fwT > 1.6f - 0.8f * fx.inten / 255.0f) {
      int n = 0, pick = 0; for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached && frand() * (++n) < 1) pick = i;
      effXY(P[pick].x * 0.5f, P[pick].y * 0.866f, fwX, fwY); fwT = 0; fwH = frand();
    }
  }
  if (fx.id == 11 && frand() < dt * rate * (0.12f + 0.6f * fx.inten / 255.0f)) {
    int n = 0, pick = 0; for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached && frand() * (++n) < 1) pick = i;
    effXY(P[pick].x * 0.5f, P[pick].y * 0.866f, fxFlashX, fxFlashY); fxFlash = 0.7f + 0.3f * frand();
  }
  // Komet: der Kopf wandert zu einem Nachbarn weiter.
  // Panels mit "Kanten einzeln": alle drei Kanten nacheinander (im oder gegen den Uhrzeigersinn, je nachdem wo es
  // weitergeht), dann von der letzten Kante hinüber ins Nachbarpanel, z. B. 1-2, 1-3, 2-1, 2-2, 2-3, 3-1 …
  if (fx.id == 14) {
    if (!P[fxHead].used || !P[fxHead].attached) { fxHead = 0; fxHeadE = 0; fxPlanN = 0; }
    if (!P[fxHead].edges) { fxHeadE = 0; fxPlanN = 0; }
    fxStep += dt * rate * 4;
    while (fxStep >= 1) {
      fxStep -= 1;
      const Panel& h = P[fxHead];
      if (h.edges && fxPlanP < fxPlanN) { fxHeadE = fxPlan[fxPlanP++]; fxA[fxHead * 3 + fxHeadE] = 1; continue; }   // nächste Kante im Panel
      // hinüber: aus Kanten-Panels über die zuletzt leuchtende Kante, sonst über irgendeine
      int cj[3], ce[3], n = 0;
      for (uint8_t e = 0; e < 3; e++) {
        if (h.edges && e != fxHeadE) continue;
        Dir w = worldDir(h, e); int nx, ny; neighbor(h.x, h.y, w, nx, ny); int j = findAt(nx, ny);
        if (j < 0) continue;
        int ej = P[j].edges ? edgeFacing(j, opposite(w)) : 0; if (ej < 0) ej = 0;
        cj[n] = j; ce[n] = ej; n++;
      }
      int fr[3], m = 0; for (int k = 0; k < n; k++) if (fxA[cj[k] * 3 + ce[k]] < 0.5f) fr[m++] = k;
      int pick = m ? fr[(int)(frand() * m) % m] : n ? (int)(frand() * n) % n : -1;
      if (pick < 0) {                                           // hier geht es nicht weiter: erst die anderen Kanten
        if (h.edges) { fxPlan[0] = (fxHeadE + 1) % 3; fxPlan[1] = (fxHeadE + 2) % 3; fxPlanN = 2; fxPlanP = 0; }
        fxA[fxHead * 3 + fxHeadE] = 1; continue;
      }
      fxHead = cj[pick]; fxHeadE = ce[pick]; fxA[fxHead * 3 + fxHeadE] = 1; fxPlanN = fxPlanP = 0;
      const Panel& q = P[fxHead];
      if (q.edges) {                                             // Reihenfolge im neuen Panel festlegen
        auto nb = [&](int e) { int nx, ny; neighbor(q.x, q.y, worldDir(q, e), nx, ny); return findAt(nx, ny) >= 0; };
        int e0 = fxHeadE, fw = (e0 + 2) % 3, bw = (e0 + 1) % 3;     // vorwärts endet bei e0+2, rückwärts bei e0+1
        bool f = nb(fw), b = nb(bw), dirF = f && b ? frand() < 0.5f : f ? true : b ? false : frand() < 0.5f;
        fxPlan[0] = dirF ? (e0 + 1) % 3 : (e0 + 2) % 3; fxPlan[1] = dirF ? fw : bw; fxPlanN = 2;
        if (!f && !b) fxPlan[fxPlanN++] = e0;                       // Sackgasse: zurück zur Eingangskante
      }
    }
    float ik = 1 - fx.inten / 255.0f;                         // Intensität = Schweiflänge: 0 kurz … 255 sehr lang
    float dec = dt * rate * (0.1f + 2.4f * ik * ik);
    for (int s = 0; s < SLOTS * 3; s++) if (s != fxHead * 3 + fxHeadE) fxA[s] = fmaxf(0, fxA[s] - dec);
  }
  // Raum-Effekte: gemeinsame Uhr und Stelle im Raum
  double rT = 0;
  if (fx.id >= 24) { room::frame(); rT = room::tsec(); }
  int beat = (int)(fxPhase * 2);                  // Disco: zweimal pro Takt neue Farben
  bool newBeat = beat != fxBeat; fxBeat = beat;

  for (int i = 0; i < SLOTS; i++) {
    const Panel& p = P[i];
    if (!p.used || !p.attached || p.state == DARK) { memset(TGT[i], 0, sizeof TGT[i]); continue; }
    bool up = isUp(p.x, p.y);
    int points = p.edges ? 3 : 1;
    for (int e = 0; e < points; e++) {
      // Position des Punkts: Mitte des Panels oder Mitte der Kante e (Seitenlänge 1)
      float px, py, dd; edgePos(i, p.edges ? e : -1, px, py, dd);   // Kante zum Elternpanel liegt näher am Hauptpanel
      float c[4] = {0, 0, 0, 0};
      int s = i * 3 + e;                                       // eigener Zufallszustand pro Punkt
      float rx, ry; effXY(px, py, rx, ry);
      float u = (rx - minX) / spanX;                            // 0 links … 1 rechts (so wie die Wand hängt)
      float v = ry;
      float depth = p.depth + dd;
      const float K = fx.inten / 255.0f;                       // Intensität 0..1
      switch (fx.id) {
        case 1: pcol(fxPhase * 0.1f + u * K * 0.6f, c, D_HUE); break;
        case 2: pcol(fxPhase * 0.15f + u * (0.2f + 1.6f * K), c, D_HUE); break;
        case 3: {
          float lo = 0.02f + 0.45f * (1 - K);
          pcol(fxPhase * 0.05f + u * 0.3f, c, D_COLOR);
          mul(c, lo + (1 - lo) * (0.5f + 0.5f * cosf(fxPhase * TAU / 4 - (p.edges ? u * 0.6f : 0)))); break;
        }
        case 4: {
          fxT[s] += dt * rate / 3.5f;
          if (fxT[s] >= 1) { fxA[s] = fxB[s]; fxB[s] = fxA[s] + (frand() * 2 - 1) * (0.1f + 0.4f * K); fxT[s] = 0; }
          float d = fxB[s] - fxA[s]; d -= floorf(d + 0.5f);
          float t = fxT[s] * fxT[s] * (3 - 2 * fxT[s]);
          pcol(fxA[s] + d * t, c, D_HUE); break;
        }
        case 5: {
          fxA[s] = fmaxf(0, fxA[s] - dt * rate * 2.2f);
          if (frand() < dt * rate * (0.05f + 0.9f * K) / points) fxA[s] = 1;
          float a = fxA[s] * fxA[s];
          pcol(u * 0.5f + fxPhase * 0.03f, c, D_COLOR);
          mul(c, 0.18f + 0.5f * a); c[3] = fminf(255, c[3] + 255 * a); break;
        }
        case 6: {
          float w = 0.5f + 0.5f * cosf(TAU * (fxPhase * 0.5f - depth * 0.17f));
          pcol(fxPhase * 0.1f - depth * 0.08f, c, D_COLOR);
          mul(c, 0.05f + 0.95f * powf(w, 1 + 7 * (1 - K))); break;
        }
        case 7: {
          fxA[s] += (frand() - fxA[s]) * fminf(1, dt * rate * 9);
          fxB[s] += (fxA[s] - fxB[s]) * fminf(1, dt * rate * 5);
          float h = 1 - (0.2f + 0.8f * K) * (1 - fxB[s]);
          if (fx.pal == 0) fireColor(h, c); else { pcol(h * 0.5f, c, D_FIRE); mul(c, h); }
          break;
        }
        case 8: {
          float sv = 0.5f + 0.5f * sinf(u * 5.0f + fxPhase * 0.9f + v * 0.7f) * sinf(u * 2.3f - fxPhase * 0.55f);
          pcol(fxPhase * 0.04f + u * 0.48f, c, D_AURORA);
          mul(c, 1 - (0.3f + 0.7f * K) * (1 - sv)); break;
        }
        case 9: {                                              // Lauflicht: ein heller Streifen mit Schweif
          float d = u - (fxPhase * 0.2f - floorf(fxPhase * 0.2f)); d -= floorf(d);
          d = 1 - d;                                           // Abstand hinter dem Streifen
          pcol(fxPhase * 0.02f, c, D_COLOR);
          mul(c, 0.03f + 0.97f * expf(-d * (3 + 14 * K))); break;
        }
        case 10: {                                             // Spirale um die Mitte der Wand
          float a = atan2f(v - cyW, rx - cxW) / TAU, r = hypotf(rx - cxW, v - cyW) / maxR;
          pcol(a + fxPhase * 0.08f + r * (0.2f + 1.2f * K), c, D_HUE); break;
        }
        case 11: {                                             // Gewitter: dunkelblau, Blitze in der Nähe eines Panels
          float d2 = (rx - fxFlashX) * (rx - fxFlashX) + (v - fxFlashY) * (v - fxFlashY);
          float f = fxFlash * expf(-d2 / (1.5f + 3 * K)) * (0.7f + 0.3f * frand());
          if (fx.pal) { pcol(0.1f, c, D_COLOR); mul(c, 0.12f); } else { c[0] = 2; c[1] = 6; c[2] = 34; }
          c[0] += 170 * f; c[1] += 170 * f; c[2] += 255 * f; c[3] += 255 * f; break;
        }
        case 12: {                                             // Kerzenlicht: warm, ruhig flackernd
          fxA[s] += (frand() - fxA[s]) * fminf(1, dt * rate * 6);
          fxB[s] += (fxA[s] - fxB[s]) * fminf(1, dt * rate * 3);
          float h = 1 - (0.15f + 0.5f * K) * (1 - fxB[s]);
          if (fx.pal == 0) { c[0] = 255 * h; c[1] = 105 * h * h; c[2] = 12 * h; c[3] = 70 * h * h; }
          else { pcol(fxB[s] * 0.3f, c, D_FIRE); mul(c, h); }
          break;
        }
        case 13: {                                             // Disco: im Takt neue Farben
          if (newBeat && frand() < 0.3f + 0.7f * K) fxA[s] = frand();
          pcol(fxA[s], c, D_HUE); break;
        }
        case 14: {                                             // Komet: heller Kopf, der durch die Wand wandert
          float b = fxA[s];
          pcol(fxPhase * 0.03f, c, D_COLOR);
          mul(c, 0.02f + 0.98f * powf(b, 1.6f));
          if (i == fxHead && e == fxHeadE) c[3] = fminf(255, c[3] + 120);
          break;
        }
        case 16: {                                             // Farbverlauf: ruhig, in Effektrichtung, wandert mit dem Tempo
          float t = u * (0.6f + 1.4f * K) + (fx.speed > 1 ? fxPhase * 0.02f : 0);
          if (fx.pal == 0) {                                   // Standard: Effektfarbe → Farbe 2 → zurück
            float m = t - floorf(t); m = m < 0.5f ? m * 2 : 2 - m * 2; m = m * m * (3 - 2 * m);
            const float a1[4] = {(float)fx.r, (float)fx.g, (float)fx.b, (float)fx.w};
            for (int ch = 0; ch < 4; ch++) c[ch] = a1[ch] + (fxC2[ch] - a1[ch]) * m;
          } else pcol(t * 0.5f, c, D_COLOR);
          break;
        }
        case 15: {                                             // Lava: langsam fließende Blasen
          float sv = 0.5f + 0.5f * sinf(u * 4.0f + fxPhase * 0.35f + 1.5f * sinf(v * 1.8f - fxPhase * 0.25f));
          pcol(sv * 0.5f + fxPhase * 0.02f, c, D_FIRE);
          mul(c, 0.2f + 0.8f * powf(sv, 1 + 2 * K)); break;
        }
        case 17: {                                             // Herzschlag: doppelter Puls, läuft vom Hauptpanel nach außen
          float t = fxPhase * 0.8f - depth * (0.03f + 0.08f * K); t -= floorf(t);
          float b = expf(-(t * t) / 0.003f) + 0.65f * expf(-((t - 0.2f) * (t - 0.2f)) / 0.003f);
          pcol(fxPhase * 0.02f, c, D_COLOR);
          mul(c, 0.06f + 0.94f * fminf(1, b)); break;
        }
        case 18: {                                             // Plasma: ineinander fließende Farben
          float sv = sinf(u * (3 + 5 * K) + fxPhase * 0.6f) + sinf(v * 2.2f - fxPhase * 0.45f) + sinf((u + v) * 2.5f + fxPhase * 0.3f);
          pcol(sv * 0.17f + fxPhase * 0.03f, c, D_HUE); break;
        }
        case 19: {                                             // Regen: einzelne Tropfen blitzen auf, darunter dunkles Blau
          fxA[s] = fmaxf(0, fxA[s] - dt * rate * 3.5f);
          if (frand() < dt * rate * (0.15f + 1.6f * K) / points) fxA[s] = 1;
          float a = fxA[s] * fxA[s];
          if (fx.pal == 0) { c[0] = 4 + 40 * a; c[1] = 10 + 110 * a; c[2] = 40 + 215 * a; c[3] = 60 * a; }
          else { pcol(u * 0.4f + fxPhase * 0.02f, c, D_COLOR); mul(c, 0.12f + 0.88f * a); }
          break;
        }
        case 20: {                                             // Sternenhimmel: dunkel, einzelne Sterne funkeln langsam
          float tw = 0.5f + 0.5f * sinf(fxPhase * (0.6f + fxB[s]) + fxA[s] * TAU);
          float star = fxB[s] > 0.55f - 0.35f * K ? powf(tw, 6) : 0;
          if (fx.pal == 0) { c[0] = 2 + 60 * star; c[1] = 3 + 70 * star; c[2] = 18 + 90 * star; c[3] = 200 * star; }
          else { pcol(fxA[s], c, D_COLOR); mul(c, 0.05f + 0.95f * star); }
          break;
        }
        case 21: {                                             // Feuerwerk: Ring breitet sich von der Rakete aus und verglüht
          float d = hypotf(rx - fwX, v - fwY), r = fwT * 2.6f, life = fmaxf(0, 1 - fwT / 1.4f);
          float a = expf(-(d - r) * (d - r) / 0.25f) * life + (fwT < 0.12f && d < 0.4f ? 1 : 0);
          pcol(fwH + d * 0.08f, c, D_HUE);
          mul(c, fminf(1, a)); c[3] = fminf(255, c[3] + (fwT < 0.15f ? 255 * a : 40 * a * life)); break;
        }
        case 22: {                                             // Matrix: grüne Ströme fallen nach unten
          float off = sinf(roundf(rx * 2) * 12.9898f) * 43758.5f; off -= floorf(off);
          float d = v * 0.35f - fxPhase * 0.35f + off; d -= floorf(d);
          float b = powf(1 - d, 3 + 8 * K);
          if (fx.pal == 0) { c[0] = 0; c[1] = 20 + 235 * b; c[2] = 30 * b * b; c[3] = d > 0.96f ? 160 : 0; }
          else { pcol(off, c, D_COLOR); mul(c, 0.05f + 0.95f * b); }
          break;
        }
        case 23: {                                             // Wetter: Farbe nach Außentemperatur, dazu Regen- oder Schneetropfen
          float tc[4]; tempColor(wxOk ? wxTemp : 15, tc);
          float br = 0.55f + 0.15f * sinf(fxPhase * 0.5f - u * 2);
          for (int ch = 0; ch < 4; ch++) c[ch] = tc[ch] * br;
          if (wxOk && wxKind) {
            fxA[s] = fmaxf(0, fxA[s] - dt * rate * 2.5f);
            if (frand() < dt * rate * (0.2f + 1.2f * K) / points) fxA[s] = 1;
            float a = fxA[s] * fxA[s];
            if (wxKind == 1) { c[0] *= 1 - a; c[1] = c[1] * (1 - a) + 90 * a; c[2] = c[2] * (1 - a) + 255 * a; }
            else { c[0] += 120 * a; c[1] += 120 * a; c[2] += 140 * a; c[3] += 220 * a; }
          }
          break;
        }
        case 24: {                                             // Raum-Komet: irrt wie der Komet durch alle Wände im Raum
          float ik = 1 - K, tail = 4 / (0.1f + 2.4f * ik * ik);   // Intensität = Schweiflänge wie beim Komet
          bool hd; float b = room::comet(i, p.edges ? e : -1, tail, hd);
          pcol((float)fmod(room::stepF * 0.0075, 1.0), c, D_COLOR);
          mul(c, 0.02f + 0.98f * powf(b, 1.6f));
          if (hd) c[3] = fminf(255, c[3] + 120);
          break;
        }
        case 25: {                                             // Raum-Regenbogen: Farben ziehen durch den ganzen Raum
          float W = room::S * (3 + 25 * (1 - K));
          pcol(room::at(px, py) / W - (float)fmod(rT * rate * 0.15, 1000.0), c, D_HUE); break;
        }
        case 26: {                                             // Raum-Welle: helle Wellen laufen durch den Raum
          float W = room::S * (4 + 20 * (1 - K)), uu = room::at(px, py);
          float b = 0.5f + 0.5f * sinf(6.2831853f * (uu / W - (float)fmod(rT * rate * 0.25, 1000.0)));
          pcol(uu / (W * 4), c, D_COLOR); mul(c, 0.05f + 0.95f * b * b); break;
        }
      }
      float k = p.on ? masterK() / 255.0f : 0;
      for (int ch = 0; ch < 4; ch++) TGT[i][e][ch] = (uint8_t)fminf(255, fmaxf(0, c[ch] * k));
    }
    if (!p.edges) { memcpy(TGT[i][1], TGT[i][0], 4); memcpy(TGT[i][2], TGT[i][0], 4); }
  }
}

// ---------- Ausgabe ----------
// Alles, was die Panels zeigen, läuft hier durch: Ziel berechnen (Effekt, feste Farbe oder Pulsieren),
// Stromlimit anwenden, weich überblenden und als FRAME an alle Panels schicken (etwa 25-mal pro Sekunde,
// aber nur, wenn sich etwas ändert, sonst einmal pro Sekunde zur Sicherheit).
uint8_t FROM[SLOTS][3][4], SENT[SLOTS][3][4];
bool isPulse[SLOTS];
uint32_t transStart = 0, transDur = 0, lastKeep = 0;
bool transOn = false;
bool outForce = true, testMode = false;
float powerScale = 1, measScale = 1;

// ---------- Spiel "Simon sagt" ----------
// Die Wand zeigt eine Folge von Panels, man tippt sie nach; jede Runde ein Panel mehr.
namespace game {
enum Ph : uint8_t { G_OFF, G_SHOW, G_INPUT, G_FAIL, G_WIN };   // nicht INPUT: das ist ein Arduino-Makro
Ph ph = G_OFF;
int8_t pan[SLOTS]; uint8_t nPan = 0;      // mitspielende Panels (mit Sensor, ohne Hauptpanel)
uint8_t seq[100]; uint8_t len = 0, pos = 0, best = 0;
uint32_t t0 = 0; int8_t lit = -1; uint32_t litUntil = 0;
bool active() { return ph != G_OFF; }
uint16_t stepMs() { return len < 12 ? 650 - len * 30 : 290; }
void hueOf(uint8_t k, uint8_t* c) {
  float h = (float)k / nPan * 6; int s = (int)h; float f = h - s, q = 1 - f; float r, g, b;
  switch (s % 6) { case 0: r = 1; g = f; b = 0; break; case 1: r = q; g = 1; b = 0; break; case 2: r = 0; g = 1; b = f; break;
                   case 3: r = 0; g = q; b = 1; break; case 4: r = f; g = 0; b = 1; break; default: r = 1; g = 0; b = q; }
  c[0] = r * 255; c[1] = g * 255; c[2] = b * 255; c[3] = 0;
}
void next() { if (len < sizeof seq) seq[len++] = esp_random() % nPan; pos = 0; ph = G_SHOW; t0 = millis(); }
const char* start() {
  nPan = 0;
  for (int i = 1; i < SLOTS; i++) if (P[i].used && P[i].attached && P[i].state != DARK && (P[i].caps & 1)) pan[nPan++] = i;
  if (nPan < 2) return "Zum Spielen braucht es mindestens 2 Panels mit Sensor";
  if (!cfg.touchOn) return "Bitte zuerst unter Antippen „Panels reagieren auf Antippen“ einschalten";
  best = prefs.getUChar("simon", 0);
  len = 0; next(); t0 = millis() + 600;
  diag("Spiel gestartet");
  return nullptr;
}
void stop() { ph = G_OFF; lit = -1; outForce = true; }
void over() {
  uint8_t score = len ? len - 1 : 0;
  if (score > best) { best = score; prefs.putUChar("simon", best); }
  diag("Spiel vorbei: %u Runden geschafft", score);
  ph = G_FAIL; t0 = millis();
}
void tap(int slot) {
  if (ph != G_INPUT) return;
  int8_t k = -1; for (uint8_t j = 0; j < nPan; j++) if (pan[j] == slot) k = j;
  if (k < 0) return;
  lit = k; litUntil = millis() + 250;
  if (k != seq[pos]) { over(); return; }
  if (++pos >= len) { ph = G_WIN; t0 = millis(); }
  else t0 = millis();                                      // Zeit für den nächsten Tipp läuft neu
}
// läuft mit jedem Bild: Ablauf weiterschalten und die Wand zeichnen
void draw(uint8_t TG[][3][4]) {
  if (ph == G_OFF) return;
  uint32_t now = millis();
  int32_t t = (int32_t)(now - t0);
  if (ph == G_SHOW && t >= 0 && t / (stepMs() + 180) >= len) { ph = G_INPUT; t0 = now; }
  if (ph == G_INPUT && t > 8000) over();                      // 8 s ohne Tipp: vorbei
  if (ph == G_WIN && t > 700) { next(); t0 = now + 300; }
  if (ph == G_FAIL && t > 1800) { stop(); return; }
  t = (int32_t)(now - t0);
  for (int i = 0; i < SLOTS; i++) for (int e = 0; e < 3; e++) for (int c = 0; c < 4; c++) TG[i][e][c] = 0;
  for (uint8_t j = 0; j < nPan; j++) {
    uint8_t c[4]; hueOf(j, c); float k = 0.08f;
    if (ph == G_SHOW && t >= 0) { uint16_t st = stepMs(); int n = t / (st + 180); if (n < len && seq[n] == j && t % (st + 180) < st) k = 1; }
    if (ph == G_INPUT && lit == j && now < litUntil) k = 1;
    if (ph == G_WIN) k = 0.5f + 0.5f * ((t / 120) % 2);
    if (ph == G_FAIL) { c[0] = 255; c[1] = c[2] = c[3] = 0; k = (t / 250) % 2 ? 0.1f : 1; }
    for (int e = 0; e < 3; e++) for (int ch = 0; ch < 4; ch++) TG[pan[j]][e][ch] = c[ch] * k;
  }
  // Hauptpanel zeigt die Runde: weiß, je weiter, desto heller
  for (int e = 0; e < 3; e++) TG[0][e][3] = ph == G_FAIL ? 0 : 20 + (len > 20 ? 200 : len * 10);
}
uint8_t level() { return len ? len - (ph == G_SHOW || ph == G_WIN ? 1 : 0) : 0; }
void json(JsonObject o) { o["on"] = active(); o["level"] = level(); o["ph"] = (int)ph; o["best"] = best ? best : prefs.getUChar("simon", 0); }
}  // namespace game

// Tageslicht-Kurve nach Uhrzeit: (Stunde, Helligkeit, Wärme), dazwischen linear
const float DAYC[][3] = {{0, 0.35f, 1}, {6, 0.35f, 1}, {7.5f, 1, 0}, {18, 1, 0}, {21, 0.75f, 0.7f}, {23, 0.45f, 1}, {24, 0.35f, 1}};
void dayCurveAt(float h, float& dim, float& warm) {
  for (uint8_t k = 0; k + 1 < sizeof DAYC / sizeof DAYC[0]; k++)
    if (h >= DAYC[k][0] && h <= DAYC[k + 1][0]) {
      float f = (h - DAYC[k][0]) / (DAYC[k + 1][0] - DAYC[k][0]);
      dim = DAYC[k][1] + (DAYC[k + 1][1] - DAYC[k][1]) * f; warm = DAYC[k][2] + (DAYC[k + 1][2] - DAYC[k][2]) * f; return;
    }
  dim = 1; warm = 0;
}
void dayCurve(float& dim, float& warm) {
  time_t t = time(nullptr); struct tm lt; localtime_r(&t, &lt);
  dayCurveAt(lt.tm_hour + lt.tm_min / 60.0f, dim, warm);
}
float sleepScale = 1;                     // Sleep-Timer: blendet zum Ende hin aus
// Wellen beim Antippen: laufen vom angetippten Panel als Ring über die Wand
struct Ripple { uint32_t t0; float x, y; };
Ripple ripples[3];
const float RIPPLE_SPEED = 4.5f;          // Panels pro Sekunde
// Einschalt-Animation: Panel für Panel vom Hauptpanel nach außen
uint32_t onAnimAt = 0; bool lastMasterOn = false;
const uint16_t ON_STEP[4] = {0, 420, 210, 90}, ON_FADE[4] = {0, 600, 350, 180};   // ms je Abstand und Einblenddauer
// Signal (Benachrichtigung): die ganze Wand blinkt ein paar Mal in einer Farbe, danach läuft alles weiter
uint32_t sigAt = 0; uint8_t sigCol[4] = {0, 0, 255, 0}, sigBlinks = 3; uint16_t sigMs = 700;
// Fortschritt: die Wand füllt sich vom Hauptpanel aus (0 = aus)
float progVal = 0; uint8_t progCol[4] = {0, 255, 40, 0};
// Zeit seit t in ms; nie negativ (t wird mit "| 1" gesetzt und kann daher 1 ms in der Zukunft liegen)
uint32_t since(uint32_t t, uint32_t now) { int32_t d = (int32_t)(now - t); return d < 0 ? 0 : (uint32_t)d; }
bool overlayActive() {
  uint32_t now = millis();
  if (transOn) return true;                          // weiche Übergänge auch in der App zeigen
  for (const Ripple& r : ripples) if (r.t0 && now - r.t0 < 4000) return true;
  return (onAnimAt && now - onAnimAt < 6000) || sigAt || progVal > 0 || game::active();
}
void addRipple(int i) {
  if (!cfg.touchWave || i < 0 || i >= SLOTS) return;
  int k = 0; for (int j = 1; j < 3; j++) if (ripples[j].t0 < ripples[k].t0) k = j;
  ripples[k].t0 = millis() | 1; ripples[k].x = P[i].x * 0.5f; ripples[k].y = P[i].y * 0.866f;
  fxLastFrame = 0;
}
uint32_t estMa = 0;                       // geschätzter Strom aller LEDs und Panels in mA (bei 24 V)
const uint8_t SEG_PER_PANEL = 3;          // LED-Segmente pro Panel (eins pro Kante)
const uint16_t IDLE_PANEL_MA = 12, IDLE_MAIN_MA = 25;

// ab jetzt vom aktuellen Bild weich zum neuen Ziel überblenden
void transition() {
  memcpy(FROM, CUR, sizeof CUR);
  transDur = cfg.transMs;
  transStart = millis();
  transOn = transDur > 0;
  outForce = true;
  fxLastFrame = 0;
}

void computeTargets() {
  uint32_t now = millis();
  if (fx.id) fxCompute();
  for (int i = 0; i < SLOTS; i++) {
    const Panel& p = P[i];
    isPulse[i] = false;
    if (!p.used || !p.attached || p.state == DARK || !masterOn) { memset(TGT[i], 0, sizeof TGT[i]); continue; }
    bool joining = fx.id && p.joinAt && (int32_t)(now - p.joinAt) < 0;
    if (joining || (!fx.id && p.state == PULSE)) {               // blau pulsieren wie bisher die Panels selbst
      uint32_t t = (now - (joining ? p.joinAt - FX_JOIN_MS : p.since)) % 1600;
      t = t < 800 ? t : 1600 - t;
      uint8_t lv = (uint8_t)(6 + (t * 26) / 800);
      for (int e = 0; e < 3; e++) { TGT[i][e][0] = 0; TGT[i][e][1] = 0; TGT[i][e][2] = lv; TGT[i][e][3] = 0; }
      isPulse[i] = true;
      continue;
    }
    if (fx.id) continue;                                         // Effektbild steht schon in TGT
    uint32_t k = p.on ? (uint32_t)p.bri * master : 0;
    uint8_t c[4] = {(uint8_t)(p.r * k / 65025), (uint8_t)(p.g * k / 65025), (uint8_t)(p.b * k / 65025), (uint8_t)(p.w * k / 65025)};
    for (int e = 0; e < 3; e++) memcpy(TGT[i][e], c, 4);
  }
  // Fortschritt: Panels nach Abstand zum Hauptpanel (bei gleichem Abstand von links nach rechts) der Reihe nach füllen
  if (progVal > 0) {
    int8_t ord[SLOTS]; float key[SLOTS]; int n = 0;
    for (int i = 0; i < SLOTS; i++) {
      if (!P[i].used || !P[i].attached) continue;
      float rx, ry; viewXY(P[i].x * 0.5f, P[i].y * 0.866f, rx, ry);
      float k = P[i].depth * 1000 + rx;
      int j = n++; while (j > 0 && key[j - 1] > k) { key[j] = key[j - 1]; ord[j] = ord[j - 1]; j--; }
      key[j] = k; ord[j] = i;
    }
    float lit = progVal / 100.0f * n, kb = (masterOn ? master : 150) / 255.0f;
    for (int j = 0; j < n; j++) {
      int i = ord[j]; isPulse[i] = false;
      float a = fminf(1, fmaxf(0, lit - j));
      for (int e = 0; e < 3; e++) {
        float ae = a;
        if (P[i].edges) { int ein = -1; for (int k = 0; k < 3; k++) { float x, y, dd; edgePos(i, k, x, y, dd); if (dd < 0) ein = k; }
          int rank = e == ein ? 0 : (ein < 0 ? e : 1); if (ein >= 0 && e != ein) for (int k = 0; k < e; k++) if (k != ein) rank++;
          ae = fminf(1, fmaxf(0, a * 3 - rank)); }
        ae = 0.06f + 0.94f * ae;                                   // noch nicht erreicht: ganz schwach
        for (int c = 0; c < 4; c++) TGT[i][e][c] = (uint8_t)(progCol[c] * ae * kb);
      }
    }
  }
  // Signal: Farbe an, normales Bild, Farbe an … (auch bei ausgeschalteter Wand)
  if (sigAt) {
    uint32_t ph = since(sigAt, now) / (sigMs / 2);
    if (ph >= (uint32_t)sigBlinks * 2) sigAt = 0;
    else if (ph % 2 == 0) for (int i = 0; i < SLOTS; i++) {
      if (!P[i].used || !P[i].attached) continue;
      isPulse[i] = false;
      for (int e = 0; e < 3; e++) memcpy(TGT[i][e], sigCol, 4);
    }
  }
  // Einschalt-Animation: beim Einschalten leuchten die Panels der Reihe nach auf, nach Abstand zum Hauptpanel
  if (masterOn && !lastMasterOn && cfg.onAnim) { onAnimAt = now | 1; transOn = false; }
  lastMasterOn = masterOn;
  if (onAnimAt) {
    uint32_t t = since(onAnimAt, now); bool done = true;
    for (int i = 0; i < SLOTS; i++) {
      if (!P[i].used || !P[i].attached || isPulse[i]) continue;
      for (int e = 0; e < 3; e++) {
        float x, y, dd = 0; if (P[i].edges) edgePos(i, e, x, y, dd);
        int32_t local = (int32_t)t - (int32_t)((P[i].depth + dd + (P[i].edges ? 0.33f : 0)) * ON_STEP[cfg.onAnim]);
        float k = local <= 0 ? 0 : fminf(1, (float)local / ON_FADE[cfg.onAnim]);
        if (k < 1) { done = false; k = k * k * (3 - 2 * k); for (int c = 0; c < 4; c++) TGT[i][e][c] = (uint8_t)(TGT[i][e][c] * k); }
      }
    }
    if (done || !masterOn) onAnimAt = 0;
  }
  // Wellen beim Antippen: heller Ring, der sich ausbreitet und dabei verblasst
  for (Ripple& r : ripples) {
    if (!r.t0) continue;
    float t = since(r.t0, now) / 1000.0f, rad = t * RIPPLE_SPEED;
    if (t > 3.5f) { r.t0 = 0; continue; }
    float fade = 1 - t / 3.5f;
    for (int i = 0; i < SLOTS; i++) {
      if (!P[i].used || !P[i].attached || P[i].state == DARK) continue;
      for (int e = 0; e < 3; e++) {
        float x, y, dd; edgePos(i, P[i].edges ? e : -1, x, y, dd);
        float d = hypotf(x - r.x, y - r.y);
        float a = expf(-(d - rad) * (d - rad) / (P[i].edges ? 0.12f : 0.35f)) * fade;
        if (a < 0.02f) continue;
        TGT[i][e][0] = (uint8_t)fminf(255, TGT[i][e][0] + 90 * a); TGT[i][e][1] = (uint8_t)fminf(255, TGT[i][e][1] + 90 * a);
        TGT[i][e][2] = (uint8_t)fminf(255, TGT[i][e][2] + 110 * a); TGT[i][e][3] = (uint8_t)fminf(255, TGT[i][e][3] + 230 * a);
      }
    }
  }
  game::draw(TGT);
  // Störungsanzeige: Hauptpanel blinkt alle 10 s zweimal kurz (orange: WLAN weg, rot: ein Panel antwortet nicht mehr)
  if (cfg.faultBlink && masterOn && !game::active()) {
    bool wifiBad = !wlanOk && wSsid.length() && !apMode;
    bool panelBad = faultPanelAt && now - faultPanelAt < 120000;
    uint32_t fp = now % 10000;
    if ((wifiBad || panelBad) && (fp < 150 || (fp >= 300 && fp < 450)))
      for (int e = 0; e < 3; e++) { TGT[0][e][0] = 255; TGT[0][e][1] = panelBad ? 0 : 90; TGT[0][e][2] = 0; TGT[0][e][3] = 0; }
  }
  // Tageslicht-Kurve: abends wärmer (weniger Blau und Grün) und dunkler, tagsüber unverändert
  if (cfg.daylight && timeOk()) {
    float dim, warm; dayCurve(dim, warm);
    if (dim < 0.999f || warm > 0.001f) {
      const float k[4] = {dim, dim * (1 - 0.3f * warm), dim * (1 - 0.8f * warm), dim * (1 - 0.15f * warm)};
      for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached && !isPulse[i]) for (int e = 0; e < 3; e++) for (int c = 0; c < 4; c++) TGT[i][e][c] = (uint8_t)(TGT[i][e][c] * k[c]);
    }
  }
  // Stromlimit: Strom schätzen und bei Bedarf alles gleichmäßig dunkler machen (wie WLED)
  uint32_t led = 0, idle = 0;
  for (int i = 0; i < SLOTS; i++) {
    if (!P[i].used || !P[i].attached) continue;
    idle += i == 0 ? IDLE_MAIN_MA : IDLE_PANEL_MA;
    for (int e = 0; e < 3; e++) led += (uint32_t)(TGT[i][e][0] + TGT[i][e][1] + TGT[i][e][2] + TGT[i][e][3]) * cfg.pwrCh / 255;
  }
  powerScale = 1;
  if (cfg.pwrMax && led) {
    float avail = (float)cfg.pwrMax - idle;
    powerScale = avail <= 0 ? 0 : fminf(1, avail / led);
  }
  float sc = powerScale * measScale * sleepScale;
  estMa = idle + (uint32_t)(led * sc);
  if (sc < 0.999f) for (int i = 0; i < SLOTS; i++) if (!isPulse[i]) for (int e = 0; e < 3; e++) for (int c = 0; c < 4; c++) TGT[i][e][c] = (uint8_t)(TGT[i][e][c] * sc);
  // Panel finden: blinkt dreimal pro Sekunde weiß, auch wenn die Wand aus ist
  for (int i = 0; i < SLOTS; i++) {
    if (!P[i].identUntil) continue;
    if ((int32_t)(now - P[i].identUntil) >= 0 || !P[i].attached) { P[i].identUntil = 0; continue; }
    bool lit = (P[i].identUntil - now) % 333 > 166;
    for (int e = 0; e < 3; e++) { TGT[i][e][0] = TGT[i][e][1] = TGT[i][e][2] = lit ? 90 : 0; TGT[i][e][3] = lit ? 200 : 0; }
  }
}

// FRAME: eine Farbe pro Panel; FRAME3: drei Farben pro Panel (ab Panel-Firmware 2).
// Panels mit Kanten einzeln und alter Firmware bekommen ihre drei Farben zusätzlich per EDGES.
void sendOutput(bool force) {
  bool mainChanged = force || memcmp(SENT[0], CUR[0], sizeof CUR[0]);
  if (cfg.pins.led >= 0 && mainChanged) {
    for (int k = 0; k < 3; k++) strip.setPixelColor(k, strip.Color(CUR[0][k][0], CUR[0][k][1], CUR[0][k][2], CUR[0][k][3]));   // Pixel k = Kante k
    strip.show();
  }
  memcpy(SENT[0], CUR[0], sizeof CUR[0]);
  if (!cfg.bus) { memcpy(SENT, CUR, sizeof CUR); return; }
  bool changed = force, anyEdges = false, allNew = true;
  uint8_t lo = 0xFF, hi = 0;
  for (int i = 1; i < SLOTS; i++) {
    const Panel& p = P[i];
    if (!p.used || !p.attached || !p.addr) continue;
    if (p.addr < lo) lo = p.addr; if (p.addr > hi) hi = p.addr;
    if (memcmp(SENT[i], CUR[i], sizeof CUR[i])) changed = true;
    if (p.edges) anyEdges = true;
    if (p.fw < 2) allNew = false;
  }
  if (lo > hi || !changed) return;
  if (anyEdges && allNew) {
    for (uint8_t first = lo; first <= hi; first += 16) {        // 16 Panels × 12 Byte pro Rahmen
      uint8_t n = (hi - first + 1) > 16 ? 16 : (hi - first + 1);
      uint8_t d[2 + 16 * 12] = {first, n};
      for (int i = 1; i < SLOTS; i++) {
        const Panel& p = P[i];
        if (!p.used || !p.attached || p.addr < first || p.addr >= first + n) continue;
        memcpy(d + 2 + 12 * (p.addr - first), CUR[i], 12);
      }
      bus::send(bus::ALL, bus::C_FRAME3, d, 2 + 12 * n);
      busFramesSent++;
    }
  } else {
    for (uint8_t first = lo; first <= hi; first += 48) {        // höchstens 48 Panels pro Rahmen (Datenlänge bis 200 Byte)
      uint8_t n = (hi - first + 1) > 48 ? 48 : (hi - first + 1);
      uint8_t d[2 + 48 * 4] = {first, n};
      for (int i = 1; i < SLOTS; i++) {
        const Panel& p = P[i];
        if (!p.used || !p.attached || p.addr < first || p.addr >= first + n) continue;
        memcpy(d + 2 + 4 * (p.addr - first), CUR[i][0], 4);
      }
      bus::send(bus::ALL, bus::C_FRAME, d, 2 + 4 * n);
      busFramesSent++;
    }
    for (int i = 1; i < SLOTS; i++) {                            // Kanten einzeln mit alter Panel-Firmware
      const Panel& p = P[i];
      if (p.used && p.attached && p.addr && p.edges && (force || memcmp(SENT[i], CUR[i], sizeof CUR[i]))) bus::send(p.addr, bus::C_EDGES, &CUR[i][0][0], 12);
    }
  }
  memcpy(SENT, CUR, sizeof CUR);
}

void outLoop() {
  uint32_t now = millis();
  if (testMode || now - fxLastFrame < FX_FRAME_MS) return;
  fxLastFrame = now;
  computeTargets();
  float e = 1;
  if (transOn) {
    e = (float)(uint32_t)(now - transStart) / transDur;
    if (e >= 1) { e = 1; transOn = false; } else e = e * e * (3 - 2 * e);
  }
  for (int i = 0; i < SLOTS; i++) for (int k = 0; k < 3; k++) for (int c = 0; c < 4; c++)
    CUR[i][k][c] = e >= 1 ? TGT[i][k][c] : (uint8_t)(FROM[i][k][c] + (TGT[i][k][c] - FROM[i][k][c]) * e + 0.5f);
  bool keep = now - lastKeep > 1000;               // einmal pro Sekunde alles neu schicken, falls ein Panel neu gestartet ist
  if (keep) lastKeep = now;
  sendOutput(outForce || keep);
  outForce = false;
}

// ---------- Energieverbrauch (gemessen oder geschätzt) ----------
// Wh pro Tag (31), Monat (24) und Jahr (10), dazu gesamt. Datum per NTP, Zeitzone Österreich.
EnergyLog en;
double enPending = 0;                     // Wh, solange die Uhrzeit noch unbekannt ist
bool enDirty = false;

// ---------- Stromsensor INA226 (optional, I²C) ----------
namespace ina {
const uint8_t ADDR = 0x40;
bool ok = false;
float volts = 0, amps = 0;
uint32_t last = 0;
bool wr(uint8_t reg, uint16_t v) { Wire.beginTransmission(ADDR); Wire.write(reg); Wire.write(v >> 8); Wire.write(v & 0xFF); return Wire.endTransmission() == 0; }
bool rd(uint8_t reg, uint16_t& v) {
  Wire.beginTransmission(ADDR); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ADDR, 2) != 2) return false;
  v = (uint16_t)(Wire.read() << 8); v |= Wire.read(); return true;
}
bool started = false;
void begin() {
  ok = false; amps = volts = 0;
  if (started) { Wire.end(); started = false; }
  if (cfg.i2cSda < 0 || cfg.i2cScl < 0) return;
  const int8_t used[6] = {cfg.pins.rx, cfg.pins.tx, cfg.pins.de, cfg.pins.led, cfg.pins.snsR, cfg.pins.snsL};
  for (int8_t u : used) if (u == cfg.i2cSda || u == cfg.i2cScl) { logf("[STROM] Sensor-Pin %d ist schon belegt\n", u); return; }
  Wire.begin(cfg.i2cSda, cfg.i2cScl, 400000);
  started = true;
  uint16_t id = 0;
  ok = rd(0xFE, id) && id == 0x5449 && wr(0x00, 0x4527);    // Kennung "TI", 16-fach gemittelt, laufend messen
  logf("[STROM] INA226 %s (SDA %d, SCL %d)\n", ok ? "gefunden" : "nicht gefunden", cfg.i2cSda, cfg.i2cScl);
}
void loop() {
  if (!ok || millis() - last < 250) return;
  last = millis();
  uint16_t sv, bv;
  if (!rd(0x01, sv) || !rd(0x02, bv)) return;
  amps = (int16_t)sv * 2.5f / (cfg.shuntUo * 0.1f) / 1000.0f;   // µV / mΩ = mA
  volts = bv * 1.25e-3f;
  // Regelung nach Messung: liegt der echte Strom über dem Limit, dunkler; sonst langsam zurück
  if (cfg.pwrMax) {
    float lim = cfg.pwrMax / 1000.0f;
    if (amps > lim * 1.03f) measScale = fmaxf(0.1f, measScale * 0.92f);
    else if (amps < lim * 0.95f && measScale < 1) measScale = fminf(1, measScale * 1.03f);
  } else measScale = 1;
}
}  // namespace ina

void sendToPanel(int i);
void fxStart(int id) {
  if (id < 0 || id >= FX_COUNT) return;
  bool was = fx.id;
  fx.id = id;
  fxDirty = true; fxDirtyAt = millis();
  if (id && id != was) { fxReset(); fxPhase = 0; }   // jeder Effekt beginnt von vorn, nicht mitten in einer dunklen Phase
  fxLastFrame = 0;                                      // nächstes Bild sofort, nicht erst nach 40 ms
  transition();                                         // weich vom alten zum neuen Bild
  logf("[FX] %s\n", FX[fx.id].name);
  publishFx();
}

void fxSave() {
  prefs.putBytes("fx2", &fx, sizeof fx);
  if (prefs.getUShort("fxDir", 0) != fxDir) prefs.putUShort("fxDir", fxDir);
  if (prefs.getUChar("fxSpin", 0) != fxSpin) prefs.putUChar("fxSpin", fxSpin);
  { uint8_t o[4] = {0, 80, 255, 0}; prefs.getBytes("fxC2", o, 4); if (memcmp(o, fxC2, 4)) prefs.putBytes("fxC2", fxC2, 4); }
  if (prefs.getUChar("master", 255) != master) prefs.putUChar("master", master);
  if (prefs.getBool("mOn", true) != masterOn) prefs.putBool("mOn", masterOn);
  fxDirty = false;
}
void fxLoad() {
  cpalLoad();
  FxCfg f;
  if (prefs.getBytes("fx2", &f, sizeof f) == sizeof f && f.id < FX_COUNT && f.pal <= PAL_X) { fx = f; if (!palValid(fx.pal)) fx.pal = 0; }
  master = prefs.getUChar("master", 255); masterOn = prefs.getBool("mOn", true);
  fxDir = prefs.getUShort("fxDir", 0) % 360; fxSpin = prefs.getUChar("fxSpin", 0) % 3;
  prefs.getBytes("fxC2", fxC2, 4);
  fxFavs = prefs.getUInt("favs", 0);
  fxReset();
}

// Befehl {"effect":"regenbogen","speed":50,"intensity":128,"palette":"ozean","brightness":180,"color":{...}}
void resendAll();
void applyFx(JsonVariantConst cmd) {
  curPreset = -1;
  if (cmd["speed"].is<int>()) fx.speed = constrain(cmd["speed"].as<int>(), 1, 100);
  if (cmd["intensity"].is<int>()) fx.inten = constrain(cmd["intensity"].as<int>(), 0, 255);
  if (!cmd["color2"].isNull()) { JsonVariantConst c2 = cmd["color2"]; fxC2[0] = c2["r"] | fxC2[0]; fxC2[1] = c2["g"] | fxC2[1]; fxC2[2] = c2["b"] | fxC2[2]; fxC2[3] = c2["w"] | fxC2[3]; }
  if (cmd["direction"].is<int>()) fxDir = ((cmd["direction"].as<int>() % 360) + 360) % 360;
  if (cmd["spin"].is<int>()) { fxSpin = constrain(cmd["spin"].as<int>(), 0, 2); if (!fxSpin) fxSpinAng = 0; }
  if (cmd["brightness"].is<int>()) { master = constrain(cmd["brightness"].as<int>(), 1, 255); resendAll(); }
  if (cmd["palColors"].is<JsonArrayConst>()) {           // Farben von einer anderen Wand: genau diese verwenden
    uint32_t c[6]; uint8_t n = 0;
    for (JsonVariantConst v : cmd["palColors"].as<JsonArrayConst>()) if (n < 6) c[n++] = v.as<uint32_t>();
    palAdopt(n, c, cmd["palName"] | "");
  } else if (cmd["palette"].is<const char*>()) { int p = palFind(cmd["palette"].as<const char*>()); if (p >= 0) fx.pal = p; }
  else if (cmd["palette"].is<int>()) { int p = cmd["palette"].as<int>(); if (p >= 0 && p < PAL_ALL && palValid(p)) fx.pal = p; }
  JsonVariantConst c = cmd["color"];
  if (!c.isNull()) { fx.r = c["r"] | fx.r; fx.g = c["g"] | fx.g; fx.b = c["b"] | fx.b; fx.w = c["w"] | fx.w; }
  int id = fx.id;
  if (cmd["effect"].is<const char*>()) id = fxFind(cmd["effect"].as<const char*>());
  else if (cmd["effect"].is<int>()) id = cmd["effect"].as<int>();
  if (id >= 0 && id != fx.id) fxStart(id);
  else { fxDirty = true; fxDirtyAt = millis(); fxLastFrame = 0; publishFx(); }
}

// ---------- Ausgabe an ein Panel ----------
// Panel i hat sich geändert: weich zum neuen Zustand überblenden (die Ausgabe schickt es mit dem nächsten Bild)
void sendToPanel(int i) { (void)i; transition(); }

// ---------- Farbtemperatur ----------
// Kelvin (1500 bis 10000) in RGBW: Schwarzkörper-Farbe, der gemeinsame Weißanteil geht auf die weiße LED
void kelvinRgbw(int k, uint8_t* o) {
  float t = constrain(k, 1500, 10000) / 100.0f, r, g, b;
  r = t <= 66 ? 255 : 329.698727446f * powf(t - 60, -0.1332047592f);
  g = t <= 66 ? 99.4708025861f * logf(t) - 161.1195681661f : 288.1221695283f * powf(t - 60, -0.0755148492f);
  b = t >= 66 ? 255 : (t <= 19 ? 0 : 138.5177312231f * logf(t - 10) - 305.0447927307f);
  r = fminf(255, fmaxf(0, r)); g = fminf(255, fmaxf(0, g)); b = fminf(255, fmaxf(0, b));
  float w = fminf(r, fminf(g, b));
  o[0] = r - w; o[1] = g - w; o[2] = b - w; o[3] = w;
}
// "kelvin" (App) oder "color_temp" (Home Assistant, in Kelvin) wird zu einer normalen Farbe
void kelvinToColor(JsonDocument& d) {
  int k = d["kelvin"] | (int)(d["color_temp"] | 0);
  if (k < 1000) return;
  uint8_t c[4]; kelvinRgbw(k, c);
  JsonObject o = d["color"].to<JsonObject>(); o["r"] = c[0]; o["g"] = c[1]; o["b"] = c[2]; o["w"] = c[3];
  d.remove("kelvin"); d.remove("color_temp");
}

// ---------- MQTT / Home Assistant ----------
String tBase(int i) { return "trilumag/" + hex(P[i].chip); }

void publishState(int i) {
  if (!mqtt.connected()) return;
  const Panel& p = P[i];
  JsonDocument d;
  d["state"] = (masterOn && p.on && p.state != DARK) ? "ON" : "OFF";
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
  { JsonArray cm = d["supported_color_modes"].to<JsonArray>(); cm.add("rgbw"); cm.add("color_temp"); }
  d["color_temp_kelvin"] = true; d["min_kelvin"] = 2200; d["max_kelvin"] = 6500;
  JsonArray av = d["availability"].to<JsonArray>();
  av.add<JsonObject>()["topic"] = "trilumag/bridge/avail";
  av.add<JsonObject>()["topic"] = tBase(i) + "/avail";
  d["availability_mode"] = "all";
  JsonObject dev = d["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add("trilumag_" + hex(P[0].chip));
  dev["name"] = cfg.name;
  dev["manufacturer"] = "DIY";
  dev["model"] = "Panel v0.1";
  dev["sw_version"] = FW_VERSION;
  char buf[1400]; size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish(("homeassistant/light/trilumag_" + id + "/config").c_str(), (const uint8_t*)buf, n, true);
}

void publishAllLight() {
  JsonDocument d;
  d["name"] = "Alle Panels";
  d["unique_id"] = "trilumag_alle_" + hex(P[0].chip);
  d["schema"] = "json";
  d["command_topic"] = "trilumag/alle/set";
  d["brightness"] = true;
  { JsonArray cm = d["supported_color_modes"].to<JsonArray>(); cm.add("rgbw"); cm.add("color_temp"); }
  d["color_temp_kelvin"] = true; d["min_kelvin"] = 2200; d["max_kelvin"] = 6500;
  d["state_topic"] = "trilumag/alle/state";
  d["effect"] = true;
  JsonArray el = d["effect_list"].to<JsonArray>();
  for (uint8_t k = 0; k < FX_COUNT; k++) el.add(FX[k].name);
  d["availability_topic"] = "trilumag/bridge/avail";
  JsonObject dev = d["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add("trilumag_" + hex(P[0].chip));
  dev["name"] = cfg.name;
  char buf[1400]; size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish(("homeassistant/light/trilumag_alle_" + hex(P[0].chip) + "/config").c_str(), (const uint8_t*)buf, n, true);

  publishExtras();
}

// Gerät, zu dem alle Einträge in Home Assistant gehören
void haDevice(JsonDocument& d) {
  d["availability_topic"] = "trilumag/bridge/avail";
  JsonObject dev = d["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add("trilumag_" + hex(P[0].chip));
  dev["name"] = cfg.name;
}
void haPublish(const char* comp, const char* key, JsonDocument& d) {
  char buf[1400]; size_t n = serializeJson(d, buf, sizeof buf);
  String t = String("homeassistant/") + comp + "/trilumag_" + key + "_" + hex(P[0].chip) + "/config";
  mqtt.publish(t.c_str(), (const uint8_t*)buf, n, true);
}

// Tempo, Intensität, Palette und Presets als eigene Einträge in Home Assistant
void publishPresetEntity();
void publishExtras() {
  { JsonDocument t;                    // zeigt „Update läuft“, solange die Wand eine neue Firmware installiert
    t["name"] = "Status"; t["unique_id"] = "trilumag_status_" + hex(P[0].chip);
    t["state_topic"] = "trilumag/bridge/status"; t["icon"] = "mdi:update"; t["entity_category"] = "diagnostic";
    haDevice(t); t.remove("availability_topic"); haPublish("sensor", "status", t); }
  { JsonDocument t;
    t["name"] = "Effekt-Tempo"; t["unique_id"] = "trilumag_tempo_" + hex(P[0].chip);
    t["command_topic"] = "trilumag/tempo/set"; t["state_topic"] = "trilumag/tempo/state";
    t["min"] = 1; t["max"] = 100; t["step"] = 1; t["mode"] = "slider"; t["icon"] = "mdi:speedometer";
    haDevice(t); haPublish("number", "tempo", t); }
  { JsonDocument t;
    t["name"] = "Effekt-Intensität"; t["unique_id"] = "trilumag_intensitaet_" + hex(P[0].chip);
    t["command_topic"] = "trilumag/intensitaet/set"; t["state_topic"] = "trilumag/intensitaet/state";
    t["min"] = 0; t["max"] = 255; t["step"] = 1; t["mode"] = "slider"; t["icon"] = "mdi:tune-variant";
    haDevice(t); haPublish("number", "intensitaet", t); }
  { JsonDocument t;
    t["name"] = "Palette"; t["unique_id"] = "trilumag_palette_" + hex(P[0].chip);
    t["command_topic"] = "trilumag/palette/set"; t["state_topic"] = "trilumag/palette/state";
    JsonArray o = t["options"].to<JsonArray>();
    for (uint8_t k = 0; k < PAL_ALL; k++) if (palValid(k)) o.add(palName(k));
    t["icon"] = "mdi:palette";
    haDevice(t); haPublish("select", "palette", t); }
  publishPresetEntity();
  { JsonDocument t;                                 // Antippen als Ereignis für Automationen
    t["name"] = "Antippen"; t["unique_id"] = "trilumag_touch_" + hex(P[0].chip);
    t["state_topic"] = "trilumag/touch"; t["icon"] = "mdi:gesture-tap";
    JsonArray et = t["event_types"].to<JsonArray>(); et.add("einmal"); et.add("doppelt");
    haDevice(t); haPublish("event", "touch", t); }
  { JsonDocument t;                                 // Signal: notify.send_message mit "blau 3"
    t["name"] = "Signal"; t["unique_id"] = "trilumag_signal_" + hex(P[0].chip);
    t["command_topic"] = "trilumag/signal/set"; t["icon"] = "mdi:bell-ring";
    haDevice(t); haPublish("notify", "signal", t); }
  { JsonDocument t;                                 // Fortschritt 0..100 % (0 = aus)
    t["name"] = "Fortschritt"; t["unique_id"] = "trilumag_fortschritt_" + hex(P[0].chip);
    t["command_topic"] = "trilumag/fortschritt/set"; t["state_topic"] = "trilumag/fortschritt/state";
    t["min"] = 0; t["max"] = 100; t["step"] = 1; t["unit_of_measurement"] = "%"; t["mode"] = "slider"; t["icon"] = "mdi:progress-helper";
    haDevice(t); haPublish("number", "fortschritt", t); }
  { JsonDocument t;                                 // Zähler für das Energie-Dashboard
    t["name"] = "Energie"; t["unique_id"] = "trilumag_energie_" + hex(P[0].chip);
    t["state_topic"] = "trilumag/energie/state"; t["device_class"] = "energy"; t["unit_of_measurement"] = "kWh";
    t["state_class"] = "total_increasing"; t["suggested_display_precision"] = 2;
    haDevice(t); haPublish("sensor", "energie", t); }
  // Strom und Leistung (gemessen mit INA226, sonst geschätzt)
  const char* keys[3] = {"strom", "leistung", "spannung"};
  const char* names[3] = {"Strom", "Leistung", "Spannung"};
  const char* cls[3] = {"current", "power", "voltage"};
  const char* units[3] = {"A", "W", "V"};
  const char* tpl[3] = {"{{ value_json.a }}", "{{ value_json.w }}", "{{ value_json.v }}"};
  for (int k = 0; k < 3; k++) {
    String topic = String("homeassistant/sensor/trilumag_") + keys[k] + "_" + hex(P[0].chip) + "/config";
    if (k == 2 && !ina::ok) { mqtt.publish(topic.c_str(), "", true); continue; }   // Spannung nur mit Sensor
    JsonDocument t;
    t["name"] = String(names[k]) + (ina::ok ? "" : " (geschätzt)");
    t["unique_id"] = String("trilumag_") + keys[k] + "_" + hex(P[0].chip);
    t["state_topic"] = "trilumag/strom/state"; t["value_template"] = tpl[k];
    t["device_class"] = cls[k]; t["unit_of_measurement"] = units[k]; t["state_class"] = "measurement";
    haDevice(t); haPublish("sensor", keys[k], t);
  }
}

void publishPower() {
  if (!mqtt.connected()) return;
  float a = ina::ok ? ina::amps : estMa / 1000.0f;
  float v = ina::ok ? ina::volts : 24.0f;
  char b[96]; snprintf(b, sizeof b, "{\"a\":%.2f,\"w\":%.1f,\"v\":%.2f}", a, a * v, v);
  mqtt.publish("trilumag/strom/state", b, true);
  snprintf(b, sizeof b, "%.3f", en.total / 1000.0);
  mqtt.publish("trilumag/energie/state", b, true);
}

// Zustand von "Alle Panels", Tempo, Intensität, Palette und Preset
void publishPresetState();
void publishFx() {
  if (!mqtt.connected()) return;
  bool any = false;
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached && P[i].on && P[i].state != DARK) any = true;
  JsonDocument d;
  d["state"] = (masterOn && any) ? "ON" : "OFF";
  d["effect"] = FX[fx.id].name;
  d["brightness"] = master;
  d["color_mode"] = "rgbw";
  JsonObject c = d["color"].to<JsonObject>();
  if (fx.id) { c["r"] = fx.r; c["g"] = fx.g; c["b"] = fx.b; c["w"] = fx.w; }
  else { c["r"] = P[0].r; c["g"] = P[0].g; c["b"] = P[0].b; c["w"] = P[0].w; }
  char buf[200]; size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish("trilumag/alle/state", (const uint8_t*)buf, n, true);
  mqtt.publish("trilumag/tempo/state", String(fx.speed).c_str(), true);
  mqtt.publish("trilumag/intensitaet/state", String(fx.inten).c_str(), true);
  mqtt.publish("trilumag/palette/state", palName(fx.pal).c_str(), true);
  publishPresetState();
}

// ---------- Presets (gespeicherte Szenen) ----------
// Ein Preset merkt sich Ein/Aus, Gesamthelligkeit, Effekt mit allen Einstellungen
// und die feste Farbe jedes bekannten Panels (über die Chip-ID).
const uint8_t PRESET_MAX = 16;
String presetNames[PRESET_MAX];
uint32_t presetsVer = 1;             // ändert sich bei jedem Speichern/Löschen (für die WLED-Schnittstelle)

String presetKey(uint8_t k) { return "ps" + String(k); }
void presetsLoad() {
  for (uint8_t k = 0; k < PRESET_MAX; k++) {
    presetNames[k] = "";
    String j = prefs.getString(presetKey(k).c_str(), "");
    if (!j.length()) continue;
    JsonDocument d;
    if (!deserializeJson(d, j)) presetNames[k] = (const char*)(d["n"] | "Preset");
  }
}
int presetFind(const char* name) {
  for (uint8_t k = 0; k < PRESET_MAX; k++) if (presetNames[k].length() && presetNames[k] == name) return k;
  return -1;
}

void publishPresetEntity() {
  if (!mqtt.connected()) return;
  String topic = "homeassistant/select/trilumag_szene_" + hex(P[0].chip) + "/config";
  JsonDocument t;
  JsonArray o = t["options"].to<JsonArray>();
  for (uint8_t k = 0; k < PRESET_MAX; k++) if (presetNames[k].length()) o.add(presetNames[k]);
  if (!o.size()) { mqtt.publish(topic.c_str(), "", true); return; }   // ohne Presets kein Eintrag
  t["name"] = "Preset"; t["unique_id"] = "trilumag_szene_" + hex(P[0].chip);
  t["command_topic"] = "trilumag/szene/set"; t["state_topic"] = "trilumag/szene/state";
  t["icon"] = "mdi:palette-swatch-variant";
  haDevice(t); haPublish("select", "szene", t);
}
void publishPresetState() {
  if (!mqtt.connected()) return;
  mqtt.publish("trilumag/szene/state", curPreset >= 0 ? presetNames[curPreset].c_str() : "None", true);
}

int presetSave(int slot, const char* name) {
  if (slot < 0 || slot >= PRESET_MAX) {
    slot = presetFind(name);                                   // gleicher Name: überschreiben
    for (uint8_t k = 0; slot < 0 && k < PRESET_MAX; k++) if (!presetNames[k].length()) slot = k;
  }
  if (slot < 0) return -1;
  JsonDocument d;
  d["n"] = name;
  d["m"] = master; d["on"] = masterOn;
  JsonArray f = d["fx"].to<JsonArray>();
  f.add(fx.id); f.add(fx.speed); f.add(fx.pal); f.add(fx.inten); f.add(fx.r); f.add(fx.g); f.add(fx.b); f.add(fx.w); f.add(fxDir); f.add(fxSpin); f.add(fxC2[0]); f.add(fxC2[1]); f.add(fxC2[2]); f.add(fxC2[3]);
  JsonObject c = d["c"].to<JsonObject>();
  for (int i = 0; i < SLOTS; i++) {
    const Panel& p = P[i];
    if (!p.used || !p.hasColor) continue;
    JsonArray a = c[hex(p.chip)].to<JsonArray>();
    a.add(p.r); a.add(p.g); a.add(p.b); a.add(p.w); a.add(p.bri); a.add((int)p.on);
  }
  String out; serializeJson(d, out);
  prefs.putString(presetKey(slot).c_str(), out);
  presetNames[slot] = name;
  presetsVer++;
  curPreset = slot;
  logf("[PRESET] '%s' in Platz %d gespeichert\n", name, slot + 1);
  publishPresetEntity(); publishPresetState();
  return slot;
}

bool presetLoad(int k) {
  if (k < 0 || k >= PRESET_MAX || !presetNames[k].length()) return false;
  JsonDocument d;
  if (deserializeJson(d, prefs.getString(presetKey(k).c_str(), ""))) return false;
  master = d["m"] | master; masterOn = d["on"] | true;
  for (JsonPair kv : d["c"].as<JsonObject>()) {
    uint32_t chip = parseHex(kv.key().c_str());
    JsonArray a = kv.value();
    if (a.size() < 6) continue;
    int i = findChip(chip);
    if (i >= 0) {
      Panel& p = P[i];
      p.r = a[0]; p.g = a[1]; p.b = a[2]; p.w = a[3]; p.bri = a[4]; p.on = a[5].as<int>();
      p.hasColor = true;
      if (p.attached && p.state != DARK) p.state = ACTIVE;
    } else {
      uint8_t c[6] = {a[0], a[1], a[2], a[3], a[4], a[5]};       // Panel gerade nicht da: Farbe für später merken
      prefs.putBytes(("c" + hex(chip)).c_str(), c, 6);
    }
  }
  colorsDirty = true; colorsDirtyAt = millis();
  JsonArray f = d["fx"];
  if (f.size() >= 8) {
    fx.speed = f[1]; fx.pal = palValid((uint8_t)f[2]) ? (uint8_t)f[2] : 0; fx.inten = f[3];
    fx.r = f[4]; fx.g = f[5]; fx.b = f[6]; fx.w = f[7];
    if (f.size() >= 10) { fxDir = (uint16_t)f[8] % 360; fxSpin = (uint8_t)f[9] % 3; }
    if (f.size() >= 14) for (int k = 0; k < 4; k++) fxC2[k] = f[10 + k];
    uint8_t id = f[0];
    if (id < FX_COUNT && id != fx.id) fxStart(id);
  }
  resendAll();
  curPreset = k;
  logf("[PRESET] '%s' geladen\n", presetNames[k].c_str());
  publishFx();
  return true;
}

void presetDelete(int k) {
  if (k < 0 || k >= PRESET_MAX) return;
  presetsVer++;
  prefs.remove(presetKey(k).c_str());
  presetNames[k] = "";
  if (curPreset == k) curPreset = -1;
  publishPresetEntity(); publishPresetState();
}

// ---------- Farbe setzen (gemeinsam für App, API, MQTT) ----------
void resendAll() {
  transition();
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) publishState(i);
  fxDirty = true; fxDirtyAt = millis();
}

// Ein einzelnes Panel (App: ausgewählte Panels, Home Assistant: das Licht des Panels)
void applyCommand(int i, JsonVariantConst cmd) {
  Panel& p = P[i];
  if (!p.used || !p.attached) return;
  curPreset = -1;
  const char* st = cmd["state"] | "";
  if (!strcmp(st, "OFF")) p.on = false;
  if (!strcmp(st, "ON")) {
    p.on = true;
    if (!masterOn) { masterOn = true; resendAll(); publishFx(); }   // ein Panel einschalten weckt die Wand
  }
  if (cmd["brightness"].is<int>()) p.bri = constrain(cmd["brightness"].as<int>(), 0, 255);
  if (cmd["edges"].is<bool>()) {                       // Kanten einzeln für dieses Panel
    p.edges = cmd["edges"].as<bool>();
    String k = "e" + hex(p.chip);
    if (p.edges) prefs.putBool(k.c_str(), true); else prefs.remove(k.c_str());
    if (cmd["color"].isNull() && cmd["state"].isNull()) { outForce = true; return; }
  }
  JsonVariantConst c = cmd["color"];
  if (!c.isNull()) {
    p.r = c["r"] | p.r; p.g = c["g"] | p.g; p.b = c["b"] | p.b; p.w = c["w"] | p.w;
    p.hasColor = true;
  }
  if (!p.hasColor && p.on) { p.w = 255; p.hasColor = true; }   // nur "Ein" ohne Farbe: weiß
  if (p.state != DARK) p.state = ACTIVE;                         // erste Farbzuordnung beendet das Pulsieren
  colorsDirty = true; colorsDirtyAt = millis();
  if (fx.id && !c.isNull()) fxStart(0);   // eine feste Farbe für ein Panel beendet den Effekt
  sendToPanel(i);
  publishState(i);
}

void sleepCancel();
extern bool sleepFiring;
// Die ganze Wand (App oben, Home Assistant "Alle Panels"), wie bei WLED:
// state = Ein/Aus der Wand, brightness = Gesamthelligkeit, color = Effektfarbe bzw. Farbe aller Panels
void applyAll(JsonVariantConst cmd) {
  curPreset = -1;
  const char* st = cmd["state"] | "";
  bool wake = false;
  if (!strcmp(st, "OFF") && masterOn) { masterOn = false; wake = true; }
  if (!strcmp(st, "ON") && !masterOn) { masterOn = true; wake = true; }
  if (*st && !sleepFiring) sleepCancel();          // Ein/Aus von Hand beendet den Sleep-Timer
  if (cmd["brightness"].is<int>()) { master = constrain(cmd["brightness"].as<int>(), 1, 255); wake = true; }
  if (!cmd["effect"].isNull() || !cmd["speed"].isNull() || !cmd["palette"].isNull() || !cmd["palColors"].isNull() || !cmd["intensity"].isNull() || !cmd["direction"].isNull() || !cmd["spin"].isNull() || !cmd["color2"].isNull()) {
    JsonDocument d; d.set(cmd); d.remove("color"); d.remove("brightness");
    applyFx(d.as<JsonVariantConst>());
  }
  JsonVariantConst c = cmd["color"];
  if (!c.isNull()) {
    if (fxUsesColor()) {
      fx.r = c["r"] | fx.r; fx.g = c["g"] | fx.g; fx.b = c["b"] | fx.b; fx.w = c["w"] | fx.w;
      fxDirty = true; fxDirtyAt = millis(); fxLastFrame = 0;
    } else {
      if (fx.id) fxStart(0);                // Effekt ohne Farbe: auf Einfarbig wechseln
      for (int i = 0; i < SLOTS; i++) {
        Panel& p = P[i];
        if (!p.used || !p.attached) continue;
        p.r = c["r"] | p.r; p.g = c["g"] | p.g; p.b = c["b"] | p.b; p.w = c["w"] | p.w;
        p.hasColor = true; p.on = true;
        if (p.state != DARK) p.state = ACTIVE;
      }
      colorsDirty = true; colorsDirtyAt = millis();
      wake = true;
    }
  }
  if (wake) resendAll();
  publishFx();
}

// ---------- Antippen (Beschleunigungssensor in den Panels) ----------
enum TapAction : uint8_t { TA_NONE, TA_PANEL, TA_WALL, TA_PRESET, TA_EFFECT, TA_COUNT };
const char* const TAP_NAMES[TA_COUNT] = {"nichts", "Panel ein/aus", "Wand ein/aus", "nächstes Preset", "nächster Effekt"};

// kind: 1 = einmal, 2 = doppelt angetippt
void touchEvent(int i, uint8_t kind) {
  if (i < 0 || i >= SLOTS || !P[i].used || !P[i].attached || kind < 1 || kind > 2) return;
  if (game::active()) { game::tap(i); wsKick(); return; }    // im Spiel zählt nur, welches Panel
  String id = hex(P[i].chip);
  uint8_t a = kind == 2 ? cfg.tapA2 : cfg.tapA1;
  addRipple(i);
  diag("%s %s angetippt: %s", i ? ("Panel " + id.substring(4)).c_str() : "Hauptpanel", kind == 2 ? "doppelt" : "einmal", TAP_NAMES[a < TA_COUNT ? a : 0]);
  JsonDocument c;
  switch (a) {
  case TA_PANEL: c["state"] = (P[i].on && masterOn) ? "OFF" : "ON"; applyCommand(i, c.as<JsonVariantConst>()); break;
  case TA_WALL: c["state"] = masterOn ? "OFF" : "ON"; applyAll(c.as<JsonVariantConst>()); break;
  case TA_PRESET:
    for (int n = 1; n <= PRESET_MAX; n++) { int k = (curPreset + n + PRESET_MAX) % PRESET_MAX; if (presetNames[k].length()) { presetLoad(k); break; } }
    break;
  case TA_EFFECT:
    if (!masterOn) { c["state"] = "ON"; applyAll(c.as<JsonVariantConst>()); }
    fxStart((fx.id + 1) % FX_COUNT);
    break;
  }
  if (mqtt.connected()) {                          // Ereignis für Home-Assistant-Automationen
    String m = String("{\"event_type\":\"") + (kind == 2 ? "doppelt" : "einmal") + "\",\"panel\":\"" + id + "\"}";
    mqtt.publish("trilumag/touch", m.c_str(), false);
  }
  ws::broadcast(String("{\"t\":\"touch\",\"d\":{\"id\":\"") + id + "\",\"k\":" + kind + "}}");
  wsKick();
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
      // Hat es doch noch Strom (etwa nach einem Aussetzer), vergisst es seine Adresse und wird neu gefunden
      if (cfg.bus && P[i].addr) bus::send(P[i].addr, bus::C_RESET);
      P[i].attached = false; P[i].state = DARK; P[i].parent = -1; P[i].addr = 0;
      diag("Panel %s getrennt (hing an einem abgeklipsten Panel)", hex(P[i].chip).substring(4).c_str());
      publishAvail(i, false);
    }
  }
}

// Panel tauschen: in der App gestartet, dann 5 Minuten Zeit, das alte Panel abzuklipsen und ein neues an dieselbe
// Stelle zu setzen. Das neue übernimmt Farbe, Helligkeit und "Kanten einzeln".
const uint32_t SWAP_MS = 300000UL;
int8_t swapSlot = -1; uint32_t swapUntil = 0;
uint32_t swapDoneChip = 0, swapDoneAt = 0;
bool swapKid[SLOTS];   // Simulation: Panels, die mit dem alten abgefallen sind, hängen danach wieder am neuen
bool swapActive() { return swapSlot > 0 && P[swapSlot].used && (int32_t)(swapUntil - millis()) > 0; }
void swapCheck(int i) {
  if (!swapActive() || i == swapSlot) return;
  const Panel& b = P[swapSlot]; Panel& a = P[i];
  if (b.attached || b.x != a.x || b.y != a.y) return;
  a.r = b.r; a.g = b.g; a.b = b.b; a.w = b.w; a.bri = b.bri; a.on = b.on; a.hasColor = b.hasColor;
  a.edges = b.edges; prefs.putBool(("e" + hex(a.chip)).c_str(), a.edges);
  colorsDirty = true; colorsDirtyAt = millis();
  diag("Panel %s übernimmt die Einstellungen von %s", hex(a.chip).substring(4).c_str(), hex(b.chip).substring(4).c_str());
  swapSlot = -1; swapDoneChip = a.chip; swapDoneAt = millis(); if (!swapDoneAt) swapDoneAt = 1;
  // Simulation: in echt hängen die Panels dahinter noch aneinander und melden sich über das neue wieder
  if (!cfg.bus) for (int k = 1; k < SLOTS; k++) if (swapKid[k]) {
    swapKid[k] = false;
    if (!P[k].used || P[k].attached || findAt(P[k].x, P[k].y) >= 0) continue;
    P[k].attached = true; P[k].state = DARK; P[k].since = millis();
    publishAvail(k, true);
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
  swapCheck(i);
  reconcile();
  return true;
}

// Simulation: Panel anklipsen, die eigene Kante hängt davon ab, wie man es hält (zufällig)
bool simAttach(int i, int parent, uint8_t edge) {
  if (!placePanel(i, parent, edge, esp_random() % 3)) return false;
  P[i].state = DARK;
  diag("Panel %s an Kante %u von %s angeklipst", hex(P[i].chip).substring(4).c_str(), edge + 1, parent == 0 ? "Haupt" : hex(P[parent].chip).substring(4).c_str());
  P[i].attaches++; clipCount(i);
  publishDiscovery(i); publishAvail(i, true);
  simChanged();
  return true;
}

void simDetach(int i) {
  if (i <= 0 || i >= SLOTS || !P[i].used || !P[i].attached) return;
  bool before[SLOTS]; for (int k = 0; k < SLOTS; k++) before[k] = P[k].used && P[k].attached;
  P[i].attached = false; P[i].state = DARK;
  publishAvail(i, false);
  diag("Panel %s abgeklipst", hex(P[i].chip).substring(4).c_str());
  reconcile();
  if (i == swapSlot && swapActive()) for (int k = 1; k < SLOTS; k++) swapKid[k] = k != i && before[k] && !P[k].attached;
  simChanged();
}

int simNewPanel() {
  for (int i = 1; i < SLOTS; i++) if (!P[i].used) {
    P[i] = Panel(); P[i].used = true;
    do { P[i].chip = esp_random(); } while (P[i].chip == 0 || findChip(P[i].chip) != i);
    clipsLoad(i); litLoad(i);
    simChanged();
    return i;
  }
  return -1;
}

// Die simulierte Wand übersteht Neustarts und Updates: Panels, Positionen und Ablage im NVS
struct SimRec { uint32_t chip; int8_t x, y; uint8_t rot, attached; };
bool simDirty = false;
uint32_t simDirtyAt = 0;
void simChanged() { simDirty = true; simDirtyAt = millis(); }

void simSave() {
  SimRec r[SLOTS]; uint8_t n = 0;
  for (int i = 1; i < SLOTS; i++) if (P[i].used) r[n++] = {P[i].chip, P[i].x, P[i].y, P[i].rot, (uint8_t)P[i].attached};
  if (n) prefs.putBytes("simw", r, n * sizeof(SimRec)); else prefs.remove("simw");
  simDirty = false;
}

// true, wenn eine gespeicherte Wand geladen wurde
bool simLoad() {
  SimRec r[SLOTS];
  size_t len = prefs.getBytes("simw", r, sizeof r);
  if (!len || len % sizeof(SimRec)) return false;
  uint8_t n = len / sizeof(SimRec);
  for (uint8_t k = 0; k < n && k + 1 < SLOTS; k++) {
    int i = k + 1;
    P[i] = Panel(); P[i].used = true; P[i].chip = r[k].chip;
    P[i].x = r[k].x; P[i].y = r[k].y; P[i].rot = r[k].rot % 3; P[i].attached = r[k].attached;
    P[i].state = DARK; P[i].since = millis();                    // wie echte Panels: kurz dunkel, dann erkannt
    loadColor(i); clipsLoad(i); litLoad(i);
  }
  reconcile();                                                   // Nachbarn und Abstände neu berechnen
  logf("[SIM] gespeicherte Wand mit %d Panels geladen\n", countAttached() - 1);
  return true;
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
  cfg.mqttOn = prefs.getBool("mqttOn", cfg.mqttHost.length() > 0);
  cfg.autoUpdate = prefs.getBool("autoUpd", false);
  cfg.transMs = prefs.getUShort("trans", 700);
  cfg.pwrMax = prefs.getUShort("pwrMax", 0);
  cfg.pwrCh = prefs.getUChar("pwrCh", 12);
  int8_t dSda, dScl; i2cDefault(cfg.board.c_str(), dSda, dScl);           // ohne eigene Wahl: Vorgabe des Boards
  cfg.i2cSda = (int8_t)prefs.getChar("i2cSda", dSda); cfg.i2cScl = (int8_t)prefs.getChar("i2cScl", dScl);
  cfg.shuntUo = prefs.getUShort("shunt", 50);   // ältere Versionen: an, sobald eine Adresse da ist
  cfg.touchOn = prefs.getBool("tOn", true);
  cfg.touchSens = constrain((int)prefs.getUChar("tSens", 5), 1, 10);
  cfg.tapA1 = prefs.getUChar("tA1", TA_PANEL); cfg.tapA2 = prefs.getUChar("tA2", TA_WALL);
  if (cfg.tapA1 >= TA_COUNT) cfg.tapA1 = TA_PANEL;
  if (cfg.tapA2 >= TA_COUNT) cfg.tapA2 = TA_WALL;
  cfg.panelAuto = prefs.getBool("pAuto", true);
  cfg.zbOn = HAS_ZIGBEE && prefs.getBool("zbOn", false);
  cfg.name = prefs.getString("name", "Trilumag");
  cfg.bootMode = prefs.getUChar("bootMode", 0); if (cfg.bootMode > 3) cfg.bootMode = 0;
  cfg.bootPreset = (int8_t)prefs.getChar("bootPre", -1);
  cfg.syncOn = prefs.getBool("syncOn", false);
  cfg.guard = prefs.getBool("guard", true);
  cfg.onAnim = prefs.getUChar("onAnim", 2); if (cfg.onAnim > 3) cfg.onAnim = 2;
  cfg.touchWave = prefs.getBool("tWave", true);
  cfg.daylight = prefs.getBool("dayc", false);
  cfg.faultBlink = prefs.getBool("fault", true);
  cfg.viewRot = prefs.getUShort("viewRot", 0) % 360; cfg.viewMir = prefs.getBool("viewMir", false);
  cfg.syncGroup = constrain((int)prefs.getUChar("syncGrp", 1), 1, 9);
  if (!cfg.name.length()) cfg.name = "Trilumag";
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
  JsonObject li = d["light"].to<JsonObject>();
  li["trans"] = cfg.transMs; li["onAnim"] = cfg.onAnim; li["daylight"] = cfg.daylight; li["pwrMax"] = cfg.pwrMax; li["pwrCh"] = cfg.pwrCh;
  li["sda"] = cfg.i2cSda; li["scl"] = cfg.i2cScl; li["shunt"] = cfg.shuntUo; li["sensor"] = ina::ok;
  { int8_t a, b; i2cDefault(cfg.board.c_str(), a, b); li["defSda"] = a; li["defScl"] = b; }
  JsonArray os = d["orders"].to<JsonArray>();
  for (const char* o : ORDERS) os.add(o);
  JsonObject m = d["mqtt"].to<JsonObject>();
  m["on"] = cfg.mqttOn; m["host"] = cfg.mqttHost; m["port"] = cfg.mqttPort; m["user"] = cfg.mqttUser; m["hasPass"] = cfg.mqttPass.length() > 0;
  JsonObject bo = d["boot"].to<JsonObject>(); bo["mode"] = cfg.bootMode; bo["preset"] = cfg.bootPreset;
  d["fault"] = cfg.faultBlink;
  d["guard"] = cfg.guard; d["why"] = bootReason; d["crashes"] = prefs.getUInt("crashes", 0);
  JsonObject t = d["touch"].to<JsonObject>();
  t["on"] = cfg.touchOn; t["sens"] = cfg.touchSens; t["a1"] = cfg.tapA1; t["a2"] = cfg.tapA2; t["wave"] = cfg.touchWave;
  JsonArray tn = t["actions"].to<JsonArray>();
  for (const char* n : TAP_NAMES) tn.add(n);
  String out; serializeJson(d, out); return out;
}

// ---------- JSON für die App ----------
// Strom schwankt mit jedem Effektbild: für die App kommt er separat einmal pro Sekunde (sjNoPwr), sonst ginge
// der ganze Zustand mehrmals pro Sekunde raus.
bool sjNoPwr = false;
void pwrJson(JsonObject pw) {
  pw["est"] = estMa; pw["lim"] = cfg.pwrMax; pw["scale"] = (int)(powerScale * measScale * 100 + 0.5f);
  if (ina::ok) { pw["ma"] = (int)(ina::amps * 1000); pw["v"] = roundf(ina::volts * 100) / 100; }
  pw["sensor"] = ina::ok; pw["sda"] = cfg.i2cSda; pw["scl"] = cfg.i2cScl; pw["shunt"] = cfg.shuntUo;
}
String stateJson(bool meta) {
  JsonDocument d;
  d["sim"] = !cfg.bus;
  d["name"] = cfg.name;
  { JsonObject vw = d["view"].to<JsonObject>(); vw["rot"] = cfg.viewRot; vw["mir"] = cfg.viewMir; }
  if (sleepEnd) d["sleep"] = (sleepLeft() + 999) / 1000;
  if (progVal > 0) d["prog"] = progVal;
  d["max"] = MAX_ATTACHED;
  d["mqtt"] = mqtt.connected();
  d["mqttSet"] = cfg.mqttOn && cfg.mqttHost.length() > 0;
  d["ap"] = apMode;
  d["ssid"] = wlanOk ? WiFi.SSID() : String();
  if (wlanOk) { d["rssi"] = WiFi.RSSI() / 3 * 3; d["ip"] = WiFi.localIP().toString(); }   // in 3-dB-Schritten, sonst ändert es sich dauernd
  if (!sjNoPwr) pwrJson(d["pwr"].to<JsonObject>());     // Strom: geschätzt, Limit, Dämpfung, Messung
  d["trans"] = cfg.transMs;
  if (wx::place.length() || wx::err.length() || wx::busy) wx::json(d["wx"].to<JsonObject>());
  game::json(d["game"].to<JsonObject>());
  if (mirror::ip.length()) mirror::json(d["mirror"].to<JsonObject>());
  hueb::json(d["hue"].to<JsonObject>());
  if (cfg.daylight) { JsonObject dy = d["day"].to<JsonObject>(); dy["ok"] = timeOk(); if (timeOk()) { float dm, wm; dayCurve(dm, wm); dy["dim"] = (int)(dm * 100 + 0.5f); dy["warm"] = (int)(wm * 100 + 0.5f); } }
  d["upd"] = ota::count && ota::cmp(ota::latest(), FW_VERSION) > 0 ? ota::latest() : String();   // neuere Version verfügbar
  d["ver"] = FW_VERSION;
  d["chip"] = CHIP_FAMILY;
  JsonObject f = d["fx"].to<JsonObject>();
  f["id"] = FX[fx.id].id; f["speed"] = fx.speed; f["inten"] = fx.inten; f["pal"] = palId(fx.pal); f["dir"] = fxDir; f["spin"] = fxSpin;
  { JsonObject c2 = f["c2"].to<JsonObject>(); c2["r"] = fxC2[0]; c2["g"] = fxC2[1]; c2["b"] = fxC2[2]; c2["w"] = fxC2[3]; }
  f["r"] = fx.r; f["g"] = fx.g; f["b"] = fx.b; f["w"] = fx.w; f["usesColor"] = fxUsesColor();
  d["master"] = master; d["on"] = masterOn;
  if (swapActive()) { JsonObject w = d["swap"].to<JsonObject>(); w["id"] = hex(P[swapSlot].chip); w["left"] = (swapUntil - millis() + 999) / 1000; w["off"] = !P[swapSlot].attached; }
  if (swapDoneAt && (int32_t)(millis() - swapDoneAt) < 15000) d["swapped"] = hex(swapDoneChip);
  { JsonArray cp = d["cpal"].to<JsonArray>();
    for (uint8_t i = 0; i < CPAL_MAX; i++) if (cpal[i].used) {
      JsonObject o = cp.add<JsonObject>(); o["id"] = palId(PAL_COUNT + i); o["name"] = cpal[i].name; o["slot"] = i;
      JsonArray c = o["c"].to<JsonArray>(); for (uint8_t k = 0; k < cpal[i].n; k++) { char b[8]; snprintf(b, sizeof b, "#%06X", (unsigned)cpal[i].c[k]); c.add(b); }
    } }
  if (fx.pal == PAL_X && xpal.used) {
    JsonObject o = d["xpal"].to<JsonObject>(); o["id"] = palId(PAL_X); o["name"] = xpal.name;
    JsonArray c = o["c"].to<JsonArray>(); for (uint8_t k = 0; k < xpal.n; k++) { char b[8]; snprintf(b, sizeof b, "#%06X", (unsigned)xpal.c[k]); c.add(b); }
  }
  d["pfw"] = PANEL_FW_VERSION; d["pAuto"] = cfg.panelAuto;
  { JsonArray fv = d["favs"].to<JsonArray>(); for (uint8_t k = 0; k < FX_COUNT; k++) if (fxFavs & (1u << k)) fv.add(FX[k].id); }
  wallsync::json(d["sync"].to<JsonObject>());
  { JsonObject z = d["zb"].to<JsonObject>();            // Hue/Zigbee: nur der ESP32-C6 kann es, die App zeigt es sonst ausgegraut
    z["avail"] = (bool)HAS_ZIGBEE;
#if HAS_ZIGBEE
    z["on"] = cfg.zbOn; z["run"] = zb::started; z["join"] = zb::joined(); z["ch"] = zb::channel();
    if (zb::problem.length()) z["err"] = zb::problem;
#endif
  }
  if (meta) {                                    // feste Listen nur beim ersten Mal
    JsonArray fl = d["effects"].to<JsonArray>();
    for (uint8_t k = 0; k < FX_COUNT; k++) { JsonObject e = fl.add<JsonObject>(); e["id"] = FX[k].id; e["name"] = FX[k].name; e["color"] = FX[k].color; }
    JsonArray pl = d["palettes"].to<JsonArray>();
    for (uint8_t k = 0; k < PAL_COUNT; k++) {
      JsonObject e = pl.add<JsonObject>(); e["id"] = PALS[k].id; e["name"] = PALS[k].name;
      JsonArray cs = e["c"].to<JsonArray>();
      for (uint8_t j = 0; j < PALS[k].n; j++) { char b[8]; snprintf(b, sizeof b, "#%06X", (unsigned)PALS[k].c[j]); cs.add(b); }
    }
  }
  JsonArray pr = d["presets"].to<JsonArray>();
  for (uint8_t k = 0; k < PRESET_MAX; k++) if (presetNames[k].length()) { JsonObject e = pr.add<JsonObject>(); e["id"] = k; e["name"] = presetNames[k]; }
  d["preset"] = curPreset;
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
    o["state"] = p.state; o["on"] = p.on; o["edges"] = p.edges; o["fw"] = p.fw;
    o["clips"] = p.clips; o["caps"] = p.caps; o["lit"] = p.litSec / 60 * 60;   // minutengenau reicht
    if (p.identUntil) o["ident"] = true;
    if (p.upd) { o["upd"] = p.upd; o["pct"] = p.updPct; }
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
void wsKick();
void replyState() { server.send(200, "application/json", stateJson(!server.hasArg("slim"))); wsKick(); }
void replyError(const char* m) { server.send(400, "application/json", String("{\"error\":\"") + m + "\"}"); }

// ---------- WebSocket: Zustand und Effektbild an die App schieben ----------
String wsLast;                   // zuletzt geschickter Zustand
bool wsForce = false;
uint32_t wsLastState = 0, wsLastLive = 0;
const char* apiCall(const char* path, JsonDocument& d);
String liveJson();
void wsKick() { wsForce = true; }

String otaJson();
void wsOpen(uint8_t id) {
  ws::send(id, "{\"t\":\"state\",\"d\":" + stateJson(true) + "}");
  ws::send(id, "{\"t\":\"ota\",\"d\":" + otaJson() + "}");
  wsLast = stateJson(false);
}
// Nachricht der App: {"p":"/api/set","b":{...}}
void wsText(uint8_t id, char* msg, size_t len) {
  JsonDocument m;
  if (deserializeJson(m, msg, len)) return;
  const char* p = m["p"] | "";
  JsonDocument b; b.set(m["b"]);
  if (const char* err = apiCall(p, b)) {
    String e = "{\"t\":\"err\",\"m\":\""; e += err; e += "\"}";
    ws::send(id, e);
  }
  wsForce = true;                // Änderung sofort an alle Apps
}
void wsLoop() {
  ws::loop();
  if (!ws::count()) { wsLast = ""; return; }
  uint32_t now = millis();
  if (wsForce || now - wsLastState > 250) {
    wsLastState = now;
    sjNoPwr = true; String s = stateJson(false); sjNoPwr = false;
    if (wsForce || s != wsLast) { ws::broadcast("{\"t\":\"state\",\"d\":" + s + "}"); wsLast = s; }
    wsForce = false;
  }
  static uint32_t lastPwr = 0; static String pwrLast;
  if (now - lastPwr >= 1000) {                                        // Strom einmal pro Sekunde, nur wenn er sich ändert
    lastPwr = now; JsonDocument p; pwrJson(p.to<JsonObject>()); String ps; serializeJson(p, ps);
    if (ps != pwrLast) { ws::broadcast("{\"t\":\"pwr\",\"d\":" + ps + "}"); pwrLast = ps; }
  }
  if ((fx.id || overlayActive()) && now - wsLastLive >= 40) {        // Effektbild 25-mal pro Sekunde, so oft wie die Wand selbst (auch bei Wellen und Einschalt-Animation)
    wsLastLive = now;
    ws::broadcast("{\"t\":\"live\",\"d\":" + liveJson() + "}");
  }
}

// ---------- WLED-kompatible Schnittstelle ----------
// Trilumag meldet sich zusätzlich wie ein WLED-Gerät mit einem Segment (der ganzen Wand).
// Damit funktionieren die WLED-Integration von Home Assistant und andere WLED-Programme.
String macPlain() { uint8_t m[6]; WiFi.macAddress(m); char b[13]; snprintf(b, sizeof b, "%02x%02x%02x%02x%02x%02x", m[0], m[1], m[2], m[3], m[4], m[5]); return b; }

namespace peers { extern volatile bool busy; }
void mdnsStart() {
  if (peers::busy) return;                          // gerade läuft eine Suche nach anderen Wänden
  MDNS.end();
  MDNS.begin(HOSTNAME);
  MDNS.setInstanceName(cfg.name.c_str());          // so heißt die Wand in Geräte-Listen; die Adresse bleibt trilumag.local
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("wled", "tcp", 80);
  MDNS.addServiceTxt("wled", "tcp", "mac", macPlain().c_str());
  MDNS.addServiceTxt("wled", "tcp", "tl", FW_VERSION);          // daran erkennen sich Trilumag-Wände
  MDNS.addServiceTxt("wled", "tcp", "name", cfg.name.c_str());
}

// ---------- Andere Trilumag im WLAN finden (für die Liste in der App) ----------
// Die mDNS-Abfrage dauert 3 s, deshalb läuft sie in einer eigenen Aufgabe; die Hauptschleife übernimmt das Ergebnis.
namespace peers {
struct Peer { String name, host, ip, ver; };
const uint8_t MAXP = 16;
Peer cur[MAXP], nxt[MAXP];
uint8_t nCur = 0, nNxt = 0;
Peer wCur[MAXP], wNxt[MAXP];               // echte WLED-Geräte (zum Nachahmen)
uint8_t nwCur = 0, nwNxt = 0;
volatile bool busy = false, ready = false;
uint32_t at = 0;
void task(void*) {
  int k = MDNS.queryService("wled", "tcp");                    // ältere Trilumag-Versionen melden sich nur so
  String me = WiFi.localIP().toString();
  nNxt = 0; nwNxt = 0;
  for (int i = 0; i < k; i++) {
    String h = MDNS.hostname(i), ip = MDNS.address(i).toString();
    bool tl = MDNS.hasTxt(i, "tl");
    if (ip == me || ip == "0.0.0.0") continue;
    if (!tl && !h.startsWith("trilumag")) {                       // echtes WLED-Gerät
      if (nwNxt < MAXP) { Peer& w = wNxt[nwNxt++]; w.host = h; w.ip = ip; w.name = h; }
      continue;
    }
    if (nNxt >= MAXP) continue;
    Peer& p = nxt[nNxt++];
    p.host = h; p.ip = ip; p.ver = tl ? MDNS.txt(i, "tl") : String();
    p.name = MDNS.hasTxt(i, "name") ? MDNS.txt(i, "name") : h;
  }
  for (uint8_t i = 0; i < nwNxt; i++) {                         // WLED-Geräte: ihren Namen erfragen
    HTTPClient h; h.setTimeout(1500);
    if (h.begin("http://" + wNxt[i].ip + "/json/info") && h.GET() == 200) {
      JsonDocument f; f["name"] = true; JsonDocument d;
      if (!deserializeJson(d, h.getString(), DeserializationOption::Filter(f)) && d["name"].is<const char*>()) wNxt[i].name = d["name"].as<const char*>();
    }
    h.end();
  }
  ready = true;
  vTaskDelete(nullptr);
}
// Update an andere Wände weitergeben: erst nach Updates suchen lassen, dann die Version installieren.
// Zustand pro Wand: 1 wird geschickt, 2 installiert (Wand hat angenommen), 3 Fehler
const uint8_t MAXU = 8;
char upIp[MAXU][16]; volatile uint8_t upSt[MAXU]; uint8_t nUp = 0;
String upVer; volatile bool pushing = false;
int post(const char* ip, const String& body, String* reply) {
  HTTPClient h; h.setTimeout(4000);
  if (!h.begin(String("http://") + ip + "/api/ota")) return -1;
  h.addHeader("Content-Type", "application/json");
  int code = h.POST(body);
  if (reply) *reply = code > 0 ? h.getString() : String();
  h.end();
  return code;
}
void pushTask(void*) {
  for (uint8_t i = 0; i < nUp; i++) if (post(upIp[i], "{\"action\":\"check\"}", nullptr) != 200) upSt[i] = 3;
  vTaskDelay(pdMS_TO_TICKS(8000));                            // die anderen holen sich die Versionsliste
  for (uint8_t i = 0; i < nUp; i++) {
    if (upSt[i] == 3) continue;
    for (uint8_t t = 0; t < 5; t++) {                          // ältere Versionen kennen die Version erst nach der Suche
      String r; int c = post(upIp[i], "{\"action\":\"install\",\"version\":\"" + upVer + "\"}", &r);
      if (c == 200) { upSt[i] = 2; break; }
      upSt[i] = 3;
      if (r.indexOf("unbekannt") < 0) break;
      vTaskDelay(pdMS_TO_TICKS(4000));
    }
  }
  pushing = false;
  vTaskDelete(nullptr);
}
bool push(const String& v, JsonArrayConst ips) {
  if (pushing) return false;
  nUp = 0;
  for (JsonVariantConst x : ips) {
    const char* ip = x | "";
    if (nUp < MAXU && strlen(ip) > 6 && strlen(ip) < 16) { strcpy(upIp[nUp], ip); upSt[nUp] = 1; nUp++; }
  }
  if (!nUp) return false;
  upVer = v; pushing = true;
  if (xTaskCreate(pushTask, "peerupd", 6144, nullptr, 1, nullptr) != pdPASS) { pushing = false; return false; }
  return true;
}
uint8_t pushState(const String& ip) { for (uint8_t i = 0; i < nUp; i++) if (ip == upIp[i]) return upSt[i]; return 0; }

void start() {
  if (busy || !wlanOk || netQuiet || otaBusy) return;
  busy = true; ready = false;
  if (xTaskCreate(task, "peers", 6144, nullptr, 1, nullptr) != pdPASS) busy = false;
}
void loop() {
  if (!ready) return;
  for (uint8_t i = 0; i < nNxt; i++) cur[i] = nxt[i];
  for (uint8_t i = 0; i < nwNxt; i++) wCur[i] = wNxt[i];
  nCur = nNxt; nwCur = nwNxt; at = millis() | 1; ready = false; busy = false;
}
String json() {
  JsonDocument d;
  d["busy"] = (bool)busy; d["ago"] = at ? (int32_t)((millis() - at) / 1000) : -1;
  JsonArray a = d["list"].to<JsonArray>();
  for (uint8_t i = 0; i < nCur; i++) {
    JsonObject o = a.add<JsonObject>(); o["name"] = cur[i].name; o["host"] = cur[i].host; o["ip"] = cur[i].ip; o["ver"] = cur[i].ver;
    if (uint8_t u = pushState(cur[i].ip)) { o["upd"] = u; o["updVer"] = upVer; }
  }
  d["pushing"] = (bool)pushing;
  JsonArray w = d["wled"].to<JsonArray>();
  for (uint8_t i = 0; i < nwCur; i++) { JsonObject o = w.add<JsonObject>(); o["name"] = wCur[i].name; o["host"] = wCur[i].host; o["ip"] = wCur[i].ip; }
  String out; serializeJson(d, out); return out;
}
}  // namespace peers

// ---------- WLED nachahmen ----------
// Ein WLED-Gerät im WLAN vorgeben: Trilumag fragt alle 1,5 s dessen Zustand ab und übernimmt Änderungen
// (Ein/Aus, Helligkeit, Farbe, Tempo, Intensität und – wo es ein Gegenstück gibt – den Effekt).
namespace mirror {
String ip;                                  // leer = aus
volatile bool run = false, fresh = false, fail = false;
struct St { bool on; uint8_t bri, fx, sx, ix; uint8_t c[4]; };
St got, last; bool haveLast = false;
uint32_t okAt = 0;
// WLED-Effektnummer → Trilumag-Effekt (-1: keine Entsprechung, dann nur die Farbe)
int mapFx(int f) {
  switch (f) {
    case 0: return 0;  case 2: return 3;  case 8: return 4;  case 9: return 1;  case 10: case 11: return 9;
    case 20: case 21: case 22: case 74: return 5;  case 38: return 8;  case 45: return 12;  case 57: return 11;
    case 66: return 7;  case 76: return 14;  case 63: return 2;  case 89: case 90: return 21;
    default: return -1;
  }
}
void pollOnce() {
    if (netQuiet || !wlanOk) return;       // ohne WLAN (auch direkt nach dem Start) keine Verbindung versuchen
    wledInReq = true;
    HTTPClient h; h.setTimeout(2500);
    bool ok = false;
    if (h.begin("http://" + ip + "/json/state") && h.GET() == 200) {
      JsonDocument f; f["on"] = true; f["bri"] = true;
      JsonObject sf = f["seg"][0].to<JsonObject>(); sf["col"] = true; sf["fx"] = true; sf["sx"] = true; sf["ix"] = true;
      JsonDocument d;
      if (!deserializeJson(d, h.getString(), DeserializationOption::Filter(f))) {
        St n; n.on = d["on"] | false; n.bri = d["bri"] | 128;
        JsonVariant sg = d["seg"][0]; n.fx = sg["fx"] | 0; n.sx = sg["sx"] | 128; n.ix = sg["ix"] | 128;
        JsonVariant c0 = sg["col"][0];
        for (int k = 0; k < 4; k++) n.c[k] = c0[k] | 0;
        if (!fresh) { got = n; fresh = true; }
        ok = true;
      }
    }
    h.end(); wledInReq = false;
    fail = !ok; if (ok) okAt = millis() | 1;
}
void task(void*) {
  while (run) { pollOnce(); for (int t = 0; t < 15 && run; t++) vTaskDelay(pdMS_TO_TICKS(100)); }
  vTaskDelete(nullptr);
}
void start(const String& a) {
  ip = a; haveLast = false; fresh = false; fail = false; okAt = 0;
  if (!run && ip.length()) { run = true; if (xTaskCreate(task, "wled", 6144, nullptr, 1, nullptr) != pdPASS) run = false; }
}
void stop() { run = false; ip = ""; }
// Hauptschleife: nur übernehmen, was sich am WLED-Gerät geändert hat (sonst darf man hier weiter selbst schalten)
void loop() {
  if (!fresh) return;
  St n = got; fresh = false;
  JsonDocument d;
  if (!haveLast || n.on != last.on) d["state"] = n.on ? "ON" : "OFF";
  if (!haveLast || n.bri != last.bri) d["brightness"] = n.bri ? n.bri : 1;
  bool fxCh = !haveLast || n.fx != last.fx;
  int k = mapFx(n.fx);
  if (fxCh) d["effect"] = k >= 0 ? FX[k].id : "aus";
  if (!haveLast || n.sx != last.sx) d["speed"] = 1 + n.sx * 99 / 255;
  if (!haveLast || n.ix != last.ix) d["intensity"] = n.ix;
  bool colOk = k <= 0 || FX[k].color;                         // Effekte mit eigenen Farben nicht auf Einfarbig umschalten
  if (colOk && (!haveLast || memcmp(n.c, last.c, 4) || fxCh)) { JsonObject c = d["color"].to<JsonObject>(); c["r"] = n.c[0]; c["g"] = n.c[1]; c["b"] = n.c[2]; c["w"] = n.c[3]; }
  last = n; haveLast = true;
  if (d.size()) { applyAll(d.as<JsonVariantConst>()); wsKick(); }
}
void json(JsonObject o) { o["ip"] = ip; o["ok"] = okAt && !fail && millis() - okAt < 10000; }
}  // namespace mirror

// ---------- Hue-Lampe nachahmen (über die Hue Bridge im WLAN, auf jedem Chip) ----------
// Bridge finden (mDNS), einmal koppeln (Knopf auf der Bridge), Lampe wählen. Dann fragt Trilumag die Lampe jede
// Sekunde über die lokale Schnittstelle (API v2, HTTPS) ab und übernimmt Änderungen wie beim WLED-Nachahmen.
namespace hueb {
String ip, key, light, lname, err;            // Bridge, Schlüssel, gefolgte Lampe (ID, Name), letzter Fehler
volatile bool busy = false, run = false, fresh = false, fail = false;
String want;                                  // Auftrag für die Aufgabe: "find", "pair", "lights"
String bridges[4]; uint8_t nBr = 0;
struct L { String id, name; }; L lights[40]; uint8_t nL = 0;
struct St { bool on; uint8_t bri; bool ct; uint16_t mirek; float x, y; String fx; };
St got, last; bool haveLast = false; uint32_t okAt = 0;
volatile bool rOn = false, rSync = false, rHave = false; volatile uint8_t rBri = 0; volatile uint16_t rMirek = 0; volatile uint32_t rRgb = 0;   // was die Bridge zuletzt meldete (für die App)
// CIE xy → RGB (hellster Kanal 255)
void xyRgb(float x, float y, uint8_t* o) {
  if (y < 0.001f) y = 0.001f;
  float X = x / y, Z = (1 - x - y) / y;
  float r = 3.2406f * X - 1.5372f - 0.4986f * Z, g = -0.9689f * X + 1.8758f + 0.0415f * Z, b = 0.0557f * X - 0.2040f + 1.0570f * Z;
  r = fmaxf(0, r); g = fmaxf(0, g); b = fmaxf(0, b); float m = fmaxf(r, fmaxf(g, b)); if (m <= 0) m = 1;
  o[0] = lroundf(255 * r / m); o[1] = lroundf(255 * g / m); o[2] = lroundf(255 * b / m);
}
int req(const char* method, const String& path, const String& body, String& out, bool auth) {
  NetworkClientSecure c; c.setInsecure();                 // die Bridge hat ein selbst ausgestelltes Zertifikat
  HTTPClient h; h.setTimeout(4000);
  if (!h.begin(c, "https://" + ip + path)) return -1;
  if (auth) h.addHeader("hue-application-key", key.c_str());
  int code;
  if (!strcmp(method, "POST")) { h.addHeader("Content-Type", "application/json"); code = h.POST(body); } else code = h.GET();
  out = code > 0 ? h.getString() : String();
  h.end();
  return code;
}
void work(void*) {
  String e;
  if (want == "find") {
    int k = MDNS.queryService("hue", "tcp"); nBr = 0;
    for (int i = 0; i < k && nBr < 4; i++) { String a = MDNS.address(i).toString(); if (a != "0.0.0.0") bridges[nBr++] = a; }
    if (!nBr) e = "Keine Hue Bridge gefunden";
    else if (!ip.length()) ip = bridges[0];
  } else if (want == "pair") {
    String r; int c = req("POST", "/api", "{\"devicetype\":\"trilumag#wand\"}", r, false);
    JsonDocument d;
    if (c != 200 || deserializeJson(d, r)) e = "Bridge nicht erreichbar";
    else if (d[0]["success"]["username"].is<const char*>()) { key = d[0]["success"]["username"].as<const char*>(); prefs.putString("hueKey", key); prefs.putString("hueIp", ip); want = "lights"; }
    else if ((d[0]["error"]["type"] | 0) == 101) e = "Bitte zuerst den runden Knopf auf der Bridge drücken, dann gleich noch einmal koppeln";
    else e = "Koppeln ging nicht";
  }
  if (!e.length() && want == "lights") {
    String r; int c = req("GET", "/clip/v2/resource/light", "", r, true);
    JsonDocument f; JsonObject fd = f["data"][0].to<JsonObject>(); fd["id"] = true; fd["metadata"]["name"] = true;
    JsonDocument d;
    if (c == 403 || c == 401) { e = "Bridge kennt Trilumag nicht mehr, bitte neu koppeln"; key = ""; prefs.remove("hueKey"); }
    else if (c != 200 || deserializeJson(d, r, DeserializationOption::Filter(f))) e = "Lampen ließen sich nicht abrufen";
    else { nL = 0; for (JsonVariant v : d["data"].as<JsonArray>()) if (nL < 40) { lights[nL].id = (const char*)(v["id"] | ""); lights[nL].name = (const char*)(v["metadata"]["name"] | "?"); nL++; } }
  }
  err = e; want = ""; busy = false;
  vTaskDelete(nullptr);
}
void start(const char* what) {
  if (busy || !wlanOk) return;
  want = what; busy = true;
  if (xTaskCreate(work, "hue", 8192, nullptr, 1, nullptr) != pdPASS) busy = false;
}
NetworkClientSecure* pc = nullptr;        // offene Verbindung zur Bridge
void closeConn() { if (pc) pc->stop(); }  // vor Updates: Speicher der Verschlüsselung freigeben
void pollOnce() {
  if (netQuiet || !wlanOk) return;         // ohne WLAN (auch direkt nach dem Start) keine Verbindung versuchen
  hueInReq = true;
  if (!pc) { pc = new NetworkClientSecure(); pc->setInsecure(); }
  static HTTPClient ph; ph.setReuse(true); ph.setTimeout(4000);           // Verbindung offen halten: nicht jede Sekunde neu verschlüsseln
  String r; int c = -1;
  if (ph.begin(*pc, "https://" + ip + "/clip/v2/resource/light/" + light)) { ph.addHeader("hue-application-key", key.c_str()); c = ph.GET(); r = c > 0 ? ph.getString() : String(); ph.end(); }
  hueInReq = false;
  JsonDocument f; JsonObject fd = f["data"][0].to<JsonObject>();
  fd["on"] = true; fd["dimming"] = true; fd["color"]["xy"] = true; fd["color_temperature"] = true; fd["effects"]["status"] = true; fd["mode"] = true;
  JsonDocument d; bool ok = false;
  if (c == 200 && !deserializeJson(d, r, DeserializationOption::Filter(f))) {
    JsonVariant v = d["data"][0];
    St n; n.on = v["on"]["on"] | false; n.bri = constrain((int)lroundf((v["dimming"]["brightness"] | 100.0f) * 2.55f), 1, 255);
    n.ct = v["color_temperature"]["mirek_valid"] | false; n.mirek = v["color_temperature"]["mirek"] | 366;
    n.x = v["color"]["xy"]["x"] | 0.4573f; n.y = v["color"]["xy"]["y"] | 0.41f;
    n.fx = (const char*)(v["effects"]["status"] | "no_effect");
    bool sync = !strcmp(v["mode"] | "normal", "streaming");      // Hue Sync (Fernseher, PC): Farben kommen nicht über die Schnittstelle
    uint8_t c[4]; if (n.ct) { kelvinRgbw(1000000 / (n.mirek ? n.mirek : 366), c); c[0] = fminf(255, c[0] + c[3]); c[1] = fminf(255, c[1] + c[3]); c[2] = fminf(255, c[2] + c[3]); } else xyRgb(n.x, n.y, c);
    rOn = n.on; rBri = (uint8_t)((n.bri * 100 + 127) / 255); rMirek = n.ct ? n.mirek : 0; rRgb = ((uint32_t)c[0] << 16) | (c[1] << 8) | c[2]; rSync = sync; rHave = true;
    if (!fresh && !sync) { got = n; fresh = true; }
    ok = true;
  }
  fail = !ok; if (ok) okAt = millis() | 1;
}
void task(void*) {
  while (run) { pollOnce(); for (int t = 0; t < 10 && run; t++) vTaskDelay(pdMS_TO_TICKS(100)); }
  vTaskDelete(nullptr);
}
void follow(const String& id, const String& nm) {
  light = id; lname = nm; haveLast = false; fresh = false; fail = false; okAt = 0;
  prefs.putString("hueLight", id); prefs.putString("hueLName", nm);
  if (!run && id.length()) { run = true; if (xTaskCreate(task, "huepoll", 8192, nullptr, 1, nullptr) != pdPASS) run = false; }
}
void stop() { run = false; light = ""; lname = ""; prefs.remove("hueLight"); }
// Hue-Effekte auf Trilumag-Effekte
const char* mapFx(const String& f) {
  if (f == "candle") return "kerzen"; if (f == "fire") return "feuer"; if (f == "sparkle" || f == "glisten") return "funkeln";
  if (f == "prism") return "regenbogen"; if (f == "opal") return "polarlicht"; if (f == "cosmos") return "sterne"; if (f == "sunbeam") return "atmen";
  return "aus";
}
void loop() {
  if (!fresh) return;
  St n = got; fresh = false;
  JsonDocument d;
  if (!haveLast || n.on != last.on) d["state"] = n.on ? "ON" : "OFF";
  if (!haveLast || n.bri != last.bri) d["brightness"] = n.bri;
  bool fxCh = !haveLast || n.fx != last.fx;
  const char* fxId = mapFx(n.fx);
  if (fxCh) d["effect"] = fxId;
  bool colCh = !haveLast || n.ct != last.ct || (n.ct ? n.mirek != last.mirek : (fabsf(n.x - last.x) > 0.002f || fabsf(n.y - last.y) > 0.002f));
  bool colOk = !strcmp(fxId, "aus") || FX[fxFind(fxId)].color;
  if (colOk && (colCh || fxCh)) {
    uint8_t c[4] = {0, 0, 0, 0};
    if (n.ct) kelvinRgbw(1000000 / (n.mirek ? n.mirek : 366), c); else xyRgb(n.x, n.y, c);
    JsonObject o = d["color"].to<JsonObject>(); o["r"] = c[0]; o["g"] = c[1]; o["b"] = c[2]; o["w"] = c[3];
  }
  last = n; haveLast = true;
  if (d.size()) { applyAll(d.as<JsonVariantConst>()); wsKick(); }
}
void load() {
  ip = prefs.getString("hueIp", ""); key = prefs.getString("hueKey", "");
  String l = prefs.getString("hueLight", "");
  if (key.length() && l.length() && !safeBoot) follow(l, prefs.getString("hueLName", ""));
}
void json(JsonObject o) {
  o["ip"] = ip; o["paired"] = key.length() > 0; o["busy"] = (bool)busy;
  if (light.length()) {
    o["light"] = light; o["name"] = lname; o["ok"] = okAt && !fail && millis() - okAt < 10000;
    if (rHave) { JsonObject r = o["rep"].to<JsonObject>(); r["on"] = (bool)rOn; r["bri"] = rBri; if (rMirek) r["k"] = 1000000 / rMirek; char b[8]; snprintf(b, sizeof b, "#%06X", (unsigned)rRgb); r["rgb"] = b; r["sync"] = (bool)rSync; }
  }
  if (err.length()) o["err"] = err;
}
String listJson() {
  JsonDocument d; d["busy"] = (bool)busy; d["ip"] = ip; d["paired"] = key.length() > 0; if (err.length()) d["err"] = err;
  JsonArray b = d["bridges"].to<JsonArray>(); for (uint8_t i = 0; i < nBr; i++) b.add(bridges[i]);
  JsonArray l = d["lights"].to<JsonArray>(); for (uint8_t i = 0; i < nL; i++) { JsonObject o = l.add<JsonObject>(); o["id"] = lights[i].id; o["name"] = lights[i].name; }
  String out; serializeJson(d, out); return out;
}
}  // namespace hueb

// ---------- Wetter (Open-Meteo, kostenlos, ohne Anmeldung) ----------
// Ort einmal in Koordinaten umrechnen, dann alle 15 Minuten Temperatur und Niederschlag holen. Läuft in einer eigenen Aufgabe.
namespace wx {
String place, err;                     // Anzeigename des Orts, letzter Fehler
float lat = 0, lon = 0;
volatile bool busy = false;
String want;                           // neu gesuchter Ort (leer = nur Wetter holen)
uint32_t at = 0, next = 0;
String enc(const String& s) {
  String o; const char* hx = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) { uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '.') o += (char)c; else { o += '%'; o += hx[c >> 4]; o += hx[c & 15]; } }
  return o;
}
bool get(const String& url, JsonDocument& d) {
  HTTPClient h; h.setTimeout(6000);
  if (!h.begin(url)) return false;
  int code = h.GET();
  bool ok = code == 200 && !deserializeJson(d, h.getString());
  h.end();
  return ok;
}
void task(void*) {
  String e;
  if (want.length()) {
    JsonDocument g;
    if (!get("http://geocoding-api.open-meteo.com/v1/search?count=1&language=de&name=" + enc(want), g)) e = "Wetterdienst nicht erreichbar";
    else if (g["results"][0].isNull()) e = "Ort nicht gefunden";
    else {
      JsonVariant r = g["results"][0];
      lat = r["latitude"] | 0.0f; lon = r["longitude"] | 0.0f;
      place = String((const char*)(r["name"] | "")) + (r["country_code"].is<const char*>() ? String(", ") + (const char*)r["country_code"] : String());
      prefs.putFloat("wxLat", lat); prefs.putFloat("wxLon", lon); prefs.putString("wxPlace", place);
    }
    want = "";
  }
  if (!e.length() && place.length()) {
    JsonDocument f;
    char ll[48]; snprintf(ll, sizeof ll, "latitude=%.3f&longitude=%.3f", lat, lon);
    String u = String("http://api.open-meteo.com/v1/forecast?") + ll +
               "&current=temperature_2m,precipitation,weather_code&hourly=precipitation_probability&forecast_hours=3&timezone=auto";
    if (!get(u, f)) e = "Wetterdienst nicht erreichbar";
    else {
      wxTemp = f["current"]["temperature_2m"] | 15.0f;
      int code = f["current"]["weather_code"] | 0; float pr = f["current"]["precipitation"] | 0.0f;
      int prob = 0; for (JsonVariant v : f["hourly"]["precipitation_probability"].as<JsonArray>()) { int q = v | 0; if (q > prob) prob = q; }
      bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
      bool rain = pr > 0.05f || (code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code >= 95 || prob >= 60;
      wxKind = snow ? 2 : rain ? 1 : 0; wxOk = true; at = millis() | 1;
    }
  }
  err = e;
  busy = false;
  vTaskDelete(nullptr);
}
void start() {
  if (busy || !wlanOk || netQuiet || otaBusy) return;
  busy = true; next = millis() + 15UL * 60 * 1000;
  if (xTaskCreate(task, "wetter", 8192, nullptr, 1, nullptr) != pdPASS) busy = false;
}
void load() { place = prefs.getString("wxPlace", ""); lat = prefs.getFloat("wxLat", 0); lon = prefs.getFloat("wxLon", 0); }
void loop() { if (place.length() && wlanOk && (int32_t)(millis() - next) >= 0) start(); }
void json(JsonObject o) {
  o["place"] = place; o["busy"] = (bool)busy;
  if (wxOk) { o["temp"] = roundf(wxTemp * 10) / 10; o["kind"] = wxKind; o["min"] = (millis() - at) / 60000; }
  if (err.length()) o["err"] = err;
}
}  // namespace wx

int ledCount() { return countAttached() * SEG_PER_PANEL; }

void wledInfo(JsonObject in) {
  in["ver"] = "0.15.0"; in["vid"] = 2410000;
  in["brand"] = "WLED"; in["product"] = "Trilumag"; in["name"] = cfg.name;
  in["release"] = String("Trilumag ") + FW_VERSION; in["repo"] = "mariofritzer/Trilumag";
  in["arch"] = "esp32"; in["core"] = "3.0.7"; in["freeheap"] = ESP.getFreeHeap(); in["uptime"] = millis() / 1000;
  in["mac"] = macPlain(); in["ip"] = WiFi.localIP().toString();
  JsonObject l = in["leds"].to<JsonObject>();
  l["count"] = ledCount(); l["rgbw"] = true; l["wv"] = true; l["cct"] = false; l["fps"] = 25; l["maxseg"] = 1;
  l["pwr"] = cfg.pwrMax ? estMa : 0; l["maxpwr"] = cfg.pwrMax; l["lc"] = 3;
  l["seglc"].to<JsonArray>().add(3);
  in["ws"] = -1;                                     // keine WLED-WebSocket-Verbindung: Programme fragen per HTTP
  in["fxcount"] = FX_COUNT; in["palcount"] = PAL_COUNT;
  JsonObject fs = in["fs"].to<JsonObject>(); fs["u"] = 4; fs["t"] = 1024; fs["pmt"] = presetsVer;
  in["live"] = false; in["lm"] = ""; in["lip"] = ""; in["udpport"] = 21324; in["str"] = false;
  JsonObject w = in["wifi"].to<JsonObject>();
  int rssi = wlanOk ? WiFi.RSSI() : 0;
  w["bssid"] = WiFi.BSSIDstr(); w["rssi"] = rssi; w["signal"] = rssi ? constrain(2 * (rssi + 100), 0, 100) : 0; w["channel"] = WiFi.channel();
}

void wledState(JsonObject st) {
  st["on"] = masterOn; st["bri"] = master; st["transition"] = cfg.transMs / 100;
  st["ps"] = curPreset >= 0 ? curPreset + 1 : -1; st["pl"] = -1; st["lor"] = 0; st["mainseg"] = 0;
  JsonObject nl = st["nl"].to<JsonObject>(); nl["on"] = false; nl["dur"] = 60; nl["mode"] = 1; nl["tbri"] = 0; nl["rem"] = -1;
  JsonObject u = st["udpn"].to<JsonObject>(); u["send"] = false; u["recv"] = false; u["sgrp"] = 0; u["rgrp"] = 0;
  JsonObject sg = st["seg"].to<JsonArray>().add<JsonObject>();
  int n = ledCount();
  sg["id"] = 0; sg["start"] = 0; sg["stop"] = n; sg["len"] = n; sg["grp"] = 1; sg["spc"] = 0; sg["of"] = 0;
  sg["on"] = true; sg["frz"] = false; sg["bri"] = 255; sg["cct"] = 127; sg["set"] = 0; sg["n"] = "Wand";
  JsonArray col = sg["col"].to<JsonArray>();
  JsonArray c0 = col.add<JsonArray>();
  if (fx.id) { c0.add(fx.r); c0.add(fx.g); c0.add(fx.b); c0.add(fx.w); }
  else { c0.add(P[0].r); c0.add(P[0].g); c0.add(P[0].b); c0.add(P[0].w); }
  for (int k = 0; k < 2; k++) { JsonArray z = col.add<JsonArray>(); for (int j = 0; j < 4; j++) z.add(0); }
  sg["fx"] = fx.id; sg["sx"] = (fx.speed * 255 + 50) / 100; sg["ix"] = fx.inten; sg["pal"] = fx.pal;
  sg["c1"] = 128; sg["c2"] = 128; sg["c3"] = 16; sg["sel"] = true; sg["rev"] = false; sg["mi"] = false;
  sg["o1"] = false; sg["o2"] = false; sg["o3"] = false; sg["si"] = 0; sg["m12"] = 0;
}

// Befehl im WLED-Format auf Trilumag übertragen
void wledApply(JsonVariantConst in) {
  if (in["ps"].is<int>()) { presetLoad(in["ps"].as<int>() - 1); return; }
  JsonDocument d;
  if (in["on"].is<bool>()) d["state"] = in["on"].as<bool>() ? "ON" : "OFF";
  else if (in["on"].is<const char*>() && !strcmp(in["on"], "t")) d["state"] = masterOn ? "OFF" : "ON";
  if (in["bri"].is<int>()) { int b = in["bri"]; if (b <= 0) d["state"] = "OFF"; else d["brightness"] = b; }
  JsonVariantConst sg = in["seg"].is<JsonArrayConst>() ? in["seg"][0] : in["seg"];
  if (!sg.isNull()) {
    if (sg["fx"].is<int>() && sg["fx"].as<int>() < FX_COUNT) d["effect"] = sg["fx"].as<int>();
    if (sg["sx"].is<int>()) d["speed"] = constrain((sg["sx"].as<int>() * 100 + 127) / 255, 1, 100);
    if (sg["ix"].is<int>()) d["intensity"] = sg["ix"].as<int>();
    if (sg["pal"].is<int>() && sg["pal"].as<int>() < PAL_COUNT) d["palette"] = sg["pal"].as<int>();
    JsonVariantConst c = sg["col"][0];
    if (c.is<JsonArrayConst>() && c.size() >= 3) {
      JsonObject o = d["color"].to<JsonObject>();
      o["r"] = c[0]; o["g"] = c[1]; o["b"] = c[2]; o["w"] = c.size() > 3 ? c[3].as<int>() : 0;
    }
  }
  if (d.size()) applyAll(d.as<JsonVariantConst>());
}

String wledJson(const char* what) {
  JsonDocument d;
  if (!strcmp(what, "info")) wledInfo(d.to<JsonObject>());
  else if (!strcmp(what, "state")) wledState(d.to<JsonObject>());
  else if (!strcmp(what, "eff")) { JsonArray a = d.to<JsonArray>(); for (uint8_t k = 0; k < FX_COUNT; k++) a.add(FX[k].name); }
  else if (!strcmp(what, "pal")) { JsonArray a = d.to<JsonArray>(); for (uint8_t k = 0; k < PAL_COUNT; k++) a.add(PALS[k].name); }
  else if (!strcmp(what, "presets")) {
    d["0"].to<JsonObject>();
    for (uint8_t k = 0; k < PRESET_MAX; k++) if (presetNames[k].length()) d[String(k + 1)]["n"] = presetNames[k];
  } else {                                           // alles zusammen (/json)
    wledState(d["state"].to<JsonObject>()); wledInfo(d["info"].to<JsonObject>());
    JsonArray e = d["effects"].to<JsonArray>(); for (uint8_t k = 0; k < FX_COUNT; k++) e.add(FX[k].name);
    JsonArray p = d["palettes"].to<JsonArray>(); for (uint8_t k = 0; k < PAL_COUNT; k++) p.add(PALS[k].name);
  }
  String out; serializeJson(d, out); return out;
}

// ---------- Diagnose für die App ----------
String diagJson() {
  JsonDocument d;
  d["bus"] = cfg.bus; d["up"] = millis() / 1000;
  d["frames"] = busFramesSent; d["crc"] = busCrcErr; d["timeouts"] = busTimeouts; d["discovers"] = busDiscovers;
  d["heap"] = ESP.getFreeHeap();
  d["boot"] = bootReason; d["crashes"] = prefs.getUInt("crashes", 0); d["guard"] = cfg.guard;
  JsonArray pa = d["panels"].to<JsonArray>();
  for (int i = 1; i < SLOTS; i++) {
    const Panel& p = P[i];
    if (!p.used || !p.attached) continue;
    JsonObject o = pa.add<JsonObject>();
    o["id"] = hex(p.chip); o["addr"] = p.addr; o["pings"] = p.pings; o["missed"] = p.missed;
    o["rtt"] = p.rtt; o["rttMax"] = p.rttMax; o["fw"] = p.fw; o["att"] = p.attaches; o["depth"] = p.depth;
    o["clips"] = p.clips; o["caps"] = p.caps; o["lit"] = p.litSec;
  }
  JsonArray lg = d["log"].to<JsonArray>();
  for (uint8_t k = 0; k < diagCount; k++) {                  // neueste zuerst
    const DiagEvent& e = diagLogBuf[(diagHead + DIAG_N - 1 - k) % DIAG_N];
    JsonObject o = lg.add<JsonObject>(); o["ago"] = (millis() - e.t) / 1000; o["m"] = e.m;
  }
  String out; serializeJson(d, out); return out;
}

// ---------- Sichern und Wiederherstellen ----------
// Alles außer den WLAN-Zugangsdaten als eine JSON-Datei: Einstellungen, Effekt, Presets, Farben, simulierte Wand
String backupJson() {
  JsonDocument d;
  d["trilumag"] = "backup"; d["ver"] = FW_VERSION; d["chip"] = CHIP_FAMILY;
  JsonObject c = d["config"].to<JsonObject>();
  c["bus"] = cfg.bus; c["board"] = cfg.board; c["order"] = cfg.order;
  JsonObject p = c["pins"].to<JsonObject>();
  p["rx"] = cfg.pins.rx; p["tx"] = cfg.pins.tx; p["de"] = cfg.pins.de; p["led"] = cfg.pins.led; p["snsR"] = cfg.pins.snsR; p["snsL"] = cfg.pins.snsL;
  JsonObject m = c["mqtt"].to<JsonObject>();
  m["on"] = cfg.mqttOn; m["host"] = cfg.mqttHost; m["port"] = cfg.mqttPort; m["user"] = cfg.mqttUser; m["pass"] = cfg.mqttPass;
  c["autoUpd"] = cfg.autoUpdate; c["trans"] = cfg.transMs; c["pwrMax"] = cfg.pwrMax; c["pwrCh"] = cfg.pwrCh;
  c["i2cSda"] = cfg.i2cSda; c["i2cScl"] = cfg.i2cScl; c["shunt"] = cfg.shuntUo;
  JsonObject t = c["touch"].to<JsonObject>();
  t["on"] = cfg.touchOn; t["sens"] = cfg.touchSens; t["a1"] = cfg.tapA1; t["a2"] = cfg.tapA2;
  c["pAuto"] = cfg.panelAuto; c["zbOn"] = cfg.zbOn; c["name"] = cfg.name;
  c["guard"] = cfg.guard; c["favs"] = fxFavs; c["onAnim"] = cfg.onAnim; c["tWave"] = cfg.touchWave; c["dayc"] = cfg.daylight; c["fault"] = cfg.faultBlink; c["cpal"] = prefs.getString("cpal", ""); c["viewRot"] = cfg.viewRot; c["viewMir"] = cfg.viewMir;
  c["bootMode"] = cfg.bootMode; c["bootPre"] = cfg.bootPreset; c["syncOn"] = cfg.syncOn; c["syncGrp"] = cfg.syncGroup;
  JsonObject lh = d["lit"].to<JsonObject>();
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].litSec) lh[hex(P[i].chip)] = P[i].litSec;
  { JsonObject eo = d["energy"].to<JsonObject>(); eo["total"] = en.total;
    const char* nm[3] = {"days", "months", "years"}; EBin* ar[3] = {en.days, en.months, en.years}; uint8_t ns[3] = {31, 24, 10};
    for (int k = 0; k < 3; k++) { JsonArray a = eo[nm[k]].to<JsonArray>(); for (uint8_t j = 0; j < ns[k]; j++) if (ar[k][j].key) { JsonArray e = a.add<JsonArray>(); e.add(ar[k][j].key); e.add(ar[k][j].wh); } } }
  JsonObject cl = d["clips"].to<JsonObject>();
  for (int i = 1; i < SLOTS; i++) if (P[i].used && P[i].clips) cl[hex(P[i].chip)] = P[i].clips;
  d["master"] = master; d["on"] = masterOn;
  JsonObject f = d["fx"].to<JsonObject>();
  f["id"] = fx.id; f["speed"] = fx.speed; f["pal"] = fx.pal; f["inten"] = fx.inten; f["r"] = fx.r; f["g"] = fx.g; f["b"] = fx.b; f["w"] = fx.w;
  JsonArray pr = d["presets"].to<JsonArray>();
  for (uint8_t k = 0; k < PRESET_MAX; k++) {
    if (!presetNames[k].length()) continue;
    JsonObject o = pr.add<JsonObject>(); o["slot"] = k;
    JsonDocument pd; deserializeJson(pd, prefs.getString(presetKey(k).c_str(), "{}"));
    o["data"] = pd;
  }
  JsonObject col = d["colors"].to<JsonObject>();
  JsonArray sim = d["sim"].to<JsonArray>();
  JsonArray eg = d["edges"].to<JsonArray>();
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].edges) eg.add(hex(P[i].chip));
  for (int i = 0; i < SLOTS; i++) {
    const Panel& q = P[i];
    if (!q.used) continue;
    if (q.hasColor) { JsonArray a = col[hex(q.chip)].to<JsonArray>(); a.add(q.r); a.add(q.g); a.add(q.b); a.add(q.w); a.add(q.bri); a.add((int)q.on); }
    if (i > 0 && !cfg.bus) { JsonObject o = sim.add<JsonObject>(); o["chip"] = hex(q.chip); o["x"] = q.x; o["y"] = q.y; o["rot"] = q.rot; o["att"] = q.attached; }
  }
  String out; serializeJson(d, out); return out;
}

const char* restoreBackup(JsonDocument& d) {
  if (strcmp(d["trilumag"] | "", "backup")) return "Das ist keine Trilumag-Sicherung";
  JsonObject c = d["config"];
  if (!c.isNull()) {
    prefs.putBool("bus", c["bus"] | false);
    prefs.putString("board", (const char*)(c["board"] | cfg.board.c_str()));
    prefs.putString("order", (const char*)(c["order"] | "RGBW"));
    JsonObject p = c["pins"];
    if (!p.isNull()) {
      PinSet ps = {(int8_t)(p["rx"] | -1), (int8_t)(p["tx"] | -1), (int8_t)(p["de"] | -1), (int8_t)(p["led"] | -1), (int8_t)(p["snsR"] | -1), (int8_t)(p["snsL"] | -1)};
      if (!checkPins(ps)) prefs.putBytes("pins", &ps, sizeof ps);   // Pins eines anderen Chips werden übersprungen
    }
    JsonObject m = c["mqtt"];
    if (!m.isNull()) {
      prefs.putBool("mqttOn", m["on"] | false); prefs.putString("mqttHost", (const char*)(m["host"] | ""));
      prefs.putUShort("mqttPort", m["port"] | 1883); prefs.putString("mqttUser", (const char*)(m["user"] | ""));
      prefs.putString("mqttPass", (const char*)(m["pass"] | ""));
    }
    prefs.putBool("autoUpd", c["autoUpd"] | false);
    prefs.putUShort("trans", c["trans"] | 700); prefs.putUShort("pwrMax", c["pwrMax"] | 0); prefs.putUChar("pwrCh", c["pwrCh"] | 12);
    prefs.putChar("i2cSda", c["i2cSda"] | -1); prefs.putChar("i2cScl", c["i2cScl"] | -1); prefs.putUShort("shunt", c["shunt"] | 50);
    JsonObject t = c["touch"];
    if (!t.isNull()) {
      prefs.putBool("tOn", t["on"] | true); prefs.putUChar("tSens", t["sens"] | 5);
      prefs.putUChar("tA1", t["a1"] | (int)TA_PANEL); prefs.putUChar("tA2", t["a2"] | (int)TA_WALL);
    }
    prefs.putBool("pAuto", c["pAuto"] | true);
    if (HAS_ZIGBEE) prefs.putBool("zbOn", c["zbOn"] | false);
    if (c["name"].is<const char*>() && strlen(c["name"]) > 0) prefs.putString("name", (const char*)c["name"]);
    prefs.putBool("guard", c["guard"] | true); prefs.putUInt("favs", c["favs"] | 0); prefs.putUChar("onAnim", c["onAnim"] | 2); prefs.putBool("tWave", c["tWave"] | true); prefs.putBool("dayc", c["dayc"] | false); prefs.putBool("fault", c["fault"] | true); if (c["cpal"].is<const char*>()) prefs.putString("cpal", c["cpal"].as<const char*>());
    prefs.putUShort("viewRot", c["viewRot"] | 0); prefs.putBool("viewMir", c["viewMir"] | false);
    prefs.putUChar("bootMode", c["bootMode"] | 0); prefs.putChar("bootPre", c["bootPre"] | -1);
    prefs.putBool("syncOn", c["syncOn"] | false); prefs.putUChar("syncGrp", c["syncGrp"] | 1);
  }
  for (JsonPair kv : d["clips"].as<JsonObject>()) prefs.putUInt(("n" + String(kv.key().c_str())).c_str(), kv.value().as<uint32_t>());
  for (JsonPair kv : d["lit"].as<JsonObject>()) prefs.putUInt(("h" + String(kv.key().c_str())).c_str(), kv.value().as<uint32_t>());
  if (!d["energy"].isNull()) {
    EnergyLog e; memset(&e, 0, sizeof e); JsonObject eo = d["energy"]; e.total = eo["total"] | 0.0;
    const char* nm[3] = {"days", "months", "years"}; EBin* ar[3] = {e.days, e.months, e.years}; uint8_t ns[3] = {31, 24, 10};
    for (int k = 0; k < 3; k++) { uint8_t j = 0; for (JsonArray x : eo[nm[k]].as<JsonArray>()) { if (j >= ns[k]) break; ar[k][j].key = x[0]; ar[k][j].wh = x[1]; j++; } }
    prefs.putBytes("energy", &e, sizeof e);
  }
  FxCfg f;
  JsonObject fo = d["fx"];
  if (!fo.isNull()) {
    f.id = fo["id"] | 0; f.speed = fo["speed"] | 50; f.pal = fo["pal"] | 0; f.inten = fo["inten"] | 128;
    f.r = fo["r"] | 255; f.g = fo["g"] | 120; f.b = fo["b"] | 30; f.w = fo["w"] | 0;
    if (f.id < FX_COUNT && f.pal <= PAL_X) prefs.putBytes("fx2", &f, sizeof f);
  }
  prefs.putUChar("master", d["master"] | 255); prefs.putBool("mOn", d["on"] | true);
  for (uint8_t k = 0; k < PRESET_MAX; k++) prefs.remove(presetKey(k).c_str());
  for (JsonObject o : d["presets"].as<JsonArray>()) {
    int k = o["slot"] | -1;
    if (k < 0 || k >= PRESET_MAX) continue;
    String js; serializeJson(o["data"], js);
    prefs.putString(presetKey(k).c_str(), js);
  }
  for (JsonPair kv : d["colors"].as<JsonObject>()) {
    JsonArray a = kv.value();
    if (a.size() < 6) continue;
    uint8_t c6[6] = {a[0], a[1], a[2], a[3], a[4], a[5]};
    prefs.putBytes(("c" + String(kv.key().c_str())).c_str(), c6, 6);
  }
  for (JsonVariant v : d["edges"].as<JsonArray>()) prefs.putBool(("e" + String((const char*)(v | ""))).c_str(), true);
  JsonArray sim = d["sim"];
  if (sim.size()) {
    SimRec r[SLOTS]; uint8_t n = 0;
    for (JsonObject o : sim) { if (n >= SLOTS - 1) break; r[n++] = {parseHex(o["chip"] | "0"), (int8_t)(o["x"] | 0), (int8_t)(o["y"] | 0), (uint8_t)(o["rot"] | 0), (uint8_t)(o["att"] | false)}; }
    prefs.putBytes("simw", r, n * sizeof(SimRec));
  }
  logf("[SICHERUNG] eingespielt, starte neu\n");
  return nullptr;
}


// ---------- Update-Absicherung ----------
// Eine neue Version muss sich nach dem Start bewähren: 45 Sekunden laufen, ohne abzustürzen oder
// zu hängen. Erst dann gilt sie als gut. Startet das Gerät vorher neu (Absturz, Hänger, Stromausfall),
// nimmt der Bootloader automatisch wieder die alte Version.
extern "C" bool verifyRollbackLater() { return true; }   // nicht sofort als gültig markieren (Arduino-Core)
bool otaVerifying = false;
volatile uint32_t otaBeat = 0;
esp_timer_handle_t otaWd = nullptr;
String otaNotice;

void otaBootCheck() {
  esp_ota_img_states_t st;
  const esp_partition_t* run = esp_ota_get_running_partition();
  otaVerifying = run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
  String tried = prefs.getString("otaTry", "");
  if (tried.length() && tried != FW_VERSION) {               // es läuft nicht die Version, die installiert wurde
    otaNotice = "Version " + tried + " ist nicht richtig gestartet. Trilumag läuft wieder mit " + FW_VERSION + ".";
    prefs.putString("otaBad", tried);
    prefs.remove("otaTry");
    logf("[OTA] %s\n", otaNotice.c_str());
  }
  if (otaVerifying) {
    otaBeat = millis();
    esp_timer_create_args_t a = {};
    a.callback = [](void*) { if (millis() - otaBeat > 25000) esp_restart(); };   // hängt die Hauptschleife: neu starten = zurück
    a.name = "otaWd";
    if (esp_timer_create(&a, &otaWd) == ESP_OK) esp_timer_start_periodic(otaWd, 1000000);
    logf("[OTA] neue Version %s muss sich 45 s bewähren\n", FW_VERSION);
  }
}
void otaVerifyLoop() {
  otaBeat = millis();
  if (!otaVerifying || millis() < 45000) return;
  esp_ota_mark_app_valid_cancel_rollback();
  otaVerifying = false;
  if (otaWd) { esp_timer_stop(otaWd); esp_timer_delete(otaWd); otaWd = nullptr; }
  if (prefs.getString("otaTry", "") == FW_VERSION) { otaNotice = "Update auf " + String(FW_VERSION) + " erfolgreich."; prefs.remove("otaTry"); }
  logf("[OTA] Version %s bewährt\n", FW_VERSION);
}

// ---------- Signale und Fortschritt (für Home Assistant und andere) ----------
struct NamedCol { const char* n; uint8_t c[4]; };
const NamedCol NAMED_COLS[] = {
  {"rot", {255, 0, 0, 0}}, {"grün", {0, 255, 0, 0}}, {"gruen", {0, 255, 0, 0}}, {"blau", {0, 0, 255, 0}}, {"gelb", {255, 170, 0, 0}},
  {"orange", {255, 70, 0, 0}}, {"lila", {140, 0, 255, 0}}, {"violett", {140, 0, 255, 0}}, {"pink", {255, 0, 120, 0}},
  {"türkis", {0, 220, 200, 0}}, {"tuerkis", {0, 220, 200, 0}}, {"cyan", {0, 220, 255, 0}}, {"weiß", {0, 0, 0, 255}}, {"weiss", {0, 0, 0, 255}},
  {"red", {255, 0, 0, 0}}, {"green", {0, 255, 0, 0}}, {"blue", {0, 0, 255, 0}}, {"yellow", {255, 170, 0, 0}}, {"white", {0, 0, 0, 255}},
};
// Farbe aus Name ("blau"), "#RRGGBB" oder {"r":..,"g":..,"b":..,"w":..}; false, wenn unbekannt
bool parseColor(JsonVariantConst v, uint8_t* out) {
  if (v.is<JsonObjectConst>()) { out[0] = v["r"] | 0; out[1] = v["g"] | 0; out[2] = v["b"] | 0; out[3] = v["w"] | 0; return true; }
  String t = v | ""; t.trim(); t.toLowerCase();
  if (t.startsWith("#") && t.length() == 7) { uint32_t x = strtoul(t.c_str() + 1, nullptr, 16); out[0] = x >> 16; out[1] = x >> 8; out[2] = x; out[3] = 0; return true; }
  for (const NamedCol& c : NAMED_COLS) if (t == c.n) { memcpy(out, c.c, 4); return true; }
  return false;
}
// {"color":"blau","blink":3,"ms":700} oder als Text "blau 3"
const char* signalCmd(JsonVariantConst d) {
  uint8_t c[4];
  if (!parseColor(d["color"], c)) return "Farbe unbekannt (z. B. rot, grün, blau, gelb, weiß oder #RRGGBB)";
  memcpy(sigCol, c, 4);
  sigBlinks = constrain((int)(d["blink"] | 3), 1, 20);
  sigMs = constrain((int)(d["ms"] | 700), 200, 3000);
  sigAt = millis() | 1; fxLastFrame = 0;
  diag("Signal: %u-mal blinken", sigBlinks);
  return nullptr;
}
const char* signalText(const char* txt) {           // "blau", "grün 2", "#ff8800 5"
  JsonDocument d; String t(txt); t.trim();
  int sp = t.indexOf(' ');
  d["color"] = sp > 0 ? t.substring(0, sp) : t;
  if (sp > 0) d["blink"] = t.substring(sp + 1).toInt();
  return signalCmd(d.as<JsonVariantConst>());
}
// {"value":40,"color":"grün"}; 0 schaltet den Fortschritt aus
const char* progressCmd(JsonVariantConst d) {
  if (!d["color"].isNull()) { uint8_t c[4]; if (!parseColor(d["color"], c)) return "Farbe unbekannt"; memcpy(progCol, c, 4); }
  if (!d["value"].is<float>() && !d["value"].is<int>()) return "Wert fehlt (0 bis 100)";
  float v = constrain(d["value"].as<float>(), 0.0f, 100.0f);
  if ((progVal > 0) != (v > 0)) transition();
  progVal = v; fxLastFrame = 0;
  if (mqtt.connected()) mqtt.publish("trilumag/fortschritt/state", String((int)roundf(v)).c_str(), true);
  return nullptr;
}

// ---------- Panel finden ----------
void identify(int i) { if (i >= 0 && i < SLOTS && P[i].used && P[i].attached) { P[i].identUntil = (millis() + 3000) | 1; fxLastFrame = 0; } }

// ---------- Sleep-Timer ----------
extern bool sleepFiring;
// Nach der eingestellten Zeit geht die Wand aus. In den letzten Minuten blendet sie langsam aus
// (höchstens 5 Minuten, bei kurzen Timern die Hälfte der Zeit). Ein/Aus von Hand bricht den Timer ab.
uint32_t sleepEnd = 0, sleepFade = 0;
bool sleepFiring = false;
void sleepSet(uint16_t min) {
  sleepScale = 1;
  if (!min) { sleepEnd = 0; return; }
  uint32_t dur = (uint32_t)min * 60000UL;
  sleepEnd = (millis() + dur) | 1;
  sleepFade = dur / 2 < 300000UL ? dur / 2 : 300000UL;
  if (!masterOn) { JsonDocument d; d["state"] = "ON"; sleepFiring = true; applyAll(d.as<JsonVariantConst>()); sleepFiring = false; }
}
void sleepCancel() { if (sleepEnd) { sleepEnd = 0; sleepScale = 1; } }
uint32_t sleepLeft() { return sleepEnd ? (uint32_t)((int32_t)(sleepEnd - millis()) > 0 ? sleepEnd - millis() : 0) : 0; }
void sleepLoop() {
  if (!sleepEnd) return;
  uint32_t left = sleepLeft();
  if (!left || !masterOn) {
    bool fire = !left && masterOn;
    sleepEnd = 0; sleepScale = 1;
    if (fire) { JsonDocument d; d["state"] = "OFF"; sleepFiring = true; applyAll(d.as<JsonVariantConst>()); sleepFiring = false; diag("Sleep-Timer: Wand aus"); }
    return;
  }
  sleepScale = left < sleepFade ? (float)left / sleepFade : 1;
  sleepScale *= sleepScale;                          // wirkt fürs Auge gleichmäßiger
}

// ---------- Nach Stromausfall ----------
// Nur nach echtem Stromausfall (nicht nach Update oder Neustart aus der App), sonst bleibt alles wie vorher.
void bootApply() {
  esp_reset_reason_t why = esp_reset_reason();
  if (why != ESP_RST_POWERON && why != ESP_RST_BROWNOUT) return;
  switch (cfg.bootMode) {
  case 1: masterOn = false; break;
  case 2: masterOn = true; break;
  case 3: if (cfg.bootPreset >= 0 && presetNames[cfg.bootPreset].length()) presetLoad(cfg.bootPreset); else masterOn = true; break;
  default: return;
  }
  logf("[START] nach Stromausfall: %s\n", cfg.bootMode == 1 ? "aus" : cfg.bootMode == 2 ? "an" : "Preset");
}

// ---------- Betriebsstunden pro Panel ----------
String litKey(uint32_t chip) { return "h" + hex(chip); }
void litLoad(int i) { P[i].litSec = P[i].litSaved = prefs.getUInt(litKey(P[i].chip).c_str(), 0); }
void litLoop() {
  static uint32_t last = 0, lastSave = 0;
  uint32_t now = millis();
  if (now - last < 1000) return;
  last += 1000; if (now - last > 5000) last = now;
  for (int i = 0; i < SLOTS; i++) {
    if (!P[i].used || !P[i].attached) continue;
    bool lit = false;
    for (int e = 0; e < 3 && !lit; e++) for (int c = 0; c < 4; c++) if (CUR[i][e][c]) { lit = true; break; }
    if (lit) P[i].litSec++;
  }
  if (now - lastSave < 600000UL) return;            // alle 10 Minuten speichern (schont den Flash)
  lastSave = now;
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].litSec != P[i].litSaved) { prefs.putUInt(litKey(P[i].chip).c_str(), P[i].litSec); P[i].litSaved = P[i].litSec; }
}

// ---------- Energieverbrauch ----------
bool timeOk() { return time(nullptr) > 1700000000; }
void enLoad() { if (prefs.getBytes("energy", &en, sizeof en) != sizeof en) memset(&en, 0, sizeof en); }
void enSave() { prefs.putBytes("energy", &en, sizeof en); enDirty = false; }
// Wh in die Liste mit diesem Schlüssel buchen; neuer Zeitraum rückt vorne ein, der älteste fällt raus
void enAdd(EBin* a, uint8_t n, uint32_t key, double wh) {
  if (a[0].key != key) { memmove(a + 1, a, (n - 1) * sizeof(EBin)); a[0].key = key; a[0].wh = 0; }
  a[0].wh += wh;
}
void enBook(double wh) {
  en.total += wh; enDirty = true;
  if (!timeOk()) { enPending += wh; return; }
  wh += enPending; enPending = 0;
  time_t t = time(nullptr); struct tm lt; localtime_r(&t, &lt);
  uint32_t y = lt.tm_year + 1900, m = lt.tm_mon + 1, d = lt.tm_mday;
  enAdd(en.days, 31, y * 10000 + m * 100 + d, wh);
  enAdd(en.months, 24, y * 100 + m, wh);
  enAdd(en.years, 10, y, wh);
}
float enWatts() { return ina::ok ? ina::volts * ina::amps : estMa * 24.0f / 1000.0f; }
void enLoop() {
  static uint32_t last = 0, lastSave = 0; static uint32_t lastDay = 0;
  uint32_t now = millis();
  if (!last) { last = now; lastSave = now; return; }
  if (now - last < 1000) return;
  double h = (now - last) / 3600000.0; last = now;
  enBook(enWatts() * h);
  uint32_t day = en.days[0].key;
  if (enDirty && (now - lastSave > 600000UL || (lastDay && day != lastDay))) { enSave(); lastSave = now; }
  lastDay = day;
}
String energyJson() {
  JsonDocument d;
  d["time"] = timeOk(); d["w"] = roundf(enWatts() * 10) / 10; d["meas"] = ina::ok;
  d["total"] = en.total;
  time_t t = time(nullptr); struct tm lt; localtime_r(&t, &lt);
  uint32_t y = lt.tm_year + 1900, m = lt.tm_mon + 1, dd = lt.tm_mday;
  d["today"] = timeOk() && en.days[0].key == y * 10000 + m * 100 + dd ? en.days[0].wh : 0;
  d["month"] = timeOk() && en.months[0].key == y * 100 + m ? en.months[0].wh : 0;
  d["year"] = timeOk() && en.years[0].key == y ? en.years[0].wh : 0;
  const char* names[3] = {"days", "months", "years"};
  EBin* arr[3] = {en.days, en.months, en.years}; uint8_t ns[3] = {31, 24, 10};
  for (int k = 0; k < 3; k++) {
    JsonArray a = d[names[k]].to<JsonArray>();
    for (uint8_t j = 0; j < ns[k]; j++) if (arr[k][j].key) { JsonArray e = a.add<JsonArray>(); e.add(arr[k][j].key); e.add(roundf(arr[k][j].wh * 10) / 10); }
  }
  String out; serializeJson(d, out); return out;
}

// ---------- Mehrere Wände im Gleichtakt ----------
// Alle Wände einer Gruppe im selben WLAN teilen Ein/Aus, Helligkeit und Effekt mit allen Einstellungen.
// Die letzte Änderung gilt (fortlaufende Nummer), egal an welcher Wand sie gemacht wurde. Den Takt der
// Effekte gibt die Wand mit der kleinsten Chip-ID vor, die anderen gleichen ihre Phase langsam an.
namespace wallsync {
const uint16_t PORT = 21330;
struct __attribute__((packed)) Pkt {
  char magic[4]; uint8_t ver, group, on, master, fx, speed, inten, pal, r, g, b, w, spin;
  uint16_t dir; uint32_t chip, seq; float phase; char name[24];
  uint8_t pn; uint32_t pc[6]; char pname[24];         // ab Version 3: Farben einer eigenen Palette, damit alle Wände dieselben zeigen
};
uint32_t palSig() { const CPal* q = palCustom(fx.pal); uint32_t h = 0; if (q) for (uint8_t k = 0; k < q->n; k++) h = h * 16777619u ^ q->c[k]; return h; }
struct Peer { uint32_t chip = 0, seen = 0; String name, ip; uint8_t group = 0; };
const uint8_t MAXP = 8;
Peer peers[MAXP];
WiFiUDP udp;
bool started = false, applying = false;
uint32_t seq = 0, lastSig = 0, lastBeat = 0;

uint32_t sig() {
  return ((uint32_t)masterOn << 31) ^ ((uint32_t)master << 23) ^ ((uint32_t)fx.id << 17) ^ ((uint32_t)fx.speed << 10) ^ ((uint32_t)fx.inten * 2654435761u) ^
         ((uint32_t)fx.pal << 3) ^ ((uint32_t)fx.r * 40503u + fx.g * 52711u + fx.b * 17u + fx.w * 7u) ^ ((uint32_t)fxDir * 977u) ^ ((uint32_t)fxSpin << 29) ^ palSig();
}
bool timeMaster() {                                  // kleinste Chip-ID der Gruppe gibt den Takt vor
  for (const Peer& p : peers) if (p.chip && millis() - p.seen < 5000 && p.group == cfg.syncGroup && p.chip < P[0].chip) return false;
  return true;
}
void send() {
  if (!started) return;
  Pkt k; memset(&k, 0, sizeof k);
  memcpy(k.magic, "TLSY", 4); k.ver = 3;
  if (const CPal* q = palCustom(fx.pal)) { k.pn = q->n; memcpy(k.pc, q->c, q->n * 4); strncpy(k.pname, q->name.c_str(), sizeof k.pname - 1); } k.dir = fxDir; k.spin = fxSpin; k.group = cfg.syncGroup;
  k.on = masterOn; k.master = master; k.fx = fx.id; k.speed = fx.speed; k.inten = fx.inten; k.pal = fx.pal;
  k.r = fx.r; k.g = fx.g; k.b = fx.b; k.w = fx.w; k.chip = P[0].chip; k.seq = seq; k.phase = fxPhase;
  strncpy(k.name, cfg.name.c_str(), sizeof k.name - 1);
  udp.beginPacket(IPAddress(255, 255, 255, 255), PORT);
  udp.write((const uint8_t*)&k, sizeof k);
  udp.endPacket();
}
void begin() {
  if (started || !cfg.syncOn || !wlanOk) return;
  started = udp.begin(PORT);
  if (started) { lastSig = sig(); send(); diag("Gleichtakt mit anderen Wänden an (Gruppe %u)", cfg.syncGroup); }
}
void stop() { if (started) { udp.stop(); started = false; } for (Peer& p : peers) p = Peer(); }

bool palDiffers(const Pkt& k) {
  const CPal* q = palCustom(fx.pal);
  if (k.pn < 2) return q != nullptr || k.pal != fx.pal;      // eingebaute Palette: Nummer vergleichen
  return !q || q->n != k.pn || memcmp(q->c, k.pc, k.pn * 4);
}
bool differs(const Pkt& k) {
  return palDiffers(k) || k.on != masterOn || k.master != master || k.fx != fx.id || k.speed != fx.speed || k.inten != fx.inten ||
         k.r != fx.r || k.g != fx.g || k.b != fx.b || k.w != fx.w || k.dir != fxDir || k.spin != fxSpin;
}
// Zustand einer anderen Wand übernehmen, ohne ihn selbst wieder zu senden
void adopt(const Pkt& k) {
  applying = true;
  bool look = k.on != masterOn || k.master != master;
  masterOn = k.on; master = k.master;
  fx.speed = k.speed; fx.inten = k.inten; if (k.pn >= 2) { char nm[25]; memcpy(nm, k.pname, 24); nm[24] = 0; palAdopt(k.pn, k.pc, nm); }
  else fx.pal = k.pal < PAL_COUNT ? k.pal : 0;
  fx.r = k.r; fx.g = k.g; fx.b = k.b; fx.w = k.w;
  fxDir = k.dir % 360; fxSpin = k.spin % 3;
  if (k.fx < FX_COUNT && k.fx != fx.id) fxStart(k.fx);
  else { fxDirty = true; fxDirtyAt = millis(); fxLastFrame = 0; publishFx(); }
  if (look) resendAll();
  curPreset = -1;
  lastSig = sig();
  applying = false;
  wsKick();
}

void loop() {
  if (!cfg.syncOn) { if (started) stop(); return; }
  if (!started) { begin(); return; }
  uint32_t now = millis();
  int n;
  while ((n = udp.parsePacket()) > 0) {
    Pkt k;
    if (n != (int)sizeof k) { udp.flush(); continue; }
    udp.read((uint8_t*)&k, sizeof k);
    if (memcmp(k.magic, "TLSY", 4) || k.ver != 3 || k.chip == P[0].chip) continue;
    int slot = -1, old = 0;
    for (int i = 0; i < MAXP; i++) { if (peers[i].chip == k.chip) { slot = i; break; } if (peers[i].seen < peers[old].seen) old = i; }
    if (slot < 0) { slot = old; if (peers[slot].chip == 0 || now - peers[slot].seen > 30000) diag("Wand „%.*s“ gefunden (Gruppe %u)", 24, k.name, k.group); }
    Peer& pe = peers[slot];
    pe.chip = k.chip; pe.seen = now; pe.group = k.group; pe.ip = udp.remoteIP().toString();
    char nm[25]; memcpy(nm, k.name, 24); nm[24] = 0; pe.name = nm;
    if (k.group != cfg.syncGroup) continue;
    // neuere Änderung übernehmen; bei gleicher Nummer gewinnt die kleinere Chip-ID
    if (k.seq > seq || (k.seq == seq && k.chip < P[0].chip && differs(k))) { seq = k.seq; if (differs(k)) adopt(k); }
    // Takt: der Taktgeber hat die kleinste Chip-ID
    if (k.chip < P[0].chip && k.fx == fx.id && fx.id) {
      bool master_ = true;
      for (const Peer& p : peers) if (p.chip && p.chip < k.chip && now - p.seen < 5000 && p.group == cfg.syncGroup) master_ = false;
      if (master_) {
        float d = k.phase - fxPhase;
        if (fabsf(d) > 2) fxPhase = k.phase; else fxPhase += d * 0.3f;   // springen oder sanft nachziehen
      }
    }
  }
  uint32_t s = sig();
  if (s != lastSig && !applying) {                    // hier geändert (App, Home Assistant, Antippen …): allen sagen
    lastSig = s; seq++; send(); lastBeat = now;
  }
  if (now - lastBeat > 1000) { lastBeat = now; send(); }   // Lebenszeichen mit Takt und Zustand
}

void json(JsonObject o) {
  o["on"] = cfg.syncOn; o["group"] = cfg.syncGroup; o["run"] = started; o["lead"] = started && timeMaster();
  JsonArray a = o["walls"].to<JsonArray>();
  for (const Peer& p : peers) {
    if (!p.chip || millis() - p.seen > 10000) continue;
    JsonObject w = a.add<JsonObject>(); w["name"] = p.name; w["ip"] = p.ip; w["group"] = p.group;
  }
}
}  // namespace wallsync

// ---------- Philips Hue über Zigbee (nur ESP32-C6) ----------
uint32_t restartAt = 0, zbResetAt = 0;
#if HAS_ZIGBEE
void zbStart() {
  if (!cfg.zbOn) return;
  uint8_t tries = prefs.getUChar("zbBoot", 0);        // dreimal hintereinander beim Start hängen geblieben: Zigbee aus
  if (tries >= 3) {
    cfg.zbOn = false; prefs.putBool("zbOn", false); prefs.remove("zbBoot");
    diag("Zigbee abgeschaltet: Das Hauptpanel ist damit dreimal nicht richtig gestartet");
    return;
  }
  prefs.putUChar("zbBoot", tries + 1);
  if (zb::begin()) diag("Zigbee läuft: in der Hue-App als „Trilumag Wand“ suchen");
  else diag("Zigbee: %s", zb::problem.c_str());
}

void zbLoop() {
  if (!zb::started) return;
  static bool bootOk = false;
  if (!bootOk && millis() > 60000) { bootOk = true; prefs.remove("zbBoot"); }
  if (zb::pending) {                                    // Befehl von der Hue Bridge
    bool on, hOn, hLv, hCol; uint8_t lv; uint16_t x, y;
    portENTER_CRITICAL(&zb::mux);
    on = zb::pOn; lv = zb::pLevel; x = zb::pX; y = zb::pY; hOn = zb::hasOn; hLv = zb::hasLevel; hCol = zb::hasColor;
    zb::pending = zb::hasOn = zb::hasLevel = zb::hasColor = false;
    portEXIT_CRITICAL(&zb::mux);
    JsonDocument d;
    if (hOn) d["state"] = on ? "ON" : "OFF";
    if (hLv) d["brightness"] = lv < 1 ? 1 : lv;
    if (hCol) {                                         // der weiße Anteil geht auf die weißen LEDs (RGBW)
      uint8_t r, g, b; zb::xyToRgb(x, y, r, g, b);
      uint8_t w = min(r, min(g, b));
      d["color"]["r"] = r - w; d["color"]["g"] = g - w; d["color"]["b"] = b - w; d["color"]["w"] = w;
    }
    applyAll(d.as<JsonVariantConst>());
    wsKick();
  }
  static uint32_t last = 0; static uint32_t lastSig = 0;
  if (millis() - last < 1000) return;                   // eigene Änderungen (App, Home Assistant) einmal pro Sekunde melden
  last = millis();
  bool col = fxUsesColor();
  uint8_t w = col ? fx.w : P[0].w;
  uint8_t r = min(255, (col ? fx.r : P[0].r) + w), g = min(255, (col ? fx.g : P[0].g) + w), b = min(255, (col ? fx.b : P[0].b) + w);
  if (!r && !g && !b) r = g = b = 255;
  uint32_t sig = ((uint32_t)masterOn << 24) ^ ((uint32_t)master << 16) ^ ((uint32_t)r * 7919 + g * 104729 + b * 31);
  if (sig == lastSig) return;
  lastSig = sig;
  zb::report(masterOn, master, r, g, b);
}
#else
void zbStart() {}
void zbLoop() {}
#endif

// ---------- Wächter und Neustart-Grund ----------
const char* reasonText(esp_reset_reason_t r, uint8_t code) {
  switch (r) {
  case ESP_RST_POWERON: return "Strom eingeschaltet oder Stromausfall";
  case ESP_RST_BROWNOUT: return "Spannung zu niedrig (Netzteil prüfen)";
  case ESP_RST_PANIC: return "Absturz";
  case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "hing, vom Watchdog neu gestartet";
  case ESP_RST_EXT: return "Reset-Taster";
  case ESP_RST_SW:
    switch (code) {
    case R_UPDATE: return "nach einem Update";
    case R_APP: return "aus der App (Einstellungen, Sicherung oder WLAN)";
    case R_WLAN: return "vom Wächter: WLAN war 10 Minuten weg";
    case R_HANG: return "vom Wächter: Hauptschleife hing";
    default: return "Neustart (Software)";
    }
  default: return "unbekannt";
  }
}
void guardBoot() {
  esp_reset_reason_t r = esp_reset_reason();
  uint8_t code = rstMagic == 0x7121A600 ? rstCode : 0;
  rstMagic = 0; rstCode = 0;
  bootReason = reasonText(r, code);
  bool crash = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
  if (crash) prefs.putUInt("crashes", prefs.getUInt("crashes", 0) + 1);
  // mehrmals hintereinander gleich nach dem Start abgestürzt: diesmal ohne Hue- und WLED-Nachahmen starten
  uint8_t early = crash ? prefs.getUChar("bootCrash", 0) + 1 : 0;
  prefs.putUChar("bootCrash", early);
  safeBoot = early >= 3;
  diag("Gestartet: %s", bootReason.c_str());
  if (safeBoot) diag("Sicherer Start: Hue- und WLED-Nachahmen bleiben aus");
  // läuft die Hauptschleife eine Minute nicht mehr, neu starten (nur mit eingeschaltetem Wächter)
  static esp_timer_handle_t t = nullptr;
  esp_timer_create_args_t a = {};
  a.callback = [](void*) { if (cfg.guard && millis() - loopBeat > 60000) { rstMagic = 0x7121A600; rstCode = R_HANG; esp_restart(); } };
  a.name = "guard";
  loopBeat = millis();
  if (esp_timer_create(&a, &t) == ESP_OK) esp_timer_start_periodic(t, 5000000);
}
// WLAN weg: nach 3 Minuten neu verbinden, nach 10 Minuten neu starten (nicht, solange jemand im Einrichtungs-WLAN ist)
void guardLoop() {
  loopBeat = millis();
  static bool bootOk = false;
  if (!bootOk && millis() > 60000) { bootOk = true; if (prefs.getUChar("bootCrash", 0)) prefs.putUChar("bootCrash", 0); }   // eine Minute gelaufen: Start war in Ordnung
  static uint32_t lostAt = 0, lastMdns = 0; static bool retried = false;
  if (!cfg.guard) { lostAt = 0; return; }
  uint32_t now = millis();
  String ss = prefs.getString("ssid", "");
  if (wlanOk || !ss.length() || wPhase == W_CONNECTING) {
    lostAt = 0; retried = false;
    if (wlanOk && now - lastMdns > 1800000UL) { lastMdns = now; mdnsStart(); }   // trilumag.local alle 30 Minuten neu ankündigen
    return;
  }
  if (!lostAt) { lostAt = now | 1; return; }
  if (!retried && now - lostAt > 180000UL) {
    retried = true;
    diag("Wächter: WLAN seit 3 Minuten weg, verbinde neu");
    WiFi.disconnect(false); wifiStart(ss, prefs.getString("pass", ""), 20000, false);
  }
  if (now - lostAt > 600000UL && WiFi.softAPgetStationNum() == 0) {
    diag("Wächter: WLAN seit 10 Minuten weg, starte neu");
    fxSave(); saveColors(); enSave();
    restartWith(R_WLAN);
  }
}

// ---------- Online-Updates ----------
// Vor der Installation alle Verbindungen nach außen trennen: laufende Abfragen abwarten, MQTT sauber abmelden
void mqttStatus(const char* s);
void otaQuiet(bool on) {
  if (!on) { otaBusy = false; netQuiet = false; logf("[OTA] Verbindungen wieder frei\n"); return; }
  otaBusy = true; netQuietOn();
  for (int i = 0; i < 800 && (wx::busy || peers::busy); i++) delay(10);     // Wetter oder Suche fertig laufen lassen
  hueb::closeConn();
  if (mqtt.connected()) { mqttStatus("Update läuft"); mqtt.loop(); delay(50); mqtt.disconnect(); }
  netClient.stop();
  logf("[OTA] Verbindungen getrennt (MQTT, Hue, WLED, Wetter, Suche)\n");
}
bool otaCheckNow = false;
bool otaLatestAfterCheck = false;     // Notfall-Seite: nach der Abfrage die neueste Version installieren
String otaInstallVer;
String otaWantVer, otaAfterPeers;      // erst nach der Suche bekannte Version; eigenes Update nach dem Weitergeben
uint32_t otaNext = 0;
const uint32_t OTA_EVERY = 6UL * 3600 * 1000;    // alle 6 Stunden nachsehen

String otaJson() {
  JsonDocument d;
  d["cur"] = FW_VERSION; d["chip"] = ota::CHIP_KEY; d["auto"] = cfg.autoUpdate;
  d["latest"] = ota::latest();
  d["newer"] = ota::count && ota::cmp(ota::latest(), FW_VERSION) > 0;
  d["ago"] = ota::lastCheck ? (long)((millis() - ota::lastCheck) / 1000) : -1;
  d["busy"] = otaCheckNow || otaInstallVer.length() > 0 || ota::progress >= 0;
  d["p"] = ota::progress; d["err"] = ota::error;
  d["notice"] = otaNotice; d["bad"] = prefs.getString("otaBad", ""); d["verifying"] = otaVerifying;
  JsonArray a = d["versions"].to<JsonArray>();
  for (uint8_t k = 0; k < ota::count; k++) {
    JsonObject o = a.add<JsonObject>(); o["v"] = ota::list[k].v; o["date"] = ota::list[k].date; o["notes"] = ota::list[k].notes;
  }
  String out; serializeJson(d, out); return out;
}
void otaPush() { ws::broadcast("{\"t\":\"ota\",\"d\":" + otaJson() + "}"); }

void otaLoop() {
  if (!wlanOk) return;
  uint32_t now = millis();
  if (otaAfterPeers.length() && !peers::pushing) { otaInstallVer = otaAfterPeers; otaAfterPeers = ""; }   // erst die anderen, dann diese Wand
  if (!otaNext) otaNext = now + 15000;                         // erste Abfrage 15 s nach dem WLAN
  if ((int32_t)(now - otaNext) >= 0) { otaCheckNow = true; otaNext = now + OTA_EVERY; }
  if (otaCheckNow) {
    otaCheckNow = false;
    netQuietOn();
    bool ok = ota::check();
    netQuiet = false;
    logf("[OTA] %s\n", ok ? ("neueste Version " + ota::latest()).c_str() : ota::error.c_str());
    otaPush(); wsForce = true;
    if (ok && cfg.autoUpdate && ota::cmp(ota::latest(), FW_VERSION) > 0 && ota::latest() != prefs.getString("otaBad", "")) otaInstallVer = ota::latest();
    if (ok && otaLatestAfterCheck && ota::latest() != FW_VERSION) otaInstallVer = ota::latest();
    if (otaWantVer.length()) { if (ok) for (uint8_t k = 0; k < ota::count; k++) if (ota::list[k].v == otaWantVer) (peers::pushing ? otaAfterPeers : otaInstallVer) = otaWantVer; otaWantVer = ""; }
    otaLatestAfterCheck = false;
  }
  if (otaInstallVer.length()) {
    String v = otaInstallVer; otaInstallVer = "";
    logf("[OTA] installiere %s …\n", v.c_str());
    saveColors(); fxSave(); if (simDirty) simSave();           // nichts verlieren
    prefs.putString("otaTry", v);                              // nach dem Neustart prüfen, ob sie wirklich läuft
    otaQuiet(true);
    bool ok = ota::install(v);
    if (!ok) otaQuiet(false);                                  // sonst startet sie gleich neu
    if (!ok) prefs.remove("otaTry");
    otaPush();
    if (ok) {
      logf("[OTA] fertig, starte neu\n");
      ws::broadcast("{\"t\":\"otadone\",\"v\":\"" + v + "\"}");
      delay(600);
      restartWith(R_UPDATE);
    }
    logf("[OTA] %s\n", ota::error.c_str());
  }
}

// Ein Befehl der App, egal ob über HTTP oder WebSocket. Liefert eine Fehlermeldung oder nullptr.
const char* apiCall(const char* path, JsonDocument& d) {
  if (!strcmp(path, "/api/set")) {
    kelvinToColor(d);
    const char* id = d["id"] | "";
    if (d["panelBri"].is<int>()) {                      // Helligkeit aller Panels auf einmal (auch der in der Ablage)
      uint8_t v = constrain(d["panelBri"].as<int>(), 1, 255);
      for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].bri != v) {
        P[i].bri = v;
        if (P[i].attached) { sendToPanel(i); publishState(i); }
      }
      curPreset = -1; colorsDirty = true; colorsDirtyAt = millis();
      return nullptr;
    }
    if (d["ids"].is<JsonArrayConst>()) {                 // mehrere ausgewählte Panels
      for (JsonVariantConst v : d["ids"].as<JsonArrayConst>()) { int i = findChip(parseHex(v | "0")); if (i >= 0) applyCommand(i, d.as<JsonVariantConst>()); }
    } else if (!strcmp(id, "alle")) applyAll(d.as<JsonVariantConst>());
    else { int i = findChip(parseHex(id)); if (i < 0) return "Panel unbekannt"; applyCommand(i, d.as<JsonVariantConst>()); }
    return nullptr;
  }
  // Presets: {"action":"save","name":"Abend"} / {"action":"load","id":2} / {"action":"delete","id":2}
  if (!strcmp(path, "/api/presets")) {
    const char* a = d["action"] | "";
    int id = d["id"] | -1;
    if (!strcmp(a, "save")) {
      String name = d["name"] | "";
      name.trim();
      if (!name.length()) return "Name fehlt";
      if (name.length() > 24) name = name.substring(0, 24);
      if (presetSave(id, name.c_str()) < 0) return "Alle 16 Plätze belegt";
    } else if (!strcmp(a, "load")) {
      if (!presetLoad(id)) return "Preset unbekannt";
    } else if (!strcmp(a, "delete")) presetDelete(id);
    else return "Unbekannte Aktion";
    return nullptr;
  }
  if (!strcmp(path, "/api/effect")) {
    if (d["effect"].is<const char*>() && fxFind(d["effect"].as<const char*>()) < 0) return "Effekt unbekannt";
    applyAll(d.as<JsonVariantConst>());
    return nullptr;
  }
  if (!strcmp(path, "/api/test")) {
    int ch = d["ch"] | -1;
    uint8_t c[4] = {0, 0, 0, 0};
    if (ch >= 0 && ch < 4) c[ch] = 70;
    testMode = ch >= 0 && ch < 4;                 // solange der Test läuft, schickt die Ausgabe nichts
    if (cfg.bus) bus::send(bus::ALL, bus::C_COLOR, c, 4);
    if (cfg.pins.led >= 0) { for (int k = 0; k < 3; k++) strip.setPixelColor(k, strip.Color(c[0], c[1], c[2], c[3])); strip.show(); }
    if (!testMode) outForce = true;               // Test beenden: wieder das normale Bild
    return nullptr;
  }
  if (!strcmp(path, "/api/diag")) {                // Zähler zurücksetzen
    busFramesSent = busCrcErr = busTimeouts = busDiscovers = 0;
    for (int i = 0; i < SLOTS; i++) { P[i].pings = P[i].missed = 0; P[i].rttMax = 0; }
    diagCount = 0; diagHead = 0;
    return nullptr;
  }
  // Licht-Einstellungen ohne Neustart: {"trans":700,"pwrMax":5000,"pwrCh":12}
  if (!strcmp(path, "/api/light")) {
    if (d["trans"].is<int>()) { cfg.transMs = constrain(d["trans"].as<int>(), 0, 10000); prefs.putUShort("trans", cfg.transMs); }
    if (d["onAnim"].is<int>()) { cfg.onAnim = constrain(d["onAnim"].as<int>(), 0, 3); prefs.putUChar("onAnim", cfg.onAnim); }
    if (d["daylight"].is<bool>()) { cfg.daylight = d["daylight"]; prefs.putBool("dayc", cfg.daylight); outForce = true; }
    if (d["pwrMax"].is<int>()) { cfg.pwrMax = constrain(d["pwrMax"].as<int>(), 0, 60000); prefs.putUShort("pwrMax", cfg.pwrMax); measScale = 1; }
    if (d["pwrCh"].is<int>()) { cfg.pwrCh = constrain(d["pwrCh"].as<int>(), 1, 100); prefs.putUChar("pwrCh", cfg.pwrCh); }
    if (d["sda"].is<int>() || d["scl"].is<int>() || d["shunt"].is<int>()) {   // Stromsensor, gilt sofort
      int sda = d["sda"] | (int)cfg.i2cSda, scl = d["scl"] | (int)cfg.i2cScl;
      if ((sda < 0) != (scl < 0)) return "Für den Stromsensor beide Pins wählen oder keinen";
      if (sda >= 0) {
        if (!pinValid(sda) || !pinValid(scl) || sda == scl) return "Pins für den Stromsensor ungültig";
        const int8_t used[6] = {cfg.pins.rx, cfg.pins.tx, cfg.pins.de, cfg.pins.led, cfg.pins.snsR, cfg.pins.snsL};
        for (int8_t u : used) if (u == sda || u == scl) return "Pin ist schon für den Bus oder die LEDs belegt";
      }
      cfg.i2cSda = sda; cfg.i2cScl = scl;
      if (d["shunt"].is<int>()) cfg.shuntUo = constrain(d["shunt"].as<int>(), 1, 10000);
      prefs.putChar("i2cSda", sda); prefs.putChar("i2cScl", scl); prefs.putUShort("shunt", cfg.shuntUo);
      ina::begin();
      if (mqtt.connected()) publishExtras();          // Spannung-Sensor in Home Assistant ein- oder ausblenden
    }
    outForce = true;
    return nullptr;
  }
  // Updates: {"action":"check"} / {"action":"install","version":"0.6.13"} / {"action":"auto","on":true}
  if (!strcmp(path, "/api/ota")) {
    const char* a = d["action"] | "";
    if (!strcmp(a, "check")) { if (!wlanOk) return "Kein WLAN"; otaCheckNow = true; return nullptr; }
    if (!strcmp(a, "latest")) { if (!wlanOk) return "Kein WLAN"; otaCheckNow = true; otaLatestAfterCheck = true; return nullptr; }
    if (!strcmp(a, "install")) {
      if (!wlanOk) return "Kein WLAN";
      String v = d["version"] | "";
      bool self = d["self"] | true;
      bool known = false;
      for (uint8_t k = 0; k < ota::count; k++) if (ota::list[k].v == v) known = true;
      if (!known && !v.length()) return "Version unbekannt, bitte zuerst nach Updates suchen";
      // auch auf anderen Trilumag im WLAN installieren: {"peers":["192.168.1.61",…]}
      if (d["peers"].is<JsonArrayConst>() && d["peers"].size()) {
        if (!peers::push(v, d["peers"].as<JsonArrayConst>())) return "Weitergeben läuft schon";
        if (self && v != FW_VERSION) { if (known) otaAfterPeers = v; else { otaWantVer = v; otaCheckNow = true; } }
        return nullptr;
      }
      if (!self) return nullptr;
      if (!known) { otaWantVer = v; otaCheckNow = true; return nullptr; }   // erst nach Updates suchen, dann installieren
      otaInstallVer = v;
      return nullptr;
    }
    if (!strcmp(a, "auto")) {
      cfg.autoUpdate = d["on"] | false;
      prefs.putBool("autoUpd", cfg.autoUpdate);
      if (cfg.autoUpdate && ota::count && ota::cmp(ota::latest(), FW_VERSION) > 0 && ota::latest() != prefs.getString("otaBad", "")) otaInstallVer = ota::latest();
      otaPush();
      return nullptr;
    }
    return "Unbekannte Aktion";
  }
  // Antippen: {"on":true,"sens":5,"a1":1,"a2":2}
  if (!strcmp(path, "/api/touch")) {
    if (d["on"].is<bool>()) { cfg.touchOn = d["on"]; prefs.putBool("tOn", cfg.touchOn); }
    if (d["sens"].is<int>()) { cfg.touchSens = constrain(d["sens"].as<int>(), 1, 10); prefs.putUChar("tSens", cfg.touchSens); }
    if (d["a1"].is<int>() && d["a1"].as<int>() >= 0 && d["a1"].as<int>() < TA_COUNT) { cfg.tapA1 = d["a1"]; prefs.putUChar("tA1", cfg.tapA1); }
    if (d["a2"].is<int>() && d["a2"].as<int>() >= 0 && d["a2"].as<int>() < TA_COUNT) { cfg.tapA2 = d["a2"]; prefs.putUChar("tA2", cfg.tapA2); }
    if (d["wave"].is<bool>()) { cfg.touchWave = d["wave"]; prefs.putBool("tWave", cfg.touchWave); }
    if (cfg.bus) bus::sendTouch(bus::ALL);
    return nullptr;
  }
  // Panel-Firmware: {"action":"all"} / {"action":"one","id":"…"} / {"action":"auto","on":true}
  if (!strcmp(path, "/api/panelfw")) {
    const char* a = d["action"] | "";
    if (!strcmp(a, "auto")) { cfg.panelAuto = d["on"] | true; prefs.putBool("pAuto", cfg.panelAuto); return nullptr; }
    int n = 0;
    for (int i = 1; i < SLOTS; i++) {
      if (!strcmp(a, "one") && P[i].chip != parseHex(d["id"] | "0")) continue;
      if (pupd::canUpdate(P[i]) && P[i].upd != 2) { P[i].upd = 1; P[i].updFails = 0; n++; }
    }
    if (strcmp(a, "all") && strcmp(a, "one")) return "Unbekannte Aktion";
    return n ? nullptr : "Kein Panel braucht ein Update";
  }
  if (!strcmp(path, "/api/signal")) return signalCmd(d.as<JsonVariantConst>());
  if (!strcmp(path, "/api/progress")) return progressCmd(d.as<JsonVariantConst>());
  // Ansicht: {"rot":60,"mir":false} so hängt die Wand
  if (!strcmp(path, "/api/view")) {
    if (d["rot"].is<int>()) { cfg.viewRot = ((d["rot"].as<int>() % 360 + 360) % 360) / 30 * 30; prefs.putUShort("viewRot", cfg.viewRot); }
    if (d["mir"].is<bool>()) { cfg.viewMir = d["mir"]; prefs.putBool("viewMir", cfg.viewMir); }
    return nullptr;
  }
  // Favoriten: {"effect":"lauflicht","on":true}
  if (!strcmp(path, "/api/favs")) {
    int k = fxFind(d["effect"] | ""); if (k < 0) return "Effekt unbekannt";
    if (d["on"] | true) fxFavs |= 1u << k; else fxFavs &= ~(1u << k);
    prefs.putUInt("favs", fxFavs);
    return nullptr;
  }
  // Panel tauschen: {"id":"altes Panel"} startet, {"stop":true} bricht ab
  if (!strcmp(path, "/api/swap")) {
    if (d["stop"] | false) { swapSlot = -1; return nullptr; }
    int i = findChip(parseHex(d["id"] | "0"));
    if (i <= 0 || !P[i].attached) return "Panel unbekannt";
    swapSlot = i; swapUntil = millis() + SWAP_MS; swapDoneAt = 0; memset(swapKid, 0, sizeof swapKid);
    diag("Tausch von Panel %s gestartet: 5 Minuten Zeit", hex(P[i].chip).substring(4).c_str());
    return nullptr;
  }
  // Wächter: {"on":true}
  if (!strcmp(path, "/api/guard")) { cfg.guard = d["on"] | true; prefs.putBool("guard", cfg.guard); return nullptr; }
  // Panel finden: {"id":"…"} blinkt 3 s weiß
  if (!strcmp(path, "/api/identify")) {
    int i = findChip(parseHex(d["id"] | "0"));
    if (i < 0 || !P[i].attached) return "Panel unbekannt";
    identify(i);
    return nullptr;
  }
  // Sleep-Timer: {"min":30}, 0 = abbrechen
  if (!strcmp(path, "/api/sleep")) {
    int m = d["min"] | -1;
    if (m < 0 || m > 720) return "Zeit ungültig";
    sleepSet(m);
    if (m) diag("Sleep-Timer: Wand geht in %d min aus", m);
    return nullptr;
  }
  // Nach Stromausfall: {"mode":0..3,"preset":2}
  if (!strcmp(path, "/api/boot")) {
    int m = d["mode"] | -1;
    if (m < 0 || m > 3) return "Unbekannte Einstellung";
    if (m == 3) { int k = d["preset"] | -1; if (k < 0 || k >= PRESET_MAX || !presetNames[k].length()) return "Bitte ein Preset wählen"; cfg.bootPreset = k; prefs.putChar("bootPre", k); }
    cfg.bootMode = m; prefs.putUChar("bootMode", m);
    return nullptr;
  }
  // Energie: {"action":"reset"} setzt alle Zähler zurück
  // Wetter: {"place":"Wien"} sucht den Ort und holt das Wetter, {"place":""} löscht ihn, {} holt nur neu
  // Eigene Paletten: {"action":"save","slot":0,"name":"…","colors":["#FF0000","#FFB000","#FFFFFF"]} / {"action":"delete","slot":0}
  if (!strcmp(path, "/api/palette")) {
    const char* a = d["action"] | "";
    int sl = d["slot"] | -1;
    if (!strcmp(a, "save")) {
      if (sl < 0) for (int i = 0; i < CPAL_MAX && sl < 0; i++) if (!cpal[i].used) sl = i;
      if (sl < 0 || sl >= CPAL_MAX) return "Alle 4 eigenen Paletten belegt";
      JsonArrayConst cs = d["colors"].as<JsonArrayConst>();
      if (cs.size() < 2 || cs.size() > 6) return "Eine Palette braucht 2 bis 6 Farben";
      String nm = d["name"] | ""; nm.trim(); if (!nm.length()) nm = "Eigene " + String(sl + 1); if (nm.length() > 24) nm = nm.substring(0, 24);
      CPal& q = cpal[sl]; q.n = 0;
      for (JsonVariantConst v : cs) { const char* h = v | "#000000"; q.c[q.n++] = strtoul(h[0] == '#' ? h + 1 : h, nullptr, 16) & 0xFFFFFF; }
      q.name = nm; q.used = true; cpalSave();
      if (d["use"] | false) fx.pal = PAL_COUNT + sl;
      fxDirty = true; fxDirtyAt = millis(); fxLastFrame = 0;
      if (mqtt.connected()) publishExtras();
      publishFx(); return nullptr;
    }
    if (!strcmp(a, "delete")) {
      if (sl < 0 || sl >= CPAL_MAX || !cpal[sl].used) return "Palette unbekannt";
      cpal[sl].used = false; cpalSave();
      if (fx.pal == PAL_COUNT + sl) { fx.pal = 0; fxDirty = true; fxDirtyAt = millis(); }
      if (mqtt.connected()) publishExtras();
      publishFx(); return nullptr;
    }
    return "Unbekannte Aktion";
  }
  // Hue-Lampe nachahmen: {"action":"find"} | {"action":"pair","ip":"…"} | {"action":"lights"} | {"action":"follow","id":"…","name":"…"} | {"action":"stop"} | {"action":"forget"}
  if (!strcmp(path, "/api/hue")) {
    const char* a = d["action"] | "";
    if (!wlanOk && strcmp(a, "stop") && strcmp(a, "forget")) return "Kein WLAN";
    if (hueb::busy && strcmp(a, "stop")) return "Hue: einen Moment, es läuft noch etwas";
    if (!strcmp(a, "find")) { hueb::start("find"); return nullptr; }
    if (!strcmp(a, "pair")) { String i = d["ip"] | hueb::ip; if (!i.length()) return "Bitte zuerst die Bridge suchen oder ihre IP eingeben"; hueb::ip = i; hueb::start("pair"); return nullptr; }
    if (!strcmp(a, "lights")) { if (!hueb::key.length()) return "Bitte zuerst koppeln"; hueb::start("lights"); return nullptr; }
    if (!strcmp(a, "follow")) { String id = d["id"] | ""; if (!id.length() || !hueb::key.length()) return "Lampe unbekannt"; mirror::stop(); prefs.putString("wledIp", ""); hueb::follow(id, d["name"] | ""); diag("Ahmt die Hue-Lampe „%s“ nach", hueb::lname.c_str()); return nullptr; }
    if (!strcmp(a, "stop")) { hueb::stop(); return nullptr; }
    if (!strcmp(a, "forget")) { hueb::stop(); hueb::key = ""; hueb::nL = 0; prefs.remove("hueKey"); return nullptr; }
    return "Unbekannte Aktion";
  }
  // WLED nachahmen: {"ip":"192.168.1.70"} oder {"ip":""} zum Beenden
  if (!strcmp(path, "/api/mirror")) {
    String a = d["ip"] | "";
    prefs.putString("wledIp", a);
    if (a.length()) { hueb::stop(); mirror::start(a); } else mirror::stop();
    if (a.length()) diag("Ahmt das WLED-Gerät %s nach", a.c_str()); else diag("WLED nachahmen aus");
    return nullptr;
  }
  // Spiel: {"action":"start"} / {"action":"stop"}
  if (!strcmp(path, "/api/game")) {
    const char* a = d["action"] | "";
    if (!strcmp(a, "start")) return game::start();
    if (!strcmp(a, "stop")) { game::stop(); return nullptr; }
    return "Unbekannte Aktion";
  }
  // Störungsanzeige: {"on":true}
  if (!strcmp(path, "/api/fault")) { cfg.faultBlink = d["on"] | true; prefs.putBool("fault", cfg.faultBlink); return nullptr; }
  if (!strcmp(path, "/api/weather")) {
    if (!wlanOk) return "Kein WLAN";
    if (wx::busy) return "Wetter wird gerade geholt";
    if (d["place"].is<const char*>()) {
      String pl = d["place"].as<const char*>(); pl.trim();
      if (!pl.length()) { wx::place = ""; wxOk = false; wx::err = ""; prefs.remove("wxPlace"); return nullptr; }
      wx::want = pl.substring(0, 60);
    }
    wx::start();
    return nullptr;
  }
  if (!strcmp(path, "/api/peers")) { if (!wlanOk) return "Kein WLAN"; peers::start(); return nullptr; }
  if (!strcmp(path, "/api/energy")) {
    if (strcmp(d["action"] | "", "reset")) return "Unbekannte Aktion";
    memset(&en, 0, sizeof en); enPending = 0; enSave();
    diag("Energiezähler zurückgesetzt");
    return nullptr;
  }
  // Gleichtakt: {"on":true,"group":1}
  // Raum: {"action":"save","layout":{…}} / {"action":"fx","fx":{"effect":"raum-komet",…}} – geht an alle Wände im Raum
  if (!strcmp(path, "/api/room")) {
    const char* a = d["action"] | "";
    bool fwd = d["fwd"] | true;
    if (!strcmp(a, "save")) { if (!d["layout"].is<JsonObjectConst>()) return "Raum fehlt"; return room::save(d["layout"].as<JsonVariantConst>(), fwd); }
    if (!strcmp(a, "fx")) {
      if (d["layout"].is<JsonObjectConst>()) room::save(d["layout"].as<JsonVariantConst>(), false);   // Raum gleich mitgeschickt
      JsonDocument f; f.set(d["fx"]);
      if (const char* e = apiCall("/api/effect", f)) return e;
      if (fwd) {
        JsonObject o = f.as<JsonObject>(); palCarry(o); String b; serializeJson(f, b);
        room::fanout("/api/room", "{\"action\":\"fx\",\"fwd\":false,\"fx\":" + b + (room::raw.length() ? ",\"layout\":" + room::raw : String()) + "}");
      }
      return nullptr;
    }
    return "Unbekannte Aktion";
  }
  if (!strcmp(path, "/api/sync")) {
    if (d["group"].is<int>()) { cfg.syncGroup = constrain(d["group"].as<int>(), 1, 9); prefs.putUChar("syncGrp", cfg.syncGroup); }
    if (d["on"].is<bool>()) { cfg.syncOn = d["on"]; prefs.putBool("syncOn", cfg.syncOn); if (!cfg.syncOn) diag("Gleichtakt mit anderen Wänden aus"); }
    wallsync::lastSig = 0;                               // gleich mit dem eigenen Zustand melden
    return nullptr;
  }
  // Name der Wand: {"name":"Wohnzimmer"}
  if (!strcmp(path, "/api/name")) {
    String n = d["name"] | "";
    n.trim();
    if (!n.length()) return "Bitte einen Namen eingeben";
    if (n.length() > 32) n = n.substring(0, 32);
    if (n == cfg.name) return nullptr;
    cfg.name = n; prefs.putString("name", n);
    MDNS.setInstanceName(n.c_str());
    MDNS.addServiceTxt("wled", "tcp", "name", n.c_str());
    diag("Die Wand heißt jetzt „%s“", n.c_str());
    if (mqtt.connected()) {                            // Gerätename in Home Assistant nachziehen
      publishAllLight();
      for (int i = 0; i < SLOTS; i++) if (P[i].used) publishDiscovery(i);
    }
    return nullptr;
  }
  // Hue / Zigbee (nur ESP32-C6): {"action":"on","value":true} startet neu, {"action":"pair"} koppelt neu
  if (!strcmp(path, "/api/zigbee")) {
    if (!HAS_ZIGBEE) return "Zigbee gibt es nur mit dem ESP32-C6";
    const char* a = d["action"] | "";
    if (!strcmp(a, "on")) {
      bool v = d["value"] | false;
      prefs.putBool("zbOn", v); prefs.remove("zbBoot");
      diag("Zigbee %s, Hauptpanel startet neu", v ? "eingeschaltet" : "ausgeschaltet");
      restartAt = millis() + 800;
      return nullptr;
    }
#if HAS_ZIGBEE
    if (!strcmp(a, "pair")) {
      if (!zb::started) return "Zigbee ist nicht eingeschaltet";
      diag("Zigbee wird zurückgesetzt und ist danach wieder koppelbar");
      zbResetAt = millis() + 500;
      return nullptr;
    }
#endif
    return "Unbekannte Aktion";
  }
  if (!strncmp(path, "/api/sim/", 9) && cfg.bus) return "nur in der Simulation";
  if (!strcmp(path, "/api/sim/tap")) {             // Antippen ausprobieren: {"id":"…","double":false}
    int i = findChip(parseHex(d["id"] | "0"));
    if (i < 0 || !P[i].attached) return "Panel unbekannt";
    touchEvent(i, (d["double"] | false) ? 2 : 1);
    return nullptr;
  }
  if (!strcmp(path, "/api/sim/new")) return simNewPanel() < 0 ? "Ablage voll" : nullptr;
  // Panels aus der Ablage löschen: {"id":"…"} oder {"all":true}
  if (!strcmp(path, "/api/sim/remove")) {
    bool all = d["all"] | false; int one = all ? -1 : findChip(parseHex(d["id"] | "0"));
    if (!all && (one <= 0 || P[one].attached)) return "Panel nicht in der Ablage";
    for (int i = 1; i < SLOTS; i++) {
      if (!P[i].used || P[i].attached || (!all && i != one)) continue;
      String h = hex(P[i].chip);
      for (const char* k : {"c", "e", "n", "h", "f"}) prefs.remove((k + h).c_str());   // Farbe, Kanten, Zähler, Stunden, Firmware
      if (swapSlot == i) swapSlot = -1;
      P[i] = Panel();
    }
    simChanged();
    return nullptr;
  }
  if (!strcmp(path, "/api/sim/attach")) {
    int i = findChip(parseHex(d["id"] | "0")), par = findChip(parseHex(d["parent"] | "0"));
    return simAttach(i, par, d["edge"] | 9) ? nullptr : "Anklipsen nicht möglich";
  }
  if (!strcmp(path, "/api/sim/rotate")) {           // Panel an Ort und Stelle drehen: eine andere eigene Kante zeigt zum Nachbarn
    int i = findChip(parseHex(d["id"] | "0"));
    if (i <= 0 || !P[i].attached) return "Panel unbekannt";
    P[i].rot = (P[i].rot + 1) % 3;
    reconcile(); outForce = true; simChanged();
    return nullptr;
  }
  if (!strcmp(path, "/api/sim/detach")) { simDetach(findChip(parseHex(d["id"] | "0"))); return nullptr; }
  return "Unbekannter Befehl";
}

// aktuelles Effektbild, damit die App mitleuchtet
String liveJson() {
  String out = "{\"fx\":\""; out += FX[fx.id].id; out += overlayActive() ? "\",\"ov\":1,\"c\":{" : "\",\"c\":{";
  bool first = true;
  for (int i = 0; i < SLOTS; i++) {
    if (!P[i].used || !P[i].attached) continue;
    char b[64];
    int n = snprintf(b, sizeof b, "%s\"%08X\":\"", first ? "" : ",", (unsigned)P[i].chip);
    for (int e = 0; e < (P[i].edges ? 3 : 1); e++)                // Kanten einzeln: drei Farben, durch Komma getrennt
      n += snprintf(b + n, sizeof b - n, "%s%02X%02X%02X%02X", e ? "," : "", CUR[i][e][0], CUR[i][e][1], CUR[i][e][2], CUR[i][e][3]);
    snprintf(b + n, sizeof b - n, "\"");
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
  return out;
}

// WLAN-Suche, die nicht leer zurückkommt, nur weil das Panel gerade selbst verbinden will
int scanWifi() {
  int n = WiFi.scanNetworks(false, false);
  if (n < 0) { WiFi.scanDelete(); delay(100); n = WiFi.scanNetworks(false, false); }
  if (n < 0 && wPhase == W_CONNECTING) { WiFi.disconnect(false); delay(50); n = WiFi.scanNetworks(false, false); WiFi.begin(wSsid.c_str(), wPass.c_str()); }
  return n < 0 ? 0 : n;
}

void setupWeb() {
  // Die App direkt aus dem Flash schicken, ohne sie erst in den Arbeitsspeicher zu kopieren (sie ist rund 70 KB groß)
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html; charset=utf-8", (const char*)WEBAPP_GZ, WEBAPP_GZ_LEN);
  });
  // Notfall-Seite für Updates: klein, funktioniert auch, wenn die App selbst nicht lädt
  server.on("/update", HTTP_GET, [] {
    String h = String("<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>Trilumag Update</title>"
      "<style>body{font:16px system-ui;background:#111;color:#eee;max-width:520px;margin:24px auto;padding:0 16px}button,input{font:inherit;margin:6px 0;padding:10px 14px;border-radius:10px}"
      "button{background:#6e8bff;border:0;color:#111;font-weight:600}p{color:#aaa}</style><h1>Trilumag Update</h1>");
    h += "<p>Installierte Version: <b>" + String(FW_VERSION) + "</b> auf " + CHIP_FAMILY + "</p>";
    h += String("<button onclick=\"fetch('/api/ota',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'latest'})}).then(()=>{document.getElementById('m').textContent='Suche und installiere die neueste Version. Trilumag startet danach neu, die Seite in etwa einer Minute neu laden.'})\">Neueste Version installieren</button>"
      "<p id=m></p><h2>Datei hochladen</h2><form method=post action=/update enctype=multipart/form-data><input type=file name=f accept=.bin><br><button>Hochladen und installieren</button></form>"
      "<p><a href=/ style=color:#6e8bff>zur App</a></p>");
    server.send(200, "text/html; charset=utf-8", h);
  });
  server.on("/api/state", HTTP_GET, replyState);
  server.on("/api/energy", HTTP_GET, [] { server.send(200, "application/json", energyJson()); });
  server.on("/api/peers", HTTP_GET, [] { server.send(200, "application/json", peers::json()); });
  server.on("/api/room", HTTP_GET, [] { server.send(200, "application/json", room::json()); });
  // Geometrie für den Raumplan: darf von der App einer anderen Wand gelesen werden
  server.on("/api/room/geo", HTTP_GET, [] { server.sendHeader("Access-Control-Allow-Origin", "*"); server.send(200, "application/json", room::geoJson()); });
  server.on("/api/hue", HTTP_GET, [] { server.send(200, "application/json", hueb::listJson()); });
  // Befehle laufen über apiCall(), damit HTTP und WebSocket dasselbe tun
  const char* cmds[] = {"/api/set", "/api/presets", "/api/effect", "/api/test", "/api/ota", "/api/light", "/api/diag", "/api/touch", "/api/panelfw", "/api/zigbee", "/api/name", "/api/identify", "/api/guard", "/api/swap", "/api/favs", "/api/view", "/api/signal", "/api/progress", "/api/sleep", "/api/boot", "/api/energy", "/api/peers", "/api/weather", "/api/game", "/api/fault", "/api/mirror", "/api/hue", "/api/palette", "/api/sync", "/api/room",
                        "/api/sim/new", "/api/sim/remove", "/api/sim/rotate", "/api/sim/attach", "/api/sim/detach", "/api/sim/tap"};
  for (const char* path : cmds) {
    server.on(path, HTTP_POST, [path] {
      JsonDocument d;
      if (server.hasArg("plain") && server.arg("plain").length()) deserializeJson(d, server.arg("plain"));
      if (const char* err = apiCall(path, d)) return replyError(err);
      if (!strcmp(path, "/api/test")) server.send(200, "application/json", "{\"ok\":true}");
      else if (!strcmp(path, "/api/ota")) server.send(200, "application/json", otaJson());
      else if (!strcmp(path, "/api/diag")) server.send(200, "application/json", diagJson());
      else replyState();
    });
  }
  server.on("/api/live", HTTP_GET, [] { server.send(200, "application/json", liveJson()); });
  server.on("/api/ota", HTTP_GET, [] { server.send(200, "application/json", otaJson()); });
  server.on("/api/diag", HTTP_GET, [] { server.send(200, "application/json", diagJson()); });
  // WLED-Schnittstelle
  server.on("/json", HTTP_GET, [] { server.send(200, "application/json", wledJson("all")); });
  server.on("/json/si", HTTP_GET, [] { server.send(200, "application/json", wledJson("all")); });
  server.on("/json/info", HTTP_GET, [] { server.send(200, "application/json", wledJson("info")); });
  server.on("/json/state", HTTP_GET, [] { server.send(200, "application/json", wledJson("state")); });
  server.on("/json/eff", HTTP_GET, [] { server.send(200, "application/json", wledJson("eff")); });
  server.on("/json/pal", HTTP_GET, [] { server.send(200, "application/json", wledJson("pal")); });
  server.on("/presets.json", HTTP_GET, [] { server.send(200, "application/json", wledJson("presets")); });
  auto wledPost = [] {
    JsonDocument d; if (!readBody(d)) return replyError("JSON fehlt");
    wledApply(d.as<JsonVariantConst>());
    wsKick();
    if (d["v"] | false) server.send(200, "application/json", wledJson("state"));
    else server.send(200, "application/json", "{\"success\":true}");
  };
  server.on("/json/state", HTTP_POST, wledPost);
  server.on("/json", HTTP_POST, wledPost);
  server.on("/json/si", HTTP_POST, wledPost);
  server.on("/api/backup", HTTP_GET, [] {
    server.sendHeader("Content-Disposition", String("attachment; filename=\"trilumag-sicherung-") + FW_VERSION + ".json\"");
    server.send(200, "application/json", backupJson());
  });
  server.on("/api/restore", HTTP_POST, [] {
    JsonDocument d; if (!readBody(d)) return replyError("Datei unlesbar");
    if (const char* err = restoreBackup(d)) return replyError(err);
    server.send(200, "application/json", "{\"ok\":true,\"restart\":true}");
    delay(600);
    restartWith(R_APP);
  });
  // Firmware-Datei hochladen (wie bei WLED unter /update)
  server.on("/update", HTTP_POST, [] {
    bool ok = !Update.hasError();
    server.send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"Datei passt nicht oder ist beschädigt\"}");
    if (ok) { logf("[OTA] Datei installiert, starte neu\n"); delay(600); restartWith(R_UPDATE); }
  }, [] {
    HTTPUpload& u = server.upload();
    if (u.status == UPLOAD_FILE_START) {
      logf("[OTA] Datei-Upload %s\n", u.filename.c_str());
      saveColors(); fxSave(); if (simDirty) simSave();
      otaQuiet(true);
      Update.begin(UPDATE_SIZE_UNKNOWN);
    } else if (u.status == UPLOAD_FILE_WRITE) {
      Update.write(u.buf, u.currentSize);
    } else if (u.status == UPLOAD_FILE_END) {
      if (!Update.end(true)) otaQuiet(false);
    } else if (u.status == UPLOAD_FILE_ABORTED) {
      Update.abort(); otaQuiet(false);
    }
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
    JsonObject li = d["light"];
    if (!li.isNull()) {                            // Stromsensor: beide Pins oder keiner, frei und gültig
      int sda = li["sda"] | -1, scl = li["scl"] | -1;
      if ((sda < 0) != (scl < 0)) return replyError("Für den Stromsensor beide Pins wählen oder keinen");
      if (sda >= 0) {
        const int8_t used[6] = {ps.rx, ps.tx, ps.de, ps.led, ps.snsR, ps.snsL};
        if (!pinValid(sda) || !pinValid(scl) || sda == scl) return replyError("Pins für den Stromsensor ungültig");
        for (int8_t u : used) if (u == sda || u == scl) return replyError("Pin des Stromsensors ist schon belegt");
      }
      prefs.putChar("i2cSda", sda); prefs.putChar("i2cScl", scl);
      prefs.putUShort("shunt", constrain((int)(li["shunt"] | (int)cfg.shuntUo), 1, 10000));
    }
    String order = d["order"] | cfg.order.c_str();
    bool okOrder = false; for (const char* o : ORDERS) if (order == o) okOrder = true;
    if (!okOrder) return replyError("Unbekannte Farbreihenfolge");
    prefs.putBytes("pins", &ps, sizeof ps);
    prefs.putString("board", (const char*)(d["board"] | cfg.board.c_str()));
    prefs.putBool("bus", !strcmp(d["mode"] | (cfg.bus ? "bus" : "sim"), "bus"));
    prefs.putString("order", order);
    JsonObject m = d["mqtt"];
    if (!m.isNull()) {
      if (!m["on"].isNull()) prefs.putBool("mqttOn", m["on"].as<bool>());
      prefs.putString("mqttHost", (const char*)(m["host"] | ""));
      prefs.putUShort("mqttPort", m["port"] | 1883);
      prefs.putString("mqttUser", (const char*)(m["user"] | ""));
      if (m["pass"].is<const char*>()) prefs.putString("mqttPass", (const char*)m["pass"]);
    }
    saveColors(); fxSave(); if (simDirty) simSave();
    server.send(200, "application/json", "{\"ok\":true,\"restart\":true}");
    logf("Einstellungen gespeichert, starte neu\n");
    delay(600);
    restartWith(R_APP);
  });
  server.on("/api/wifi/scan", HTTP_GET, [] {
    int n = scanWifi();
    JsonDocument d; JsonArray a = d["networks"].to<JsonArray>();
    JsonArray l = d["list"].to<JsonArray>();
    for (int k = 0; k < n && k < 30; k++) {          // die Liste kommt nach Signalstärke sortiert
      String ss = WiFi.SSID(k);
      if (!ss.length()) continue;
      bool dup = false; for (JsonVariant v : a) if (v.as<String>() == ss) dup = true;
      if (dup) continue;
      a.add(ss);
      JsonObject o = l.add<JsonObject>(); o["ssid"] = ss; o["rssi"] = WiFi.RSSI(k); o["lock"] = WiFi.encryptionType(k) != WIFI_AUTH_OPEN;
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
    restartWith(R_APP);
  });
  ws::onOpen = wsOpen; ws::onText = wsText;
  ota::onProgress = [](int p) {                 // Fortschritt an die App; ws::loop läuft während des Downloads nicht
    otaBeat = millis();
    if (p % 5 == 0) { ws::broadcast("{\"t\":\"otap\",\"p\":" + String(p) + "}"); logf("[OTA] %d %%\n", p); }
  };
  ws::begin();
  server.onNotFound([] { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();
}

// ---------- MQTT-Empfang ----------
void onMqtt(char* topic, byte* payload, unsigned int len) {
  char b[64]; unsigned n = len < 63 ? len : 63; memcpy(b, payload, n); b[n] = 0;
  if (!strcmp(topic, "trilumag/tempo/set")) { JsonDocument d; d["speed"] = atoi(b); applyFx(d.as<JsonVariantConst>()); return; }
  if (!strcmp(topic, "trilumag/intensitaet/set")) { JsonDocument d; d["intensity"] = atoi(b); applyFx(d.as<JsonVariantConst>()); return; }
  if (!strcmp(topic, "trilumag/palette/set")) { JsonDocument d; d["palette"] = b; applyFx(d.as<JsonVariantConst>()); return; }
  if (!strcmp(topic, "trilumag/szene/set")) { presetLoad(presetFind(b)); return; }
  if (!strcmp(topic, "trilumag/signal/set")) {      // Text "blau 3" oder JSON {"color":"blau","blink":3}
    JsonDocument d;
    if (len && payload[0] == '{' && !deserializeJson(d, payload, len)) signalCmd(d.as<JsonVariantConst>()); else signalText(b);
    wsKick(); return;
  }
  if (!strcmp(topic, "trilumag/fortschritt/set")) {
    JsonDocument d;
    if (len && payload[0] == '{' && !deserializeJson(d, payload, len)) progressCmd(d.as<JsonVariantConst>());
    else { d["value"] = atof(b); progressCmd(d.as<JsonVariantConst>()); }
    wsKick(); return;
  }
  JsonDocument d;
  if (deserializeJson(d, payload, len)) return;
  kelvinToColor(d);
  String t(topic);                       // trilumag/<ID>/set
  String id = t.substring(9, t.lastIndexOf('/'));
  if (id == "alle") applyAll(d.as<JsonVariantConst>());
  else { int i = findChip(parseHex(id.c_str())); if (i >= 0) applyCommand(i, d.as<JsonVariantConst>()); }
}

void mqttStatus(const char* s) { if (mqtt.connected()) mqtt.publish("trilumag/bridge/status", s, true); }
void mqttLoop() {
  if (!cfg.mqttOn || !cfg.mqttHost.length() || !wlanOk || otaBusy) return;
  if (mqtt.connected()) {
    mqtt.loop();
    static uint32_t lastPwr = 0;
    if (millis() - lastPwr > 10000) { lastPwr = millis(); publishPower(); }
    return;
  }
  if (millis() - lastMqttTry < 5000) return;
  lastMqttTry = millis();
  String cid = "trilumag-" + hex(P[0].chip);
  const char* u = cfg.mqttUser.length() ? cfg.mqttUser.c_str() : nullptr;
  const char* pw = cfg.mqttPass.length() ? cfg.mqttPass.c_str() : nullptr;
  if (mqtt.connect(cid.c_str(), u, pw, "trilumag/bridge/avail", 0, true, "offline")) {
    logf("[MQTT] verbunden\n");
    mqtt.publish("trilumag/bridge/avail", "online", true);
    mqttStatus("Bereit");
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

// Die Verbindung läuft im Hintergrund. So bleibt das Hauptpanel währenddessen für App und
// Webinstaller ansprechbar (der Webinstaller gibt nach 1,5 bis 10 s auf).

void improvSendUrl(Stream& s, uint8_t cmd);

void wifiStart(const String& ss, const String& pw, uint32_t timeout, bool fromImprov) {
  WiFi.mode(apMode ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);            // ohne Funk-Sparmodus antwortet die App sofort
  if (WiFi.status() == WL_CONNECTED) WiFi.disconnect(false);
  wlanOk = false;
  WiFi.begin(ss.c_str(), pw.c_str());
  wSsid = ss; wPass = pw; wImprov = fromImprov; wSaveLate = fromImprov;
  wPhase = W_CONNECTING; wDeadline = millis() + timeout;
  logf("[WLAN] verbinde mit '%s' …\n", ss.c_str());
}

void wifiConnected() {
  wlanOk = true;
  if (wSaveLate) { saveWifi(wSsid, wPass); wSaveLate = false; }
  if (apMode) { WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); apMode = false; }
  mdnsStart();
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");   // Uhrzeit für den Energieverbrauch
  logf("[WLAN] verbunden. App: http://%s  oder  http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
  // Webinstaller Bescheid geben, auch wenn die Verbindung erst nach seiner Wartezeit kam
  Stream* outs[2] = {&Serial, nullptr};
#if HAS_UART_CONSOLE
  outs[1] = &Serial0;
#endif
  for (Stream* o : outs) {
    if (!o) continue;
    improv::sendState(*o, improv::PROVISIONED);
    if (wImprov && o == wImprovStream) improvSendUrl(*o, improv::WIFI_SETTINGS);
  }
  wImprov = false; wImprovStream = nullptr;
}

void wifiLoop() {
  bool up = WiFi.status() == WL_CONNECTED;
  if (wPhase == W_CONNECTING) {
    if (up) { wPhase = W_IDLE; wifiConnected(); return; }
    if ((int32_t)(millis() - wDeadline) < 0) return;
    wPhase = W_IDLE;
    logf("[WLAN] keine Verbindung zu '%s'\n", wSsid.c_str());
    if (wImprov && wImprovStream) {
      improv::sendError(*wImprovStream, improv::UNABLE_TO_CONNECT);
      improv::sendState(*wImprovStream, improv::AUTHORIZED);
      wImprov = false; wImprovStream = nullptr;
      wSaveLate = false;
      WiFi.disconnect(false);
      String ss = prefs.getString("ssid", ""), pw = prefs.getString("pass", "");
      if (ss.length() && ss != wSsid) { wifiStart(ss, pw, 15000, false); return; }   // zurück ins alte WLAN
    }
    if (!apMode) startSetupAp();     // das Panel versucht es im Hintergrund weiter
    return;
  }
  if (up && !wlanOk) wifiConnected();                       // später doch verbunden oder wieder da
  else if (!up && wlanOk) { wlanOk = false; logf("[WLAN] Verbindung verloren, verbinde neu …\n"); }
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
      improv::sendState(s, wlanOk ? improv::PROVISIONED : (wPhase == W_CONNECTING && wImprov) ? improv::PROVISIONING : improv::AUTHORIZED);
      if (wlanOk) improvSendUrl(s, improv::GET_CURRENT_STATE);
      break;
    case improv::GET_DEVICE_INFO: {
      const char* a[4] = {FW_NAME, FW_VERSION, CHIP_FAMILY, "Trilumag Hauptpanel"};
      improv::sendResult(s, improv::GET_DEVICE_INFO, a, 4);
      break;
    }
    case improv::GET_WIFI_NETWORKS: {
      int n = scanWifi();
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
      wImprovStream = &s;
      wifiStart(ss, pw, 30000, true);     // Antwort kommt aus wifiLoop, sobald klar ist, ob es klappt
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
  delay(50);
  logf("\n%s %s auf %s\n", FW_NAME, FW_VERSION, CHIP_FAMILY);

  prefs.begin("trilumag", false);
  loadConfig();
  logf("Betriebsart: %s, Board: %s\n", cfg.bus ? "Bus" : "Simulation", cfg.board.c_str());

  P[0].used = true; P[0].attached = true;
  P[0].chip = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF);
  P[0].x = 0; P[0].y = 0; P[0].rot = 0;
  P[0].state = ACTIVE; P[0].hasColor = true; P[0].w = 200;
  loadColor(0);
  otaBootCheck();
  guardBoot();
  fxLoad();
  room::load();
  wx::load();
  { String m = prefs.getString("wledIp", ""); if (m.length() && !safeBoot) mirror::start(m); }
  hueb::load();
  presetsLoad();
  bootApply();                                      // nach Stromausfall: aus, an oder Preset (Einstellung)
  enLoad();
  litLoad(0);
  ina::begin();
  if (fx.id) logf("[FX] Effekt %s läuft weiter\n", FX[fx.id].name);

  strip.updateType(neoType(cfg.order) + NEO_KHZ800);
  strip.setPin(cfg.pins.led);
  strip.begin();

  String ss = prefs.getString("ssid", WLAN_SSID), pw = prefs.getString("pass", WLAN_PASS);
  WiFi.setHostname(HOSTNAME);
  if (ss.length()) wifiStart(ss, pw, 15000, false);   // nicht warten, Improv muss sofort antworten können
  else startSetupAp();
  mdnsStart();
  zbStart();

  netClient.setTimeout(800);       // ist der Broker nicht erreichbar, nicht 3 s lang hängen
  mqtt.setSocketTimeout(2);
  mqtt.setServer(cfg.mqttHost.c_str(), cfg.mqttPort);
  mqtt.setBufferSize(1600);
  mqtt.setCallback(onMqtt);

  diag("Trilumag %s gestartet (%s, %s)", FW_VERSION, CHIP_FAMILY, cfg.bus ? "Bus" : "Simulation");
  if (cfg.bus) bus::begin();
  else if (!simLoad()) { for (int k = 0; k < 3; k++) simNewPanel(); simSave(); }   // sonst liegen drei Panels in der Ablage bereit
  setupWeb();
}

void loop() {
  guardLoop();
  otaVerifyLoop();
  ina::loop();
  server.handleClient();
  wsLoop();
  improvLoop();
  wifiLoop();
  mqttLoop();
  if (cfg.bus) busLoop();
  else {
    // simulierte Erkennung: erst dunkel, dann pulsieren oder gespeicherte Farbe
    for (int i = 1; i < SLOTS; i++) {
      Panel& p = P[i];
      if (p.used && p.attached && p.state == DARK && millis() - p.since > DETECT_MS) {
        p.state = p.hasColor ? ACTIVE : PULSE;
        p.joinAt = (millis() + FX_JOIN_MS) | 1;
        p.caps = bus::CAP_TOUCH | bus::CAP_BOOT;     // simulierte Panels: mit Sensor und Bootloader
        p.fw = prefs.getUChar(("f" + hex(p.chip)).c_str(), PANEL_FW_VERSION - 1);   // einmal Update zum Ausprobieren
        logf("[SIM] %s eingegliedert: %s\n", hex(p.chip).c_str(), p.hasColor ? "alte Farbe" : "pulsiert blau");
        sendToPanel(i); publishState(i);
      }
    }
    pupd::loop();
  }
  outLoop();
  otaLoop();
  if (colorsDirty && millis() - colorsDirtyAt > 5000) saveColors();
  if (simDirty && millis() - simDirtyAt > 1500) simSave();
  if (fxDirty && millis() - fxDirtyAt > 5000) fxSave();
  zbLoop();
  peers::loop();
  wx::loop();
  mirror::loop();
  hueb::loop();
  sleepLoop();
  litLoop();
  enLoop();
  wallsync::loop();
  room::loop();
  if (restartAt && (int32_t)(millis() - restartAt) >= 0) { fxSave(); saveColors(); enSave(); restartWith(R_APP); }
#if HAS_ZIGBEE
  if (zbResetAt && (int32_t)(millis() - zbResetAt) >= 0) { zbResetAt = 0; prefs.remove("zbBoot"); zb::factoryReset(); }
#endif
}
