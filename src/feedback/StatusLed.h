#pragma once

#include <TankPorts.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <stdint.h>

#include <atomic>

class StatusLed final : public tank::ITankObserver {
 public:
  explicit StatusLed(gpio_num_t pin);

  bool begin();

  void onModeChanged(tank::TankMode mode) override;

 private:
  static void onTimer(TimerHandle_t timer);
  void update();

  gpio_num_t pin_;
  TimerHandle_t timer_ = nullptr;
  std::atomic<tank::TankMode> mode_{tank::TankMode::Manual};
  uint32_t phase_ = 0;
};
