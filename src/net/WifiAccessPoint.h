#pragma once

#include <stdint.h>

// The tank's own Wi-Fi network (access point mode). The tank is at 192.168.4.1.
class WifiAccessPoint {
 public:
  struct Settings {
    const char* ssid;
    const char* password;
    uint8_t channel;
    uint8_t maxClients;
  };

  explicit WifiAccessPoint(const Settings& settings);

  void begin();

 private:
  Settings settings_;
};
