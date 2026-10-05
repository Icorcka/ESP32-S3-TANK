#pragma once

#include <cmath>

#include "TankTypes.h"

namespace tank {

inline float clampUnit(float value) {
  if (std::isnan(value)) {
    return 0.0f;
  }
  return value > 1.0f ? 1.0f : (value < -1.0f ? -1.0f : value);
}

inline float applyDeadzone(float value, float deadzone) {
  const float magnitude = std::fabs(value);
  if (magnitude <= deadzone) {
    return 0.0f;
  }
  const float rescaled = (std::fmin(magnitude, 1.0f) - deadzone) / (1.0f - deadzone);
  return std::copysign(rescaled, value);
}

inline TrackSpeeds mixArcade(float throttle, float turn) {
  float left = throttle + turn;
  float right = throttle - turn;
  const float peak = std::fmax(std::fabs(left), std::fabs(right));
  if (peak > 1.0f) {
    left /= peak;
    right /= peak;
  }
  return {left, right};
}

inline TrackSpeeds scaled(const TrackSpeeds& tracks, float factor) {
  return {tracks.left * factor, tracks.right * factor};
}

class SlewRateLimiter {
 public:
  SlewRateLimiter(float accelPerSecond, float decelPerSecond)
      : accelPerSecond_(accelPerSecond), decelPerSecond_(decelPerSecond) {}

  float update(float target, float dtSeconds) {
    const bool slowingDown = std::fabs(target) < std::fabs(value_) || target * value_ < 0.0f;
    const float maxStep = (slowingDown ? decelPerSecond_ : accelPerSecond_) * dtSeconds;
    const float delta = target - value_;
    if (std::fabs(delta) <= maxStep) {
      value_ = target;
    } else {
      value_ += std::copysign(maxStep, delta);
    }
    return value_;
  }

  void reset(float value = 0.0f) { value_ = value; }
  float value() const { return value_; }

 private:
  float accelPerSecond_;
  float decelPerSecond_;
  float value_ = 0.0f;
};

}  // namespace tank
