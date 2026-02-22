#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.fps");

PH_TEST("primehost.fps", "compute stats") {
  std::array<std::chrono::nanoseconds, 4> frames = {
      std::chrono::milliseconds(10),
      std::chrono::milliseconds(20),
      std::chrono::milliseconds(30),
      std::chrono::milliseconds(40),
  };

  auto stats = computeFpsStats(frames);
  PH_CHECK(stats.sampleCount == 4u);
  PH_CHECK(stats.minFrameTime == std::chrono::milliseconds(10));
  PH_CHECK(stats.maxFrameTime == std::chrono::milliseconds(40));
  PH_CHECK(stats.meanFrameTime == std::chrono::milliseconds(25));
  PH_CHECK(stats.p50FrameTime == std::chrono::milliseconds(20));
  PH_CHECK(stats.p95FrameTime == std::chrono::milliseconds(40));
  PH_CHECK(stats.p99FrameTime == std::chrono::milliseconds(40));
  PH_CHECK(stats.fps > 39.0);
  PH_CHECK(stats.fps < 41.0);
}

PH_TEST("primehost.fps", "tracker warmup and report") {
  FpsTracker tracker(2u, std::chrono::nanoseconds(0));
  PH_CHECK(tracker.sampleCount() == 0u);
  PH_CHECK(!tracker.isWarmedUp());

  tracker.framePresented();
  PH_CHECK(tracker.sampleCount() == 0u);
  tracker.framePresented();
  PH_CHECK(tracker.sampleCount() == 1u);
  PH_CHECK(!tracker.isWarmedUp());
  tracker.framePresented();
  PH_CHECK(tracker.sampleCount() == 2u);
  PH_CHECK(tracker.isWarmedUp());

  PH_CHECK(tracker.shouldReport());
  PH_CHECK(tracker.shouldReport());

  tracker.reset();
  PH_CHECK(tracker.sampleCount() == 0u);
  PH_CHECK(!tracker.isWarmedUp());
}

TEST_SUITE_END();
