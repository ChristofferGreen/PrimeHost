#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.smoke");

PH_TEST("primehost.smoke", "version constant") {
  PH_CHECK(PrimeHostVersion == 1u);
}

TEST_SUITE_END();
