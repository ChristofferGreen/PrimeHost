#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <CoreFoundation/CoreFoundation.h>

#include <atomic>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frame");

PH_TEST("primehost.frame", "host limiter continuous ticks") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 16u;
  config.height = 16u;
  config.headless = true;

  auto surfaceResult = host->createSurface(config);
  PH_REQUIRE(surfaceResult.has_value());
  SurfaceId surface = surfaceResult.value();

  std::atomic<uint32_t> frames{0u};
  Callbacks callbacks{};
  callbacks.onFrame = [&frames, surface](SurfaceId id,
                                         const FrameTiming&,
                                         const FrameDiagnostics&) {
    if (id == surface) {
      frames.fetch_add(1u, std::memory_order_relaxed);
    }
  };
  auto callbackStatus = host->setCallbacks(callbacks);
  PH_CHECK(callbackStatus.has_value());

  FrameConfig frameConfig{};
  frameConfig.framePolicy = FramePolicy::Continuous;
  frameConfig.framePacingSource = FramePacingSource::HostLimiter;
  frameConfig.frameInterval = std::chrono::milliseconds(5);

  auto configStatus = host->setFrameConfig(surface, frameConfig);
  PH_REQUIRE(configStatus.has_value());

  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
  while (frames.load(std::memory_order_relaxed) == 0u &&
         std::chrono::steady_clock::now() < deadline) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.02, false);
  }

  PH_CHECK(frames.load(std::memory_order_relaxed) > 0u);
  host->destroySurface(surface);
}

TEST_SUITE_END();
