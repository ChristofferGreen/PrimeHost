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

TEST_SUITE_END();
