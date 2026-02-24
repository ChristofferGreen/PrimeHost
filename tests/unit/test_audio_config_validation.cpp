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

  config.format.format = SampleFormat::Int16;
  auto okInt = validateAudioStreamConfig(config);
  PH_CHECK(okInt.has_value());

  config.format.channels = 0u;
  auto badChannels = validateAudioStreamConfig(config);
  PH_CHECK(!badChannels.has_value());

  config.format.channels = 2u;
  config.format.sampleRate = 0u;
  auto badRate = validateAudioStreamConfig(config);
  PH_CHECK(!badRate.has_value());

  config.format.channels = 9u;
  config.format.sampleRate = 48000u;
  config.format.format = SampleFormat::Float32;
  auto tooManyChannels = validateAudioStreamConfig(config);
  PH_CHECK(!tooManyChannels.has_value());

  config.format.channels = 2u;
  config.format.sampleRate = 48000u;
  config.format.format = static_cast<SampleFormat>(99u);
  auto badFormat = validateAudioStreamConfig(config);
  PH_CHECK(!badFormat.has_value());
}

TEST_SUITE_END();
