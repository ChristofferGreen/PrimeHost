#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace PrimeHost {

struct GamepadProfile {
  std::string_view token;
  bool hasAnalogButtons = false;
};

std::optional<GamepadProfile> findGamepadProfile(std::string_view name);
std::optional<GamepadProfile> findGamepadProfile(uint16_t vendorId,
                                                 uint16_t productId,
                                                 std::string_view name);

} // namespace PrimeHost
