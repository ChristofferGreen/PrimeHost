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

PH_TEST("primehost.frameconfig", "effective buffer count uses config when caps unset") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 0u;
  caps.maxBufferCount = 0u;

  FrameConfig config{};
  config.bufferCount = 4u;
  PH_CHECK(effectiveBufferCount(config, caps) == 4u);

  config.bufferCount = 0u;
  PH_CHECK(effectiveBufferCount(config, caps) == 0u);

  caps.minBufferCount = 0u;
  caps.maxBufferCount = 2u;
  config.bufferCount = 1u;
  PH_CHECK(effectiveBufferCount(config, caps) == 1u);

  caps.minBufferCount = 2u;
  caps.maxBufferCount = 0u;
  config.bufferCount = 5u;
  PH_CHECK(effectiveBufferCount(config, caps) == 5u);
}

TEST_SUITE_END();
