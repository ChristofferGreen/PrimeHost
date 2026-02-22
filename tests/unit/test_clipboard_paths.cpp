#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.clipboard_paths");

PH_TEST("primehost.clipboard_paths", "count query") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto count = host->clipboardPathsCount();
  if (!count.has_value()) {
    PH_CHECK(count.error().code == HostErrorCode::Unsupported);
    return;
  }
  PH_CHECK(count.value() >= 0u);
}

PH_TEST("primehost.clipboard_paths", "buffer too small") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<TextSpan, 0> spans{};
  std::array<char, 0> buffer{};
  auto result = host->clipboardPaths(spans, buffer);
  PH_CHECK(!result.has_value());
  if (!result.has_value()) {
    PH_CHECK(result.error().code == HostErrorCode::BufferTooSmall);
  }
}

TEST_SUITE_END();
