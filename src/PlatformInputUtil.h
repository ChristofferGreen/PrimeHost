#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

#include "PrimeHost/Host.h"

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
  return static_cast<float>(value);
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
