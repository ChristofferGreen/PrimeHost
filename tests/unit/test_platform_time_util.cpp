#include "src/PlatformTimeUtil.h"

#include "tests/unit/test_helpers.h"

#include <limits>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.time.util");

PH_TEST("primehost.time.util", "steady time from uptime") {
  auto origin = std::chrono::steady_clock::time_point{};
  auto now = origin + std::chrono::seconds(100);
  auto expected = origin + std::chrono::seconds(90);

  PH_CHECK(steadyTimeFromUptime(90.0, 100.0, now) == expected);
  PH_CHECK(steadyTimeFromUptime(110.0, 100.0, now) == now);
  PH_CHECK(steadyTimeFromUptime(-1.0, 100.0, now) == now);
  PH_CHECK(steadyTimeFromUptime(90.0, -1.0, now) == now);
}

PH_TEST("primehost.time.util", "steady time from uptime handles non finite") {
  auto now = std::chrono::steady_clock::now();
  PH_CHECK(steadyTimeFromUptime(std::numeric_limits<double>::infinity(), 1.0, now) == now);
  PH_CHECK(steadyTimeFromUptime(1.0, std::numeric_limits<double>::infinity(), now) == now);
  PH_CHECK(steadyTimeFromUptime(std::numeric_limits<double>::quiet_NaN(), 1.0, now) == now);
}

TEST_SUITE_END();
