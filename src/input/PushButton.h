#pragma once

#include <TankPorts.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <stdint.h>

class PushButton {
 public:
  struct Settings {
    uint32_t debounceMs;
    uint32_t longPressMs;
  };

  PushButton(gpio_num_t pin, tank::ICommandSink& sink, const tank::Command& shortPress,
             const tank::Command& longPress, const Settings& settings);

  bool begin();

 private:
  static void onEdgeIsr(void* self);
  static void onDebounced(TimerHandle_t timer);
  void handleStableLevel();
  bool isPressed() const;

  gpio_num_t pin_;
  tank::ICommandSink& sink_;
  tank::Command shortPress_;
  tank::Command longPress_;
  Settings settings_;
  TimerHandle_t debounceTimer_ = nullptr;
  bool pressed_ = false;
  uint32_t pressedAtMs_ = 0;
};
