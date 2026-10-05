#pragma once

#include <stddef.h>
#include <stdint.h>

#include "GamepadMapper.h"
#include "TankTypes.h"

namespace tank {
namespace remote {

enum ButtonBit : uint32_t {
  kCross = 1u << 0,
  kCircle = 1u << 1,
  kOptions = 1u << 2,
  kL1 = 1u << 3,
  kR1 = 1u << 4,
};

constexpr char kEmergencyStopFrame[] = "E";

bool parseGamepadFrame(const char* frame, GamepadState& out);

size_t formatStatusFrame(char* buffer, size_t size, TankMode mode, unsigned gear);

}  // namespace remote
}  // namespace tank
