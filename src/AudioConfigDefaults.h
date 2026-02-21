#pragma once

#include <algorithm>
#include <cstdint>

#include "PrimeHost/Audio.h"

namespace PrimeHost {

constexpr uint32_t AudioDefaultBufferFrames = 512u;
constexpr uint32_t AudioDefaultPeriodFrames = 256u;

inline AudioStreamConfig resolveAudioStreamConfig(const AudioStreamConfig& config) {
  AudioStreamConfig resolved = config;
  if (resolved.bufferFrames == 0u) {
    resolved.bufferFrames = AudioDefaultBufferFrames;
  }
  if (resolved.periodFrames == 0u) {
    resolved.periodFrames = std::min(resolved.bufferFrames, AudioDefaultPeriodFrames);
  }
  if (resolved.periodFrames > resolved.bufferFrames) {
    resolved.periodFrames = resolved.bufferFrames;
  }
  return resolved;
}

} // namespace PrimeHost
