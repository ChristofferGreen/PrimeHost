#include "PrimeHost/Audio.h"

#include "tests/unit/test_helpers.h"

#include <vector>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.audio");

PH_TEST("primehost.audio", "create audio host") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  PH_CHECK(result.value() != nullptr);
  AudioCallbacks callbacks{};
  callbacks.onDeviceEvent = [](const AudioDeviceEvent&) {};
  auto status = result.value()->setCallbacks(std::move(callbacks));
  PH_CHECK(status.has_value());

  auto inactiveConfig = result.value()->activeConfig();
  PH_CHECK(!inactiveConfig.has_value());
  if (!inactiveConfig.has_value()) {
    PH_CHECK(inactiveConfig.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.audio", "open stream updates active config") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto audio = std::move(result.value());

  auto defaultDevice = audio->defaultOutputDevice();
  if (!defaultDevice) {
    PH_CHECK(defaultDevice.error().code == HostErrorCode::DeviceUnavailable);
    return;
  }

  AudioStreamConfig config{};
  config.format.sampleRate = 48000;
  config.format.channels = 2;
  config.format.format = SampleFormat::Float32;
  config.format.interleaved = true;
  config.bufferFrames = 256;
  config.periodFrames = 128;

  auto callback = [](std::span<float> interleaved, const AudioCallbackContext&, void*) {
    for (float& sample : interleaved) {
      sample = 0.0f;
    }
  };

  auto openStatus = audio->openStream(defaultDevice.value(), config, callback, nullptr);
  if (!openStatus.has_value()) {
    // Some environments may not allow audio devices; skip if unsupported.
    return;
  }

  auto active = audio->activeConfig();
  PH_CHECK(active.has_value());
  if (active.has_value()) {
    PH_CHECK(active->format.sampleRate > 0u);
    PH_CHECK(active->format.channels > 0u);
  }

  audio->closeStream();
}

PH_TEST("primehost.audio", "device count query") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto audio = std::move(result.value());

  auto countResult = audio->outputDevices({});
  PH_CHECK(countResult.has_value());
  if (!countResult) {
    return;
  }

  if (countResult.value() == 0u) {
    return;
  }

  std::vector<AudioDeviceInfo> devices(countResult.value());
  auto fillResult = audio->outputDevices(devices);
  PH_CHECK(fillResult.has_value());
  if (fillResult) {
    PH_CHECK(fillResult.value() == devices.size());
  }

  if (countResult.value() > 0u) {
    std::vector<AudioDeviceInfo> tooSmall(countResult.value() - 1u);
    auto tooSmallResult = audio->outputDevices(tooSmall);
    PH_CHECK(!tooSmallResult.has_value());
    if (!tooSmallResult.has_value()) {
      PH_CHECK(tooSmallResult.error().code == HostErrorCode::BufferTooSmall);
    }
  }
}

PH_TEST("primehost.audio", "invalid device open") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto audio = std::move(result.value());

  AudioStreamConfig config{};
  config.format.sampleRate = 48000;
  config.format.channels = 2;
  config.format.format = SampleFormat::Float32;
  config.format.interleaved = true;
  config.bufferFrames = 256;
  config.periodFrames = 128;

  auto callback = [](std::span<float> interleaved, const AudioCallbackContext&, void*) {
    for (float& sample : interleaved) {
      sample = 0.0f;
    }
  };

  auto status = audio->openStream(static_cast<AudioDeviceId>(0u), config, callback, nullptr);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidDevice);
  }
}

PH_TEST("primehost.audio", "invalid callback") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto audio = std::move(result.value());

  auto defaultDevice = audio->defaultOutputDevice();
  if (!defaultDevice) {
    PH_CHECK(defaultDevice.error().code == HostErrorCode::DeviceUnavailable);
    return;
  }

  AudioStreamConfig config{};
  config.format.sampleRate = 48000;
  config.format.channels = 2;
  config.format.format = SampleFormat::Float32;
  config.format.interleaved = true;
  config.bufferFrames = 256;
  config.periodFrames = 128;

  auto status = audio->openStream(defaultDevice.value(), config, nullptr, nullptr);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.audio", "unsupported sample format") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto audio = std::move(result.value());

  auto defaultDevice = audio->defaultOutputDevice();
  if (!defaultDevice) {
    PH_CHECK(defaultDevice.error().code == HostErrorCode::DeviceUnavailable);
    return;
  }

  AudioStreamConfig config{};
  config.format.sampleRate = 48000;
  config.format.channels = 2;
  config.format.format = static_cast<SampleFormat>(99);
  config.format.interleaved = true;
  config.bufferFrames = 256;
  config.periodFrames = 128;

  auto callback = [](std::span<float> interleaved, const AudioCallbackContext&, void*) {
    for (float& sample : interleaved) {
      sample = 0.0f;
    }
  };

  auto status = audio->openStream(defaultDevice.value(), config, callback, nullptr);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.audio", "start without stream") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto audio = std::move(result.value());

  auto status = audio->startStream();
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.audio", "invalid channel count") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto audio = std::move(result.value());

  auto defaultDevice = audio->defaultOutputDevice();
  if (!defaultDevice) {
    PH_CHECK(defaultDevice.error().code == HostErrorCode::DeviceUnavailable);
    return;
  }

  AudioStreamConfig config{};
  config.format.sampleRate = 48000;
  config.format.channels = 0;
  config.format.format = SampleFormat::Float32;
  config.format.interleaved = true;
  config.bufferFrames = 256;
  config.periodFrames = 128;

  auto callback = [](std::span<float> interleaved, const AudioCallbackContext&, void*) {
    for (float& sample : interleaved) {
      sample = 0.0f;
    }
  };

  auto status = audio->openStream(defaultDevice.value(), config, callback, nullptr);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.audio", "stop without stream") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto audio = std::move(result.value());

  auto status = audio->stopStream();
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.audio", "invalid sample rate") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto audio = std::move(result.value());

  auto defaultDevice = audio->defaultOutputDevice();
  if (!defaultDevice) {
    PH_CHECK(defaultDevice.error().code == HostErrorCode::DeviceUnavailable);
    return;
  }

  AudioStreamConfig config{};
  config.format.sampleRate = 0;
  config.format.channels = 2;
  config.format.format = SampleFormat::Float32;
  config.format.interleaved = true;
  config.bufferFrames = 256;
  config.periodFrames = 128;

  auto callback = [](std::span<float> interleaved, const AudioCallbackContext&, void*) {
    for (float& sample : interleaved) {
      sample = 0.0f;
    }
  };

  auto status = audio->openStream(defaultDevice.value(), config, callback, nullptr);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.audio", "too many channels") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto audio = std::move(result.value());

  auto defaultDevice = audio->defaultOutputDevice();
  if (!defaultDevice) {
    PH_CHECK(defaultDevice.error().code == HostErrorCode::DeviceUnavailable);
    return;
  }

  AudioStreamConfig config{};
  config.format.sampleRate = 48000;
  config.format.channels = 9;
  config.format.format = SampleFormat::Float32;
  config.format.interleaved = true;
  config.bufferFrames = 256;
  config.periodFrames = 128;

  auto callback = [](std::span<float> interleaved, const AudioCallbackContext&, void*) {
    for (float& sample : interleaved) {
      sample = 0.0f;
    }
  };

  auto status = audio->openStream(defaultDevice.value(), config, callback, nullptr);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidConfig);
  }
}

TEST_SUITE_END();
