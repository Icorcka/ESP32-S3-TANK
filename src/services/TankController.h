#pragma once

#include <TankStateMachine.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "util/RtosTask.h"

class TankController final : public tank::ICommandSink, private RtosTask {
 public:
  struct Settings {
    size_t queueLength;
    uint32_t tickPeriodMs;
  };

  TankController(tank::TankStateMachine& machine, const Settings& settings);

  bool start(const TaskConfig& task, tank::TankMode initialMode);

  bool post(const tank::Command& command) override;

 private:
  void run() override;
  static void logCommand(const tank::Command& command);

  tank::TankStateMachine& machine_;
  Settings settings_;
  tank::TankMode initialMode_ = tank::TankMode::Manual;
  QueueHandle_t queue_ = nullptr;
};
