#pragma once

#include <DriveMath.h>
#include <TankPorts.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>

#include "drivers/Motor.h"
#include "util/RtosTask.h"

class DriveSystem final : public tank::IDrive, private RtosTask {
 public:
  struct Settings {
    uint32_t loopPeriodMs;
    uint32_t commandTimeoutMs;
    float accelPerSecond;
    float decelPerSecond;
  };

  DriveSystem(Motor& left, Motor& right, const Settings& settings);

  bool start(const TaskConfig& task);

  void setTarget(const tank::TrackSpeeds& target) override;
  void emergencyStop() override;

 private:
  struct Setpoint {
    tank::TrackSpeeds tracks;
    TickType_t issuedAt;
  };

  void run() override;
  tank::TrackSpeeds freshTarget() const;

  Motor& left_;
  Motor& right_;
  Settings settings_;
  QueueHandle_t mailbox_ = nullptr;
  std::atomic<bool> emergencyStopRequested_{false};
  tank::SlewRateLimiter leftRamp_;
  tank::SlewRateLimiter rightRamp_;
};
