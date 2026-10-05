#include "TankStateMachine.h"

namespace tank {

// ─── MANUAL ─────────────────────────────────────────────────────────────────

void ManualState::onEnter(TankContext& tank, uint32_t /*nowMs*/) {
  tank.drive().setTarget({});
}

void ManualState::onCommand(TankContext& tank, const Command& command, uint32_t nowMs) {
  switch (command.type) {
    case CommandType::Drive:
      tank.drive().setTarget(command.tracks);
      break;
    case CommandType::Fire:
      tank.cannon().fire();
      break;
    case CommandType::ToggleDemo:
      tank.transitionTo(TankMode::Demo, nowMs);
      break;
    case CommandType::ToggleEmergencyStop:
      tank.transitionTo(TankMode::EmergencyStop, nowMs);
      break;
  }
}

// ─── DEMO ───────────────────────────────────────────────────────────────────

void DemoState::onEnter(TankContext& tank, uint32_t nowMs) {
  lastShotMs_ = nowMs;
  driveForward(tank);
}

void DemoState::onExit(TankContext& tank) {
  tank.drive().setTarget({});
}

void DemoState::onCommand(TankContext& tank, const Command& command, uint32_t nowMs) {
  switch (command.type) {
    case CommandType::Drive:
      break;  // the autopilot drives, sticks are ignored
    case CommandType::Fire:
      tank.cannon().fire();
      break;
    case CommandType::ToggleDemo:
      tank.transitionTo(TankMode::Manual, nowMs);
      break;
    case CommandType::ToggleEmergencyStop:
      tank.transitionTo(TankMode::EmergencyStop, nowMs);
      break;
  }
}

void DemoState::onTick(TankContext& tank, uint32_t nowMs) {
  driveForward(tank);
  if (nowMs - lastShotMs_ >= settings_.fireIntervalMs) {
    lastShotMs_ = nowMs;
    tank.cannon().fire();
  }
}

void DemoState::driveForward(TankContext& tank) const {
  tank.drive().setTarget({settings_.speed, settings_.speed});
}

// ─── E-STOP ─────────────────────────────────────────────────────────────────

void EmergencyStopState::onEnter(TankContext& tank, uint32_t /*nowMs*/) {
  tank.drive().emergencyStop();
  tank.cannon().setLocked(true);
}

void EmergencyStopState::onExit(TankContext& tank) {
  tank.cannon().setLocked(false);
}

void EmergencyStopState::onCommand(TankContext& tank, const Command& command, uint32_t nowMs) {
  // Only releasing the E-STOP is accepted, and it always returns to MANUAL.
  if (command.type == CommandType::ToggleEmergencyStop) {
    tank.transitionTo(TankMode::Manual, nowMs);
  }
}

// ─── State machine ──────────────────────────────────────────────────────────

TankStateMachine::TankStateMachine(IDrive& drive, ICannon& cannon, const DemoSettings& demo)
    : drive_(drive), cannon_(cannon), demo_(demo) {}

bool TankStateMachine::addObserver(ITankObserver& observer) {
  if (observerCount_ >= kMaxObservers) {
    return false;
  }
  observers_[observerCount_++] = &observer;
  return true;
}

void TankStateMachine::start(TankMode initialMode, uint32_t nowMs) {
  state_ = &stateFor(initialMode);
  state_->onEnter(*this, nowMs);
  notifyObservers();
}

void TankStateMachine::handle(const Command& command, uint32_t nowMs) {
  state_->onCommand(*this, command, nowMs);
}

void TankStateMachine::tick(uint32_t nowMs) {
  state_->onTick(*this, nowMs);
}

void TankStateMachine::transitionTo(TankMode mode, uint32_t nowMs) {
  TankState& next = stateFor(mode);
  if (&next == state_) {
    return;
  }
  state_->onExit(*this);
  state_ = &next;
  state_->onEnter(*this, nowMs);
  notifyObservers();
}

TankState& TankStateMachine::stateFor(TankMode mode) {
  switch (mode) {
    case TankMode::Demo:
      return demo_;
    case TankMode::EmergencyStop:
      return emergencyStop_;
    case TankMode::Manual:
      break;
  }
  return manual_;
}

void TankStateMachine::notifyObservers() {
  for (size_t i = 0; i < observerCount_; ++i) {
    observers_[i]->onModeChanged(state_->mode());
  }
}

}  // namespace tank
