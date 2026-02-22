#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.display");

PH_TEST("primehost.display", "set surface display invalid ids") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto invalidSurface = host->setSurfaceDisplay(SurfaceId{999u}, 1u);
  PH_CHECK(!invalidSurface.has_value());
  if (!invalidSurface.has_value()) {
    PH_CHECK(invalidSurface.error().code == HostErrorCode::InvalidSurface);
  }

  auto invalidDisplay = host->setSurfaceDisplay(SurfaceId{0u}, 0u);
  PH_CHECK(!invalidDisplay.has_value());
  if (!invalidDisplay.has_value()) {
    PH_CHECK(invalidDisplay.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
