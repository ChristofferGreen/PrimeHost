#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frameconfig");

PH_TEST("primehost.frameconfig", "query config for invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto config = host->frameConfig(SurfaceId{12345u});
  PH_CHECK(!config.has_value());
  if (!config.has_value()) {
    PH_CHECK(config.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.frameconfig", "query config for valid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  config.headless = true;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto frameConfig = host->frameConfig(surface.value());
  PH_CHECK(frameConfig.has_value());
  if (frameConfig) {
    PH_CHECK(frameConfig->presentMode == PresentMode::LowLatency);
    PH_CHECK(frameConfig->framePolicy == FramePolicy::EventDriven);
    PH_CHECK(frameConfig->framePacingSource == FramePacingSource::Platform);
    PH_CHECK(frameConfig->colorFormat == ColorFormat::B8G8R8A8_UNORM);
    PH_CHECK(frameConfig->vsync);
    PH_CHECK(!frameConfig->allowTearing);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
