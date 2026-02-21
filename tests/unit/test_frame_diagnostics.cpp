#include "FrameDiagnosticsUtil.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.framediagnostics");

PH_TEST("primehost.framediagnostics", "computes missed frames") {
  auto target = std::chrono::milliseconds(16);

  auto diagFast = buildFrameDiagnostics(target, std::chrono::milliseconds(8), FramePolicy::Continuous);
  PH_CHECK(!diagFast.missedDeadline);
  PH_CHECK(diagFast.droppedFrames == 0u);

  auto diagExact = buildFrameDiagnostics(target, target, FramePolicy::Continuous);
  PH_CHECK(!diagExact.missedDeadline);
  PH_CHECK(diagExact.droppedFrames == 0u);

  auto diagLate = buildFrameDiagnostics(target, std::chrono::milliseconds(33), FramePolicy::Continuous);
  PH_CHECK(diagLate.missedDeadline);
  PH_CHECK(diagLate.droppedFrames == 1u);

  auto diagVeryLate = buildFrameDiagnostics(target, std::chrono::milliseconds(50), FramePolicy::Continuous);
  PH_CHECK(diagVeryLate.missedDeadline);
  PH_CHECK(diagVeryLate.droppedFrames == 2u);
}

PH_TEST("primehost.framediagnostics", "handles missing target") {
  auto diag = buildFrameDiagnostics(std::nullopt, std::chrono::milliseconds(16), FramePolicy::Capped);
  PH_CHECK(!diag.missedDeadline);
  PH_CHECK(diag.droppedFrames == 0u);
  PH_CHECK(diag.wasThrottled);
}

TEST_SUITE_END();
