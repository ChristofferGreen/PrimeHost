#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.cursor_image");

PH_TEST("primehost.cursor_image", "invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<uint8_t, 4> pixels = {0, 0, 0, 0};
  CursorImage image{};
  image.width = 1u;
  image.height = 1u;
  image.pixels = pixels;

  auto status = host->setCursorImage(SurfaceId{4242u}, image);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.cursor_image", "buffer too small") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<uint8_t, 3> pixels = {0, 0, 0};
  CursorImage image{};
  image.width = 1u;
  image.height = 1u;
  image.pixels = pixels;

  auto status = host->setCursorImage(SurfaceId{4242u}, image);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.cursor_image", "invalid config") {
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

  CursorImage empty{};
  auto emptyStatus = host->setCursorImage(surface.value(), empty);
  PH_CHECK(!emptyStatus.has_value());
  if (!emptyStatus.has_value()) {
    PH_CHECK(emptyStatus.error().code == HostErrorCode::InvalidConfig);
  }

  std::array<uint8_t, 3> tooSmall = {0u, 0u, 0u};
  CursorImage small{};
  small.width = 1u;
  small.height = 1u;
  small.pixels = tooSmall;
  auto smallStatus = host->setCursorImage(surface.value(), small);
  PH_CHECK(!smallStatus.has_value());
  if (!smallStatus.has_value()) {
    PH_CHECK(smallStatus.error().code == HostErrorCode::BufferTooSmall);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

PH_TEST("primehost.cursor_image", "invalid hotspot") {
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

  std::array<uint8_t, 4> pixels = {0u, 0u, 0u, 0u};
  CursorImage image{};
  image.width = 1u;
  image.height = 1u;
  image.pixels = pixels;

  image.hotX = -1;
  image.hotY = 0;
  auto negative = host->setCursorImage(surface.value(), image);
  PH_CHECK(!negative.has_value());
  if (!negative.has_value()) {
    PH_CHECK(negative.error().code == HostErrorCode::InvalidConfig);
  }

  image.hotX = 1;
  image.hotY = 0;
  auto outOfBounds = host->setCursorImage(surface.value(), image);
  PH_CHECK(!outOfBounds.has_value());
  if (!outOfBounds.has_value()) {
    PH_CHECK(outOfBounds.error().code == HostErrorCode::InvalidConfig);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
