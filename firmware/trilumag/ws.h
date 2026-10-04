#pragma once
// Kleiner WebSocket-Server (RFC 6455) für die Web-App, wie bei WLED:
// Die App hält eine einzige Verbindung offen. Das Hauptpanel schickt Zustand und Effektbild
// von selbst, die App schickt ihre Befehle über dieselbe Verbindung. Kein ständiges Nachfragen mehr.
// Bewusst ohne fremde Bibliothek: nur Textnachrichten, kein Fragmentieren, höchstens 4 Verbindungen.

#include <WiFi.h>

namespace ws {

const uint16_t PORT = 81;
const uint8_t MAXC = 4;
const uint16_t BUF = 1024;          // größte Nachricht der App (Befehle sind klein)

// ---------- SHA-1 und Base64, nur für den Handshake ----------
inline uint32_t rol(uint32_t v, int b) { return (v << b) | (v >> (32 - b)); }
inline void sha1(const uint8_t* msg, size_t len, uint8_t out[20]) {
  uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
  size_t total = ((len + 8) / 64 + 1) * 64;
  for (size_t off = 0; off < total; off += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
      uint32_t v = 0;
      for (int k = 0; k < 4; k++) {
        size_t p = off + i * 4 + k;
        uint8_t b;
        if (p < len) b = msg[p];
        else if (p == len) b = 0x80;
        else if (p >= total - 8) b = (uint8_t)(((uint64_t)len * 8) >> (8 * (total - 1 - p)));
        else b = 0;
        v = (v << 8) | b;
      }
      w[i] = v;
    }
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
      uint32_t f, k;
      if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
      else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
      else { f = b ^ c ^ d; k = 0xCA62C1D6; }
      uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
  }
  for (int i = 0; i < 20; i++) out[i] = (uint8_t)(h[i / 4] >> (24 - 8 * (i % 4)));
}
inline String base64(const uint8_t* d, size_t n) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String o;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
    o += T[(v >> 18) & 63]; o += T[(v >> 12) & 63];
    o += i + 1 < n ? T[(v >> 6) & 63] : '=';
    o += i + 2 < n ? T[v & 63] : '=';
  }
  return o;
}

// ---------- Verbindungen ----------
struct Conn {
  NetworkClient c;
  bool used = false, open = false;
  uint16_t n = 0;
  uint32_t since = 0;
  uint8_t buf[BUF];
};

NetworkServer server(PORT);
Conn conns[MAXC];
void (*onOpen)(uint8_t id) = nullptr;
void (*onText)(uint8_t id, char* msg, size_t len) = nullptr;

inline void begin() { server.begin(); server.setNoDelay(true); }

inline void drop(uint8_t i) { conns[i].c.stop(); conns[i].used = conns[i].open = false; conns[i].n = 0; }

inline uint8_t count() { uint8_t k = 0; for (auto& c : conns) if (c.used && c.open) k++; return k; }

inline bool sendFrame(uint8_t i, uint8_t op, const char* d, size_t n) {
  Conn& k = conns[i];
  if (!k.used || !k.open || n > 65535) return false;
  uint8_t small[256 + 4];
  uint8_t hl = n < 126 ? 2 : 4;
  uint8_t* h = small;
  h[0] = 0x80 | op;
  if (n < 126) h[1] = n; else { h[1] = 126; h[2] = n >> 8; h[3] = n & 0xFF; }
  bool ok;
  if (n <= 256) { memcpy(small + hl, d, n); ok = k.c.write(small, hl + n) == hl + n; }   // ein Paket
  else ok = k.c.write(h, hl) == hl && k.c.write((const uint8_t*)d, n) == n;
  if (!ok) drop(i);
  return ok;
}
inline void send(uint8_t i, const String& s) { sendFrame(i, 1, s.c_str(), s.length()); }
inline void broadcast(const String& s) { for (uint8_t i = 0; i < MAXC; i++) if (conns[i].used && conns[i].open) sendFrame(i, 1, s.c_str(), s.length()); }

