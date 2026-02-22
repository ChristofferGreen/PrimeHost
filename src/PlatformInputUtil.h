#pragma once

#include <algorithm>

namespace PrimeHost {

inline float tiltNormalizedToDegrees(float normalized) {
  return std::clamp(normalized, -1.0f, 1.0f) * 90.0f;
}

} // namespace PrimeHost
