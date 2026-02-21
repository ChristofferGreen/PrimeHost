#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudio/CoreAudio.h>

#include "PrimeHost/Audio.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace PrimeHost {
namespace {

struct AudioDeviceRecord {
  AudioDeviceInfo info;
  std::string nameStorage;
};

class AudioHostMac final : public AudioHost {
public:
  AudioHostMac() {
    refreshDevices();
  }

  ~AudioHostMac() override {
    closeStream();
  }

  HostResult<size_t> outputDevices(std::span<AudioDeviceInfo> outDevices) const override {
    refreshDevices();
    if (outDevices.size() < deviceOrder_.size()) {
      return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
    }
    size_t count = 0u;
    for (AudioDeviceId deviceId : deviceOrder_) {
      auto it = devices_.find(deviceId);
      if (it != devices_.end()) {
        outDevices[count++] = it->second.info;
      }
    }
    return count;
  }

  HostResult<AudioDeviceInfo> outputDeviceInfo(AudioDeviceId deviceId) const override {
    refreshDevices();
    auto it = devices_.find(deviceId);
    if (it == devices_.end()) {
      return std::unexpected(HostError{HostErrorCode::InvalidDevice});
    }
    return it->second.info;
  }

  HostResult<AudioDeviceId> defaultOutputDevice() const override {
    refreshDevices();
    if (defaultDevice_ == 0) {
      return std::unexpected(HostError{HostErrorCode::DeviceUnavailable});
    }
    return defaultDevice_;
  }

  HostStatus openStream(AudioDeviceId deviceId,
                        const AudioStreamConfig& config,
                        AudioCallback callback,
                        void* userData) override {
    refreshDevices();
    if (devices_.find(deviceId) == devices_.end()) {
      return std::unexpected(HostError{HostErrorCode::InvalidDevice});
    }
    if (!callback) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    if (config.format.format != SampleFormat::Float32 || !config.format.interleaved || config.format.channels == 0u) {
      return std::unexpected(HostError{HostErrorCode::Unsupported});
    }

    closeStream();

    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
    if (!component) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }

    AudioComponentInstance unit = nullptr;
    if (AudioComponentInstanceNew(component, &unit) != noErr) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }

    UInt32 enableIO = 1u;
    if (AudioUnitSetProperty(unit,
                             kAudioOutputUnitProperty_EnableIO,
                             kAudioUnitScope_Output,
                             0,
                             &enableIO,
                             sizeof(enableIO)) != noErr) {
      AudioComponentInstanceDispose(unit);
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    enableIO = 0u;
    AudioUnitSetProperty(unit,
                         kAudioOutputUnitProperty_EnableIO,
                         kAudioUnitScope_Input,
                         1,
                         &enableIO,
                         sizeof(enableIO));

    AudioDeviceID coreId = static_cast<AudioDeviceID>(deviceId);
    if (AudioUnitSetProperty(unit,
                             kAudioOutputUnitProperty_CurrentDevice,
                             kAudioUnitScope_Global,
                             0,
                             &coreId,
                             sizeof(coreId)) != noErr) {
      AudioComponentInstanceDispose(unit);
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }

    AudioStreamBasicDescription format{};
    format.mSampleRate = static_cast<Float64>(config.format.sampleRate);
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = static_cast<AudioFormatFlags>(kAudioFormatFlagsNativeEndian) |
                          static_cast<AudioFormatFlags>(kAudioFormatFlagIsFloat) |
                          static_cast<AudioFormatFlags>(kAudioFormatFlagIsPacked);
    format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = config.format.channels;
    format.mBitsPerChannel = 32;
    format.mBytesPerFrame = format.mChannelsPerFrame * sizeof(float);
    format.mBytesPerPacket = format.mBytesPerFrame * format.mFramesPerPacket;

    if (AudioUnitSetProperty(unit,
                             kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Input,
                             0,
                             &format,
                             sizeof(format)) != noErr) {
      AudioComponentInstanceDispose(unit);
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }

    AURenderCallbackStruct render{};
    render.inputProc = &AudioHostMac::renderCallback;
    render.inputProcRefCon = this;
    if (AudioUnitSetProperty(unit,
                             kAudioUnitProperty_SetRenderCallback,
                             kAudioUnitScope_Input,
                             0,
                             &render,
                             sizeof(render)) != noErr) {
      AudioComponentInstanceDispose(unit);
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }

    if (AudioUnitInitialize(unit) != noErr) {
      AudioComponentInstanceDispose(unit);
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }

    unit_ = unit;
    callback_ = callback;
    userData_ = userData;
    activeConfig_ = config;
    activeDevice_ = deviceId;
    activeChannels_ = config.format.channels;
    frameIndex_ = 0u;
    return {};
  }

