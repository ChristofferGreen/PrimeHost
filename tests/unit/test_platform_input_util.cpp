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

TEST_SUITE_END();
