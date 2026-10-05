#include "services/TankController.h"

#include <esp_log.h>

#include "util/Clock.h"

namespace {

constexpr const char* kTag = "CTRL";

}  // namespace

TankController::TankController(tank::TankStateMachine& machine, const Settings& settings)
    : machine_(machine), settings_(settings) {}

bool TankController::start(const TaskConfig& task, tank::TankMode initialMode) {
  initialMode_ = initialMode;
  queue_ = xQueueCreate(settings_.queueLength, sizeof(tank::Command));
  return queue_ != nullptr && startTask(task);
}

bool TankController::post(const tank::Command& command) {
  if (queue_ == nullptr) {
    return false;
  }
  const bool urgent = command.type == tank::CommandType::ToggleEmergencyStop;
  const BaseType_t sent = urgent ? xQueueSendToFront(queue_, &command, 0)
                                 : xQueueSendToBack(queue_, &command, 0);
  return sent == pdTRUE;
}

void TankController::run() {
  machine_.start(initialMode_, uptimeMs());

  for (;;) {
    tank::Command command;
    if (xQueueReceive(queue_, &command, pdMS_TO_TICKS(settings_.tickPeriodMs)) == pdTRUE) {
      logCommand(command);
      machine_.handle(command, uptimeMs());
    }
    machine_.tick(uptimeMs());
  }
}

void TankController::logCommand(const tank::Command& command) {
  if (command.type == tank::CommandType::Drive) {
    return;
  }
  ESP_LOGI(kTag, "%s (%s)", tank::toString(command.type), tank::toString(command.source));
}