  HostStatus startStream() override {
    if (!unit_) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    if (AudioOutputUnitStart(unit_) != noErr) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    return {};
  }

  HostStatus stopStream() override {
    if (!unit_) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    if (AudioOutputUnitStop(unit_) != noErr) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    return {};
  }

  HostStatus closeStream() override {
    if (!unit_) {
      return {};
    }
    AudioOutputUnitStop(unit_);
    AudioUnitUninitialize(unit_);
    AudioComponentInstanceDispose(unit_);
    unit_ = nullptr;
    callback_ = nullptr;
    userData_ = nullptr;
    activeDevice_ = 0;
    activeChannels_ = 0u;
    return {};
  }

  HostResult<AudioStreamConfig> activeConfig() const override {
    if (!unit_) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    return activeConfig_;
  }

private:
  static OSStatus renderCallback(void* refCon,
                                 AudioUnitRenderActionFlags* flags,
                                 const AudioTimeStamp* timeStamp,
                                 UInt32 busNumber,
                                 UInt32 numFrames,
                                 AudioBufferList* ioData) {
    (void)flags;
    (void)timeStamp;
    (void)busNumber;
    auto* self = static_cast<AudioHostMac*>(refCon);
    if (!self || !ioData || ioData->mNumberBuffers == 0) {
      return noErr;
    }
    AudioBuffer& buffer = ioData->mBuffers[0];
    if (!buffer.mData || buffer.mDataByteSize == 0) {
      return noErr;
    }
    uint32_t channels = self->activeChannels_ > 0 ? self->activeChannels_ : 1u;
    size_t sampleCount = static_cast<size_t>(numFrames) * channels;
    size_t requiredBytes = sampleCount * sizeof(float);
    if (buffer.mDataByteSize < requiredBytes) {
      sampleCount = buffer.mDataByteSize / sizeof(float);
    }
    auto* samples = static_cast<float*>(buffer.mData);
    if (!self->callback_) {
      std::memset(samples, 0, buffer.mDataByteSize);
      return noErr;
    }

    AudioCallbackContext ctx{};
    ctx.frameIndex = self->frameIndex_++;
    ctx.time = std::chrono::steady_clock::now();
    ctx.requestedFrames = numFrames;
    ctx.isUnderrun = false;

    std::span<float> span{samples, sampleCount};
    self->callback_(span, ctx, self->userData_);
    return noErr;
  }

