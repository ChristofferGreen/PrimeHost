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

PH_TEST("primehost.surface", "surface constraints invalid config") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 48u;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto minBadWidth = host->setSurfaceMinSize(surface.value(), 0u, 10u);
  PH_CHECK(!minBadWidth.has_value());
  if (!minBadWidth.has_value()) {
    PH_CHECK(minBadWidth.error().code == HostErrorCode::InvalidConfig);
  }

  auto maxBadHeight = host->setSurfaceMaxSize(surface.value(), 10u, 0u);
  PH_CHECK(!maxBadHeight.has_value());
  if (!maxBadHeight.has_value()) {
    PH_CHECK(maxBadHeight.error().code == HostErrorCode::InvalidConfig);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
