#include <esp_log.h>

namespace {

constexpr const char* kTag = "BOOT";

}  // namespace

extern "C" void app_main() {
  ESP_LOGI(kTag, "ESP32-S3-TANK: tank on ESP-IDF + FreeRTOS");
}
