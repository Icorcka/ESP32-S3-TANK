#include "GamepadMapper.h"

#include "DriveMath.h"

namespace tank {

namespace {

bool justPressed(bool now, bool before) {
  return now && !before;
}

}  // namespace

GamepadMapper::GamepadMapper(ICommandSink& sink, CommandSource source, const Settings& settings)
    : sink_(sink),
      source_(source),
      settings_(settings),
      gear_(settings.initialGear < kGearCount ? settings.initialGear : kGearCount - 1) {}

void GamepadMapper::reset(const GamepadState& current) {
  previous_ = current;
}

void GamepadMapper::update(const GamepadState& state) {
  handleButtons(state);
  previous_ = state;
  sink_.post(Command::drive(source_, tracksFrom(state)));
}

void GamepadMapper::stop() {
  sink_.post(Command::drive(source_, {}));
}

void GamepadMapper::handleButtons(const GamepadState& now) {
  if (justPressed(now.cross, previous_.cross) ||
      justPressed(triggerPulled(now), triggerPulled(previous_))) {
    sink_.post(Command::fire(source_));
  }
  if (justPressed(now.circle, previous_.circle)) {
    sink_.post(Command::toggleEmergencyStop(source_));
  }
  if (justPressed(now.options, previous_.options)) {
    sink_.post(Command::toggleDemo(source_));
  }
  if (justPressed(now.r1, previous_.r1)) {
    shiftGear(+1);
  }
  if (justPressed(now.l1, previous_.l1)) {
    shiftGear(-1);
  }
}

void GamepadMapper::shiftGear(int delta) {
  const int next = gear_ + delta;
  if (next >= 0 && next < static_cast<int>(kGearCount)) {
    gear_ = static_cast<uint8_t>(next);
  }
}

bool GamepadMapper::triggerPulled(const GamepadState& state) const {
  return state.r2 >= settings_.triggerThreshold;
}

TrackSpeeds GamepadMapper::tracksFrom(const GamepadState& state) const {
  const float throttle = applyDeadzone(state.leftY, settings_.stickDeadzone);
  const float turn = applyDeadzone(state.rightX, settings_.stickDeadzone);
  return scaled(mixArcade(throttle, turn), speedLimit());
}

}  // namespace tank
