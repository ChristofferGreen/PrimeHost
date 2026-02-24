#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.fps");

PH_TEST("primehost.fps", "fps stats defaults") {
  FpsStats stats{};
  PH_CHECK(stats.sampleCount == 0u);
  PH_CHECK(stats.minFrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.maxFrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.meanFrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.p50FrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.p95FrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.p99FrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.fps == 0.0);
}

PH_TEST("primehost.fps", "compute stats empty") {
  std::array<std::chrono::nanoseconds, 0> frames{};
  auto stats = computeFpsStats(frames);
  PH_CHECK(stats.sampleCount == 0u);
  PH_CHECK(stats.minFrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.maxFrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.meanFrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.p50FrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.p95FrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.p99FrameTime == std::chrono::nanoseconds(0));
  PH_CHECK(stats.fps == 0.0);
}

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

PH_TEST("primehost.fps", "tracker zero capacity") {
  FpsTracker tracker(0u, std::chrono::nanoseconds(0));
  PH_CHECK(tracker.sampleCapacity() == 0u);
  PH_CHECK(tracker.sampleCount() == 0u);
  PH_CHECK(!tracker.isWarmedUp());

  tracker.framePresented();
  tracker.framePresented();
  PH_CHECK(tracker.sampleCount() == 0u);

  auto stats = tracker.stats();
  PH_CHECK(stats.sampleCount == 0u);
  PH_CHECK(stats.fps == 0.0);
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

PH_TEST("primehost.fps", "tracker report interval reset") {
  FpsTracker tracker(std::chrono::milliseconds(5));
  PH_CHECK(tracker.reportInterval() == std::chrono::milliseconds(5));
  PH_CHECK(tracker.shouldReport());

  tracker.setReportInterval(std::chrono::milliseconds(10));
  PH_CHECK(tracker.reportInterval() == std::chrono::milliseconds(10));
  PH_CHECK(tracker.shouldReport());
}

PH_TEST("primehost.fps", "tracker zero report interval always reports") {
  FpsTracker tracker(std::chrono::nanoseconds(0));
  PH_CHECK(tracker.shouldReport());
  PH_CHECK(tracker.shouldReport());
}

PH_TEST("primehost.fps", "compute stats single sample") {
  std::array<std::chrono::nanoseconds, 1> frames = {std::chrono::milliseconds(16)};
  auto stats = computeFpsStats(frames);
  PH_CHECK(stats.sampleCount == 1u);
  PH_CHECK(stats.minFrameTime == std::chrono::milliseconds(16));
  PH_CHECK(stats.maxFrameTime == std::chrono::milliseconds(16));
  PH_CHECK(stats.meanFrameTime == std::chrono::milliseconds(16));
  PH_CHECK(stats.p50FrameTime == std::chrono::milliseconds(16));
  PH_CHECK(stats.p95FrameTime == std::chrono::milliseconds(16));
  PH_CHECK(stats.p99FrameTime == std::chrono::milliseconds(16));
  PH_CHECK(stats.fps > 59.0);
  PH_CHECK(stats.fps < 63.0);
}

PH_TEST("primehost.fps", "compute stats identical samples") {
  std::array<std::chrono::nanoseconds, 3> frames = {
      std::chrono::milliseconds(20),
      std::chrono::milliseconds(20),
      std::chrono::milliseconds(20),
  };
  auto stats = computeFpsStats(frames);
  PH_CHECK(stats.sampleCount == 3u);
  PH_CHECK(stats.minFrameTime == std::chrono::milliseconds(20));
  PH_CHECK(stats.maxFrameTime == std::chrono::milliseconds(20));
  PH_CHECK(stats.meanFrameTime == std::chrono::milliseconds(20));
  PH_CHECK(stats.p50FrameTime == std::chrono::milliseconds(20));
  PH_CHECK(stats.p95FrameTime == std::chrono::milliseconds(20));
  PH_CHECK(stats.p99FrameTime == std::chrono::milliseconds(20));
}

PH_TEST("primehost.fps", "tracker zero interval after set") {
  FpsTracker tracker(std::chrono::milliseconds(5));
  tracker.setReportInterval(std::chrono::nanoseconds(0));
  PH_CHECK(tracker.reportInterval() == std::chrono::nanoseconds(0));
  PH_CHECK(tracker.shouldReport());
  PH_CHECK(tracker.shouldReport());
}

TEST_SUITE_END();
