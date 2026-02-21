#include "AudioConfigDefaults.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.audio");

PH_TEST("primehost.audio", "audio config defaults") {
  AudioStreamConfig config{};
  config.bufferFrames = 0u;
  config.periodFrames = 0u;

  auto resolved = resolveAudioStreamConfig(config);
  PH_CHECK(resolved.bufferFrames == AudioDefaultBufferFrames);
  PH_CHECK(resolved.periodFrames == AudioDefaultPeriodFrames);

  config.bufferFrames = 128u;
  config.periodFrames = 0u;
  auto resolvedSmall = resolveAudioStreamConfig(config);
  PH_CHECK(resolvedSmall.bufferFrames == 128u);
  PH_CHECK(resolvedSmall.periodFrames == 128u);

  config.bufferFrames = 128u;
  config.periodFrames = 256u;
  auto resolvedClamp = resolveAudioStreamConfig(config);
  PH_CHECK(resolvedClamp.periodFrames == 128u);
}

TEST_SUITE_END();
