#include "PrimeHost/Host.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frametiming");

PH_TEST("primehost.frametiming", "frame timing defaults") {
  FrameTiming timing{};
  PH_CHECK(timing.time.time_since_epoch() == std::chrono::steady_clock::duration::zero());
  PH_CHECK(timing.delta == std::chrono::nanoseconds(0));
  PH_CHECK(timing.frameIndex == 0u);
}

PH_TEST("primehost.frametiming", "frame diagnostics defaults") {
  FrameDiagnostics diag{};
  PH_CHECK(diag.targetInterval == std::chrono::nanoseconds(0));
  PH_CHECK(diag.actualInterval == std::chrono::nanoseconds(0));
  PH_CHECK(!diag.missedDeadline);
  PH_CHECK(!diag.wasThrottled);
  PH_CHECK(diag.droppedFrames == 0u);
}

PH_TEST("primehost.frametiming", "frame timing writable") {
  FrameTiming timing{};
  auto now = std::chrono::steady_clock::now();
  timing.time = now;
  timing.delta = std::chrono::milliseconds(5);
  timing.frameIndex = 12u;

  PH_CHECK(timing.time == now);
  PH_CHECK(timing.delta == std::chrono::milliseconds(5));
  PH_CHECK(timing.frameIndex == 12u);
}

PH_TEST("primehost.frametiming", "frame diagnostics writable") {
  FrameDiagnostics diag{};
  diag.targetInterval = std::chrono::milliseconds(16);
  diag.actualInterval = std::chrono::milliseconds(20);
  diag.missedDeadline = true;
  diag.wasThrottled = true;
  diag.droppedFrames = 3u;

  PH_CHECK(diag.targetInterval == std::chrono::milliseconds(16));
  PH_CHECK(diag.actualInterval == std::chrono::milliseconds(20));
  PH_CHECK(diag.missedDeadline);
  PH_CHECK(diag.wasThrottled);
  PH_CHECK(diag.droppedFrames == 3u);
}

TEST_SUITE_END();
