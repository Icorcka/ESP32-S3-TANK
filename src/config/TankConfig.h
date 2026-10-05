#pragma once

#include <TankStateMachine.h>

#include "drivers/Motor.h"
#include "input/PushButton.h"
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

constexpr BaseType_t kAppCore = 1;
constexpr TaskConfig kDriveTask{"drive", 4096, 5, kAppCore};
constexpr TaskConfig kControllerTask{"control", 4096, 4, kAppCore};
constexpr TaskConfig kCannonTask{"cannon", 3072, 3, kAppCore};

}  // namespace config
