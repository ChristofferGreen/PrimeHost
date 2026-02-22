#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>
#include <string>

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

  std::string invalidTitle;
  invalidTitle.push_back(static_cast<char>(0xFF));
  auto badTitle = host->setSurfaceTitle(surface.value(), invalidTitle);
  PH_CHECK(!badTitle.has_value());
  if (!badTitle.has_value()) {
    PH_CHECK(badTitle.error().code == HostErrorCode::InvalidConfig);
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

  auto cursorShapeAfter = host->setCursorShape(surface.value(), CursorShape::Hand);
  PH_CHECK(!cursorShapeAfter.has_value());
  if (!cursorShapeAfter.has_value()) {
    PH_CHECK(cursorShapeAfter.error().code == HostErrorCode::InvalidSurface);
  }

  std::array<uint8_t, 4> cursorPixels{255u, 255u, 255u, 255u};
  CursorImage cursor{};
  cursor.width = 1u;
  cursor.height = 1u;
  cursor.hotX = 0;
  cursor.hotY = 0;
  cursor.pixels = cursorPixels;
  auto cursorImageAfter = host->setCursorImage(surface.value(), cursor);
  PH_CHECK(!cursorImageAfter.has_value());
  if (!cursorImageAfter.has_value()) {
    PH_CHECK(cursorImageAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto cursorVisibleAfter = host->setCursorVisible(surface.value(), false);
  PH_CHECK(!cursorVisibleAfter.has_value());
  if (!cursorVisibleAfter.has_value()) {
    PH_CHECK(cursorVisibleAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto minSizeAfter = host->setSurfaceMinSize(surface.value(), 10u, 10u);
  PH_CHECK(!minSizeAfter.has_value());
  if (!minSizeAfter.has_value()) {
    PH_CHECK(minSizeAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto maxSizeAfter = host->setSurfaceMaxSize(surface.value(), 100u, 100u);
  PH_CHECK(!maxSizeAfter.has_value());
  if (!maxSizeAfter.has_value()) {
    PH_CHECK(maxSizeAfter.error().code == HostErrorCode::InvalidSurface);
  }

  std::array<uint8_t, 4> iconPixels{0u, 0u, 0u, 255u};
  IconImage iconImage{};
  iconImage.size = ImageSize{1u, 1u};
  iconImage.pixels = iconPixels;
  WindowIcon icon{};
  icon.images = std::span<const IconImage>(&iconImage, 1);
  auto iconAfter = host->setSurfaceIcon(surface.value(), icon);
  PH_CHECK(!iconAfter.has_value());
  if (!iconAfter.has_value()) {
    PH_CHECK(iconAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto relativeAfter = host->setRelativePointerCapture(surface.value(), true);
  PH_CHECK(!relativeAfter.has_value());
  if (!relativeAfter.has_value()) {
    PH_CHECK(relativeAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto imeAfter = host->setImeCompositionRect(surface.value(), 1, 2, 3, 4);
  PH_CHECK(!imeAfter.has_value());
  if (!imeAfter.has_value()) {
    PH_CHECK(imeAfter.error().code == HostErrorCode::InvalidSurface);
  }

  auto setDisplayAfter = host->setSurfaceDisplay(surface.value(), 1u);
  PH_CHECK(!setDisplayAfter.has_value());
  if (!setDisplayAfter.has_value()) {
    PH_CHECK(setDisplayAfter.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
