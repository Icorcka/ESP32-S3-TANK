// Unit tests for the tank logic. They run on a computer, no board needed:
//   pio test -e native

#include <DriveMath.h>
#include <GamepadMapper.h>
#include <RemoteProtocol.h>
#include <TankStateMachine.h>
#include <unity.h>

#include <cmath>
#include <cstring>

using namespace tank;

namespace {

constexpr float kEps = 1e-4f;

struct FakeDrive : IDrive {
  TrackSpeeds target;
  int emergencyStops = 0;

  void setTarget(const TrackSpeeds& value) override { target = value; }
  void emergencyStop() override {
    ++emergencyStops;
    target = {};
  }
};

struct FakeCannon : ICannon {
  int fireRequests = 0;
  bool locked = false;

  void fire() override { ++fireRequests; }
  void setLocked(bool value) override { locked = value; }
};

struct ModeRecorder : ITankObserver {
  TankMode last = TankMode::Manual;
  int notifications = 0;

  void onModeChanged(TankMode mode) override {
    last = mode;
    ++notifications;
  }
};

constexpr DemoSettings kDemo{0.8f, 5000};

/// A tank in MANUAL mode with fake hardware.
struct TestTank {
  FakeDrive drive;
  FakeCannon cannon;
  ModeRecorder observer;
  TankStateMachine machine{drive, cannon, kDemo};

  TestTank() {
    machine.addObserver(observer);
    machine.start(TankMode::Manual, 0);
  }

  void send(const Command& command, uint32_t nowMs = 0) { machine.handle(command, nowMs); }
};

const Command kFire = Command::fire(CommandSource::Web);
const Command kToggleDemo = Command::toggleDemo(CommandSource::Web);
const Command kToggleEStop = Command::toggleEmergencyStop(CommandSource::Button);

Command driveCommand(float left, float right) {
  return Command::drive(CommandSource::Web, {left, right});
}

struct RecordingSink : ICommandSink {
  static constexpr size_t kCapacity = 16;
  Command commands[kCapacity] = {};
  size_t count = 0;

  bool post(const Command& command) override {
    if (count < kCapacity) {
      commands[count++] = command;
    }
    return true;
  }

  size_t countOf(CommandType type) const {
    size_t matches = 0;
    for (size_t i = 0; i < count; ++i) {
      matches += commands[i].type == type ? 1 : 0;
    }
    return matches;
  }

  const Command& last() const { return commands[count - 1]; }
};

// Deadzone 0.1, R2 fires from 0.5, gears 40/70/100 %, starts in the second.
constexpr GamepadMapper::Settings kMapping{0.1f, 0.5f, {0.4f, 0.7f, 1.0f}, 1};

}  // namespace

void setUp() {}
void tearDown() {}

// ─── DriveMath ──────────────────────────────────────────────────────────────

void test_deadzone_suppresses_small_values() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, applyDeadzone(0.05f, 0.1f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, applyDeadzone(-0.1f, 0.1f));
}

void test_deadzone_rescales_remaining_range() {
  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, applyDeadzone(0.55f, 0.1f));
  TEST_ASSERT_FLOAT_WITHIN(kEps, -1.0f, applyDeadzone(-1.2f, 0.1f));
}

void test_mix_arcade_straight_and_spin_in_place() {
  const TrackSpeeds forward = mixArcade(0.6f, 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.6f, forward.left);
  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.6f, forward.right);

  const TrackSpeeds spinLeft = mixArcade(0.0f, -1.0f);
  TEST_ASSERT_FLOAT_WITHIN(kEps, -1.0f, spinLeft.left);
  TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, spinLeft.right);
}

void test_mix_arcade_keeps_turn_ratio_when_saturated() {
  const TrackSpeeds tracks = mixArcade(1.0f, 0.5f);  // 1.5 / 0.5 → 1.0 / 0.333
  TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, tracks.left);
  TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f / 3.0f, tracks.right);
}

