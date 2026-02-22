#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

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

inline std::optional<float> analogButtonValue(bool isAnalog, float value) {
  if (!isAnalog) {
    return std::nullopt;
  }
  if (!std::isfinite(value)) {
    return std::nullopt;
  }
  return std::clamp(value, 0.0f, 1.0f);
}

} // namespace PrimeHost
