#include "FrameLimiter.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.framelimiter");

PH_TEST("primehost.framelimiter", "capped policy respects interval") {
  auto now = std::chrono::steady_clock::now();
  auto interval = std::chrono::milliseconds(16);

  PH_CHECK(shouldPresent(FramePolicy::Capped,
                         FramePacingSource::HostLimiter,
                         false,
                         interval,
                         std::nullopt,
                         now));

  auto last = now - std::chrono::milliseconds(8);
  PH_CHECK(!shouldPresent(FramePolicy::Capped,
                          FramePacingSource::HostLimiter,
                          false,
                          interval,
                          last,
                          now));

  auto later = now + std::chrono::milliseconds(17);
  PH_CHECK(shouldPresent(FramePolicy::Capped,
                         FramePacingSource::HostLimiter,
                         false,
                         interval,
                         last,
                         later));

  PH_CHECK(shouldPresent(FramePolicy::Capped,
                         FramePacingSource::HostLimiter,
                         true,
                         interval,
                         last,
                         now));
}

PH_TEST("primehost.framelimiter", "non capped always presents") {
  auto now = std::chrono::steady_clock::now();
  auto interval = std::chrono::milliseconds(16);
  auto last = now;
  PH_CHECK(shouldPresent(FramePolicy::Continuous,
                         FramePacingSource::HostLimiter,
                         false,
                         interval,
                         last,
                         now));
  PH_CHECK(shouldPresent(FramePolicy::EventDriven,
                         FramePacingSource::Platform,
                         false,
                         interval,
                         last,
                         now));
}

PH_TEST("primehost.framelimiter", "platform pacing skips host cap") {
  auto now = std::chrono::steady_clock::now();
  auto interval = std::chrono::milliseconds(16);
  auto last = now;
  PH_CHECK(shouldPresent(FramePolicy::Capped,
                         FramePacingSource::Platform,
                         false,
                         interval,
                         last,
                         now));
}

TEST_SUITE_END();
