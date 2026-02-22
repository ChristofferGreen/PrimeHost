#include "PrimeHost/FrameConfigUtil.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frameconfig");

PH_TEST("primehost.frameconfig", "effective buffer count clamps") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;

  FrameConfig config{};
  config.bufferCount = 2u;
  PH_CHECK(effectiveBufferCount(config, caps) == 2u);

  config.bufferCount = 3u;
  PH_CHECK(effectiveBufferCount(config, caps) == 3u);

  config.bufferCount = 1u;
  PH_CHECK(effectiveBufferCount(config, caps) == 2u);

  config.bufferCount = 5u;
  PH_CHECK(effectiveBufferCount(config, caps) == 3u);
}

TEST_SUITE_END();
