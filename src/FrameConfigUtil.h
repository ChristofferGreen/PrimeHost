#pragma once

#include <algorithm>
#include <cstdint>

#include "PrimeHost/Host.h"

namespace PrimeHost {

inline uint32_t effectiveBufferCount(const FrameConfig& config, const SurfaceCapabilities& caps) {
  if (caps.minBufferCount == 0u || caps.maxBufferCount == 0u) {
    return config.bufferCount;
  }
  return std::clamp(config.bufferCount, caps.minBufferCount, caps.maxBufferCount);
}

} // namespace PrimeHost
