// Unit tests for the tank logic. They run on a computer, no board needed:
//   pio test -e native

#include <DriveMath.h>
#include <unity.h>

#include <cmath>

using namespace tank;

namespace {

constexpr float kEps = 1e-4f;

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

int runAllTests() {
  UNITY_BEGIN();
  RUN_TEST(test_deadzone_suppresses_small_values);
  RUN_TEST(test_deadzone_rescales_remaining_range);
  RUN_TEST(test_mix_arcade_straight_and_spin_in_place);
  RUN_TEST(test_mix_arcade_keeps_turn_ratio_when_saturated);
  RUN_TEST(test_slew_limiter_ramps_up_and_brakes_faster);
  RUN_TEST(test_clamp_unit_turns_nan_into_zero);
  return UNITY_END();
}

int main() {
  return runAllTests();
}
