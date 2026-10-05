#pragma once

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <stdint.h>

struct MotorPins {
  gpio_num_t pwm;
  gpio_num_t in1;
  gpio_num_t in2;
};

struct PwmSettings {
  uint32_t frequencyHz;
  uint8_t resolutionBits;
  uint32_t minDuty;
};

// One TB6612FNG channel:
//   IN1=1, IN2=0 → forward;   IN1=0, IN2=1 → reverse;   IN1=IN2=1 → brake.
class Motor {
 public:
  Motor(const MotorPins& pins, ledc_channel_t channel, const PwmSettings& pwm, bool inverted);

  void begin();
  void setSpeed(float speed);

 private:
  uint32_t dutyFor(float magnitude) const;
  void writeDuty(uint32_t duty);

  MotorPins pins_;
  ledc_channel_t channel_;
  PwmSettings pwm_;
  bool inverted_;
};
