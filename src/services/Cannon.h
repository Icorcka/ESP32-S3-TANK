#pragma once

#include <TankPorts.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <stdint.h>

#include "util/RtosTask.h"

class Cannon final : public tank::ICannon, private RtosTask {
 public:
  struct Settings {
    uint32_t pulseMs;
    uint32_t cooldownMs;
  };

  Cannon(gpio_num_t triggerPin, const Settings& settings);

  void begin();
  bool start(const TaskConfig& task);

  void fire() override;
  void setLocked(bool locked) override;

 private:
  void run() override;
  bool pullTrigger();
  void releaseTrigger();
  bool isReloading(TickType_t now) const;

  gpio_num_t pin_;
  Settings settings_;
  portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  bool locked_ = false;
  bool hasFired_ = false;
  TickType_t lastShotAt_ = 0;
};
