#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surface");

PH_TEST("primehost.surface", "create surface invalid config") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 0u;
  config.height = 720u;
  auto badWidth = host->createSurface(config);
  PH_CHECK(!badWidth.has_value());
  if (!badWidth.has_value()) {
    PH_CHECK(badWidth.error().code == HostErrorCode::InvalidConfig);
  }

  config.width = 1280u;
  config.height = 0u;
  auto badHeight = host->createSurface(config);
  PH_CHECK(!badHeight.has_value());
  if (!badHeight.has_value()) {
    PH_CHECK(badHeight.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.surface", "surface size invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto size = host->surfaceSize(SurfaceId{999u});
  PH_CHECK(!size.has_value());
  if (!size.has_value()) {
    PH_CHECK(size.error().code == HostErrorCode::InvalidSurface);
  }

  auto status = host->setSurfaceSize(SurfaceId{999u}, 1280, 720);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
