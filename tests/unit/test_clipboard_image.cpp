#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.clipboard_image");

PH_TEST("primehost.clipboard_image", "empty buffer") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<uint8_t, 0> buffer{};
  auto result = host->clipboardImage(buffer);
  PH_CHECK(!result.has_value());
  if (!result.has_value()) {
    PH_CHECK(result.error().code == HostErrorCode::BufferTooSmall);
  }
}

PH_TEST("primehost.clipboard_image", "invalid config") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<uint8_t, 4> pixels = {0, 0, 0, 0};
  ImageData image{};
  image.size = ImageSize{0u, 0u};
  image.pixels = pixels;

  auto status = host->setClipboardImage(image);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.clipboard_image", "size query") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto size = host->clipboardImageSize();
  if (!size.has_value()) {
    PH_CHECK(size.error().code == HostErrorCode::Unsupported);
    return;
  }
}

TEST_SUITE_END();
