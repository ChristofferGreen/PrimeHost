#pragma once

#include <optional>
#include <string_view>

namespace PrimeHost {

struct GamepadProfile {
  std::string_view token;
  bool hasAnalogButtons = false;
};

std::optional<GamepadProfile> findGamepadProfile(std::string_view name);

} // namespace PrimeHost
