#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TankPorts.h"
#include "TankTypes.h"

namespace tank {

struct GamepadState {
  float leftY = 0.0f;   // -1 (towards you) … +1 (away)
  float rightX = 0.0f;  // -1 (left) … +1 (right)
  float r2 = 0.0f;      // 0 (released) … 1 (fully pulled)
  bool cross = false;
  bool circle = false;
  bool options = false;
  bool l1 = false;
  bool r1 = false;
};

class GamepadMapper {
 public:
  static constexpr size_t kGearCount = 3;

  struct Settings {
    float stickDeadzone;
    float triggerThreshold;
    float gears[kGearCount];
    uint8_t initialGear;
  };

  GamepadMapper(ICommandSink& sink, CommandSource source, const Settings& settings);

  void reset(const GamepadState& current);

  void update(const GamepadState& state);
  void stop();

  uint8_t gear() const { return gear_; }
  float speedLimit() const { return settings_.gears[gear_]; }

 private:
  void handleButtons(const GamepadState& now);
  void shiftGear(int delta);
  bool triggerPulled(const GamepadState& state) const;
  TrackSpeeds tracksFrom(const GamepadState& state) const;

  ICommandSink& sink_;
  CommandSource source_;
  Settings settings_;
  GamepadState previous_;
  uint8_t gear_;
};

}  // namespace tank
