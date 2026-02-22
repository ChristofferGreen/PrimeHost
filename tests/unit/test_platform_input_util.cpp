#include "src/PlatformInputUtil.h"

#include "tests/unit/test_helpers.h"

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

TEST_SUITE_END();
