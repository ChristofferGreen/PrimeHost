#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surface");

PH_TEST("primehost.surface", "surface position invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto pos = host->surfacePosition(SurfaceId{1234u});
  PH_CHECK(!pos.has_value());
  if (!pos.has_value()) {
    PH_CHECK(pos.error().code == HostErrorCode::InvalidSurface);
  }

  auto status = host->setSurfacePosition(SurfaceId{1234u}, 100, 100);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
