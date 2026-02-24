#include "PrimeHost/FrameConfigDefaults.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frameconfig");

PH_TEST("primehost.frameconfig", "preferred buffer count uses caps") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;
  PH_CHECK(preferredBufferCount(PresentMode::LowLatency, caps) == 2u);
  PH_CHECK(preferredBufferCount(PresentMode::Smooth, caps) == 3u);
  PH_CHECK(preferredBufferCount(PresentMode::Uncapped, caps) == 3u);

  caps.minBufferCount = 4u;
  caps.maxBufferCount = 2u;
  PH_CHECK(preferredBufferCount(PresentMode::LowLatency, caps) == 4u);
  PH_CHECK(preferredBufferCount(PresentMode::Smooth, caps) == 4u);
  PH_CHECK(preferredBufferCount(PresentMode::Uncapped, caps) == 2u);

  caps.minBufferCount = 0u;
  caps.maxBufferCount = 5u;
  PH_CHECK(preferredBufferCount(PresentMode::Smooth, caps) == 0u);

  caps.minBufferCount = 4u;
  caps.maxBufferCount = 6u;
  PH_CHECK(preferredBufferCount(PresentMode::Smooth, caps) == 4u);
}

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

PH_TEST("primehost.frameconfig", "defaults respect present mode buffer counts") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 1u;
  caps.maxBufferCount = 4u;

  FrameConfig config{};
  config.bufferCount = 0u;

  config.presentMode = PresentMode::LowLatency;
  auto low = resolveFrameConfig(config, caps);
  PH_CHECK(low.bufferCount == 1u);

  config.presentMode = PresentMode::Smooth;
  auto smooth = resolveFrameConfig(config, caps);
  PH_CHECK(smooth.bufferCount == 3u);

  config.presentMode = PresentMode::Uncapped;
  auto uncapped = resolveFrameConfig(config, caps);
  PH_CHECK(uncapped.bufferCount == 4u);
}

PH_TEST("primehost.frameconfig", "defaults use max buffer for uncapped") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 5u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::Uncapped;

  auto resolved = resolveFrameConfig(config, caps);
  PH_CHECK(resolved.bufferCount == 5u);
}

PH_TEST("primehost.frameconfig", "defaults use provided interval for capped") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::LowLatency;
  config.framePolicy = FramePolicy::Capped;
  config.frameInterval.reset();

  auto resolved = resolveFrameConfig(config, caps, std::chrono::milliseconds(8));
  PH_CHECK(resolved.frameInterval.has_value());
  if (resolved.frameInterval) {
    PH_CHECK(resolved.frameInterval.value() == std::chrono::milliseconds(8));
  }
}

PH_TEST("primehost.frameconfig", "defaults leave capped interval empty without default") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::LowLatency;
  config.framePolicy = FramePolicy::Capped;
  config.frameInterval.reset();

  auto resolved = resolveFrameConfig(config, caps);
  PH_CHECK(!resolved.frameInterval.has_value());
}

PH_TEST("primehost.frameconfig", "defaults ignore interval when not capped") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::LowLatency;
  config.framePolicy = FramePolicy::Continuous;
  config.frameInterval = std::chrono::milliseconds(5);

  auto resolved = resolveFrameConfig(config, caps, std::chrono::milliseconds(16));
  PH_CHECK(resolved.frameInterval.has_value());
  if (resolved.frameInterval) {
    PH_CHECK(resolved.frameInterval.value() == std::chrono::milliseconds(5));
  }
}

PH_TEST("primehost.frameconfig", "defaults do not override explicit interval") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::Smooth;
  config.framePolicy = FramePolicy::Capped;
  config.frameInterval = std::chrono::milliseconds(12);

  auto resolved = resolveFrameConfig(config, caps, std::chrono::milliseconds(8));
  PH_CHECK(resolved.frameInterval.has_value());
  if (resolved.frameInterval) {
    PH_CHECK(resolved.frameInterval.value() == std::chrono::milliseconds(12));
  }
}

PH_TEST("primehost.frameconfig", "defaults keep explicit capped interval over default") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::LowLatency;
  config.framePolicy = FramePolicy::Capped;
  config.frameInterval = std::chrono::milliseconds(7);

  auto resolved = resolveFrameConfig(config, caps, std::chrono::milliseconds(16));
  PH_CHECK(resolved.frameInterval.has_value());
  if (resolved.frameInterval) {
    PH_CHECK(resolved.frameInterval.value() == std::chrono::milliseconds(7));
  }
}

PH_TEST("primehost.frameconfig", "defaults preserve zero buffer limits") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 0u;
  caps.maxBufferCount = 0u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::Smooth;

  auto resolved = resolveFrameConfig(config, caps);
  PH_CHECK(resolved.bufferCount == 0u);
}

PH_TEST("primehost.frameconfig", "defaults handle zero min with max set") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 0u;
  caps.maxBufferCount = 3u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::Smooth;

  auto resolved = resolveFrameConfig(config, caps);
  PH_CHECK(resolved.bufferCount == 0u);
}

PH_TEST("primehost.frameconfig", "defaults handle min set with zero max") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 0u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::LowLatency;

  auto resolved = resolveFrameConfig(config, caps);
  PH_CHECK(resolved.bufferCount == 2u);
}

PH_TEST("primehost.frameconfig", "defaults keep explicit buffer with zero caps") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 0u;
  caps.maxBufferCount = 0u;

  FrameConfig config{};
  config.bufferCount = 3u;
  config.presentMode = PresentMode::LowLatency;

  auto resolved = resolveFrameConfig(config, caps);
  PH_CHECK(resolved.bufferCount == 3u);
}

PH_TEST("primehost.frameconfig", "defaults clamp max frame latency with zero buffer") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 4u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::LowLatency;
  config.maxFrameLatency = 0u;

  auto resolved = resolveFrameConfig(config, caps);
  PH_CHECK(resolved.bufferCount == 2u);
  PH_CHECK(resolved.maxFrameLatency == 1u);
}

PH_TEST("primehost.frameconfig", "defaults clamp max frame latency to buffer count") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 4u;

  FrameConfig config{};
  config.bufferCount = 0u;
  config.presentMode = PresentMode::Smooth;
  config.maxFrameLatency = 5u;

  auto resolved = resolveFrameConfig(config, caps);
  PH_CHECK(resolved.bufferCount == 3u);
  PH_CHECK(resolved.maxFrameLatency == 3u);
}

PH_TEST("primehost.frameconfig", "defaults clamp max frame latency with explicit buffer") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 4u;

  FrameConfig config{};
  config.bufferCount = 2u;
  config.presentMode = PresentMode::LowLatency;
  config.maxFrameLatency = 5u;

  auto resolved = resolveFrameConfig(config, caps);
  PH_CHECK(resolved.bufferCount == 2u);
  PH_CHECK(resolved.maxFrameLatency == 2u);
}

TEST_SUITE_END();
