#pragma once

#include <chrono>
#include <optional>

#include "PrimeHost/Host.h"

namespace PrimeHost {

inline bool shouldPresentCapped(bool bypassCap,
                                std::optional<std::chrono::nanoseconds> interval,
                                std::optional<std::chrono::steady_clock::time_point> last,
                                std::chrono::steady_clock::time_point now) {
  if (bypassCap) {
    return true;
  }
  if (!interval || interval->count() <= 0) {
    return true;
  }
  if (!last) {
    return true;
  }
  return now - *last >= *interval;
}

inline bool shouldPresent(FramePolicy policy,
                          bool bypassCap,
                          std::optional<std::chrono::nanoseconds> interval,
                          std::optional<std::chrono::steady_clock::time_point> last,
                          std::chrono::steady_clock::time_point now) {
  if (policy != FramePolicy::Capped) {
    return true;
  }
  return shouldPresentCapped(bypassCap, interval, last, now);
}

} // namespace PrimeHost
