#include <TankStateMachine.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config/BoardPins.h"
#include "config/TankConfig.h"
#include "drivers/Motor.h"
#include "feedback/ModeLogger.h"
#include "feedback/StatusLed.h"
#include "input/PushButton.h"
#include "input/WebRemote.h"
#include "net/WifiAccessPoint.h"
#include "services/Cannon.h"
#include "services/DriveSystem.h"
#include "services/TankController.h"

using tank::Command;
using tank::CommandSource;

namespace {

constexpr const char* kTag = "BOOT";

Motor leftMotor(board::kLeftMotor, LEDC_CHANNEL_0, config::kMotorPwm, config::kInvertLeftMotor);
Motor rightMotor(board::kRightMotor, LEDC_CHANNEL_1, config::kMotorPwm, config::kInvertRightMotor);

DriveSystem drive(leftMotor, rightMotor, config::kDrive);
Cannon cannon(board::kFirePin, config::kCannon);

tank::TankStateMachine tankLogic(drive, cannon, config::kDemo);
TankController controller(tankLogic, config::kController);

ModeLogger modeLogger;
StatusLed statusLed(board::kStatusLedPin);

WifiAccessPoint wifi(config::kWifi);
WebRemote webRemote(controller, config::kWebRemote, config::kGamepadMapping);
PushButton bootButton(board::kButtonPin, controller,
                      Command::toggleEmergencyStop(CommandSource::Button),  // short press
                      Command::toggleDemo(CommandSource::Button),           // long press
                      config::kButton);

[[noreturn]] void halt(const char* component) {
  ESP_LOGE(kTag, "Failed to start: %s. Halting.", component);
  for (;;) {
    vTaskDelay(portMAX_DELAY);
  }
}

}  // namespace

extern "C" void app_main() {
  cannon.begin();
  ESP_LOGI(kTag, "ESP32-S3-TANK: tank on ESP-IDF + FreeRTOS");

  leftMotor.begin();
  rightMotor.begin();

  tankLogic.addObserver(modeLogger);
  tankLogic.addObserver(webRemote);
  if (statusLed.begin()) {
    tankLogic.addObserver(statusLed);
  }

  if (!drive.start(config::kDriveTask)) {
    halt("drive");
  }
  if (!cannon.start(config::kCannonTask)) {
    halt("cannon");
  }
  if (!controller.start(config::kControllerTask, config::kInitialMode)) {
    halt("controller");
  }
  if (!bootButton.begin()) {
    halt("button");
  }

  wifi.begin();
  if (!webRemote.begin()) {
    halt("web remote");
  }
}
