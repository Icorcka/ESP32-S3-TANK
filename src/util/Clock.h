#pragma once

#include <esp_timer.h>
#include <stdint.h>

inline uint32_t uptimeMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}
