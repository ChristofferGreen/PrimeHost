#include "GamepadProfiles.h"

#include <array>
#include <cctype>
#include <string>

namespace PrimeHost {
namespace {

constexpr std::array<GamepadProfile, 7> kProfiles{{
    {"xbox", true},
    {"dualshock", true},
    {"dualsense", true},
    {"switch pro", false},
    {"8bitdo", true},
    {"f310", true},
    {"f710", true},
}};

} // namespace

std::optional<GamepadProfile> findGamepadProfile(std::string_view name) {
  if (name.empty()) {
    return std::nullopt;
  }
  std::string lower;
  lower.reserve(name.size());
  for (char ch : name) {
    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }

  for (const auto& profile : kProfiles) {
    if (lower.find(profile.token) != std::string::npos) {
      return profile;
    }
  }
  return std::nullopt;
}

} // namespace PrimeHost
