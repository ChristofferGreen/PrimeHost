#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <vector>

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

PH_TEST("primehost.display", "set surface display headless") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  config.headless = true;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto count = host->displays({});
  PH_REQUIRE(count.has_value());
  if (count.value() > 0u) {
    std::vector<DisplayInfo> displays(count.value());
    auto filled = host->displays(displays);
    PH_REQUIRE(filled.has_value());
    auto status = host->setSurfaceDisplay(surface.value(), displays[0].displayId);
    PH_CHECK(status.has_value());
    auto interval = host->displayInterval(surface.value());
    PH_CHECK(interval.has_value());
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
