#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.ime");

PH_TEST("primehost.ime", "invalid composition rect") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto badWidth = host->setImeCompositionRect(surface.value(), 5, 6, -1, 10);
  PH_CHECK(!badWidth.has_value());
  if (!badWidth.has_value()) {
    PH_CHECK(badWidth.error().code == HostErrorCode::InvalidConfig);
  }

  auto badHeight = host->setImeCompositionRect(surface.value(), 5, 6, 10, -1);
  PH_CHECK(!badHeight.has_value());
  if (!badHeight.has_value()) {
    PH_CHECK(badHeight.error().code == HostErrorCode::InvalidConfig);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
