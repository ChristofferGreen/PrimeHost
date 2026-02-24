#include "PrimeHost/Timing.h"

#include "tests/unit/test_helpers.h"

#include <chrono>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.timing");

PH_TEST("primehost.timing", "now monotonic") {
  auto start = now();
  auto end = now();
  PH_CHECK(end >= start);
}

PH_TEST("primehost.timing", "sleepFor ignores non-positive") {
  auto start = now();
  sleepFor(std::chrono::nanoseconds(0));
  sleepFor(std::chrono::nanoseconds(-1));
  auto end = now();
  PH_CHECK(end >= start);
}

PH_TEST("primehost.timing", "sleepUntil ignores past") {
  auto start = now();
  sleepUntil(start);
  sleepUntil(start - std::chrono::nanoseconds(1));
  auto end = now();
  PH_CHECK(end >= start);
}

PH_TEST("primehost.timing", "sleepFor sleeps at least minimal duration") {
  auto start = now();
  sleepFor(std::chrono::milliseconds(2));
  auto end = now();
  PH_CHECK(end - start >= std::chrono::milliseconds(1));
}

PH_TEST("primehost.timing", "sleepUntil waits for near future") {
  auto start = now();
  auto target = start + std::chrono::milliseconds(2);
  sleepUntil(target);
  auto end = now();
  PH_CHECK(end >= target);
}

TEST_SUITE_END();
