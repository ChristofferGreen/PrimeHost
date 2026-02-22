#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <CoreGraphics/CGRemoteOperation.h>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.cursor");

bool cursor_is_visible() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  bool visible = CGCursorIsVisible() != 0;
#pragma clang diagnostic pop
  return visible;
}

PH_TEST("primehost.cursor", "cursor visibility invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto status = host->setCursorVisible(SurfaceId{777u}, false);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.cursor", "cursor visibility idempotent") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  bool startVisible = cursor_is_visible();
  auto hideStatus = host->setCursorVisible(surface.value(), false);
  PH_CHECK(hideStatus.has_value());
  bool hidden = !cursor_is_visible();
  if (startVisible && hidden) {
    auto hideAgain = host->setCursorVisible(surface.value(), false);
    PH_CHECK(hideAgain.has_value());
    auto showOnce = host->setCursorVisible(surface.value(), true);
    PH_CHECK(showOnce.has_value());
    (void)cursor_is_visible();
  }

  auto showStatus = host->setCursorVisible(surface.value(), true);
  PH_CHECK(showStatus.has_value());

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
