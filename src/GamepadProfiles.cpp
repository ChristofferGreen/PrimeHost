#include "GamepadProfiles.h"

#include <array>
#include <cctype>
#include <cstdint>
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

struct GamepadVendorProfile {
  uint16_t vendorId = 0u;
  uint16_t productId = 0u;
  GamepadProfile profile{};
};

constexpr std::array<GamepadVendorProfile, 5> kVendorProfiles{{
    {0x045E, 0u, {"xbox", true}},      // Microsoft
    {0x054C, 0u, {"dualshock", true}}, // Sony
    {0x057E, 0u, {"switch pro", false}}, // Nintendo
    {0x046D, 0u, {"f310", true}},      // Logitech
    {0x2DC8, 0u, {"8bitdo", true}},    // 8BitDo
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

std::optional<GamepadProfile> findGamepadProfile(uint16_t vendorId,
                                                 uint16_t productId,
                                                 std::string_view name) {
  if (vendorId != 0u) {
    for (const auto& vendorProfile : kVendorProfiles) {
      if (vendorProfile.vendorId != vendorId) {
        continue;
      }
      if (vendorProfile.productId != 0u && vendorProfile.productId != productId) {
        continue;
      }
      return vendorProfile.profile;
    }
  }
  return findGamepadProfile(name);
}

} // namespace PrimeHost