void test_slew_limiter_ramps_up_and_brakes_faster() {
  SlewRateLimiter ramp(2.0f, 4.0f);  // accelerate 2/s, brake 4/s

  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.2f, ramp.update(1.0f, 0.1f));
  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.4f, ramp.update(1.0f, 0.1f));
  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, ramp.update(0.0f, 0.1f));  // 0.4 − 4·0.1

  ramp.reset(0.5f);
  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.1f, ramp.update(-1.0f, 0.1f));  // reversing: brake first
}

void test_clamp_unit_turns_nan_into_zero() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, clampUnit(std::nanf("")));
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, clampUnit(-5.0f));
}

// ─── GamepadMapper ──────────────────────────────────────────────────────────

void test_mapper_drives_with_gear_speed_limit() {
  RecordingSink sink;
  GamepadMapper mapper(sink, CommandSource::Web, kMapping);
  GamepadState state;
  state.leftY = 1.0f;

  mapper.update(state);

  TEST_ASSERT_EQUAL(1, sink.count);
  TEST_ASSERT_EQUAL(static_cast<int>(CommandType::Drive), static_cast<int>(sink.last().type));
  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.7f, sink.last().tracks.left);
  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.7f, sink.last().tracks.right);
}

void test_mapper_ignores_stick_noise_in_deadzone() {
  RecordingSink sink;
  GamepadMapper mapper(sink, CommandSource::Web, kMapping);
  GamepadState state;
  state.leftY = 0.05f;
  state.rightX = -0.08f;

  mapper.update(state);

  TEST_ASSERT_EQUAL_FLOAT(0.0f, sink.last().tracks.left);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sink.last().tracks.right);
}

void test_mapper_fires_once_per_press_of_cross_or_r2() {
  RecordingSink sink;
  GamepadMapper mapper(sink, CommandSource::Web, kMapping);
  GamepadState cross;
  cross.cross = true;
  GamepadState trigger;
  trigger.r2 = 0.8f;

  mapper.update(cross);
  mapper.update(cross);  // holding the button is not a new shot
  mapper.update(trigger);

  TEST_ASSERT_EQUAL(2, sink.countOf(CommandType::Fire));
}

void test_mapper_maps_mode_buttons() {
  RecordingSink sink;
  GamepadMapper mapper(sink, CommandSource::Web, kMapping);
  GamepadState state;
  state.circle = true;
  state.options = true;

  mapper.update(state);

  TEST_ASSERT_EQUAL(1, sink.countOf(CommandType::ToggleEmergencyStop));
  TEST_ASSERT_EQUAL(1, sink.countOf(CommandType::ToggleDemo));
}

void test_mapper_shifts_gears_within_limits() {
  RecordingSink sink;
  GamepadMapper mapper(sink, CommandSource::Web, kMapping);
  GamepadState released;
  GamepadState r1;
  r1.r1 = true;
  GamepadState l1;
  l1.l1 = true;

  for (int i = 0; i < 3; ++i) {  // three shifts up — never past the top gear
    mapper.update(r1);
    mapper.update(released);
  }
  TEST_ASSERT_EQUAL(2, mapper.gear());
  TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, mapper.speedLimit());

  for (int i = 0; i < 3; ++i) {
    mapper.update(l1);
    mapper.update(released);
  }
  TEST_ASSERT_EQUAL(0, mapper.gear());
}

void test_mapper_reset_ignores_buttons_held_while_connecting() {
  RecordingSink sink;
  GamepadMapper mapper(sink, CommandSource::Web, kMapping);
  GamepadState held;
  held.circle = true;
  held.cross = true;

  mapper.reset(held);
  mapper.update(held);

  TEST_ASSERT_EQUAL(0, sink.countOf(CommandType::ToggleEmergencyStop));
  TEST_ASSERT_EQUAL(0, sink.countOf(CommandType::Fire));
}

void test_mapper_stop_sends_zero_drive() {
  RecordingSink sink;
  GamepadMapper mapper(sink, CommandSource::Web, kMapping);

  mapper.stop();

  TEST_ASSERT_EQUAL(static_cast<int>(CommandType::Drive), static_cast<int>(sink.last().type));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sink.last().tracks.left);
}

