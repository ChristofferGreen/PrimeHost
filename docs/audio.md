# PrimeHost Audio Output

## Scope
PrimeHost owns low-latency audio output for the engine. It should expose a small, platform-neutral
API for output device selection, stream configuration, and callback-driven audio rendering.

## Goals
- Low latency and glitch-free playback.
- Consistent API across platforms.
- Support for device hot-plug and default device changes.
- Zero or minimal allocations in the audio callback.

## Non-Goals (for now)
- Audio input / capture.
- Spatial audio / HRTF.
- Mixing and DSP (handled at higher layers).

## Layering Boundary
PrimeHost is the low-level audio I/O layer. Higher-level libraries own:
- 3D spatialization and HRTF.
- Mixing, voice management, and DSP graphs.
- Streaming/decoding audio assets and resource caching.

PrimeHost should not assume any particular mixing strategy beyond delivering a low-latency callback.

## Draft Concepts
- `AudioDeviceId`: opaque output device handle.
- `AudioFormat`: sample rate, channel count, sample type.
- `AudioStreamConfig`: buffer size, latency target, format.
- `AudioCallback`: pulls audio frames from the engine.

## Draft API Shape (Sketch)
```cpp
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

// Note: the callback always receives interleaved float32 samples. Backends convert to the
// configured output format (e.g., Int16 or non-interleaved) after the callback returns.

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
```

## Device Events
- Audio device connect/disconnect should be surfaced via `AudioDeviceEvent`.
- Default device changes should be reported so the engine can re-open a stream.
  - On macOS, the backend attempts to re-open the current stream on the new default device if the
    previous default device was in use or the active device disappears.

## Open Questions
- Whether to expose pull (callback) only or also push APIs.
- Per-platform backend choices (CoreAudio, WASAPI, ALSA/Pulse/PipeWire, etc.).
- Whether to allow multiple output streams simultaneously.

## Real-Time Safety Rules
- No dynamic allocations in the audio callback.
- No locks or waits in the audio callback.
- No system calls in the audio callback.
- Use lock-free queues or double-buffered state for handoff.

## Example Usage (Sketch)
```cpp
PrimeHost::AudioHost& audio = getAudioHost();
auto device = audio.defaultOutputDevice();

PrimeHost::AudioStreamConfig config{};
config.format.sampleRate = 48000;
config.format.channels = 2;
config.periodFrames = 256;
config.bufferFrames = 512;

auto callback = [](std::span<float> interleaved,
                   const PrimeHost::AudioCallbackContext& ctx,
                   void* userData) {
  auto* mixer = static_cast<MyMixer*>(userData);
  mixer->render(interleaved, ctx.requestedFrames);
};

audio.openStream(device, config, callback, myMixer);
audio.startStream();
```
