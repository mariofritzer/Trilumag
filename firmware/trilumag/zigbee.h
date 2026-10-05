#pragma once
// Philips Hue über Zigbee (nur ESP32-C6)
// Das Hauptpanel meldet sich zusätzlich als Zigbee-3.0-Farblampe "Trilumag Wand". Eine Hue Bridge (auch die
// Bridge Pro) nimmt es dann wie eine Lampe eines anderen Herstellers auf: Ein/Aus, Helligkeit und Farbe der
// ganzen Wand. Effekte und einzelne Panels bleiben in der Trilumag-App.
// Gebaut wird das nur für den C6 mit "Zigbee ZCZR" (Router, wie Netzlampen) und einer Speicheraufteilung mit
// den Bereichen zb_storage und zb_fct (partitions_c6_zigbee.csv). Alle anderen Chips haben kein Zigbee.

#if defined(ZIGBEE_MODE_ZCZR) && CONFIG_SOC_IEEE802154_SUPPORTED
#define HAS_ZIGBEE 1
#include "ZigbeeCore.h"
#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esp_coexist.h"
#include "esp_partition.h"
#include <math.h>

namespace zb {

// Eigene Lampe statt ZigbeeColorDimmableLight: die merkt sich den Zustand intern und überhört sonst Befehle,
// nachdem die App die Wand umgestellt hat.
class WallLight : public ZigbeeEP {
public:
  explicit WallLight(uint8_t ep) : ZigbeeEP(ep) {
    _device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID;
    esp_zb_color_dimmable_light_cfg_t c = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
    _cluster_list = esp_zb_color_dimmable_light_clusters_create(&c);
    _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID, .app_device_version = 0};
  }
  // läuft in der Zigbee-Aufgabe: nur merken, das Hauptprogramm übernimmt es
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t* m) override;
};

WallLight light(10);
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile bool pending = false;
bool pOn = true; bool hasOn = false, hasLevel = false, hasColor = false;
uint8_t pLevel = 254; uint16_t pX = 0, pY = 0;
bool started = false;
String problem;                           // warum Zigbee nicht läuft (leer = läuft oder aus)

void WallLight::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t* m) {
  const void* v = m->attribute.data.value;
  if (!v) return;
  portENTER_CRITICAL(&mux);
  if (m->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF && m->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) { pOn = *(const bool*)v; hasOn = true; pending = true; }
  else if (m->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL && m->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID) { pLevel = *(const uint8_t*)v; hasLevel = true; pending = true; }
  else if (m->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
    if (m->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID) { pX = *(const uint16_t*)v; hasColor = true; pending = true; }
    if (m->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID) { pY = *(const uint16_t*)v; hasColor = true; pending = true; }
  }
  portEXIT_CRITICAL(&mux);
}

// CIE xy (0..65535) in RGB für die LEDs: hellster Kanal = 255
inline void xyToRgb(uint16_t xi, uint16_t yi, uint8_t& r, uint8_t& g, uint8_t& b) {
  float x = xi / 65535.0f, y = yi / 65535.0f;
  if (y < 0.001f) y = 0.001f;
  float X = x / y, Z = (1 - x - y) / y;
  float R = 3.2406f * X - 1.5372f - 0.4986f * Z, G = -0.9689f * X + 1.8758f + 0.0415f * Z, B = 0.0557f * X - 0.2040f + 1.0570f * Z;
  R = R < 0 ? 0 : R; G = G < 0 ? 0 : G; B = B < 0 ? 0 : B;
  float m = fmaxf(R, fmaxf(G, B)); if (m <= 0) m = 1;
  r = (uint8_t)lroundf(255 * R / m); g = (uint8_t)lroundf(255 * G / m); b = (uint8_t)lroundf(255 * B / m);
}
inline void rgbToXy(uint8_t r, uint8_t g, uint8_t b, uint16_t& xi, uint16_t& yi) {
  float R = r / 255.0f, G = g / 255.0f, B = b / 255.0f;
  float X = 0.4124f * R + 0.3576f * G + 0.1805f * B, Y = 0.2126f * R + 0.7152f * G + 0.0722f * B, Z = 0.0193f * R + 0.1192f * G + 0.9505f * B;
  float s = X + Y + Z;
  if (s <= 0) { xi = 20577; yi = 21567; return; }            // Weißpunkt
  xi = (uint16_t)(X / s * 65535); yi = (uint16_t)(Y / s * 65535);
}

bool partitionsOk() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "zb_storage") &&
         esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "zb_fct");
}

bool begin() {
  if (!partitionsOk()) { problem = "Speicheraufteilung ohne Zigbee-Bereich: einmal mit dem Webinstaller flashen"; return false; }
  light.setManufacturerAndModel("Trilumag", "Trilumag Wand");
  Zigbee.addEndpoint(&light);
  esp_coex_wifi_i154_enable();            // WLAN und Zigbee teilen sich die Antenne
  started = Zigbee.begin(ZIGBEE_ROUTER);
  if (!started) problem = "Zigbee ließ sich nicht starten";
  return started;
}

bool joined() {
  if (!started || !esp_zb_lock_acquire(pdMS_TO_TICKS(20))) return false;
  bool j = esp_zb_bdb_dev_joined();
  esp_zb_lock_release();
  return j;
}
uint8_t channel() {
  if (!started || !esp_zb_lock_acquire(pdMS_TO_TICKS(20))) return 0;
  uint8_t c = esp_zb_get_current_channel();
  esp_zb_lock_release();
  return c;
}

// Zustand der Wand an die Bridge melden (wenn sie über App, Home Assistant oder Antippen geändert wurde)
void report(bool on, uint8_t level, uint8_t r, uint8_t g, uint8_t b) {
  if (!started || !esp_zb_lock_acquire(pdMS_TO_TICKS(50))) return;
  uint16_t x, y; rgbToXy(r, g, b, x, y);
  if (level < 1) level = 1;
  if (level > 254) level = 254;
  esp_zb_zcl_set_attribute_val(10, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &on, false);
  esp_zb_zcl_set_attribute_val(10, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, &level, false);
  esp_zb_zcl_set_attribute_val(10, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, &x, false);
  esp_zb_zcl_set_attribute_val(10, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID, &y, false);
  esp_zb_lock_release();
}

void factoryReset() { if (started) Zigbee.factoryReset(); }   // startet neu, danach wieder koppelbar

}  // namespace zb
#else
#define HAS_ZIGBEE 0
#endif
