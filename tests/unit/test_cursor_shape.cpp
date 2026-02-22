#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.cursor");

PH_TEST("primehost.cursor", "set shape invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto status = host->setCursorShape(SurfaceId{4242u}, CursorShape::Hand);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.cursor", "set shape valid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 48u;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto arrow = host->setCursorShape(surface.value(), CursorShape::Arrow);
  PH_CHECK(arrow.has_value());
  auto resizeLR = host->setCursorShape(surface.value(), CursorShape::ResizeLeftRight);
  PH_CHECK(resizeLR.has_value());
  auto resizeUD = host->setCursorShape(surface.value(), CursorShape::ResizeUpDown);
  PH_CHECK(resizeUD.has_value());

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
