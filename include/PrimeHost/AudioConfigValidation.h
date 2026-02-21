#pragma once

#include <cstdint>

#include "PrimeHost/Audio.h"

namespace PrimeHost {

inline HostStatus validateAudioStreamConfig(const AudioStreamConfig& config) {
  if (config.format.channels == 0u || config.format.channels > 8u) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  if (config.format.sampleRate == 0u) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  switch (config.format.format) {
    case SampleFormat::Float32:
    case SampleFormat::Int16:
      break;
    default:
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  return {};
}

} // namespace PrimeHost
