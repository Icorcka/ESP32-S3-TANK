#include "drivers/Motor.h"

#include <esp_err.h>

#include <cmath>

namespace {

constexpr float kStopThreshold = 0.01f;
constexpr ledc_mode_t kSpeedMode = LEDC_LOW_SPEED_MODE;  // ESP32-S3 has only the low-speed mode
constexpr ledc_timer_t kTimer = LEDC_TIMER_0;            // both motors share one timer

}  // namespace

Motor::Motor(const MotorPins& pins, ledc_channel_t channel, const PwmSettings& pwm, bool inverted)
    : pins_(pins), channel_(channel), pwm_(pwm), inverted_(inverted) {}

void Motor::begin() {
  gpio_config_t direction{};
  direction.pin_bit_mask = (1ULL << pins_.in1) | (1ULL << pins_.in2);
  direction.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&direction));

  ledc_timer_config_t timer{};
  timer.speed_mode = kSpeedMode;
  timer.duty_resolution = static_cast<ledc_timer_bit_t>(pwm_.resolutionBits);
  timer.timer_num = kTimer;
  timer.freq_hz = pwm_.frequencyHz;
  timer.clk_cfg = LEDC_AUTO_CLK;
  ESP_ERROR_CHECK(ledc_timer_config(&timer));

  ledc_channel_config_t channel{};
  channel.gpio_num = pins_.pwm;
  channel.speed_mode = kSpeedMode;
  channel.channel = channel_;
  channel.timer_sel = kTimer;
  channel.duty = 0;
  ESP_ERROR_CHECK(ledc_channel_config(&channel));

  setSpeed(0.0f);
}

void Motor::setSpeed(float speed) {
  if (inverted_) {
    speed = -speed;
  }
  const float magnitude = std::fabs(speed);

  if (magnitude < kStopThreshold) {
    gpio_set_level(pins_.in1, 1);  // IN1 = IN2 = 1 → short brake
    gpio_set_level(pins_.in2, 1);
    writeDuty(0);
    return;
  }

  const bool forward = speed > 0.0f;
  gpio_set_level(pins_.in1, forward ? 1 : 0);
  gpio_set_level(pins_.in2, forward ? 0 : 1);
  writeDuty(dutyFor(magnitude));
}

uint32_t Motor::dutyFor(float magnitude) const {
  const uint32_t maxDuty = (1u << pwm_.resolutionBits) - 1u;
  const float span = static_cast<float>(maxDuty - pwm_.minDuty);
  return pwm_.minDuty + static_cast<uint32_t>(std::fmin(magnitude, 1.0f) * span + 0.5f);
}

void Motor::writeDuty(uint32_t duty) {
  ledc_set_duty(kSpeedMode, channel_, duty);
  ledc_update_duty(kSpeedMode, channel_);
}
