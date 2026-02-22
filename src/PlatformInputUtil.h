#pragma once

#include <algorithm>
#include <cmath>

namespace PrimeHost {

inline float tiltNormalizedToDegrees(float normalized) {
  return std::clamp(normalized, -1.0f, 1.0f) * 90.0f;
}

inline float normalizeTwistDegrees(float degrees) {
  if (!std::isfinite(degrees)) {
    return 0.0f;
  }
  float result = std::fmod(degrees, 360.0f);
  if (result < 0.0f) {
    result += 360.0f;
  }
  return result;
}

} // namespace PrimeHost
