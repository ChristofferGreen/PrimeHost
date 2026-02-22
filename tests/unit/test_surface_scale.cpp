#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surface");

PH_TEST("primehost.surface", "surface scale invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto scale = host->surfaceScale(SurfaceId{4242u});
  PH_CHECK(!scale.has_value());
  if (!scale.has_value()) {
    PH_CHECK(scale.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