// ─── RemoteProtocol ─────────────────────────────────────────────────────────

void test_protocol_parses_gamepad_frame() {
  GamepadState state;

  TEST_ASSERT_TRUE(remote::parseGamepadFrame("S,0.500,-0.250,1.000,5", state));

  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, state.leftY);
  TEST_ASSERT_FLOAT_WITHIN(kEps, -0.25f, state.rightX);
  TEST_ASSERT_FLOAT_WITHIN(kEps, 1.0f, state.r2);
  TEST_ASSERT_TRUE(state.cross);     // bit 0
  TEST_ASSERT_FALSE(state.circle);   // bit 1
  TEST_ASSERT_TRUE(state.options);   // bit 2
}

void test_protocol_clamps_out_of_range_values() {
  GamepadState state;

  TEST_ASSERT_TRUE(remote::parseGamepadFrame("S,2,-3,1.5,0", state));

  TEST_ASSERT_EQUAL_FLOAT(1.0f, state.leftY);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, state.rightX);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, state.r2);
}

void test_protocol_rejects_malformed_frames() {
  const char* const malformed[] = {
      "", "E", "S", "S,1,2", "X,0,0,0,0", "S,a,0,0,0",
      "S,0,0,0,", "S,0,0,0,-1", "S,nan,0,0,0", "S,inf,0,0,0", "S,0,0,0,1x",
  };
  for (const char* frame : malformed) {
    GamepadState state;
    state.leftY = 0.3f;
    TEST_ASSERT_FALSE_MESSAGE(remote::parseGamepadFrame(frame, state), frame);
    TEST_ASSERT_EQUAL_FLOAT(0.3f, state.leftY);  // a malformed frame changes nothing
  }
}

void test_protocol_formats_status_frame() {
  char buffer[16];

  TEST_ASSERT_EQUAL(10, remote::formatStatusFrame(buffer, sizeof(buffer), TankMode::EmergencyStop, 2));
  TEST_ASSERT_EQUAL_STRING("T,E-STOP,2", buffer);
  TEST_ASSERT_EQUAL(0, remote::formatStatusFrame(buffer, 4, TankMode::Manual, 1));
}

// ─── TankStateMachine ───────────────────────────────────────────────────────

void test_manual_mode_follows_drive_and_fire_commands() {
  TestTank tank;

  tank.send(driveCommand(0.5f, -0.5f));
  tank.send(kFire);

  TEST_ASSERT_FLOAT_WITHIN(kEps, 0.5f, tank.drive.target.left);
  TEST_ASSERT_FLOAT_WITHIN(kEps, -0.5f, tank.drive.target.right);
  TEST_ASSERT_EQUAL(1, tank.cannon.fireRequests);
}

void test_emergency_stop_halts_drive_and_locks_cannon() {
  TestTank tank;
  tank.send(driveCommand(1.0f, 1.0f));

  tank.send(kToggleEStop);

  TEST_ASSERT_EQUAL(static_cast<int>(TankMode::EmergencyStop), static_cast<int>(tank.machine.mode()));
  TEST_ASSERT_EQUAL(1, tank.drive.emergencyStops);
  TEST_ASSERT_TRUE(tank.cannon.locked);
}

void test_emergency_stop_ignores_drive_and_fire() {
  TestTank tank;
  tank.send(kToggleEStop);

  tank.send(driveCommand(1.0f, 1.0f));
  tank.send(kFire);
  tank.send(kToggleDemo);

  TEST_ASSERT_EQUAL_FLOAT(0.0f, tank.drive.target.left);
  TEST_ASSERT_EQUAL(0, tank.cannon.fireRequests);
  TEST_ASSERT_EQUAL(static_cast<int>(TankMode::EmergencyStop), static_cast<int>(tank.machine.mode()));
}

void test_releasing_emergency_stop_returns_to_manual_and_unlocks_cannon() {
  TestTank tank;
  tank.send(kToggleDemo);
  tank.send(kToggleEStop);

  tank.send(kToggleEStop);

  TEST_ASSERT_EQUAL(static_cast<int>(TankMode::Manual), static_cast<int>(tank.machine.mode()));
  TEST_ASSERT_FALSE(tank.cannon.locked);
}

