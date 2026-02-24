#include "PrimeHost/AudioConfigDefaults.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.audio");

PH_TEST("primehost.audio", "audio config defaults") {
  AudioStreamConfig config{};
  config.format.channels = 2u;
  config.format.sampleRate = 48000u;
  config.format.format = SampleFormat::Int16;
  config.bufferFrames = 0u;
  config.periodFrames = 0u;

  auto resolved = resolveAudioStreamConfig(config);
  PH_CHECK(resolved.bufferFrames == AudioDefaultBufferFrames);
  PH_CHECK(resolved.periodFrames == AudioDefaultPeriodFrames);
  PH_CHECK(resolved.format.channels == 2u);
  PH_CHECK(resolved.format.sampleRate == 48000u);
  PH_CHECK(resolved.format.format == SampleFormat::Int16);

  config.bufferFrames = 128u;
  config.periodFrames = 0u;
  auto resolvedSmall = resolveAudioStreamConfig(config);
  PH_CHECK(resolvedSmall.bufferFrames == 128u);
  PH_CHECK(resolvedSmall.periodFrames == 128u);

  config.bufferFrames = 128u;
  config.periodFrames = 256u;
  auto resolvedClamp = resolveAudioStreamConfig(config);
  PH_CHECK(resolvedClamp.periodFrames == 128u);

  config.bufferFrames = 0u;
  config.periodFrames = 64u;
  auto resolvedZeroBuffer = resolveAudioStreamConfig(config);
  PH_CHECK(resolvedZeroBuffer.bufferFrames == AudioDefaultBufferFrames);
  PH_CHECK(resolvedZeroBuffer.periodFrames == 64u);

  config.bufferFrames = 0u;
  config.periodFrames = 1024u;
  auto resolvedLargePeriod = resolveAudioStreamConfig(config);
  PH_CHECK(resolvedLargePeriod.bufferFrames == AudioDefaultBufferFrames);
  PH_CHECK(resolvedLargePeriod.periodFrames == AudioDefaultBufferFrames);

  config.bufferFrames = 64u;
  config.periodFrames = 16u;
  auto resolvedPreserve = resolveAudioStreamConfig(config);
  PH_CHECK(resolvedPreserve.bufferFrames == 64u);
  PH_CHECK(resolvedPreserve.periodFrames == 16u);

  config.bufferFrames = 0u;
  config.periodFrames = 0u;
  config.format.sampleRate = 44100u;
  config.format.channels = 1u;
  config.format.format = SampleFormat::Float32;
  auto resolvedFormat = resolveAudioStreamConfig(config);
  PH_CHECK(resolvedFormat.format.sampleRate == 44100u);
  PH_CHECK(resolvedFormat.format.channels == 1u);
  PH_CHECK(resolvedFormat.format.format == SampleFormat::Float32);
}

TEST_SUITE_END();
