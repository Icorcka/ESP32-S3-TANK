#include "input/PushButton.h"

#include <esp_attr.h>
#include <esp_err.h>

#include "util/Clock.h"

PushButton::PushButton(gpio_num_t pin, tank::ICommandSink& sink, const tank::Command& shortPress,
                       const tank::Command& longPress, const Settings& settings)
    : pin_(pin),
      sink_(sink),
      shortPress_(shortPress),
      longPress_(longPress),
      settings_(settings) {}

bool PushButton::begin() {
  gpio_config_t input{};
  input.pin_bit_mask = 1ULL << pin_;
  input.mode = GPIO_MODE_INPUT;
  input.pull_up_en = GPIO_PULLUP_ENABLE;
  input.intr_type = GPIO_INTR_ANYEDGE;
  ESP_ERROR_CHECK(gpio_config(&input));

  debounceTimer_ = xTimerCreate("button", pdMS_TO_TICKS(settings_.debounceMs), pdFALSE, this,
                                &PushButton::onDebounced);
  if (debounceTimer_ == nullptr) {
    return false;
  }
  pressed_ = isPressed();

  // The GPIO ISR service is shared by all pins — already installed is not an error.
  const esp_err_t service = gpio_install_isr_service(0);
  if (service != ESP_OK && service != ESP_ERR_INVALID_STATE) {
    return false;
  }
  return gpio_isr_handler_add(pin_, &PushButton::onEdgeIsr, this) == ESP_OK;
}

// IRAM_ATTR keeps the ISR in RAM so it runs even while the flash cache is busy.
void IRAM_ATTR PushButton::onEdgeIsr(void* self) {
  auto* button = static_cast<PushButton*>(self);
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  xTimerResetFromISR(button->debounceTimer_, &higherPriorityTaskWoken);
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void PushButton::onDebounced(TimerHandle_t timer) {
  static_cast<PushButton*>(pvTimerGetTimerID(timer))->handleStableLevel();
}

void PushButton::handleStableLevel() {
  const bool pressedNow = isPressed();
  if (pressedNow == pressed_) {
    return;
  }
  pressed_ = pressedNow;

  const uint32_t now = uptimeMs();
  if (pressedNow) {
    pressedAtMs_ = now;  // act on release, once the press duration is known
    return;
  }
  const bool isLongPress = now - pressedAtMs_ >= settings_.longPressMs;
  sink_.post(isLongPress ? longPress_ : shortPress_);
}

bool PushButton::isPressed() const {
  return gpio_get_level(pin_) == 0;
}
