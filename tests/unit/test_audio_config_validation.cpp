#include "PrimeHost/AudioConfigValidation.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.audio");

PH_TEST("primehost.audio", "validate audio config") {
  AudioStreamConfig config{};
  config.format.channels = 2u;
  config.format.sampleRate = 48000u;
  config.format.format = SampleFormat::Float32;

  auto ok = validateAudioStreamConfig(config);
  PH_CHECK(ok.has_value());

  config.format.channels = 0u;
  auto badChannels = validateAudioStreamConfig(config);
  PH_CHECK(!badChannels.has_value());

  config.format.channels = 2u;
  config.format.sampleRate = 0u;
  auto badRate = validateAudioStreamConfig(config);
  PH_CHECK(!badRate.has_value());
}

TEST_SUITE_END();
