#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <vector>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.display");

PH_TEST("primehost.display", "display enumeration") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto countResult = host->displays({});
  PH_CHECK(countResult.has_value());
  if (!countResult) {
    return;
  }
  PH_CHECK(countResult.value() >= 1u);

  std::vector<DisplayInfo> displays(countResult.value());
  auto fillResult = host->displays(displays);
  PH_CHECK(fillResult.has_value());
  if (fillResult) {
    PH_CHECK(fillResult.value() == displays.size());
  }

  if (!displays.empty()) {
    auto info = host->displayInfo(displays[0].displayId);
    PH_CHECK(info.has_value());
    if (info) {
      PH_CHECK(info->displayId == displays[0].displayId);
      PH_CHECK(info->width == displays[0].width);
      PH_CHECK(info->height == displays[0].height);
    }
  }
}

PH_TEST("primehost.display", "invalid display id") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto info = host->displayInfo(0u);
  PH_CHECK(!info.has_value());
  if (!info.has_value()) {
    PH_CHECK(info.error().code == HostErrorCode::InvalidDisplay);
  }
}

PH_TEST("primehost.display", "surface display invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto display = host->surfaceDisplay(SurfaceId{999u});
  PH_CHECK(!display.has_value());
  if (!display.has_value()) {
    PH_CHECK(display.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
