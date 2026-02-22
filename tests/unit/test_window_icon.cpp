#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>
#include <span>

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

TEST_SUITE_END();
