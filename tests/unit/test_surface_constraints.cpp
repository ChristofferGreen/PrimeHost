#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surface");

PH_TEST("primehost.surface", "surface constraints invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto minStatus = host->setSurfaceMinSize(SurfaceId{999u}, 640, 480);
  PH_CHECK(!minStatus.has_value());
  if (!minStatus.has_value()) {
    PH_CHECK(minStatus.error().code == HostErrorCode::InvalidSurface);
  }

  auto maxStatus = host->setSurfaceMaxSize(SurfaceId{999u}, 1920, 1080);
  PH_CHECK(!maxStatus.has_value());
  if (!maxStatus.has_value()) {
    PH_CHECK(maxStatus.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
