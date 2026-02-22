#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surface");

PH_TEST("primehost.surface", "surface state invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto minimized = host->setSurfaceMinimized(SurfaceId{555u}, true);
  PH_CHECK(!minimized.has_value());
  if (!minimized.has_value()) {
    PH_CHECK(minimized.error().code == HostErrorCode::InvalidSurface);
  }

  auto maximized = host->setSurfaceMaximized(SurfaceId{555u}, true);
  PH_CHECK(!maximized.has_value());
  if (!maximized.has_value()) {
    PH_CHECK(maximized.error().code == HostErrorCode::InvalidSurface);
  }

  auto fullscreen = host->setSurfaceFullscreen(SurfaceId{555u}, true);
  PH_CHECK(!fullscreen.has_value());
  if (!fullscreen.has_value()) {
    PH_CHECK(fullscreen.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