  bool refreshDevices() const {
    devices_.clear();
    deviceOrder_.clear();
    defaultDevice_ = 0;

    AudioObjectPropertyAddress defaultAddress{};
    defaultAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    defaultAddress.mScope = kAudioObjectPropertyScopeGlobal;
    defaultAddress.mElement = kAudioObjectPropertyElementMain;
    AudioDeviceID defaultDevice = 0;
    UInt32 defaultSize = sizeof(defaultDevice);
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                   &defaultAddress,
                                   0,
                                   nullptr,
                                   &defaultSize,
                                   &defaultDevice) == noErr) {
      defaultDevice_ = static_cast<AudioDeviceId>(defaultDevice);
    }

    AudioObjectPropertyAddress devicesAddress{};
    devicesAddress.mSelector = kAudioHardwarePropertyDevices;
    devicesAddress.mScope = kAudioObjectPropertyScopeGlobal;
    devicesAddress.mElement = kAudioObjectPropertyElementMain;

    UInt32 dataSize = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                       &devicesAddress,
                                       0,
                                       nullptr,
                                       &dataSize) != noErr) {
      return false;
    }

    size_t deviceCount = dataSize / sizeof(AudioDeviceID);
    if (deviceCount == 0) {
      return true;
    }

    std::vector<AudioDeviceID> deviceIds(deviceCount);
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                   &devicesAddress,
                                   0,
                                   nullptr,
                                   &dataSize,
                                   deviceIds.data()) != noErr) {
      return false;
    }

    for (AudioDeviceID deviceId : deviceIds) {
      uint32_t channelCount = outputChannelCount(deviceId);
      if (channelCount == 0) {
        continue;
      }

      AudioDeviceRecord record{};
      record.info.id = static_cast<AudioDeviceId>(deviceId);
      record.info.isDefault = (record.info.id == defaultDevice_);

      record.nameStorage = deviceName(deviceId);
      if (record.nameStorage.empty()) {
        record.nameStorage = "Audio Device";
      }

      record.info.preferredFormat.sampleRate = static_cast<uint32_t>(deviceSampleRate(deviceId));
      record.info.preferredFormat.channels = static_cast<uint16_t>(channelCount);
      record.info.preferredFormat.format = SampleFormat::Float32;
      record.info.preferredFormat.interleaved = true;

      devices_.emplace(record.info.id, record);
      devices_[record.info.id].info.name = devices_[record.info.id].nameStorage;
      deviceOrder_.push_back(record.info.id);
    }

    return true;
  }

  static uint32_t outputChannelCount(AudioDeviceID deviceId) {
    AudioObjectPropertyAddress address{};
    address.mSelector = kAudioDevicePropertyStreamConfiguration;
    address.mScope = kAudioDevicePropertyScopeOutput;
    address.mElement = kAudioObjectPropertyElementMain;

    UInt32 dataSize = 0;
    if (AudioObjectGetPropertyDataSize(deviceId, &address, 0, nullptr, &dataSize) != noErr) {
      return 0;
    }
    if (dataSize == 0) {
      return 0;
    }

    std::vector<uint8_t> data(dataSize);
    auto* bufferList = reinterpret_cast<AudioBufferList*>(data.data());
    if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &dataSize, bufferList) != noErr) {
      return 0;
    }

    uint32_t channels = 0u;
    for (UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
      channels += bufferList->mBuffers[i].mNumberChannels;
    }
    return channels;
  }

  static double deviceSampleRate(AudioDeviceID deviceId) {
    AudioObjectPropertyAddress address{};
    address.mSelector = kAudioDevicePropertyNominalSampleRate;
    address.mScope = kAudioObjectPropertyScopeGlobal;
    address.mElement = kAudioObjectPropertyElementMain;

    Float64 sampleRate = 48000.0;
    UInt32 size = sizeof(sampleRate);
    if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size, &sampleRate) != noErr) {
      return 48000.0;
    }
    return sampleRate;
  }

  static std::string deviceName(AudioDeviceID deviceId) {
    AudioObjectPropertyAddress address{};
    address.mSelector = kAudioObjectPropertyName;
    address.mScope = kAudioObjectPropertyScopeGlobal;
    address.mElement = kAudioObjectPropertyElementMain;

    CFStringRef nameRef = nullptr;
    UInt32 size = sizeof(nameRef);
    if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size, &nameRef) != noErr || !nameRef) {
      return {};
    }

    char buffer[256];
    std::string result;
    if (CFStringGetCString(nameRef, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
      result = buffer;
    }
    CFRelease(nameRef);
    return result;
  }

  mutable std::unordered_map<AudioDeviceId, AudioDeviceRecord> devices_;
  mutable std::vector<AudioDeviceId> deviceOrder_;
  mutable AudioDeviceId defaultDevice_ = 0;

  AudioComponentInstance unit_ = nullptr;
  AudioStreamConfig activeConfig_{};
  AudioCallback callback_ = nullptr;
  void* userData_ = nullptr;
  AudioDeviceId activeDevice_ = 0;
  uint64_t frameIndex_ = 0u;
  uint32_t activeChannels_ = 0u;
};

} // namespace

HostResult<std::unique_ptr<AudioHost>> createAudioHostMac() {
  return std::make_unique<AudioHostMac>();
}

} // namespace PrimeHost
