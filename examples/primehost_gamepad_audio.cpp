#include "PrimeHost/PrimeHost.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace PrimeHost;

namespace {

struct AudioState {
  double phase = 0.0;
  double phaseStep = 0.0;
  uint32_t channels = 2;
};

void audioCallback(std::span<float> interleaved,
                   const AudioCallbackContext& ctx,
                   void* userData) {
  (void)ctx;
  auto* state = static_cast<AudioState*>(userData);
  if (!state || state->channels == 0u) {
    return;
  }
  const uint32_t channels = state->channels;
  const size_t frames = interleaved.size() / channels;
  for (size_t frame = 0; frame < frames; ++frame) {
    float sample = static_cast<float>(std::sin(state->phase));
    state->phase += state->phaseStep;
    if (state->phase > 2.0 * M_PI) {
      state->phase -= 2.0 * M_PI;
    }
    for (uint32_t ch = 0; ch < channels; ++ch) {
      interleaved[frame * channels + ch] = sample;
    }
  }
}

const char* buttonName(uint32_t controlId) {
  switch (static_cast<GamepadButtonId>(controlId)) {
    case GamepadButtonId::South: return "South";
    case GamepadButtonId::East: return "East";
    case GamepadButtonId::West: return "West";
    case GamepadButtonId::North: return "North";
    case GamepadButtonId::LeftBumper: return "LeftBumper";
    case GamepadButtonId::RightBumper: return "RightBumper";
    case GamepadButtonId::Back: return "Back";
    case GamepadButtonId::Start: return "Start";
    case GamepadButtonId::Guide: return "Guide";
    case GamepadButtonId::LeftStick: return "LeftStick";
    case GamepadButtonId::RightStick: return "RightStick";
    case GamepadButtonId::DpadUp: return "DpadUp";
    case GamepadButtonId::DpadDown: return "DpadDown";
    case GamepadButtonId::DpadLeft: return "DpadLeft";
    case GamepadButtonId::DpadRight: return "DpadRight";
    case GamepadButtonId::Misc: return "Misc";
  }
  return "Unknown";
}

const char* axisName(uint32_t controlId) {
  switch (static_cast<GamepadAxisId>(controlId)) {
    case GamepadAxisId::LeftX: return "LeftX";
    case GamepadAxisId::LeftY: return "LeftY";
    case GamepadAxisId::RightX: return "RightX";
    case GamepadAxisId::RightY: return "RightY";
    case GamepadAxisId::LeftTrigger: return "LeftTrigger";
    case GamepadAxisId::RightTrigger: return "RightTrigger";
  }
  return "Unknown";
}

} // namespace

int main() {
  auto hostResult = createHost();
  if (!hostResult) {
    std::printf("Host unavailable: %u\n", static_cast<uint32_t>(hostResult.error().code));
    return 1;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig surfaceConfig{};
  surfaceConfig.width = 800;
  surfaceConfig.height = 600;
  surfaceConfig.title = std::string("PrimeHost Gamepad + Audio");
  auto surfaceResult = host->createSurface(surfaceConfig);
  if (!surfaceResult) {
    std::printf("Surface create failed: %u\n", static_cast<uint32_t>(surfaceResult.error().code));
    return 1;
  }

  auto audioResult = createAudioHost();
  std::unique_ptr<AudioHost> audio;
  AudioState audioState{};
  if (audioResult) {
    audio = std::move(audioResult.value());
    AudioCallbacks audioCallbacks{};
    audioCallbacks.onDeviceEvent = [](const AudioDeviceEvent& event) {
      std::printf("Audio device %llu %s%s\n",
                  static_cast<unsigned long long>(event.deviceId),
                  event.connected ? "connected" : "disconnected",
                  event.isDefault ? " (default)" : "");
    };
    audio->setCallbacks(std::move(audioCallbacks));

    auto defaultDevice = audio->defaultOutputDevice();
    if (defaultDevice) {
      AudioStreamConfig audioConfig{};
      audioConfig.format.sampleRate = 48000;
      audioConfig.format.channels = 2;
      audioConfig.format.format = SampleFormat::Float32;
      audioConfig.format.interleaved = true;
      audioConfig.bufferFrames = 512;
      audioConfig.periodFrames = 256;

      audioState.channels = audioConfig.format.channels;
      audioState.phaseStep = 2.0 * M_PI * 220.0 / static_cast<double>(audioConfig.format.sampleRate);

      if (audio->openStream(defaultDevice.value(), audioConfig, audioCallback, &audioState)) {
        audio->startStream();
      }
    }
  }

  std::atomic<bool> running{true};

  EventBuffer buffer{};
  std::vector<Event> events(256);
  std::vector<char> text(2048);
  buffer.events = events;
  buffer.textBytes = text;

  while (running.load()) {
    host->waitEvents();
    auto batchResult = host->pollEvents(buffer);
    if (!batchResult) {
      continue;
    }
    const auto& batch = batchResult.value();
    for (const auto& event : batch.events) {
      if (std::holds_alternative<InputEvent>(event.payload)) {
        const auto& input = std::get<InputEvent>(event.payload);
        if (std::holds_alternative<DeviceEvent>(input)) {
          const auto& dev = std::get<DeviceEvent>(input);
          std::printf("Device %u %s type=%u\n", dev.deviceId, dev.connected ? "connected" : "disconnected", static_cast<uint32_t>(dev.deviceType));
        } else if (std::holds_alternative<TextEvent>(input)) {
          const auto& textEvent = std::get<TextEvent>(input);
          if (textEvent.text.length > 0) {
            std::string_view view(batch.textBytes.data() + textEvent.text.offset, textEvent.text.length);
            std::printf("Text: %.*s\n", static_cast<int>(view.size()), view.data());
          }
        } else if (std::holds_alternative<GamepadButtonEvent>(input)) {
          const auto& button = std::get<GamepadButtonEvent>(input);
          std::printf("Gamepad %u button %s pressed=%d value=%.2f\n",
                      button.deviceId,
                      buttonName(button.controlId),
                      button.pressed ? 1 : 0,
                      button.value.value_or(0.0f));
          if (button.controlId == static_cast<uint32_t>(GamepadButtonId::South) && button.pressed) {
            GamepadRumble rumble{};
            rumble.deviceId = button.deviceId;
            rumble.lowFrequency = 0.6f;
            rumble.highFrequency = 0.8f;
            rumble.duration = std::chrono::milliseconds(150);
            host->setGamepadRumble(rumble);
          }
        } else if (std::holds_alternative<GamepadAxisEvent>(input)) {
          const auto& axis = std::get<GamepadAxisEvent>(input);
          std::printf("Gamepad %u axis %s value=%.2f\n",
                      axis.deviceId,
                      axisName(axis.controlId),
                      axis.value);
        }
      } else if (std::holds_alternative<LifecycleEvent>(event.payload)) {
        const auto& life = std::get<LifecycleEvent>(event.payload);
        if (life.phase == LifecyclePhase::Destroyed) {
          running.store(false);
        }
      }
    }
  }

  if (audio) {
    audio->stopStream();
    audio->closeStream();
  }

  return 0;
}
