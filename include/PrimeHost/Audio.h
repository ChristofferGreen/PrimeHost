#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

#include "PrimeHost/Host.h"

namespace PrimeHost {

using AudioDeviceId = uint64_t;

enum class SampleFormat {
  Float32,
  Int16,
};

struct AudioFormat {
  uint32_t sampleRate = 48000;
  uint16_t channels = 2;
  SampleFormat format = SampleFormat::Float32;
  bool interleaved = true;
};

struct AudioStreamConfig {
  AudioFormat format{};
  uint32_t bufferFrames = 512;
  uint32_t periodFrames = 256;
  std::chrono::nanoseconds targetLatency{0};
};

struct AudioDeviceInfo {
  AudioDeviceId id{};
  Utf8TextView name;
  bool isDefault = false;
  AudioFormat preferredFormat{};
};

struct AudioCallbackContext {
  uint64_t frameIndex = 0;
  std::chrono::steady_clock::time_point time;
  uint32_t requestedFrames = 0;
  bool isUnderrun = false;
};

struct AudioDeviceEvent {
  AudioDeviceId deviceId = 0;
  bool connected = true;
  bool isDefault = false;
};

struct AudioCallbacks {
  std::function<void(const AudioDeviceEvent&)> onDeviceEvent;
};

using AudioCallback = void (*)(std::span<float> interleaved,
                               const AudioCallbackContext& ctx,
                               void* userData);

class AudioHost {
public:
  virtual ~AudioHost() = default;

  virtual HostResult<size_t> outputDevices(std::span<AudioDeviceInfo> outDevices) const = 0;
  virtual HostResult<AudioDeviceInfo> outputDeviceInfo(AudioDeviceId deviceId) const = 0;
  virtual HostResult<AudioDeviceId> defaultOutputDevice() const = 0;

  virtual HostStatus openStream(AudioDeviceId deviceId,
                                const AudioStreamConfig& config,
                                AudioCallback callback,
                                void* userData) = 0;

  virtual HostStatus startStream() = 0;
  virtual HostStatus stopStream() = 0;
  virtual HostStatus closeStream() = 0;

  virtual HostResult<AudioStreamConfig> activeConfig() const = 0;

  virtual HostStatus setCallbacks(AudioCallbacks callbacks) = 0;
};

HostResult<std::unique_ptr<AudioHost>> createAudioHost();

} // namespace PrimeHost
