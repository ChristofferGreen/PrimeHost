#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.screenshot");

PH_TEST("primehost.screenshot", "invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto status = host->writeSurfaceScreenshot(SurfaceId{4242u}, "/tmp/primehost-shot.png");
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.screenshot", "invalid path") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  config.title = "PrimeHost Screenshot Test";
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto emptyPath = host->writeSurfaceScreenshot(surface.value(), "");
  PH_CHECK(!emptyPath.has_value());
  if (!emptyPath.has_value()) {
    PH_CHECK(emptyPath.error().code == HostErrorCode::InvalidConfig);
  }

  auto wrongExt = host->writeSurfaceScreenshot(surface.value(), "/tmp/primehost-shot.bmp");
  PH_CHECK(!wrongExt.has_value());
  if (!wrongExt.has_value()) {
    PH_CHECK(wrongExt.error().code == HostErrorCode::InvalidConfig);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
