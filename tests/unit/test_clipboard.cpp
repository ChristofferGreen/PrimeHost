#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>
#include <string>
#include <vector>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.clipboard");

PH_TEST("primehost.clipboard", "clipboard buffer too small") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<char, 1> empty{};
  auto result = host->clipboardText(std::span<char>(empty.data(), 0));
  PH_CHECK(!result.has_value());
  if (!result.has_value()) {
    PH_CHECK(result.error().code == HostErrorCode::BufferTooSmall);
  }
}

PH_TEST("primehost.clipboard", "invalid utf8 set") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::string invalid;
  invalid.push_back(static_cast<char>(0xFF));
  auto status = host->setClipboardText(invalid);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.clipboard", "text size and read") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::string text = "PrimeHost Clipboard";
  auto setStatus = host->setClipboardText(text);
  if (!setStatus.has_value()) {
    bool allowed = setStatus.error().code == HostErrorCode::Unsupported;
    allowed = allowed || setStatus.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto size = host->clipboardTextSize();
  if (!size.has_value()) {
    PH_CHECK(size.error().code == HostErrorCode::Unsupported);
    return;
  }
  if (size.value() <= 1u) {
    return;
  }

  std::array<char, 1> tiny{};
  auto readTiny = host->clipboardText(std::span<char>(tiny.data(), tiny.size()));
  PH_CHECK(!readTiny.has_value());
  if (!readTiny.has_value()) {
    PH_CHECK(readTiny.error().code == HostErrorCode::BufferTooSmall);
  }

  std::vector<char> buffer(size.value());
  auto read = host->clipboardText(buffer);
  PH_CHECK(read.has_value());
  if (read.has_value()) {
    PH_CHECK(read->size() == size.value());
  }
}

PH_TEST("primehost.clipboard", "empty clipboard size") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto setStatus = host->setClipboardText("");
  if (!setStatus.has_value()) {
    bool allowed = setStatus.error().code == HostErrorCode::Unsupported;
    allowed = allowed || setStatus.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto size = host->clipboardTextSize();
  if (!size.has_value()) {
    PH_CHECK(size.error().code == HostErrorCode::Unsupported);
    return;
  }
  PH_CHECK(size.value() == 0u);

  std::array<char, 1> buffer{};
  auto read = host->clipboardText(buffer);
  PH_CHECK(read.has_value());
  if (read.has_value()) {
    PH_CHECK(read->size() == 0u);
  }
}

TEST_SUITE_END();
