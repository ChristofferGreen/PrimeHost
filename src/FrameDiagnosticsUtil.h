#pragma once

#include <chrono>
#include <optional>

#include "PrimeHost/Host.h"

namespace PrimeHost {

inline FrameDiagnostics buildFrameDiagnostics(std::optional<std::chrono::nanoseconds> target,
                                              std::chrono::nanoseconds actual,
                                              FramePolicy policy) {
  FrameDiagnostics diag{};
  if (target && target->count() > 0) {
    diag.targetInterval = *target;
    diag.actualInterval = actual;
    if (actual > *target) {
      diag.missedDeadline = true;
      const uint64_t intervals = static_cast<uint64_t>(actual / *target);
      if (intervals > 1) {
        const uint64_t dropped = intervals - 1u;
        diag.droppedFrames = dropped > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(dropped);
      }
    }
  }
  diag.wasThrottled = policy == FramePolicy::Capped;
  return diag;
}

} // namespace PrimeHost
