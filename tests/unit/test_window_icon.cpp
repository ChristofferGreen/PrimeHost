#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>
#include <span>
#include <vector>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.window_icon");

PH_TEST("primehost.window_icon", "invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<uint8_t, 4> pixels = {0, 0, 0, 0};
  IconImage image{};
  image.size = ImageSize{1u, 1u};
  image.pixels = pixels;
  WindowIcon icon{};
  icon.images = std::span<const IconImage>(&image, 1);

  auto status = host->setSurfaceIcon(SurfaceId{4242u}, icon);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.window_icon", "invalid config") {
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

  WindowIcon empty{};
  auto emptyStatus = host->setSurfaceIcon(surface.value(), empty);
  PH_CHECK(!emptyStatus.has_value());
  if (!emptyStatus.has_value()) {
    PH_CHECK(emptyStatus.error().code == HostErrorCode::InvalidConfig);
  }

  std::array<uint8_t, 3> pixels = {0, 0, 0};
  IconImage image{};
  image.size = ImageSize{1u, 1u};
  image.pixels = pixels;
  WindowIcon icon{};
  icon.images = std::span<const IconImage>(&image, 1);
  auto smallStatus = host->setSurfaceIcon(surface.value(), icon);
  PH_CHECK(!smallStatus.has_value());
  if (!smallStatus.has_value()) {
    PH_CHECK(smallStatus.error().code == HostErrorCode::BufferTooSmall);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
