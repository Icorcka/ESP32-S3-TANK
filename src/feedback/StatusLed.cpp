#include "feedback/StatusLed.h"

#include <esp_err.h>

namespace {

constexpr uint32_t kTickMs = 100;
constexpr uint32_t kSlowBlinkTicks = 5;
constexpr uint32_t kFastBlinkTicks = 1;

bool blink(uint32_t phase, uint32_t halfPeriodTicks) {
  return (phase / halfPeriodTicks) % 2 == 0;
}

}  // namespace

StatusLed::StatusLed(gpio_num_t pin) : pin_(pin) {}

bool StatusLed::begin() {
  if (pin_ == GPIO_NUM_NC) {
    return false;
  }
  gpio_config_t output{};
  output.pin_bit_mask = 1ULL << pin_;
  output.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&output));

  timer_ = xTimerCreate("status-led", pdMS_TO_TICKS(kTickMs), pdTRUE, this, &StatusLed::onTimer);
  return timer_ != nullptr && xTimerStart(timer_, 0) == pdPASS;
}

void StatusLed::onModeChanged(tank::TankMode mode) {
  mode_ = mode;
}

void StatusLed::onTimer(TimerHandle_t timer) {
  static_cast<StatusLed*>(pvTimerGetTimerID(timer))->update();
}

void StatusLed::update() {
  ++phase_;
  bool on = true;
  switch (mode_.load()) {
    case tank::TankMode::Manual:
      on = true;
      break;
    case tank::TankMode::Demo:
      on = blink(phase_, kSlowBlinkTicks);
      break;
    case tank::TankMode::EmergencyStop:
      on = blink(phase_, kFastBlinkTicks);
      break;
  }
  gpio_set_level(pin_, on ? 1 : 0);
}
