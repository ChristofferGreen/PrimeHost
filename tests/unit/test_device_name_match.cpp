#include "DeviceNameMatch.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.device");

PH_TEST("primehost.device", "name matching") {
  PH_CHECK(deviceNameMatchScore("Xbox Wireless Controller", "Xbox Wireless Controller") == 100);
  PH_CHECK(deviceNameMatchScore("Xbox Wireless Controller",
                                "Xbox Wireless Controller (Vendor 045e Product 02e0)") >= 80);
  PH_CHECK(deviceNameMatches("Sony DualSense", "DualSense Wireless Controller"));
  PH_CHECK(!deviceNameMatches("Nintendo Switch Pro Controller", "Joy-Con (L)"));
}

TEST_SUITE_END();
