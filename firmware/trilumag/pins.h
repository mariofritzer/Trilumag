#pragma once
// Pinbelegung: Vorlagen pro Board und erlaubte Pins pro Chip

#include <Arduino.h>

struct PinSet {
  int8_t rx, tx, de, led, snsR, snsL;
};

struct BoardPreset {
  const char* id;
  const char* name;
  const char* chip;
  PinSet pins;
};

// Reihenfolge der Pins: RX, TX, DE, LED, SNS rechts, SNS links
const BoardPreset BOARDS[] = {
  {"s3-devkitc",   "ESP32-S3-DevKitC / Trilumag-Hauptplatine", "ESP32-S3", {18, 17,  8, 16,  4,  5}},
  {"esp32-devkit", "ESP32-DevKit (WROOM-32)",                  "ESP32",    {16, 17,  4, 18, 32, 33}},
  {"c3-supermini", "ESP32-C3 SuperMini",                       "ESP32-C3", {20, 21, 10,  7,  3,  4}},
  {"c6-devkit",    "ESP32-C6 DevKitC / SuperMini",             "ESP32-C6", {19, 18, 20, 21, 22, 23}},
};
const size_t BOARD_COUNT = sizeof(BOARDS) / sizeof(BOARDS[0]);

// Vorgabe für den Stromsensor INA226 (I²C) pro Board: freie Pins, die nicht mit Boot, Flash oder USB kollidieren
struct I2cDefault { const char* id; int8_t sda, scl; };
const I2cDefault I2C_DEFAULTS[] = {
  {"s3-devkitc",   1,  2},
  {"esp32-devkit", 21, 22},
  {"c3-supermini", 5,  6},
  {"c6-devkit",    6,  7},
};
inline void i2cDefault(const char* board, int8_t& sda, int8_t& scl) {
  sda = scl = -1;
  for (const I2cDefault& d : I2C_DEFAULTS) if (!strcmp(d.id, board)) { sda = d.sda; scl = d.scl; }
}

// Pins, die auf dem jeweiligen Chip frei nutzbar sind (ohne Boot-, Flash-, PSRAM- und USB-Pins)
#if CONFIG_IDF_TARGET_ESP32S3
const int8_t VALID_PINS[] = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42, 47, 48};
#elif CONFIG_IDF_TARGET_ESP32C3
const int8_t VALID_PINS[] = {0, 1, 3, 4, 5, 6, 7, 10, 20, 21};
#elif CONFIG_IDF_TARGET_ESP32C6
const int8_t VALID_PINS[] = {0, 1, 2, 3, 6, 7, 10, 11, 14, 16, 17, 18, 19, 20, 21, 22, 23};
#else
const int8_t VALID_PINS[] = {4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
#endif
const size_t VALID_COUNT = sizeof(VALID_PINS) / sizeof(VALID_PINS[0]);

inline bool pinValid(int p) {
  for (size_t i = 0; i < VALID_COUNT; i++) if (VALID_PINS[i] == p) return true;
  return false;
}

// Liefert eine Fehlermeldung oder nullptr, wenn alles passt
inline const char* checkPins(const PinSet& s) {
  const int8_t v[6] = {s.rx, s.tx, s.de, s.led, s.snsR, s.snsL};
  for (int i = 0; i < 6; i++) {
    if (!pinValid(v[i])) return "Ein Pin ist auf diesem Chip nicht nutzbar";
    for (int j = 0; j < i; j++) if (v[i] == v[j]) return "Ein Pin ist doppelt belegt";
  }
  return nullptr;
}

inline const BoardPreset* defaultBoard(const char* chip) {
  for (size_t i = 0; i < BOARD_COUNT; i++) if (!strcmp(BOARDS[i].chip, chip)) return &BOARDS[i];
  return &BOARDS[0];
}

inline const BoardPreset* findBoard(const char* id) {
  for (size_t i = 0; i < BOARD_COUNT; i++) if (!strcmp(BOARDS[i].id, id)) return &BOARDS[i];
  return nullptr;
}
