#pragma once

#include <cstddef>
#include <limits>
#include <optional>

namespace PrimeHost {

inline std::optional<uint32_t> checkedU32(size_t value) {
  if (value > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    return std::nullopt;
  }
  return static_cast<uint32_t>(value);
}

inline std::optional<size_t> checkedSizeMul(size_t a, size_t b) {
  if (a == 0u || b == 0u) {
    return static_cast<size_t>(0u);
  }
  if (a > (std::numeric_limits<size_t>::max() / b)) {
    return std::nullopt;
  }
  return a * b;
}

} // namespace PrimeHost
