#pragma once

#include <chrono>
#include <cmath>
#include <optional>

namespace PrimeHost {

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
