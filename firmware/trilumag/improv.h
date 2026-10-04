#pragma once
// Improv Wi-Fi über die serielle Schnittstelle (https://www.improv-wifi.com/serial/)
// Damit kann der Webinstaller nach dem Flashen direkt WLAN-Name und Passwort übergeben.

#include <Arduino.h>

namespace improv {

enum State : uint8_t { AUTHORIZED = 0x02, PROVISIONING = 0x03, PROVISIONED = 0x04 };
enum Error : uint8_t { NO_ERROR = 0x00, INVALID_RPC = 0x01, UNKNOWN_RPC = 0x02, UNABLE_TO_CONNECT = 0x03, UNKNOWN = 0xFF };
enum Type : uint8_t { CURRENT_STATE = 0x01, ERROR_STATE = 0x02, RPC = 0x03, RPC_RESULT = 0x04 };
enum Command : uint8_t { WIFI_SETTINGS = 0x01, GET_CURRENT_STATE = 0x02, GET_DEVICE_INFO = 0x03, GET_WIFI_NETWORKS = 0x04 };

inline void send(Stream& s, uint8_t type, const uint8_t* data, uint8_t len) {
  uint8_t buf[268];
  uint16_t n = 0;
  const char* h = "IMPROV";
  for (int i = 0; i < 6; i++) buf[n++] = (uint8_t)h[i];
  buf[n++] = 0x01;            // Version
  buf[n++] = type;
  buf[n++] = len;
  if (len) { memcpy(buf + n, data, len); n += len; }
  uint8_t sum = 0;
  for (uint16_t i = 0; i < n; i++) sum += buf[i];
  buf[n++] = sum;
  buf[n++] = '\n';
  s.write(buf, n);
  s.flush();
}

inline void sendState(Stream& s, uint8_t state) { send(s, CURRENT_STATE, &state, 1); }
inline void sendError(Stream& s, uint8_t err) { send(s, ERROR_STATE, &err, 1); }

// Ergebnis eines Befehls: eine Liste von Zeichenketten
inline void sendResult(Stream& s, uint8_t cmd, const char* const* strs, uint8_t count) {
  uint8_t d[250];
  uint8_t n = 2;
  for (uint8_t k = 0; k < count; k++) {
    size_t L = strlen(strs[k]);
    if (L > 200 || n + 1 + L > sizeof d) break;
    d[n++] = (uint8_t)L;
    memcpy(d + n, strs[k], L);
    n += L;
  }
  d[0] = cmd;
  d[1] = n - 2;
  send(s, RPC_RESULT, d, n);
}

// Liest Byte für Byte und meldet, wenn ein vollständiges, gültiges RPC-Paket angekommen ist
struct Parser {
  uint8_t buf[264];
  uint16_t pos = 0;

  bool feed(uint8_t b) {
    static const char H[] = "IMPROV";
    if (pos < 6) {
      if (b != (uint8_t)H[pos]) {
        pos = 0;
        if (b == (uint8_t)H[0]) buf[pos++] = b;
        return false;
      }
      buf[pos++] = b;
      return false;
    }
    if (pos == 6) {
      if (b != 0x01) { pos = 0; return false; }
      buf[pos++] = b;
      return false;
    }
    if (pos == 7 || pos == 8) { buf[pos++] = b; return false; }
    uint16_t checksumAt = 9 + buf[8];
    if (pos < checksumAt) { buf[pos++] = b; return false; }
    uint8_t sum = 0;
    for (uint16_t i = 0; i < pos; i++) sum += buf[i];
    bool ok = (sum == b) && buf[7] == RPC && buf[8] >= 2;
    pos = 0;
    return ok;
  }

  uint8_t command() const { return buf[9]; }
  const uint8_t* payload() const { return buf + 11; }
  uint8_t payloadLen() const { return buf[10]; }
};

}  // namespace improv
