#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.safe_area");

PH_TEST("primehost.safe_area", "invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto result = host->surfaceSafeAreaInsets(SurfaceId{4242u});
  PH_CHECK(!result.has_value());
  if (!result.has_value()) {
    PH_CHECK(result.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