// Handshake: HTTP-Anfrage lesen und mit 101 antworten
inline bool handshake(uint8_t i) {
  Conn& k = conns[i];
  k.buf[k.n < BUF ? k.n : BUF - 1] = 0;
  char* end = strstr((char*)k.buf, "\r\n\r\n");
  if (!end) return false;
  char* key = nullptr;
  for (char* p = (char*)k.buf; p && *p; p = strstr(p, "\r\n")) {
    if (*p == '\r') p += 2;
    if (!strncasecmp(p, "Sec-WebSocket-Key:", 18)) { key = p + 18; break; }
  }
  if (!key) { drop(i); return false; }
  while (*key == ' ') key++;
  char* ke = key; while (*ke && *ke != '\r') ke++;
  String src = String(key).substring(0, ke - key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  uint8_t dg[20];
  sha1((const uint8_t*)src.c_str(), src.length(), dg);
  String resp = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + base64(dg, 20) + "\r\n\r\n";
  k.c.write((const uint8_t*)resp.c_str(), resp.length());
  size_t used = end + 4 - (char*)k.buf;
  memmove(k.buf, k.buf + used, k.n - used); k.n -= used;
  k.open = true;
  if (onOpen) onOpen(i);
  return true;
}

// Rahmen der App auswerten (die App maskiert immer)
inline void frames(uint8_t i) {
  Conn& k = conns[i];
  while (k.used && k.n >= 2) {
    uint8_t op = k.buf[0] & 0x0F;
    bool masked = k.buf[1] & 0x80;
    uint32_t len = k.buf[1] & 0x7F, hdr = 2;
    if (len == 126) { if (k.n < 4) return; len = (k.buf[2] << 8) | k.buf[3]; hdr = 4; }
    else if (len == 127) { drop(i); return; }
    if (masked) hdr += 4;
    if (hdr + len >= BUF) { drop(i); return; }
    if (k.n < hdr + len) return;
    uint8_t* pl = k.buf + hdr;
    if (masked) { uint8_t* m = k.buf + hdr - 4; for (uint32_t j = 0; j < len; j++) pl[j] ^= m[j & 3]; }
    if (op == 1) {                                  // Text
      char tmp[BUF];
      memcpy(tmp, pl, len); tmp[len] = 0;
      if (onText) onText(i, tmp, len);
    } else if (op == 8) { sendFrame(i, 8, nullptr, 0); drop(i); return; }   // Schließen
    else if (op == 9) sendFrame(i, 10, (const char*)pl, len);               // Ping → Pong
    if (!k.used) return;
    memmove(k.buf, k.buf + hdr + len, k.n - hdr - len); k.n -= hdr + len;
  }
}

inline void loop() {
  NetworkClient nc = server.accept();
  if (nc) {
    int8_t slot = -1;
    for (uint8_t i = 0; i < MAXC; i++) if (!conns[i].used) { slot = i; break; }
    if (slot < 0) {                                  // alle belegt: älteste Verbindung freigeben
      slot = 0;
      for (uint8_t i = 1; i < MAXC; i++) if (conns[i].since < conns[slot].since) slot = i;
      drop(slot);
    }
    Conn& k = conns[slot];
    k.c = nc; k.c.setNoDelay(true);
    k.used = true; k.open = false; k.n = 0; k.since = millis();
  }
  for (uint8_t i = 0; i < MAXC; i++) {
    Conn& k = conns[i];
    if (!k.used) continue;
    if (!k.c.connected()) { drop(i); continue; }
    int av = k.c.available();
    while (av > 0 && k.n < BUF - 1) {
      int r = k.c.read(k.buf + k.n, (size_t)av < (size_t)(BUF - 1 - k.n) ? av : BUF - 1 - k.n);
      if (r <= 0) break;
      k.n += r; av = k.c.available();
    }
    if (!k.open) {
      if (!handshake(i) && k.used && (k.n >= BUF - 1 || millis() - k.since > 3000)) drop(i);
      continue;
    }
    frames(i);
  }
}

}  // namespace ws
