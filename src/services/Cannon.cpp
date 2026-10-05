#include "services/Cannon.h"

#include <esp_err.h>
#include <esp_log.h>

namespace {

constexpr const char* kTag = "CANNON";

}  // namespace

Cannon::Cannon(gpio_num_t triggerPin, const Settings& settings)
    : pin_(triggerPin), settings_(settings) {}

void Cannon::begin() {
  gpio_set_level(pin_, 0);
  gpio_config_t output{};
  output.pin_bit_mask = 1ULL << pin_;
  output.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&output));
}

bool Cannon::start(const TaskConfig& task) {
  return startTask(task);
}

void Cannon::fire() {
  if (taskHandle() != nullptr) {
    xTaskNotifyGive(taskHandle());
  }
}

void Cannon::setLocked(bool locked) {
  portENTER_CRITICAL(&lock_);
  locked_ = locked;
  if (locked) {
    gpio_set_level(pin_, 0);
  }
  portEXIT_CRITICAL(&lock_);
}

void Cannon::run() {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    const TickType_t now = xTaskGetTickCount();
    if (isReloading(now)) {
      ESP_LOGI(kTag, "Reloading — shot skipped");
      continue;
    }
    if (!pullTrigger()) {
      ESP_LOGI(kTag, "Cannon is locked");
      continue;
    }
    hasFired_ = true;
    lastShotAt_ = now;
    vTaskDelay(pdMS_TO_TICKS(settings_.pulseMs));
    releaseTrigger();
    ESP_LOGI(kTag, "Fire!");
  }
}

bool Cannon::pullTrigger() {
  portENTER_CRITICAL(&lock_);
  const bool allowed = !locked_;
  if (allowed) {
    gpio_set_level(pin_, 1);
  }
  portEXIT_CRITICAL(&lock_);
  return allowed;
}

void Cannon::releaseTrigger() {
  gpio_set_level(pin_, 0);
}

bool Cannon::isReloading(TickType_t now) const {
  return hasFired_ && now - lastShotAt_ < pdMS_TO_TICKS(settings_.cooldownMs);
}
