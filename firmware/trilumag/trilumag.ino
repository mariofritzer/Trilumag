/*
  Trilumag – Firmware für das Hauptpanel, Version 0.1 (Simulationsmodus)
  ----------------------------------------------------------------------
  Läuft auf jedem ESP32 (klassischer ESP32, ESP32-S3, ESP32-C3, ESP32-C6).
  Am einfachsten über den Webinstaller: https://mariofritzer.github.io/Trilumag/

  Was sie kann:
   - WLAN-Einrichtung direkt im Webinstaller (Improv) oder über das Setup-Netz "Trilumag-Setup"
   - Web-App unter http://trilumag.local bzw. der IP aus dem seriellen Monitor
   - Panels in der App per Ziehen anklipsen und abklipsen (simuliert)
   - Erkennung, Position und Drehung im Dreiecksraster
   - neue Panels pulsieren blau, bis sie ihre erste Farbe bekommen
   - abgeklipste Panels behalten ihre Farbe (Wiedererkennung über Chip-ID)
   - HTTP-API und Home Assistant über MQTT mit Auto-Discovery

  Selbst kompilieren mit der Arduino IDE:
   - Boardverwalter: "esp32" von Espressif
   - Bibliotheken: ArduinoJson (ab Version 7) und PubSubClient
   - ESP32-S3/C3/C6: "USB CDC On Boot" auf "Enabled" stellen
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <stdarg.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "webapp.h"
#include "improv.h"

// ================= Einstellungen =================
// WLAN wird normalerweise im Webinstaller oder im Setup-Netz eingegeben und im Flash gespeichert.
// Nur wenn dort nichts gespeichert ist, gelten diese Werte:
const char* WLAN_SSID   = "";
const char* WLAN_PASS   = "";

const char* MQTT_HOST   = "";        // z. B. "192.168.1.10" (Home Assistant mit Mosquitto). Leer = ohne MQTT
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER   = "";
const char* MQTT_PASS   = "";

const bool     SIMULATION   = true;   // später false, wenn der RS-485-Bus angeschlossen ist
const uint8_t  MAX_ATTACHED = 30;     // höchstens so viele Panels an der Wand (inkl. Hauptpanel)
const uint32_t DETECT_MS    = 1200;   // simulierte Erkennungszeit nach dem Anklipsen
// ==================================================

const char* FW_NAME    = "Trilumag";
const char* FW_VERSION = "0.1.0";
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
  uint8_t state = DARK;
  uint32_t since = 0;
  bool hasColor = false;
  bool on = true;
  uint8_t r = 0, g = 0, b = 0, w = 0, bri = 180;
};

Panel P[SLOTS];
WebServer server(80);
WiFiClient netClient;
PubSubClient mqtt(netClient);
uint32_t lastMqttTry = 0;
bool wlanOk = false;
bool apMode = false;
Preferences prefs;
improv::Parser improvMain;
#if ARDUINO_USB_CDC_ON_BOOT
improv::Parser improvUart;
#endif

// Ausgabe auf USB und, falls vorhanden, zusätzlich auf dem UART-Anschluss
void logf(const char* fmt, ...) {
  char b[320];
  va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
  Serial.print(b);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.print(b);
#endif
}

void saveWifi(const String& ss, const String& pw);
bool connectWifi(const String& ss, const String& pw, uint32_t timeout);
void startSetupAp();

// ---------- Hilfsfunktionen Raster ----------
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
int edgeFacing(int i, Dir d) {
  for (uint8_t e = 0; e < 3; e++) if (worldDir(P[i], e) == d) return e;
  return -1;
}
int countAttached() { int n = 0; for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) n++; return n; }
String hex(uint32_t c) { char b[9]; snprintf(b, sizeof b, "%08X", c); return String(b); }
uint32_t parseHex(const char* s) { return (uint32_t)strtoul(s, nullptr, 16); }

// ---------- Bus (später RS-485) ----------
void sendToPanel(int i) {
  const Panel& p = P[i];
  if (SIMULATION) {
    logf("[BUS] %s  Zustand=%u  an=%d  RGBW=%u,%u,%u,%u  Hell=%u\n", hex(p.chip).c_str(), p.state, p.on, p.r, p.g, p.b, p.w, p.bri);
  }
  // TODO Phase 2: Befehl über RS-485 an Kurzadresse i senden
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
  d["availability_topic"] = "trilumag/bridge/avail";
  JsonObject dev = d["device"].to<JsonObject>();
  dev["identifiers"].to<JsonArray>().add("trilumag_" + hex(P[0].chip));
  dev["name"] = "Trilumag";
  char buf[600]; size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish(("homeassistant/light/trilumag_alle_" + hex(P[0].chip) + "/config").c_str(), (const uint8_t*)buf, n, true);
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
  if (!p.hasColor && p.on) { p.w = 255; p.hasColor = true; }   // nur "Ein" ohne Farbe: warmweiß
  if (p.state != DARK) p.state = ACTIVE;                         // erste Farbzuordnung beendet das Pulsieren
  sendToPanel(i);
  publishState(i);
}

void applyAll(JsonVariantConst cmd) {
  for (int i = 0; i < SLOTS; i++) if (P[i].used && P[i].attached) applyCommand(i, cmd);
}

// ---------- Topologie ----------
// Prüft von Hauptpanel aus, welche Panels noch verbunden sind. Nicht erreichbare wandern in die Ablage.
void reconcile() {
  int8_t q[SLOTS]; bool seen[SLOTS] = {false};
  int head = 0, tail = 0;
  q[tail++] = 0; seen[0] = true; P[0].parent = -1;
  while (head < tail) {
    int i = q[head++];
    for (uint8_t e = 0; e < 3; e++) {
      if (!hasConnector(i, e)) continue;
      Dir d = worldDir(P[i], e); int nx, ny; neighbor(P[i].x, P[i].y, d, nx, ny);
      int j = findAt(nx, ny);
      if (j < 0 || seen[j]) continue;
      int ej = edgeFacing(j, opposite(d));
      if (ej < 0 || !hasConnector(j, ej)) continue;
      seen[j] = true; P[j].parent = i; q[tail++] = j;
    }
  }
  for (int i = 1; i < SLOTS; i++) {
    if (P[i].used && P[i].attached && !seen[i]) {
      P[i].attached = false; P[i].state = DARK; P[i].parent = -1;
      logf("[TOPO] %s getrennt, liegt jetzt in der Ablage\n", hex(P[i].chip).c_str());
      publishAvail(i, false);
    }
  }
}

bool attachPanel(int i, int parent, uint8_t edge) {
  if (i <= 0 || i >= SLOTS || !P[i].used || P[i].attached) return false;
  if (parent < 0 || parent >= SLOTS || !P[parent].used || !P[parent].attached) return false;
  if (edge > 2 || !hasConnector(parent, edge)) return false;
  if (countAttached() >= MAX_ATTACHED) return false;
  Dir d = worldDir(P[parent], edge); int nx, ny; neighbor(P[parent].x, P[parent].y, d, nx, ny);
  if (findAt(nx, ny) >= 0) return false;
  // Welche eigene Kante den Kontakt hat, hängt davon ab, wie das Panel gehalten wird: hier zufällig
  uint8_t own = esp_random() % 3;
  const Dir* list = slotEdges(isUp(nx, ny));
  Dir back = opposite(d); uint8_t k = 0;
  for (uint8_t m = 0; m < 3; m++) if (list[m] == back) k = m;
  P[i].x = nx; P[i].y = ny; P[i].rot = (k - own + 3) % 3;
  P[i].attached = true; P[i].state = DARK; P[i].since = millis();
  reconcile();
  logf("[TOPO] %s an Kante %u von %s erkannt, Position %d/%d, eigene Kante %u\n",
                hex(P[i].chip).c_str(), edge + 1, hex(P[parent].chip).c_str(), nx, ny, own + 1);
  publishDiscovery(i); publishAvail(i, true);
  return true;
}

void detachPanel(int i) {
  if (i <= 0 || i >= SLOTS || !P[i].used || !P[i].attached) return;
  P[i].attached = false; P[i].state = DARK;
  publishAvail(i, false);
  logf("[TOPO] %s abgeklipst\n", hex(P[i].chip).c_str());
  reconcile();
}

int newPanel() {
  for (int i = 1; i < SLOTS; i++) if (!P[i].used) {
    P[i] = Panel(); P[i].used = true;
    do { P[i].chip = esp_random(); } while (P[i].chip == 0 || findChip(P[i].chip) != i);
    return i;
  }
  return -1;
}

// ---------- JSON für die App ----------
String stateJson() {
  JsonDocument d;
  d["sim"] = SIMULATION;
  d["max"] = MAX_ATTACHED;
  d["mqtt"] = mqtt.connected();
  d["ap"] = apMode;
  d["ver"] = FW_VERSION;
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
  if (countAttached() < MAX_ATTACHED) {
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
  server.on("/api/sim/new", HTTP_POST, [] {
    if (!SIMULATION) return replyError("nur in der Simulation");
    if (newPanel() < 0) return replyError("Ablage voll");
    replyState();
  });
  server.on("/api/sim/attach", HTTP_POST, [] {
    if (!SIMULATION) return replyError("nur in der Simulation");
    JsonDocument d; if (!readBody(d)) return replyError("JSON fehlt");
    int i = findChip(parseHex(d["id"] | "0")), par = findChip(parseHex(d["parent"] | "0"));
    if (!attachPanel(i, par, d["edge"] | 9)) return replyError("Anklipsen nicht möglich");
    replyState();
  });
  server.on("/api/sim/detach", HTTP_POST, [] {
    if (!SIMULATION) return replyError("nur in der Simulation");
    JsonDocument d; if (!readBody(d)) return replyError("JSON fehlt");
    detachPanel(findChip(parseHex(d["id"] | "0")));
    replyState();
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
  JsonDocument d;
  if (deserializeJson(d, payload, len)) return;
  String t(topic);                       // trilumag/<ID>/set
  String id = t.substring(9, t.lastIndexOf('/'));
  if (id == "alle") applyAll(d.as<JsonVariantConst>());
  else { int i = findChip(parseHex(id.c_str())); if (i >= 0) applyCommand(i, d.as<JsonVariantConst>()); }
}

void mqttLoop() {
  if (!strlen(MQTT_HOST) || !wlanOk) return;
  if (mqtt.connected()) { mqtt.loop(); return; }
  if (millis() - lastMqttTry < 5000) return;
  lastMqttTry = millis();
  String cid = "trilumag-" + hex(P[0].chip);
  const char* u = strlen(MQTT_USER) ? MQTT_USER : nullptr;
  const char* pw = strlen(MQTT_PASS) ? MQTT_PASS : nullptr;
  if (mqtt.connect(cid.c_str(), u, pw, "trilumag/bridge/avail", 0, true, "offline")) {
    logf("[MQTT] verbunden\n");
    mqtt.publish("trilumag/bridge/avail", "online", true);
    mqtt.subscribe("trilumag/+/set");
    publishAllLight();
    for (int i = 0; i < SLOTS; i++) if (P[i].used) {
      publishDiscovery(i); publishAvail(i, P[i].attached);
      if (P[i].attached) publishState(i);
    }
  } else {
    logf("[MQTT] keine Verbindung (Fehler %d), neuer Versuch in 5 s\n", mqtt.state());
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
#if ARDUINO_USB_CDC_ON_BOOT
  while (Serial0.available()) if (improvUart.feed((uint8_t)Serial0.read())) improvRpc(Serial0, improvUart);
#endif
}

// ---------- Start ----------

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.begin(115200);
  Serial.setTxTimeoutMs(0);
#endif
  delay(300);
  logf("\n%s %s auf %s\n", FW_NAME, FW_VERSION, CHIP_FAMILY);

  P[0].used = true; P[0].attached = true;
  P[0].chip = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF);
  P[0].x = 0; P[0].y = 0; P[0].rot = 0;
  P[0].state = ACTIVE; P[0].hasColor = true; P[0].w = 200;

  prefs.begin("trilumag", false);
  String ss = prefs.getString("ssid", WLAN_SSID), pw = prefs.getString("pass", WLAN_PASS);
  WiFi.setHostname(HOSTNAME);
  if (ss.length() && connectWifi(ss, pw, 15000)) {
    logf("Verbunden. App: http://%s  oder  http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
  } else {
    startSetupAp();
  }
  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(1024);
  mqtt.setCallback(onMqtt);

  if (SIMULATION) for (int k = 0; k < 3; k++) newPanel();   // drei Panels liegen in der Ablage bereit
  setupWeb();
}

void loop() {
  server.handleClient();
  improvLoop();
  mqttLoop();
  // simulierte Erkennung: erst dunkel, dann pulsieren oder gespeicherte Farbe
  for (int i = 1; i < SLOTS; i++) {
    Panel& p = P[i];
    if (p.used && p.attached && p.state == DARK && millis() - p.since > DETECT_MS) {
      p.state = p.hasColor ? ACTIVE : PULSE;
      logf("[TOPO] %s eingegliedert: %s\n", hex(p.chip).c_str(), p.hasColor ? "alte Farbe wiederhergestellt" : "pulsiert blau");
      sendToPanel(i); publishState(i);
    }
  }
}
