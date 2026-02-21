#include "FrameConfigDefaults.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frameconfig");

PH_TEST("primehost.frameconfig", "defaults prefer present mode") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::LowLatency;
  auto low = resolveFrameConfig(config, caps);
  PH_CHECK(low.bufferCount == 2u);

  config.presentMode = PresentMode::Smooth;
  auto smooth = resolveFrameConfig(config, caps);
  PH_CHECK(smooth.bufferCount == 3u);

  config.presentMode = PresentMode::Uncapped;
  auto uncapped = resolveFrameConfig(config, caps);
  PH_CHECK(uncapped.bufferCount == 3u);
}

PH_TEST("primehost.frameconfig", "defaults clamp to min") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 2u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::Smooth;
  auto resolved = resolveFrameConfig(config, caps);
  PH_CHECK(resolved.bufferCount == 2u);
}

TEST_SUITE_END();
