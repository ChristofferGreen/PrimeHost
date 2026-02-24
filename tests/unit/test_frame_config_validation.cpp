#include "PrimeHost/FrameConfigValidation.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frameconfig");

PH_TEST("primehost.frameconfig", "mask helpers") {
  PresentModeMask presentMask = 0u;
  presentMask |= (1u << static_cast<uint32_t>(PresentMode::LowLatency));
  presentMask |= (1u << static_cast<uint32_t>(PresentMode::Smooth));
  PH_CHECK(maskHas(presentMask, PresentMode::LowLatency));
  PH_CHECK(maskHas(presentMask, PresentMode::Smooth));
  PH_CHECK(!maskHas(presentMask, PresentMode::Uncapped));

  ColorFormatMask formatMask = 0u;
  formatMask |= (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM));
  PH_CHECK(maskHas(formatMask, ColorFormat::B8G8R8A8_UNORM));
  PH_CHECK(!maskHas(formatMask, static_cast<ColorFormat>(1u)));
}

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
  config.framePolicy = FramePolicy::EventDriven;
  config.frameInterval.reset();

  auto ok = validateFrameConfig(config, caps);
  PH_CHECK(ok.has_value());

  FrameConfig badBuffer = config;
  badBuffer.bufferCount = 4u;
  auto badBufferStatus = validateFrameConfig(badBuffer, caps);
  PH_CHECK(!badBufferStatus.has_value());

  FrameConfig badBufferZeroLatency = config;
  badBufferZeroLatency.bufferCount = 4u;
  badBufferZeroLatency.maxFrameLatency = 0u;
  auto badBufferZeroLatencyStatus = validateFrameConfig(badBufferZeroLatency, caps);
  PH_CHECK(!badBufferZeroLatencyStatus.has_value());

  FrameConfig badMode = config;
  badMode.presentMode = PresentMode::Smooth;
  auto badModeStatus = validateFrameConfig(badMode, caps);
  PH_CHECK(!badModeStatus.has_value());

  FrameConfig badFormat = config;
  badFormat.colorFormat = static_cast<ColorFormat>(1u);
  auto badFormatStatus = validateFrameConfig(badFormat, caps);
  PH_CHECK(!badFormatStatus.has_value());

  FrameConfig badVsync = config;
  badVsync.vsync = false;
  auto badVsyncStatus = validateFrameConfig(badVsync, caps);
  PH_CHECK(!badVsyncStatus.has_value());

  FrameConfig badTearing = config;
  badTearing.allowTearing = true;
  auto badTearingStatus = validateFrameConfig(badTearing, caps);
  PH_CHECK(!badTearingStatus.has_value());

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

  FrameConfig badIntervalWithBuffer = config;
  badIntervalWithBuffer.frameInterval = std::chrono::milliseconds(1);
  badIntervalWithBuffer.bufferCount = 4u;
  auto badIntervalBufferStatus = validateFrameConfig(badIntervalWithBuffer, caps);
  PH_CHECK(!badIntervalBufferStatus.has_value());

  FrameConfig badLatency = config;
  badLatency.maxFrameLatency = 4u;
  auto badLatencyStatus = validateFrameConfig(badLatency, caps);
  PH_CHECK(!badLatencyStatus.has_value());

  FrameConfig allowZeroBuffer = config;
  allowZeroBuffer.bufferCount = 0u;
  allowZeroBuffer.maxFrameLatency = 0u;
  auto allowZeroStatus = validateFrameConfig(allowZeroBuffer, caps);
  PH_CHECK(allowZeroStatus.has_value());

  FrameConfig zeroBufferWithLatency = config;
  zeroBufferWithLatency.bufferCount = 0u;
  zeroBufferWithLatency.maxFrameLatency = 2u;
  auto zeroBufferLatencyStatus = validateFrameConfig(zeroBufferWithLatency, caps);
  PH_CHECK(zeroBufferLatencyStatus.has_value());

  FrameConfig cappedZeroInterval = config;
  cappedZeroInterval.framePolicy = FramePolicy::Capped;
  cappedZeroInterval.framePacingSource = FramePacingSource::HostLimiter;
  cappedZeroInterval.frameInterval = std::chrono::nanoseconds(0);
  auto cappedZeroStatus = validateFrameConfig(cappedZeroInterval, caps);
  PH_CHECK(!cappedZeroStatus.has_value());

  FrameConfig uncappedZeroInterval = config;
  uncappedZeroInterval.framePolicy = FramePolicy::Continuous;
  uncappedZeroInterval.frameInterval = std::chrono::nanoseconds(0);
  auto uncappedZeroStatus = validateFrameConfig(uncappedZeroInterval, caps);
  PH_CHECK(!uncappedZeroStatus.has_value());

  FrameConfig cappedHostLimiterValid = config;
  cappedHostLimiterValid.framePolicy = FramePolicy::Capped;
  cappedHostLimiterValid.framePacingSource = FramePacingSource::HostLimiter;
  cappedHostLimiterValid.frameInterval = std::chrono::milliseconds(16);
  auto cappedHostLimiterStatus = validateFrameConfig(cappedHostLimiterValid, caps);
  PH_CHECK(cappedHostLimiterStatus.has_value());

  FrameConfig cappedHostLimiterNegative = config;
  cappedHostLimiterNegative.framePolicy = FramePolicy::Capped;
  cappedHostLimiterNegative.framePacingSource = FramePacingSource::HostLimiter;
  cappedHostLimiterNegative.frameInterval = std::chrono::nanoseconds(-1);
  auto cappedHostLimiterNegativeStatus = validateFrameConfig(cappedHostLimiterNegative, caps);
  PH_CHECK(!cappedHostLimiterNegativeStatus.has_value());

  FrameConfig cappedPlatformNoInterval = config;
  cappedPlatformNoInterval.framePolicy = FramePolicy::Capped;
  cappedPlatformNoInterval.framePacingSource = FramePacingSource::Platform;
  cappedPlatformNoInterval.frameInterval.reset();
  auto cappedPlatformStatus = validateFrameConfig(cappedPlatformNoInterval, caps);
  PH_CHECK(cappedPlatformStatus.has_value());

  FrameConfig cappedPlatformZeroInterval = config;
  cappedPlatformZeroInterval.framePolicy = FramePolicy::Capped;
  cappedPlatformZeroInterval.framePacingSource = FramePacingSource::Platform;
  cappedPlatformZeroInterval.frameInterval = std::chrono::nanoseconds(0);
  auto cappedPlatformZeroStatus = validateFrameConfig(cappedPlatformZeroInterval, caps);
  PH_CHECK(!cappedPlatformZeroStatus.has_value());
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

PH_TEST("primehost.frameconfig", "allows tearing with vsync when supported") {
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
  config.vsync = true;
  config.allowTearing = true;

  auto status = validateFrameConfig(config, caps);
  PH_CHECK(status.has_value());
}

PH_TEST("primehost.frameconfig", "allows vsync toggle when supported") {
  SurfaceCapabilities caps{};
  caps.supportsVsyncToggle = true;
  caps.supportsTearing = false;
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;
  caps.presentModes = (1u << static_cast<uint32_t>(PresentMode::LowLatency));
  caps.colorFormats = (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM));

  FrameConfig config{};
  config.presentMode = PresentMode::LowLatency;
  config.colorFormat = ColorFormat::B8G8R8A8_UNORM;
  config.bufferCount = 2u;
  config.vsync = false;
  config.allowTearing = false;

  auto status = validateFrameConfig(config, caps);
  PH_CHECK(status.has_value());
}

PH_TEST("primehost.frameconfig", "rejects vsync toggle when unsupported even with tearing") {
  SurfaceCapabilities caps{};
  caps.supportsVsyncToggle = false;
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
  PH_CHECK(!status.has_value());
}

TEST_SUITE_END();
