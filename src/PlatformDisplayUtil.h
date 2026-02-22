#pragma once

#include <chrono>
#include <cmath>
#include <optional>

namespace PrimeHost {

inline float resolvedRefreshRate(double refreshRate, double fallback) {
  if (refreshRate > 0.0 && std::isfinite(refreshRate)) {
    return static_cast<float>(refreshRate);
  }
  if (fallback > 0.0 && std::isfinite(fallback)) {
    return static_cast<float>(fallback);
  }
  return 0.0f;
}

inline std::optional<std::chrono::nanoseconds> intervalFromRefreshRate(double refreshRate) {
  if (!(refreshRate > 0.0) || !std::isfinite(refreshRate)) {
    return std::nullopt;
  }
  double seconds = 1.0 / refreshRate;
  if (!(seconds > 0.0) || !std::isfinite(seconds)) {
    return std::nullopt;
  }
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(seconds));
}

} // namespace PrimeHost
