#include "services/DriveSystem.h"

DriveSystem::DriveSystem(Motor& left, Motor& right, const Settings& settings)
    : left_(left),
      right_(right),
      settings_(settings),
      leftRamp_(settings.accelPerSecond, settings.decelPerSecond),
      rightRamp_(settings.accelPerSecond, settings.decelPerSecond) {}

bool DriveSystem::start(const TaskConfig& task) {
  mailbox_ = xQueueCreate(1, sizeof(Setpoint));
  return mailbox_ != nullptr && startTask(task);
}

void DriveSystem::setTarget(const tank::TrackSpeeds& target) {
  if (mailbox_ == nullptr) {
    return;
  }
  const Setpoint setpoint{{tank::clampUnit(target.left), tank::clampUnit(target.right)},
                          xTaskGetTickCount()};
  xQueueOverwrite(mailbox_, &setpoint);
}

void DriveSystem::emergencyStop() {
  emergencyStopRequested_ = true;
  setTarget({});
}

void DriveSystem::run() {
  const TickType_t period = pdMS_TO_TICKS(settings_.loopPeriodMs);
  const float dtSeconds = settings_.loopPeriodMs / 1000.0f;
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWake, period);

    if (emergencyStopRequested_.exchange(false)) {
      leftRamp_.reset();
      rightRamp_.reset();
    }
    const tank::TrackSpeeds target = freshTarget();
    left_.setSpeed(leftRamp_.update(target.left, dtSeconds));
    right_.setSpeed(rightRamp_.update(target.right, dtSeconds));
  }
}

tank::TrackSpeeds DriveSystem::freshTarget() const {
  Setpoint setpoint;
  if (xQueuePeek(mailbox_, &setpoint, 0) != pdTRUE) {
    return {};
  }
  const TickType_t age = xTaskGetTickCount() - setpoint.issuedAt;
  const bool expired = age > pdMS_TO_TICKS(settings_.commandTimeoutMs);
  return expired ? tank::TrackSpeeds{} : setpoint.tracks;  // failsafe
}
