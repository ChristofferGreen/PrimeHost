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

PH_TEST("primehost.frameconfig", "defaults fill capped interval") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::Smooth;
  config.framePolicy = FramePolicy::Capped;
  config.frameInterval.reset();

  auto resolved = resolveFrameConfig(config, caps, std::chrono::milliseconds(16));
  PH_CHECK(resolved.frameInterval.has_value());
  if (resolved.frameInterval) {
    PH_CHECK(resolved.frameInterval->count() > 0);
  }
}

PH_TEST("primehost.frameconfig", "defaults clamp max frame latency") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::Smooth;
  config.maxFrameLatency = 0u;

  auto resolved = resolveFrameConfig(config, caps, std::chrono::milliseconds(16));
  PH_CHECK(resolved.bufferCount == 3u);
  PH_CHECK(resolved.maxFrameLatency == 1u);

  config.maxFrameLatency = 5u;
  auto clamped = resolveFrameConfig(config, caps, std::chrono::milliseconds(16));
  PH_CHECK(clamped.maxFrameLatency == clamped.bufferCount);
}

TEST_SUITE_END();
