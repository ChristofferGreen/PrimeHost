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

TEST_SUITE_END();
