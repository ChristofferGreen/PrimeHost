#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.timing");

PH_TEST("primehost.timing", "sleep for and until") {
  auto start = now();
  sleepFor(std::chrono::milliseconds(1));
  auto afterSleep = now();
  PH_CHECK(afterSleep >= start);
  PH_CHECK(afterSleep - start < std::chrono::seconds(1));

  auto target = now() + std::chrono::milliseconds(1);
  sleepUntil(target);
  auto afterUntil = now();
  PH_CHECK(afterUntil >= start);
  PH_CHECK(afterUntil - start < std::chrono::seconds(1));
}

TEST_SUITE_END();
