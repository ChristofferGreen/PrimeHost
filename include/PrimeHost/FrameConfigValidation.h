#pragma once

#include "PrimeHost/Host.h"

namespace PrimeHost {

inline bool maskHas(PresentModeMask mask, PresentMode mode) {
  return (mask & (1u << static_cast<uint32_t>(mode))) != 0u;
}

inline bool maskHas(ColorFormatMask mask, ColorFormat format) {
  return (mask & (1u << static_cast<uint32_t>(format))) != 0u;
}

inline HostStatus validateFrameConfig(const FrameConfig& config, const SurfaceCapabilities& caps) {
  if (config.frameInterval && config.frameInterval->count() <= 0) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  if (config.bufferCount != 0u) {
    if (config.bufferCount < caps.minBufferCount || config.bufferCount > caps.maxBufferCount) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
  }
  if (!maskHas(caps.presentModes, config.presentMode)) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  if (!maskHas(caps.colorFormats, config.colorFormat)) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  if (config.allowTearing && !caps.supportsTearing) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  if (!config.vsync && !caps.supportsVsyncToggle) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  if (config.framePolicy == FramePolicy::Capped &&
      config.framePacingSource == FramePacingSource::HostLimiter) {
    if (!config.frameInterval || config.frameInterval->count() <= 0) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
  }
  if (config.bufferCount != 0u && config.maxFrameLatency != 0u) {
    if (config.maxFrameLatency > config.bufferCount) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
  }
  return {};
}

} // namespace PrimeHost
