#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

struct TaskConfig {
  const char* name;
  uint32_t stackBytes;
  UBaseType_t priority;
  BaseType_t core;
};

class RtosTask {
 public:
  RtosTask() = default;
  virtual ~RtosTask() = default;
  RtosTask(const RtosTask&) = delete;
  RtosTask& operator=(const RtosTask&) = delete;

 protected:
  bool startTask(const TaskConfig& config) {
    return xTaskCreatePinnedToCore(&RtosTask::entry, config.name, config.stackBytes, this,
                                   config.priority, &handle_, config.core) == pdPASS;
  }

  TaskHandle_t taskHandle() const { return handle_; }

  virtual void run() = 0;

 private:
  static void entry(void* self) {
    static_cast<RtosTask*>(self)->run();
    vTaskDelete(nullptr);
  }

  TaskHandle_t handle_ = nullptr;
};
