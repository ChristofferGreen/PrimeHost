#include "PrimeHost/Audio.h"

namespace PrimeHost {
namespace {

class AudioHostMac final : public AudioHost {
public:
  HostResult<size_t> outputDevices(std::span<AudioDeviceInfo> outDevices) const override {
    (void)outDevices;
    return std::unexpected(HostError{HostErrorCode::Unsupported});
  }

  HostResult<AudioDeviceInfo> outputDeviceInfo(AudioDeviceId deviceId) const override {
    (void)deviceId;
    return std::unexpected(HostError{HostErrorCode::Unsupported});
  }

  HostResult<AudioDeviceId> defaultOutputDevice() const override {
    return std::unexpected(HostError{HostErrorCode::Unsupported});
  }

  HostStatus openStream(AudioDeviceId deviceId,
                        const AudioStreamConfig& config,
                        AudioCallback callback,
                        void* userData) override {
    (void)deviceId;
    (void)config;
    (void)callback;
    (void)userData;
    return std::unexpected(HostError{HostErrorCode::Unsupported});
  }

  HostStatus startStream() override {
    return std::unexpected(HostError{HostErrorCode::Unsupported});
  }

  HostStatus stopStream() override {
    return std::unexpected(HostError{HostErrorCode::Unsupported});
  }

  HostStatus closeStream() override {
    return std::unexpected(HostError{HostErrorCode::Unsupported});
  }

  HostResult<AudioStreamConfig> activeConfig() const override {
    return std::unexpected(HostError{HostErrorCode::Unsupported});
  }
};

} // namespace

HostResult<std::unique_ptr<AudioHost>> createAudioHostMac() {
  return std::make_unique<AudioHostMac>();
}

} // namespace PrimeHost
