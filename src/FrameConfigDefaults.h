#pragma once

#include <algorithm>
#include <cstdint>

#include "PrimeHost/Host.h"

namespace PrimeHost {

inline uint32_t preferredBufferCount(PresentMode mode, const SurfaceCapabilities& caps) {
  uint32_t minCount = caps.minBufferCount;
  uint32_t maxCount = caps.maxBufferCount;
  if (minCount == 0u || maxCount == 0u) {
    return minCount;
  }
  switch (mode) {
    case PresentMode::LowLatency:
      return minCount;
    case PresentMode::Smooth: {
      uint32_t preferred = std::min<uint32_t>(3u, maxCount);
      return std::max(preferred, minCount);
    }
    case PresentMode::Uncapped:
      return maxCount;
  }
  return minCount;
}

inline FrameConfig resolveFrameConfig(const FrameConfig& config, const SurfaceCapabilities& caps) {
  FrameConfig resolved = config;
  if (resolved.bufferCount == 0u) {
    resolved.bufferCount = preferredBufferCount(resolved.presentMode, caps);
  }
  return resolved;
}

} // namespace PrimeHost
