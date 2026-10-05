#include "net/WifiAccessPoint.h"

#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <string.h>

namespace {

constexpr const char* kTag = "WIFI";

void initNvs() {
  // Wi-Fi keeps radio calibration in NVS. If the partition is full or was written
  // by another ESP-IDF version, erase it and retry (the standard recipe).
  esp_err_t result = nvs_flash_init();
  if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    result = nvs_flash_init();
  }
  ESP_ERROR_CHECK(result);
}

}  // namespace

WifiAccessPoint::WifiAccessPoint(const Settings& settings) : settings_(settings) {}

void WifiAccessPoint::begin() {
  initNvs();
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t* netif = esp_netif_create_default_wifi_ap();

  const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init));

  wifi_config_t config{};
  strlcpy(reinterpret_cast<char*>(config.ap.ssid), settings_.ssid, sizeof(config.ap.ssid));
  strlcpy(reinterpret_cast<char*>(config.ap.password), settings_.password,
          sizeof(config.ap.password));
  config.ap.ssid_len = static_cast<uint8_t>(strlen(settings_.ssid));
  config.ap.channel = settings_.channel;
  config.ap.max_connection = settings_.maxClients;
  config.ap.authmode = WIFI_AUTH_WPA2_PSK;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
  ESP_ERROR_CHECK(esp_wifi_start());

  esp_netif_ip_info_t ip{};
  ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));
  ESP_LOGI(kTag, "Network \"%s\" is up, remote: http://" IPSTR "/", settings_.ssid, IP2STR(&ip.ip));
}
