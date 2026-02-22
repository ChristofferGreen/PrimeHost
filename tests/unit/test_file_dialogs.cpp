#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.dialogs");

PH_TEST("primehost.dialogs", "file dialog buffer validation") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto caps = host->hostCapabilities();
  PH_CHECK(caps.has_value());
  if (caps) {
    PH_CHECK(caps->supportsFileDialogs);
  }

  FileDialogConfig config{};
  std::span<char> emptyBuffer{};
  auto result = host->fileDialog(config, emptyBuffer);
  PH_CHECK(!result.has_value());
  if (!result.has_value()) {
    PH_CHECK(result.error().code == HostErrorCode::BufferTooSmall);
  }

  std::array<TextSpan, 1> spans{};
  std::array<char, 1> buffer{};
  std::span<TextSpan> emptySpans{};
  auto multi = host->fileDialogPaths(config, emptySpans, buffer);
  PH_CHECK(!multi.has_value());
  if (!multi.has_value()) {
    PH_CHECK(multi.error().code == HostErrorCode::BufferTooSmall);
  }

  std::span<char> emptyBytes{};
  auto multiEmpty = host->fileDialogPaths(config, spans, emptyBytes);
  PH_CHECK(!multiEmpty.has_value());
  if (!multiEmpty.has_value()) {
    PH_CHECK(multiEmpty.error().code == HostErrorCode::BufferTooSmall);
  }
}

TEST_SUITE_END();