void test_demo_drives_forward_and_fires_on_interval() {
  TestTank tank;
  tank.send(kToggleDemo, 1000);

  TEST_ASSERT_FLOAT_WITHIN(kEps, kDemo.speed, tank.drive.target.left);
  TEST_ASSERT_FLOAT_WITHIN(kEps, kDemo.speed, tank.drive.target.right);

  tank.machine.tick(1000 + kDemo.fireIntervalMs - 1);
  TEST_ASSERT_EQUAL(0, tank.cannon.fireRequests);

  tank.machine.tick(1000 + kDemo.fireIntervalMs);
  TEST_ASSERT_EQUAL(1, tank.cannon.fireRequests);

  tank.machine.tick(1000 + 2 * kDemo.fireIntervalMs);
  TEST_ASSERT_EQUAL(2, tank.cannon.fireRequests);
}

void test_demo_ignores_manual_driving_and_stops_on_exit() {
  TestTank tank;
  tank.send(kToggleDemo);

  tank.send(driveCommand(-1.0f, -1.0f));
  TEST_ASSERT_FLOAT_WITHIN(kEps, kDemo.speed, tank.drive.target.left);

  tank.send(kToggleDemo);
  TEST_ASSERT_EQUAL(static_cast<int>(TankMode::Manual), static_cast<int>(tank.machine.mode()));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, tank.drive.target.left);
}

void test_observers_are_notified_on_every_mode_change() {
  TestTank tank;  // start() notifies too
  TEST_ASSERT_EQUAL(1, tank.observer.notifications);

  tank.send(kToggleDemo);
  tank.send(kToggleEStop);

  TEST_ASSERT_EQUAL(3, tank.observer.notifications);
  TEST_ASSERT_EQUAL(static_cast<int>(TankMode::EmergencyStop), static_cast<int>(tank.observer.last));
}

int runAllTests() {
  UNITY_BEGIN();
  RUN_TEST(test_deadzone_suppresses_small_values);
  RUN_TEST(test_deadzone_rescales_remaining_range);
  RUN_TEST(test_mix_arcade_straight_and_spin_in_place);
  RUN_TEST(test_mix_arcade_keeps_turn_ratio_when_saturated);
  RUN_TEST(test_slew_limiter_ramps_up_and_brakes_faster);
  RUN_TEST(test_clamp_unit_turns_nan_into_zero);
  RUN_TEST(test_mapper_drives_with_gear_speed_limit);
  RUN_TEST(test_mapper_ignores_stick_noise_in_deadzone);
  RUN_TEST(test_mapper_fires_once_per_press_of_cross_or_r2);
  RUN_TEST(test_mapper_maps_mode_buttons);
  RUN_TEST(test_mapper_shifts_gears_within_limits);
  RUN_TEST(test_mapper_reset_ignores_buttons_held_while_connecting);
  RUN_TEST(test_mapper_stop_sends_zero_drive);
  RUN_TEST(test_protocol_parses_gamepad_frame);
  RUN_TEST(test_protocol_clamps_out_of_range_values);
  RUN_TEST(test_protocol_rejects_malformed_frames);
  RUN_TEST(test_protocol_formats_status_frame);
  RUN_TEST(test_manual_mode_follows_drive_and_fire_commands);
  RUN_TEST(test_emergency_stop_halts_drive_and_locks_cannon);
  RUN_TEST(test_emergency_stop_ignores_drive_and_fire);
  RUN_TEST(test_releasing_emergency_stop_returns_to_manual_and_unlocks_cannon);
  RUN_TEST(test_demo_drives_forward_and_fires_on_interval);
  RUN_TEST(test_demo_ignores_manual_driving_and_stops_on_exit);
  RUN_TEST(test_observers_are_notified_on_every_mode_change);
  return UNITY_END();
}

int main() {
  return runAllTests();
}
