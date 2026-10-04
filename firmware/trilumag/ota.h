#pragma once
// Online-Updates (OTA) von GitHub
// - versions.json auf der Installer-Seite listet alle Versionen (neueste zuerst)
// - die Firmware jeder Version liegt als Release-Datei auf GitHub (auch ältere, für Downgrades)
// - automatische Updates nur, wenn in der App angehakt; sonst zeigt die App neue Versionen nur an

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include "cacerts.h"

namespace ota {

const char* LIST_URL = "https://mariofritzer.github.io/Trilumag/versions.json";
const char* FILE_URL = "https://github.com/mariofritzer/Trilumag/releases/download/v";   // + Version + "/trilumag-<chip>.bin"
const uint8_t MAXV = 20;

#if CONFIG_IDF_TARGET_ESP32S3
const char* CHIP_KEY = "esp32s3";
#elif CONFIG_IDF_TARGET_ESP32C3
const char* CHIP_KEY = "esp32c3";
#elif CONFIG_IDF_TARGET_ESP32C6
const char* CHIP_KEY = "esp32c6";
#else
const char* CHIP_KEY = "esp32";
#endif

struct Ver { String v, date, notes; };
Ver list[MAXV];
uint8_t count = 0;
String error;              // letzte Fehlermeldung, leer = alles gut
int progress = -1;         // -1 = kein Update läuft, sonst 0..100
uint32_t lastCheck = 0;    // millis() der letzten erfolgreichen Abfrage, 0 = noch nie
void (*onProgress)(int pct) = nullptr;

// Versionen vergleichen: 0.6.12 < 0.6.13 < 0.7.14
inline int cmp(const String& a, const String& b) {
  int i = 0, j = 0;
  while (i < (int)a.length() || j < (int)b.length()) {
    long x = 0, y = 0;
    while (i < (int)a.length() && a[i] != '.') { if (isdigit(a[i])) x = x * 10 + (a[i] - '0'); i++; }
    while (j < (int)b.length() && b[j] != '.') { if (isdigit(b[j])) y = y * 10 + (b[j] - '0'); j++; }
    if (x != y) return x < y ? -1 : 1;
    i++; j++;
  }
  return 0;
}

inline String latest() { return count ? list[0].v : String(); }

// Versionsliste holen. Dauert etwa 1 bis 2 Sekunden (TLS).
inline bool check() {
  NetworkClientSecure c;
  c.setCACert(CA_CERTS);
  HTTPClient h;
  h.setTimeout(8000);
  h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!h.begin(c, LIST_URL)) { error = "Update-Server nicht erreichbar"; return false; }
  int code = h.GET();
  if (code != 200) { error = "Versionsliste nicht abrufbar (" + String(code) + ")"; h.end(); return false; }
  JsonDocument d;
  DeserializationError e = deserializeJson(d, h.getStream());
  h.end();
  if (e) { error = "Versionsliste unlesbar"; return false; }
  count = 0;
  for (JsonObject o : d["versions"].as<JsonArray>()) {
    if (count >= MAXV) break;
    list[count].v = (const char*)(o["v"] | "");
    list[count].date = (const char*)(o["date"] | "");
    list[count].notes = (const char*)(o["notes"] | "");
    if (list[count].v.length()) count++;
  }
  error = "";
  lastCheck = millis() | 1;
  return true;
}

// Version herunterladen und installieren. Bei Erfolg muss danach neu gestartet werden.
inline bool install(const String& v) {
  String url = String(FILE_URL) + v + "/trilumag-" + CHIP_KEY + ".bin";
  NetworkClientSecure c;
  c.setCACert(CA_CERTS);
  c.setTimeout(15000);
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);   // GitHub leitet auf seinen Dateiserver um
  httpUpdate.rebootOnUpdate(false);
  httpUpdate.onProgress([](int cur, int total) {
    int p = total > 0 ? (int)((int64_t)cur * 100 / total) : 0;
    if (p != progress) { progress = p; if (onProgress) onProgress(p); }
  });
  progress = 0;
  if (onProgress) onProgress(0);
  t_httpUpdate_return r = httpUpdate.update(c, url);
  if (r == HTTP_UPDATE_OK) { progress = 100; error = ""; return true; }
  progress = -1;
  error = "Update fehlgeschlagen: " + httpUpdate.getLastErrorString();
  return false;
}

}  // namespace ota
