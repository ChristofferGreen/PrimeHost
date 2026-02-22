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

PH_TEST("primehost.clipboard_paths", "buffer too small for paths") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<TextSpan, 1> spans{};
  std::array<char, 1> buffer{};
  auto result = host->clipboardPaths(spans, buffer);
  if (result.has_value()) {
    PH_CHECK(!result->available);
    PH_CHECK(result->paths.size() == 0u);
  } else {
    bool allowed = result.error().code == HostErrorCode::BufferTooSmall;
    allowed = allowed || result.error().code == HostErrorCode::Unsupported;
    PH_CHECK(allowed);
  }
}

PH_TEST("primehost.clipboard_paths", "text size query") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto size = host->clipboardPathsTextSize();
  if (!size.has_value()) {
    PH_CHECK(size.error().code == HostErrorCode::Unsupported);
    return;
  }
  PH_CHECK(size.value() >= 0u);
}

PH_TEST("primehost.clipboard_paths", "count and size consistency") {
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

  auto size = host->clipboardPathsTextSize();
  if (!size.has_value()) {
    PH_CHECK(size.error().code == HostErrorCode::Unsupported);
    return;
  }

  if (count.value() == 0u) {
    PH_CHECK(size.value() == 0u);
  } else {
    PH_CHECK(size.value() >= count.value());
  }
}

TEST_SUITE_END();
