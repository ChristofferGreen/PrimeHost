#pragma once

#include <chrono>
#include <cmath>

namespace PrimeHost {

inline std::chrono::steady_clock::time_point steadyTimeFromUptime(
    double eventSeconds,
    double uptimeSeconds,
    std::chrono::steady_clock::time_point now) {
  if (!std::isfinite(eventSeconds) || !std::isfinite(uptimeSeconds)) {
    return now;
  }
  if (eventSeconds < 0.0 || uptimeSeconds < 0.0) {
    return now;
  }
  double delta = uptimeSeconds - eventSeconds;
  if (delta < 0.0) {
    delta = 0.0;
  }
  auto offset = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(delta));
  return now - offset;
}

} // namespace PrimeHost
