#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surface");

PH_TEST("primehost.surface", "surface capabilities invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto caps = host->surfaceCapabilities(SurfaceId{12345u});
  PH_CHECK(!caps.has_value());
  if (!caps.has_value()) {
    PH_CHECK(caps.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
