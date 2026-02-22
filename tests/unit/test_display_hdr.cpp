#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <vector>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.display_hdr");

PH_TEST("primehost.display_hdr", "invalid display id") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto result = host->displayHdrInfo(0u);
  PH_CHECK(!result.has_value());
  if (!result.has_value()) {
    PH_CHECK(result.error().code == HostErrorCode::InvalidDisplay);
  }
}

PH_TEST("primehost.display_hdr", "valid display id") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto countResult = host->displays({});
  PH_CHECK(countResult.has_value());
  if (!countResult || countResult.value() == 0u) {
    return;
  }

  std::vector<DisplayInfo> displays(countResult.value());
  auto fillResult = host->displays(displays);
  PH_CHECK(fillResult.has_value());
  if (!fillResult || displays.empty()) {
    return;
  }

  auto hdr = host->displayHdrInfo(displays[0].displayId);
  PH_CHECK(hdr.has_value());
}

TEST_SUITE_END();
