#include "src/PlatformInputUtil.h"

#include "tests/unit/test_helpers.h"

#include <limits>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.input.util");

PH_TEST("primehost.input.util", "tilt normalized to degrees") {
  PH_CHECK(tiltNormalizedToDegrees(0.0f) == 0.0f);
  PH_CHECK(tiltNormalizedToDegrees(1.0f) == 90.0f);
  PH_CHECK(tiltNormalizedToDegrees(-1.0f) == -90.0f);
  PH_CHECK(tiltNormalizedToDegrees(2.0f) == 90.0f);
  PH_CHECK(tiltNormalizedToDegrees(-2.0f) == -90.0f);
}

PH_TEST("primehost.input.util", "normalize twist degrees") {
  PH_CHECK(normalizeTwistDegrees(0.0f) == 0.0f);
  PH_CHECK(normalizeTwistDegrees(90.0f) == 90.0f);
  PH_CHECK(normalizeTwistDegrees(360.0f) == 0.0f);
  PH_CHECK(normalizeTwistDegrees(450.0f) == 90.0f);
  PH_CHECK(normalizeTwistDegrees(-90.0f) == 270.0f);
  PH_CHECK(normalizeTwistDegrees(-450.0f) == 270.0f);
}

PH_TEST("primehost.input.util", "analog button value") {
  PH_CHECK(!analogButtonValue(false, 0.5f).has_value());
  PH_CHECK(analogButtonValue(true, -0.5f).value() == 0.0f);
  PH_CHECK(analogButtonValue(true, 0.5f).value() == 0.5f);
  PH_CHECK(analogButtonValue(true, 1.5f).value() == 1.0f);
}

PH_TEST("primehost.input.util", "analog button value ignores non finite") {
  auto value = analogButtonValue(true, std::numeric_limits<float>::quiet_NaN());
  PH_CHECK(!value.has_value());
}

PH_TEST("primehost.input.util", "clamp gamepad axis value") {
  PH_CHECK(clampGamepadAxisValue(static_cast<uint32_t>(GamepadAxisId::LeftX), -2.0f) == -1.0f);
  PH_CHECK(clampGamepadAxisValue(static_cast<uint32_t>(GamepadAxisId::LeftX), 2.0f) == 1.0f);
  PH_CHECK(clampGamepadAxisValue(static_cast<uint32_t>(GamepadAxisId::RightY), 0.25f) == 0.25f);
  PH_CHECK(clampGamepadAxisValue(static_cast<uint32_t>(GamepadAxisId::LeftTrigger), -1.0f) == 0.0f);
  PH_CHECK(clampGamepadAxisValue(static_cast<uint32_t>(GamepadAxisId::RightTrigger), 2.0f) == 1.0f);
}

PH_TEST("primehost.input.util", "clamp gamepad axis value ignores non finite") {
  auto value = clampGamepadAxisValue(static_cast<uint32_t>(GamepadAxisId::LeftX),
                                     std::numeric_limits<float>::infinity());
  PH_CHECK(value == 0.0f);
}

PH_TEST("primehost.input.util", "normalized pressure") {
  PH_CHECK(normalizedPressure(-1.0f).value() == 0.0f);
  PH_CHECK(normalizedPressure(0.5f).value() == 0.5f);
  PH_CHECK(normalizedPressure(2.0f).value() == 1.0f);
}

PH_TEST("primehost.input.util", "normalized pressure ignores non finite") {
  auto value = normalizedPressure(std::numeric_limits<float>::quiet_NaN());
  PH_CHECK(!value.has_value());
}

PH_TEST("primehost.input.util", "normalized scroll delta") {
  PH_CHECK(normalizedScrollDelta(1.5) == 1.5f);
  PH_CHECK(normalizedScrollDelta(-2.0) == -2.0f);
}

PH_TEST("primehost.input.util", "normalized scroll delta ignores non finite") {
  PH_CHECK(normalizedScrollDelta(std::numeric_limits<double>::infinity()) == 0.0f);
  PH_CHECK(normalizedScrollDelta(std::numeric_limits<double>::quiet_NaN()) == 0.0f);
}

TEST_SUITE_END();
