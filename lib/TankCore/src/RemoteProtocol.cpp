#include "RemoteProtocol.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "DriveMath.h"

namespace tank {
namespace remote {

namespace {

bool readNumber(const char*& cursor, char separator, float& value) {
  char* end = nullptr;
  value = std::strtof(cursor, &end);
  if (end == cursor || *end != separator || !std::isfinite(value)) {
    return false;
  }
  cursor = end + 1;
  return true;
}

float clampTrigger(float value) {
  return std::fmin(std::fmax(value, 0.0f), 1.0f);
}

}  // namespace

bool parseGamepadFrame(const char* frame, GamepadState& out) {
  if (frame == nullptr || std::strncmp(frame, "S,", 2) != 0) {
    return false;
  }
  const char* cursor = frame + 2;
  float leftY = 0.0f;
  float rightX = 0.0f;
  float r2 = 0.0f;
  if (!readNumber(cursor, ',', leftY) || !readNumber(cursor, ',', rightX) ||
      !readNumber(cursor, ',', r2)) {
    return false;
  }

  if (*cursor < '0' || *cursor > '9') {
    return false;
  }
  char* end = nullptr;
  const unsigned long buttons = std::strtoul(cursor, &end, 10);
  if (*end != '\0') {
    return false;
  }

  out.leftY = clampUnit(leftY);
  out.rightX = clampUnit(rightX);
  out.r2 = clampTrigger(r2);
  out.cross = (buttons & kCross) != 0;
  out.circle = (buttons & kCircle) != 0;
  out.options = (buttons & kOptions) != 0;
  out.l1 = (buttons & kL1) != 0;
  out.r1 = (buttons & kR1) != 0;
  return true;
}

size_t formatStatusFrame(char* buffer, size_t size, TankMode mode, unsigned gear) {
  const int written = std::snprintf(buffer, size, "T,%s,%u", toString(mode), gear);
  if (written <= 0 || static_cast<size_t>(written) >= size) {
    return 0;
  }
  return static_cast<size_t>(written);
}

}  // namespace remote
}  // namespace tank
