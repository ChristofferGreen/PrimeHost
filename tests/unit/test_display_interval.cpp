#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.presentation");

PH_TEST("primehost.presentation", "display interval invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto interval = host->displayInterval(SurfaceId{999u});
  PH_CHECK(!interval.has_value());
  if (!interval.has_value()) {
    PH_CHECK(interval.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.presentation", "display interval valid surface") {
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

  auto interval = host->displayInterval(surface.value());
  PH_CHECK(interval.has_value());
  if (interval && interval->has_value()) {
    PH_CHECK(interval->value().count() > 0);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
