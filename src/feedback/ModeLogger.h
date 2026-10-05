#pragma once

#include <TankPorts.h>
#include <esp_log.h>

class ModeLogger final : public tank::ITankObserver {
 public:
  void onModeChanged(tank::TankMode mode) override {
    ESP_LOGI("TANK", "Mode: %s", tank::toString(mode));
  }
};
