#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>
#include <string>

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

TEST_SUITE_END();
