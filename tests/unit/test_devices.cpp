#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.devices");

PH_TEST("primehost.devices", "count query") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());
  auto countResult = host->devices({});
  PH_CHECK(countResult.has_value());
  if (!countResult) {
    return;
  }
  PH_CHECK(countResult.value() >= 2u);

  std::vector<DeviceInfo> devices(countResult.value());
  auto filled = host->devices(devices);
  PH_CHECK(filled.has_value());
  if (filled) {
    PH_CHECK(filled.value() == devices.size());
  }
}

TEST_SUITE_END();
