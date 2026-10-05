#pragma once

#include <GamepadMapper.h>
#include <TankStateMachine.h>
#include <stddef.h>

#include "drivers/Motor.h"
#include "input/PushButton.h"
#include "input/WebRemote.h"
#include "net/WifiAccessPoint.h"
#include "services/Cannon.h"
#include "services/DriveSystem.h"
#include "services/TankController.h"
#include "util/RtosTask.h"

namespace config {

constexpr PwmSettings kMotorPwm{
    .frequencyHz = 20000,
    .resolutionBits = 8,
    .minDuty = 70,
};
static_assert(kMotorPwm.minDuty < (1u << kMotorPwm.resolutionBits), "minDuty is out of PWM range");

constexpr bool kInvertLeftMotor = false;
constexpr bool kInvertRightMotor = false;

constexpr DriveSystem::Settings kDrive{
    .loopPeriodMs = 10,
    .commandTimeoutMs = 500,
    .accelPerSecond = 2.5f,
    .decelPerSecond = 5.0f,
};

constexpr Cannon::Settings kCannon{
    .pulseMs = 150,
    .cooldownMs = 1000,
};

constexpr tank::DemoSettings kDemo{
    .speed = 0.85f,
    .fireIntervalMs = 5000,
};
constexpr tank::TankMode kInitialMode = tank::TankMode::Manual;

constexpr TankController::Settings kController{
    .queueLength = 16,
    .tickPeriodMs = 50,
};

constexpr PushButton::Settings kButton{
    .debounceMs = 30,
    .longPressMs = 800,
};

constexpr tank::GamepadMapper::Settings kGamepadMapping{
    .stickDeadzone = 0.08f,
    .triggerThreshold = 0.5f,
    .gears = {0.4f, 0.7f, 1.0f},
    .initialGear = 1,
};

constexpr WifiAccessPoint::Settings kWifi{
    .ssid = "Tank-S3",
    .password = "tank12345",
    .channel = 6,
    .maxClients = 2,
};

constexpr WebRemote::Settings kWebRemote{
    .port = 80,
    .pilotTimeoutMs = 1000,
};

constexpr size_t textLength(const char* text) {
  size_t length = 0;
  while (text[length] != '\0') {
    ++length;
  }
  return length;
}
static_assert(textLength(kWifi.password) >= 8, "Wi-Fi password (WPA2) must be at least 8 characters");

constexpr BaseType_t kAppCore = 1;
constexpr TaskConfig kDriveTask{"drive", 4096, 5, kAppCore};
constexpr TaskConfig kControllerTask{"control", 4096, 4, kAppCore};
constexpr TaskConfig kCannonTask{"cannon", 3072, 3, kAppCore};

}  // namespace config
