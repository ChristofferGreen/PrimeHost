#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>
#include <vector>

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

PH_TEST("primehost.clipboard_image", "buffer too small on set") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<uint8_t, 3> pixels = {0, 0, 0};
  ImageData image{};
  image.size = ImageSize{1u, 1u};
  image.pixels = pixels;

  auto status = host->setClipboardImage(image);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::BufferTooSmall);
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

PH_TEST("primehost.clipboard_image", "size empty after text") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto setText = host->setClipboardText("");
  if (!setText.has_value()) {
    bool allowed = setText.error().code == HostErrorCode::Unsupported;
    allowed = allowed || setText.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto size = host->clipboardImageSize();
  if (!size.has_value()) {
    PH_CHECK(size.error().code == HostErrorCode::Unsupported);
    return;
  }
  PH_CHECK(!size.value().has_value());

  std::array<uint8_t, 4> buffer{};
  auto image = host->clipboardImage(buffer);
  PH_CHECK(image.has_value());
  if (image.has_value()) {
    PH_CHECK(!image->available);
  }
}

PH_TEST("primehost.clipboard_image", "empty after clear") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto setText = host->setClipboardText("");
  if (!setText.has_value()) {
    bool allowed = setText.error().code == HostErrorCode::Unsupported;
    allowed = allowed || setText.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto size = host->clipboardImageSize();
  if (!size.has_value()) {
    PH_CHECK(size.error().code == HostErrorCode::Unsupported);
    return;
  }
  PH_CHECK(!size.value().has_value());

  std::array<uint8_t, 4> buffer{};
  auto image = host->clipboardImage(buffer);
  PH_CHECK(image.has_value());
  if (image.has_value()) {
    PH_CHECK(!image->available);
  }
}

PH_TEST("primehost.clipboard_image", "buffer too small for image") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<uint8_t, 4> pixels = {0, 0, 0, 0};
  ImageData image{};
  image.size = ImageSize{1u, 1u};
  image.pixels = pixels;

  auto setStatus = host->setClipboardImage(image);
  if (!setStatus.has_value()) {
    bool allowed = setStatus.error().code == HostErrorCode::Unsupported;
    allowed = allowed || setStatus.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  std::array<uint8_t, 1> tooSmall{};
  auto readBack = host->clipboardImage(tooSmall);
  PH_CHECK(!readBack.has_value());
  if (!readBack.has_value()) {
    PH_CHECK(readBack.error().code == HostErrorCode::BufferTooSmall);
  }
}

PH_TEST("primehost.clipboard_image", "read back image") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<uint8_t, 4> pixels = {255, 0, 0, 255};
  ImageData image{};
  image.size = ImageSize{1u, 1u};
  image.pixels = pixels;

  auto setStatus = host->setClipboardImage(image);
  if (!setStatus.has_value()) {
    bool allowed = setStatus.error().code == HostErrorCode::Unsupported;
    allowed = allowed || setStatus.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto size = host->clipboardImageSize();
  if (!size.has_value()) {
    PH_CHECK(size.error().code == HostErrorCode::Unsupported);
    return;
  }
  if (!size.value().has_value()) {
    return;
  }
  PH_CHECK(size.value()->width == 1u);
  PH_CHECK(size.value()->height == 1u);

  std::array<uint8_t, 4> buffer{};
  auto readBack = host->clipboardImage(buffer);
  PH_CHECK(readBack.has_value());
  if (readBack.has_value()) {
    PH_CHECK(readBack->available);
    PH_CHECK(readBack->size.width == 1u);
    PH_CHECK(readBack->size.height == 1u);
    PH_CHECK(readBack->pixels.size() == buffer.size());
  }

  auto textSize = host->clipboardTextSize();
  if (!textSize.has_value()) {
    PH_CHECK(textSize.error().code == HostErrorCode::Unsupported);
    return;
  }
  if (textSize.value() > 0u) {
    return;
  }
  std::array<char, 1> textBuf{};
  auto text = host->clipboardText(textBuf);
  if (text.has_value()) {
    PH_CHECK(text->size() == 0u);
  } else {
    bool allowed = text.error().code == HostErrorCode::BufferTooSmall;
    allowed = allowed || text.error().code == HostErrorCode::Unsupported;
    PH_CHECK(allowed);
  }
}

PH_TEST("primehost.clipboard_image", "no image after text") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto setText = host->setClipboardText("PrimeHost");
  if (!setText.has_value()) {
    bool allowed = setText.error().code == HostErrorCode::Unsupported;
    allowed = allowed || setText.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto size = host->clipboardImageSize();
  if (!size.has_value()) {
    PH_CHECK(size.error().code == HostErrorCode::Unsupported);
    return;
  }
  if (size.value().has_value()) {
    return;
  }

  std::array<uint8_t, 4> buffer{};
  auto image = host->clipboardImage(buffer);
  PH_CHECK(image.has_value());
  if (image.has_value()) {
    PH_CHECK(!image->available);
  }
}

TEST_SUITE_END();
