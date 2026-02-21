#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>

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

inline FrameConfig resolveFrameConfig(const FrameConfig& config,
                                      const SurfaceCapabilities& caps,
                                      std::optional<std::chrono::nanoseconds> defaultInterval) {
  FrameConfig resolved = config;
  if (resolved.bufferCount == 0u) {
    resolved.bufferCount = preferredBufferCount(resolved.presentMode, caps);
  }
  if (resolved.framePolicy == FramePolicy::Capped) {
    if (!resolved.frameInterval || resolved.frameInterval->count() <= 0) {
      if (defaultInterval && defaultInterval->count() > 0) {
        resolved.frameInterval = defaultInterval;
      }
    }
  }
  return resolved;
}

inline FrameConfig resolveFrameConfig(const FrameConfig& config, const SurfaceCapabilities& caps) {
  return resolveFrameConfig(config, caps, std::nullopt);
}

} // namespace PrimeHost
