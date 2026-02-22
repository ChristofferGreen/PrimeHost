#include "src/PlatformDisplayUtil.h"

#include "tests/unit/test_helpers.h"

#include <limits>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.display.util");

PH_TEST("primehost.display.util", "interval from refresh rate") {
  auto interval60 = intervalFromRefreshRate(60.0);
  PH_REQUIRE(interval60.has_value());
  PH_CHECK(interval60->count() == 16666666);

  auto interval120 = intervalFromRefreshRate(120.0);
  PH_REQUIRE(interval120.has_value());
  PH_CHECK(interval120->count() == 8333333);

  auto intervalZero = intervalFromRefreshRate(0.0);
  PH_CHECK(!intervalZero.has_value());

  auto intervalNegative = intervalFromRefreshRate(-60.0);
  PH_CHECK(!intervalNegative.has_value());
}

PH_TEST("primehost.display.util", "resolved refresh rate") {
  PH_CHECK(resolvedRefreshRate(60.0, 0.0) == 60.0f);
  PH_CHECK(resolvedRefreshRate(0.0, 120.0) == 120.0f);
  PH_CHECK(resolvedRefreshRate(-1.0, 75.0) == 75.0f);
  PH_CHECK(resolvedRefreshRate(std::numeric_limits<double>::infinity(), 90.0) == 90.0f);
  PH_CHECK(resolvedRefreshRate(0.0, -1.0) == 0.0f);
}

TEST_SUITE_END();
