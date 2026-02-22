#include "FrameDiagnosticsUtil.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.framediagnostics");

PH_TEST("primehost.framediagnostics", "computes missed frames") {
  auto target = std::chrono::milliseconds(16);

  auto diagFast = buildFrameDiagnostics(target,
                                        std::chrono::milliseconds(8),
                                        FramePolicy::Continuous,
                                        FramePacingSource::Platform);
  PH_CHECK(!diagFast.missedDeadline);
  PH_CHECK(diagFast.droppedFrames == 0u);

  auto diagExact = buildFrameDiagnostics(target,
                                         target,
                                         FramePolicy::Continuous,
                                         FramePacingSource::Platform);
  PH_CHECK(!diagExact.missedDeadline);
  PH_CHECK(diagExact.droppedFrames == 0u);

  auto diagLate = buildFrameDiagnostics(target,
                                        std::chrono::milliseconds(33),
                                        FramePolicy::Continuous,
                                        FramePacingSource::Platform);
  PH_CHECK(diagLate.missedDeadline);
  PH_CHECK(diagLate.droppedFrames == 1u);

  auto diagVeryLate = buildFrameDiagnostics(target,
                                            std::chrono::milliseconds(50),
                                            FramePolicy::Continuous,
                                            FramePacingSource::Platform);
  PH_CHECK(diagVeryLate.missedDeadline);
  PH_CHECK(diagVeryLate.droppedFrames == 2u);
}

PH_TEST("primehost.framediagnostics", "handles missing target") {
  auto diag = buildFrameDiagnostics(std::nullopt,
                                    std::chrono::milliseconds(16),
                                    FramePolicy::Capped,
                                    FramePacingSource::HostLimiter);
  PH_CHECK(!diag.missedDeadline);
  PH_CHECK(diag.droppedFrames == 0u);
  PH_CHECK(diag.wasThrottled);
}

PH_TEST("primehost.framediagnostics", "platform pacing does not mark throttled") {
  auto diag = buildFrameDiagnostics(std::chrono::milliseconds(16),
                                    std::chrono::milliseconds(16),
                                    FramePolicy::Capped,
                                    FramePacingSource::Platform);
  PH_CHECK(!diag.wasThrottled);
}

PH_TEST("primehost.framediagnostics", "dropped frames saturate") {
  auto target = std::chrono::milliseconds(16);
  auto actual = target * static_cast<int64_t>(UINT32_MAX) * 2;
  auto diag = buildFrameDiagnostics(target,
                                    actual,
                                    FramePolicy::Continuous,
                                    FramePacingSource::Platform);
  PH_CHECK(diag.missedDeadline);
  PH_CHECK(diag.droppedFrames == UINT32_MAX);
}

TEST_SUITE_END();
