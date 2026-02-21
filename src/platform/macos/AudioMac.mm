#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudio/CoreAudio.h>
#import <dispatch/dispatch.h>

#include "PrimeHost/Audio.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    installDeviceListeners();
    refreshDevices(false);
  }

  ~AudioHostMac() override {
    removeDeviceListeners();
    closeStream();
  }

  HostResult<size_t> outputDevices(std::span<AudioDeviceInfo> outDevices) const override {
    refreshDevices(false);
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
    refreshDevices(false);
    auto it = devices_.find(deviceId);
    if (it == devices_.end()) {
      return std::unexpected(HostError{HostErrorCode::InvalidDevice});
    }
    return it->second.info;
  }

  HostResult<AudioDeviceId> defaultOutputDevice() const override {
    refreshDevices(false);
    if (defaultDevice_ == 0) {
      return std::unexpected(HostError{HostErrorCode::DeviceUnavailable});
    }
    return defaultDevice_;
  }

  HostStatus openStream(AudioDeviceId deviceId,
                        const AudioStreamConfig& config,
                        AudioCallback callback,
                        void* userData) override {
    refreshDevices(false);
    if (devices_.find(deviceId) == devices_.end()) {
      return std::unexpected(HostError{HostErrorCode::InvalidDevice});
    }
    if (!callback) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    if (config.format.channels == 0u) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    if (config.format.format != SampleFormat::Float32 && config.format.format != SampleFormat::Int16) {
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

    UInt32 maxFrames = std::max(config.bufferFrames, config.periodFrames);
    if (maxFrames == 0u) {
      maxFrames = 512u;
    }
    AudioUnitSetProperty(unit,
                         kAudioUnitProperty_MaximumFramesPerSlice,
                         kAudioUnitScope_Global,
                         0,
                         &maxFrames,
                         sizeof(maxFrames));

    const bool interleaved = config.format.interleaved;
    const bool isFloat = config.format.format == SampleFormat::Float32;
    const uint32_t bitsPerChannel = isFloat ? 32u : 16u;
    const uint32_t bytesPerSample = bitsPerChannel / 8u;
    const uint32_t bytesPerFrame = interleaved ? bytesPerSample * config.format.channels : bytesPerSample;

    AudioStreamBasicDescription format{};
    format.mSampleRate = static_cast<Float64>(config.format.sampleRate);
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = static_cast<AudioFormatFlags>(kAudioFormatFlagsNativeEndian);
    format.mFormatFlags |= static_cast<AudioFormatFlags>(kAudioFormatFlagIsPacked);
    if (isFloat) {
      format.mFormatFlags |= static_cast<AudioFormatFlags>(kAudioFormatFlagIsFloat);
    } else {
      format.mFormatFlags |= static_cast<AudioFormatFlags>(kAudioFormatFlagIsSignedInteger);
    }
    if (!interleaved) {
      format.mFormatFlags |= static_cast<AudioFormatFlags>(kAudioFormatFlagIsNonInterleaved);
    }
    format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = config.format.channels;
    format.mBitsPerChannel = bitsPerChannel;
    format.mBytesPerFrame = bytesPerFrame;
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
    outputInterleaved_ = interleaved;
    outputSampleFormat_ = config.format.format;
    bytesPerSample_ = bytesPerSample;
    scratchFrames_ = maxFrames;
    if (!outputInterleaved_ || outputSampleFormat_ != SampleFormat::Float32) {
      scratchInterleaved_.assign(static_cast<size_t>(scratchFrames_) * activeChannels_, 0.0f);
    } else {
      scratchInterleaved_.clear();
    }
    return {};
  }

  HostStatus startStream() override {
    if (!unit_) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    if (AudioOutputUnitStart(unit_) != noErr) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    streamRunning_ = true;
    return {};
  }

  HostStatus stopStream() override {
    if (!unit_) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    if (AudioOutputUnitStop(unit_) != noErr) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    streamRunning_ = false;
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
    scratchInterleaved_.clear();
    scratchFrames_ = 0u;
    outputInterleaved_ = true;
    outputSampleFormat_ = SampleFormat::Float32;
    bytesPerSample_ = 0u;
    streamRunning_ = false;
    return {};
  }

  HostResult<AudioStreamConfig> activeConfig() const override {
    if (!unit_) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    return activeConfig_;
  }

  HostStatus setCallbacks(AudioCallbacks callbacks) override {
    callbacks_ = std::move(callbacks);
    refreshDevices(true);
    return {};
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
    const uint32_t channels = self->activeChannels_ > 0 ? self->activeChannels_ : 1u;
    const bool interleaved = self->outputInterleaved_;
    const bool outputFloat = self->outputSampleFormat_ == SampleFormat::Float32;
    const uint32_t bytesPerSample = self->bytesPerSample_ > 0 ? self->bytesPerSample_ : (outputFloat ? 4u : 2u);

    auto zeroBuffers = [&]() {
      for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
        AudioBuffer& buffer = ioData->mBuffers[i];
        if (buffer.mData && buffer.mDataByteSize > 0) {
          std::memset(buffer.mData, 0, buffer.mDataByteSize);
        }
      }
    };

    if (!self->callback_) {
      zeroBuffers();
      return noErr;
    }

    uint32_t framesAvailable = numFrames;
    if (interleaved) {
      AudioBuffer& buffer = ioData->mBuffers[0];
      if (!buffer.mData || buffer.mDataByteSize == 0) {
        return noErr;
      }
      framesAvailable = static_cast<uint32_t>(buffer.mDataByteSize / (bytesPerSample * channels));
    } else {
      framesAvailable = UINT32_MAX;
      for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
        AudioBuffer& buffer = ioData->mBuffers[i];
        if (!buffer.mData || buffer.mDataByteSize == 0) {
          framesAvailable = 0u;
          break;
        }
        uint32_t framesForBuffer = static_cast<uint32_t>(buffer.mDataByteSize / bytesPerSample);
        framesAvailable = std::min(framesAvailable, framesForBuffer);
      }
      if (framesAvailable == UINT32_MAX) {
        framesAvailable = 0u;
      }
    }

    uint32_t framesToWrite = std::min(numFrames, framesAvailable);
    if (self->scratchFrames_ > 0) {
      framesToWrite = std::min(framesToWrite, self->scratchFrames_);
    }
    if (framesToWrite == 0u) {
      zeroBuffers();
      return noErr;
    }

    const size_t sampleCount = static_cast<size_t>(framesToWrite) * channels;
    const bool useDirect = outputFloat && interleaved;
    if (!useDirect) {
      if (self->scratchInterleaved_.size() < sampleCount) {
        zeroBuffers();
        return noErr;
      }
    }

    if (framesToWrite < framesAvailable) {
      zeroBuffers();
    }

    float* target = nullptr;
    if (useDirect) {
      target = static_cast<float*>(ioData->mBuffers[0].mData);
    } else {
      target = self->scratchInterleaved_.data();
    }

    AudioCallbackContext ctx{};
    ctx.frameIndex = self->frameIndex_++;
    ctx.time = std::chrono::steady_clock::now();
    ctx.requestedFrames = framesToWrite;
    ctx.isUnderrun = false;

    std::span<float> span{target, sampleCount};
    self->callback_(span, ctx, self->userData_);

    if (useDirect) {
      return noErr;
    }

    if (outputFloat) {
      for (UInt32 c = 0; c < ioData->mNumberBuffers; ++c) {
        auto* out = static_cast<float*>(ioData->mBuffers[c].mData);
        if (!out) {
          continue;
        }
        for (uint32_t frame = 0; frame < framesToWrite; ++frame) {
          out[frame] = target[static_cast<size_t>(frame) * channels + c];
        }
      }
      return noErr;
    }

    if (interleaved) {
      auto* out = static_cast<int16_t*>(ioData->mBuffers[0].mData);
      if (!out) {
        return noErr;
      }
      for (size_t i = 0; i < sampleCount; ++i) {
        float clamped = std::clamp(target[i], -1.0f, 1.0f);
        out[i] = static_cast<int16_t>(std::lrintf(clamped * 32767.0f));
      }
    } else {
      for (UInt32 c = 0; c < ioData->mNumberBuffers; ++c) {
        auto* out = static_cast<int16_t*>(ioData->mBuffers[c].mData);
        if (!out) {
          continue;
        }
        for (uint32_t frame = 0; frame < framesToWrite; ++frame) {
          float clamped = std::clamp(target[static_cast<size_t>(frame) * channels + c], -1.0f, 1.0f);
          out[frame] = static_cast<int16_t>(std::lrintf(clamped * 32767.0f));
        }
      }
    }
    return noErr;
  }

  static OSStatus deviceListener(AudioObjectID objectId,
                                 UInt32 numberAddresses,
                                 const AudioObjectPropertyAddress addresses[],
                                 void* clientData) {
    (void)objectId;
    (void)numberAddresses;
    (void)addresses;
    auto* self = static_cast<AudioHostMac*>(clientData);
    if (!self) {
      return noErr;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      self->handleDeviceChange();
    });
    return noErr;
  }

  void installDeviceListeners() {
    if (listenersInstalled_) {
      return;
    }
    devicesAddress_.mSelector = kAudioHardwarePropertyDevices;
    devicesAddress_.mScope = kAudioObjectPropertyScopeGlobal;
    devicesAddress_.mElement = kAudioObjectPropertyElementMain;

    defaultAddress_.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    defaultAddress_.mScope = kAudioObjectPropertyScopeGlobal;
    defaultAddress_.mElement = kAudioObjectPropertyElementMain;

    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &devicesAddress_, &AudioHostMac::deviceListener, this);
    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &defaultAddress_, &AudioHostMac::deviceListener, this);
    listenersInstalled_ = true;
  }

  void removeDeviceListeners() {
    if (!listenersInstalled_) {
      return;
    }
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &devicesAddress_, &AudioHostMac::deviceListener, this);
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &defaultAddress_, &AudioHostMac::deviceListener, this);
    listenersInstalled_ = false;
  }

  void handleDeviceChange() {
    AudioDeviceId previousDefault = defaultDevice_;
    AudioDeviceId previousActive = activeDevice_;
    bool hadStream = unit_ != nullptr;
    bool wasRunning = streamRunning_;

    refreshDevices(true);

    if (!hadStream) {
      return;
    }

    const bool activePresent = lastDeviceIds_.find(previousActive) != lastDeviceIds_.end();
    const bool defaultChanged = (defaultDevice_ != 0 && defaultDevice_ != previousDefault);
    const bool shouldReopenToDefault = defaultChanged && previousActive == previousDefault;

    if (!activePresent || shouldReopenToDefault) {
      if (defaultDevice_ != 0) {
        reopenStream(defaultDevice_, wasRunning);
      } else {
        closeStream();
      }
    }
  }

  bool reopenStream(AudioDeviceId deviceId, bool restart) {
    if (!callback_) {
      return false;
    }
    AudioCallback callback = callback_;
    void* userData = userData_;
    AudioStreamConfig config = activeConfig_;

    closeStream();
    if (!openStream(deviceId, config, callback, userData)) {
      return false;
    }
    if (restart) {
      if (!startStream()) {
        return false;
      }
    }
    return true;
  }

  bool refreshDevices(bool emitEvents) const {
    std::unordered_set<AudioDeviceId> previousIds = lastDeviceIds_;
    AudioDeviceId previousDefault = defaultDevice_;

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

    std::unordered_set<AudioDeviceId> newIds;
    newIds.reserve(devices_.size());
    for (const auto& entry : devices_) {
      newIds.insert(entry.first);
    }

    if (emitEvents && callbacks_.onDeviceEvent) {
      for (AudioDeviceId oldId : previousIds) {
        if (newIds.find(oldId) == newIds.end()) {
          AudioDeviceEvent event{};
          event.deviceId = oldId;
          event.connected = false;
          event.isDefault = false;
          callbacks_.onDeviceEvent(event);
        }
      }
      for (AudioDeviceId newId : newIds) {
        if (previousIds.find(newId) == previousIds.end()) {
          AudioDeviceEvent event{};
          event.deviceId = newId;
          event.connected = true;
          event.isDefault = (newId == defaultDevice_);
          callbacks_.onDeviceEvent(event);
        }
      }
      if (defaultDevice_ != 0 && defaultDevice_ != previousDefault &&
          newIds.find(defaultDevice_) != newIds.end()) {
        if (previousIds.find(defaultDevice_) != previousIds.end()) {
          AudioDeviceEvent event{};
          event.deviceId = defaultDevice_;
          event.connected = true;
          event.isDefault = true;
          callbacks_.onDeviceEvent(event);
        }
      }
    }

    lastDeviceIds_ = std::move(newIds);
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
  mutable std::unordered_set<AudioDeviceId> lastDeviceIds_;

  AudioObjectPropertyAddress devicesAddress_{};
  AudioObjectPropertyAddress defaultAddress_{};
  bool listenersInstalled_ = false;
  mutable AudioCallbacks callbacks_{};

  AudioComponentInstance unit_ = nullptr;
  AudioStreamConfig activeConfig_{};
  AudioCallback callback_ = nullptr;
  void* userData_ = nullptr;
  AudioDeviceId activeDevice_ = 0;
  uint64_t frameIndex_ = 0u;
  uint32_t activeChannels_ = 0u;
  bool outputInterleaved_ = true;
  SampleFormat outputSampleFormat_ = SampleFormat::Float32;
  uint32_t bytesPerSample_ = 0u;
  uint32_t scratchFrames_ = 0u;
  std::vector<float> scratchInterleaved_;
  bool streamRunning_ = false;
};

} // namespace

HostResult<std::unique_ptr<AudioHost>> createAudioHostMac() {
  return std::make_unique<AudioHostMac>();
}

} // namespace PrimeHost
