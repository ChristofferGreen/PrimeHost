#include "PrimeHost/FrameConfigValidation.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frameconfig");

PH_TEST("primehost.frameconfig", "validation rejects invalid settings") {
  SurfaceCapabilities caps{};
  caps.supportsVsyncToggle = false;
  caps.supportsTearing = false;
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;
  caps.presentModes = (1u << static_cast<uint32_t>(PresentMode::LowLatency));
  caps.colorFormats = (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM));

  FrameConfig config{};
  config.presentMode = PresentMode::LowLatency;
  config.colorFormat = ColorFormat::B8G8R8A8_UNORM;
  config.bufferCount = 2u;
  config.vsync = true;
  config.allowTearing = false;
  config.maxFrameLatency = 1u;

  auto ok = validateFrameConfig(config, caps);
  PH_CHECK(ok.has_value());

  FrameConfig badBuffer = config;
  badBuffer.bufferCount = 4u;
  auto badBufferStatus = validateFrameConfig(badBuffer, caps);
  PH_CHECK(!badBufferStatus.has_value());

  FrameConfig badMode = config;
  badMode.presentMode = PresentMode::Smooth;
  auto badModeStatus = validateFrameConfig(badMode, caps);
  PH_CHECK(!badModeStatus.has_value());

  FrameConfig badVsync = config;
  badVsync.vsync = false;
  auto badVsyncStatus = validateFrameConfig(badVsync, caps);
  PH_CHECK(!badVsyncStatus.has_value());

  FrameConfig badCapped = config;
  badCapped.framePolicy = FramePolicy::Capped;
  badCapped.framePacingSource = FramePacingSource::HostLimiter;
  badCapped.frameInterval.reset();
  auto badCappedStatus = validateFrameConfig(badCapped, caps);
  PH_CHECK(!badCappedStatus.has_value());

  FrameConfig platformCapped = config;
  platformCapped.framePolicy = FramePolicy::Capped;
  platformCapped.framePacingSource = FramePacingSource::Platform;
  platformCapped.frameInterval.reset();
  auto platformStatus = validateFrameConfig(platformCapped, caps);
  PH_CHECK(platformStatus.has_value());

  FrameConfig badInterval = config;
  badInterval.frameInterval = std::chrono::nanoseconds(0);
  auto badIntervalStatus = validateFrameConfig(badInterval, caps);
  PH_CHECK(!badIntervalStatus.has_value());

  FrameConfig badLatency = config;
  badLatency.maxFrameLatency = 4u;
  auto badLatencyStatus = validateFrameConfig(badLatency, caps);
  PH_CHECK(!badLatencyStatus.has_value());

  FrameConfig allowZeroBuffer = config;
  allowZeroBuffer.bufferCount = 0u;
  allowZeroBuffer.maxFrameLatency = 0u;
  auto allowZeroStatus = validateFrameConfig(allowZeroBuffer, caps);
  PH_CHECK(allowZeroStatus.has_value());
}

PH_TEST("primehost.frameconfig", "allows tearing when supported") {
  SurfaceCapabilities caps{};
  caps.supportsVsyncToggle = true;
  caps.supportsTearing = true;
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;
  caps.presentModes = (1u << static_cast<uint32_t>(PresentMode::LowLatency));
  caps.colorFormats = (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM));

  FrameConfig config{};
  config.presentMode = PresentMode::LowLatency;
  config.colorFormat = ColorFormat::B8G8R8A8_UNORM;
  config.bufferCount = 2u;
  config.vsync = false;
  config.allowTearing = true;

  auto status = validateFrameConfig(config, caps);
  PH_CHECK(status.has_value());
}

TEST_SUITE_END();
