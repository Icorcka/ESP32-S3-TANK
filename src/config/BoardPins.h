#pragma once

#include <driver/gpio.h>
#include <sdkconfig.h>

#include "drivers/Motor.h"

#if !CONFIG_IDF_TARGET_ESP32S3
#error "The pinout is for the ESP32-S3 — for another chip update config/BoardPins.h"
#endif

namespace board {

constexpr MotorPins kLeftMotor{GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_21};
constexpr MotorPins kRightMotor{GPIO_NUM_14, GPIO_NUM_12, GPIO_NUM_13};
constexpr gpio_num_t kFirePin = GPIO_NUM_4;
constexpr gpio_num_t kButtonPin = GPIO_NUM_0;
constexpr gpio_num_t kStatusLedPin = GPIO_NUM_NC;

}  // namespace board
