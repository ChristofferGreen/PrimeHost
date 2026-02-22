#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "PrimeHost/Host.h"

namespace PrimeHost {

inline float tiltNormalizedToDegrees(float normalized) {
  return std::clamp(normalized, -1.0f, 1.0f) * 90.0f;
}

inline std::optional<float> normalizedTiltDegrees(float normalized) {
  if (!std::isfinite(normalized)) {
    return std::nullopt;
  }
  return tiltNormalizedToDegrees(normalized);
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

inline std::optional<float> normalizedPressure(float value) {
  if (!std::isfinite(value)) {
    return std::nullopt;
  }
  return std::clamp(value, 0.0f, 1.0f);
}

inline float normalizedScrollDelta(double value) {
  if (!std::isfinite(value)) {
    return 0.0f;
  }
  constexpr double kMax = static_cast<double>(std::numeric_limits<float>::max());
  if (value >= kMax) {
    return std::numeric_limits<float>::max();
  }
  if (value <= -kMax) {
    return -std::numeric_limits<float>::max();
  }
  return static_cast<float>(value);
}

inline std::optional<int32_t> normalizedPointerDelta(double value) {
  if (!std::isfinite(value)) {
    return std::nullopt;
  }
  constexpr double kMax = static_cast<double>(std::numeric_limits<int32_t>::max());
  constexpr double kMin = static_cast<double>(std::numeric_limits<int32_t>::min());
  if (value >= kMax) {
    return std::numeric_limits<int32_t>::max();
  }
  if (value <= kMin) {
    return std::numeric_limits<int32_t>::min();
  }
  return static_cast<int32_t>(std::lround(value));
}

inline float clampGamepadAxisValue(uint32_t controlId, float value) {
  if (!std::isfinite(value)) {
    return 0.0f;
  }
  if (controlId == static_cast<uint32_t>(GamepadAxisId::LeftTrigger) ||
      controlId == static_cast<uint32_t>(GamepadAxisId::RightTrigger)) {
    return std::clamp(value, 0.0f, 1.0f);
  }
  return std::clamp(value, -1.0f, 1.0f);
}

} // namespace PrimeHost
