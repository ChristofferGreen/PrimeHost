#import <CoreGraphics/CGRemoteOperation.h>

#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.cursor");

PH_TEST("primehost.cursor", "relative pointer respects cursor visibility") {
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

  auto hideStatus = host->setCursorVisible(surface.value(), false);
  PH_CHECK(hideStatus.has_value());

  bool hidden = false;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  hidden = CGCursorIsVisible() == 0;
#pragma clang diagnostic pop
  if (hidden) {
    auto captureOn = host->setRelativePointerCapture(surface.value(), true);
    PH_CHECK(captureOn.has_value());
    auto captureOff = host->setRelativePointerCapture(surface.value(), false);
    PH_CHECK(captureOff.has_value());
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    PH_CHECK(CGCursorIsVisible() == 0);
#pragma clang diagnostic pop
  }

  auto showStatus = host->setCursorVisible(surface.value(), true);
  PH_CHECK(showStatus.has_value());

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
