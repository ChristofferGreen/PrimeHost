#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surface");

PH_TEST("primehost.surface", "surface size round-trip") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 48u;
  config.title = std::string("PrimeHost Surface Test");
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto size = host->surfaceSize(surface.value());
  PH_CHECK(size.has_value());
  if (size) {
    PH_CHECK(size->width == 64u);
    PH_CHECK(size->height == 48u);
  }

  auto setSize = host->setSurfaceSize(surface.value(), 96u, 80u);
  PH_CHECK(setSize.has_value());
  if (setSize) {
    auto updated = host->surfaceSize(surface.value());
    PH_CHECK(updated.has_value());
    if (updated) {
      PH_CHECK(updated->width == 96u);
      PH_CHECK(updated->height == 80u);
    }
  }

  auto minReset = host->setSurfaceMinSize(surface.value(), 0u, 0u);
  PH_CHECK(minReset.has_value());
  auto maxReset = host->setSurfaceMaxSize(surface.value(), 0u, 0u);
  PH_CHECK(maxReset.has_value());

  auto minClamp = host->setSurfaceMinSize(surface.value(), 10u, 12u);
  PH_CHECK(minClamp.has_value());
  auto maxClamp = host->setSurfaceMaxSize(surface.value(), 1024u, 768u);
  PH_CHECK(maxClamp.has_value());

  auto scale = host->surfaceScale(surface.value());
  PH_CHECK(scale.has_value());
  if (scale) {
    PH_CHECK(scale.value() > 0.0f);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());

  auto sizeAfter = host->surfaceSize(surface.value());
  PH_CHECK(!sizeAfter.has_value());
  if (!sizeAfter.has_value()) {
    PH_CHECK(sizeAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto scaleAfter = host->surfaceScale(surface.value());
  PH_CHECK(!scaleAfter.has_value());
  if (!scaleAfter.has_value()) {
    PH_CHECK(scaleAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto positionAfter = host->surfacePosition(surface.value());
  PH_CHECK(!positionAfter.has_value());
  if (!positionAfter.has_value()) {
    PH_CHECK(positionAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto setPositionAfter = host->setSurfacePosition(surface.value(), 5, 10);
  PH_CHECK(!setPositionAfter.has_value());
  if (!setPositionAfter.has_value()) {
    PH_CHECK(setPositionAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto displayAfter = host->surfaceDisplay(surface.value());
  PH_CHECK(!displayAfter.has_value());
  if (!displayAfter.has_value()) {
    PH_CHECK(displayAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto titleAfter = host->setSurfaceTitle(surface.value(), "PrimeHost");
  PH_CHECK(!titleAfter.has_value());
  if (!titleAfter.has_value()) {
    PH_CHECK(titleAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto minimizeAfter = host->setSurfaceMinimized(surface.value(), true);
  PH_CHECK(!minimizeAfter.has_value());
  if (!minimizeAfter.has_value()) {
    PH_CHECK(minimizeAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto maximizeAfter = host->setSurfaceMaximized(surface.value(), true);
  PH_CHECK(!maximizeAfter.has_value());
  if (!maximizeAfter.has_value()) {
    PH_CHECK(maximizeAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto fullscreenAfter = host->setSurfaceFullscreen(surface.value(), true);
  PH_CHECK(!fullscreenAfter.has_value());
  if (!fullscreenAfter.has_value()) {
    PH_CHECK(fullscreenAfter.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
