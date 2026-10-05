#pragma once

#include <stdint.h>

namespace tank {

/// Tank operating mode — a state of the finite state machine (TankStateMachine).
enum class TankMode : uint8_t {
  Manual,         ///< operator in control: gamepad via the browser remote, BOOT button
  Demo,           ///< autopilot from the original sketch: drives forward, fires every 5 s
  EmergencyStop,  ///< emergency stop: motors halted, cannon locked
};

/// Track speeds: -1.0 (full reverse) … 0 (stop) … +1.0 (full forward).
struct TrackSpeeds {
  float left = 0.0f;
  float right = 0.0f;
};

enum class CommandType : uint8_t {
  Drive,
  Fire,
  ToggleDemo,
  ToggleEmergencyStop,
};

enum class CommandSource : uint8_t {
  Web,
  Button,
};

struct Command {
  CommandType type;
  CommandSource source;
  TrackSpeeds tracks;  ///< only for CommandType::Drive

  static constexpr Command drive(CommandSource source, TrackSpeeds tracks) {
    return {CommandType::Drive, source, tracks};
  }
  static constexpr Command fire(CommandSource source) {
    return {CommandType::Fire, source, {}};
  }
  static constexpr Command toggleDemo(CommandSource source) {
    return {CommandType::ToggleDemo, source, {}};
  }
  static constexpr Command toggleEmergencyStop(CommandSource source) {
    return {CommandType::ToggleEmergencyStop, source, {}};
  }
};

inline const char* toString(TankMode mode) {
  switch (mode) {
    case TankMode::Manual:
      return "MANUAL";
    case TankMode::Demo:
      return "DEMO";
    case TankMode::EmergencyStop:
      return "E-STOP";
  }
  return "?";
}

inline const char* toString(CommandType type) {
  switch (type) {
    case CommandType::Drive:
      return "DRIVE";
    case CommandType::Fire:
      return "FIRE";
    case CommandType::ToggleDemo:
      return "TOGGLE-DEMO";
    case CommandType::ToggleEmergencyStop:
      return "TOGGLE-E-STOP";
  }
  return "?";
}

inline const char* toString(CommandSource source) {
  switch (source) {
    case CommandSource::Web:
      return "web";
    case CommandSource::Button:
      return "button";
  }
  return "?";
}

}  // namespace tank
