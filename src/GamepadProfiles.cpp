#include "GamepadProfiles.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <string>

namespace PrimeHost {
namespace {

constexpr GamepadProfile kXboxProfile{"xbox", true};
constexpr GamepadProfile kDualShockProfile{"dualshock", true};
constexpr GamepadProfile kDualSenseProfile{"dualsense", true};
constexpr GamepadProfile kSwitchProProfile{"switch pro", false};
constexpr GamepadProfile kEightBitDoProfile{"8bitdo", true};
constexpr GamepadProfile kF310Profile{"f310", true};
constexpr GamepadProfile kF710Profile{"f710", true};

constexpr std::array<GamepadProfile, 7> kProfiles{{
    kXboxProfile,
    kDualShockProfile,
    kDualSenseProfile,
    kSwitchProProfile,
    kEightBitDoProfile,
    kF310Profile,
    kF710Profile,
}};

struct GamepadVendorProfile {
  uint16_t vendorId = 0u;
  uint16_t productId = 0u;
  GamepadProfile profile{};
};

constexpr std::array<GamepadVendorProfile, 19> kVendorProfiles{{
    {0x045E, 0x028E, kXboxProfile}, // Xbox 360 Controller
    {0x045E, 0x02D1, kXboxProfile}, // Xbox One Controller
    {0x045E, 0x02DD, kXboxProfile}, // Xbox One Controller (FW 2015)
    {0x045E, 0x02E0, kXboxProfile}, // Xbox One Wireless Controller
    {0x045E, 0x02E3, kXboxProfile}, // Xbox One Elite Controller
    {0x045E, 0x02EA, kXboxProfile}, // Xbox One S Controller
    {0x045E, 0x0B12, kXboxProfile}, // Xbox Series X|S Controller
    {0x045E, 0u, kXboxProfile}, // Microsoft (fallback)
    {0x054C, 0x05C4, kDualShockProfile}, // DualShock 4 (CUH-ZCT1x)
    {0x054C, 0x09CC, kDualShockProfile}, // DualShock 4 (CUH-ZCT2x)
    {0x054C, 0x0CE6, kDualSenseProfile}, // DualSense
    {0x054C, 0u, kDualShockProfile}, // Sony (fallback)
    {0x057E, 0x2009, kSwitchProProfile}, // Switch Pro Controller
    {0x057E, 0u, kSwitchProProfile}, // Nintendo (fallback)
    {0x046D, 0xC21D, kF310Profile}, // Logitech F310 (XInput)
    {0x046D, 0xC21F, kF710Profile}, // Logitech F710 (XInput)
    {0x046D, 0u, kF310Profile}, // Logitech (fallback)
    {0x2DC8, 0x6000, kEightBitDoProfile}, // 8BitDo SF30 Pro
    {0x2DC8, 0u, kEightBitDoProfile}, // 8BitDo (fallback)
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
      if (vendorProfile.vendorId != vendorId || vendorProfile.productId == 0u) {
        continue;
      }
      if (vendorProfile.productId == productId) {
        return vendorProfile.profile;
      }
    }
    for (const auto& vendorProfile : kVendorProfiles) {
      if (vendorProfile.vendorId == vendorId && vendorProfile.productId == 0u) {
        return vendorProfile.profile;
      }
    }
  }
  return findGamepadProfile(name);
}

} // namespace PrimeHost
