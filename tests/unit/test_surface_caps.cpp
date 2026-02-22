#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surface");

PH_TEST("primehost.surface", "surface capabilities invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto caps = host->surfaceCapabilities(SurfaceId{12345u});
  PH_CHECK(!caps.has_value());
  if (!caps.has_value()) {
    PH_CHECK(caps.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.surface", "surface capabilities valid surface") {
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

  auto caps = host->surfaceCapabilities(surface.value());
  PH_CHECK(caps.has_value());
  if (caps) {
    PH_CHECK(caps->minBufferCount > 0u);
    PH_CHECK(caps->maxBufferCount >= caps->minBufferCount);
    auto presentMask = caps->presentModes;
    PH_CHECK((presentMask & (1u << static_cast<uint32_t>(PresentMode::LowLatency))) != 0u);
    PH_CHECK((presentMask & (1u << static_cast<uint32_t>(PresentMode::Smooth))) != 0u);
    PH_CHECK((presentMask & (1u << static_cast<uint32_t>(PresentMode::Uncapped))) != 0u);
    auto formatMask = caps->colorFormats;
    PH_CHECK((formatMask & (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM))) != 0u);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
