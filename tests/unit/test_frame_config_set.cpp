#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frameconfig");

PH_TEST("primehost.frameconfig", "set config invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  FrameConfig config{};
  auto status = host->setFrameConfig(SurfaceId{12345u}, config);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.frameconfig", "set config invalid values") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig surfaceConfig{};
  surfaceConfig.width = 64u;
  surfaceConfig.height = 64u;
  surfaceConfig.headless = true;
  auto surface = host->createSurface(surfaceConfig);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  FrameConfig bufferTooSmall{};
  bufferTooSmall.bufferCount = 1u;
  auto statusSmall = host->setFrameConfig(surface.value(), bufferTooSmall);
  PH_CHECK(!statusSmall.has_value());
  if (!statusSmall.has_value()) {
    PH_CHECK(statusSmall.error().code == HostErrorCode::InvalidConfig);
  }

  FrameConfig bufferTooLarge{};
  bufferTooLarge.bufferCount = 4u;
  auto statusLarge = host->setFrameConfig(surface.value(), bufferTooLarge);
  PH_CHECK(!statusLarge.has_value());
  if (!statusLarge.has_value()) {
    PH_CHECK(statusLarge.error().code == HostErrorCode::InvalidConfig);
  }

  FrameConfig tearing{};
  tearing.allowTearing = true;
  auto statusTearing = host->setFrameConfig(surface.value(), tearing);
  PH_CHECK(!statusTearing.has_value());
  if (!statusTearing.has_value()) {
    PH_CHECK(statusTearing.error().code == HostErrorCode::InvalidConfig);
  }

  FrameConfig noVsync{};
  noVsync.vsync = false;
  auto statusVsync = host->setFrameConfig(surface.value(), noVsync);
  PH_CHECK(!statusVsync.has_value());
  if (!statusVsync.has_value()) {
    PH_CHECK(statusVsync.error().code == HostErrorCode::InvalidConfig);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
