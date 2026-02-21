#include "GamepadProfiles.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.gamepad");

PH_TEST("primehost.gamepad", "profile matching") {
  auto xbox = findGamepadProfile("Xbox Wireless Controller");
  PH_CHECK(xbox.has_value());
  if (xbox) {
    PH_CHECK(xbox->hasAnalogButtons);
  }

  auto dualsense = findGamepadProfile("Sony DualSense");
  PH_CHECK(dualsense.has_value());

  auto switchPro = findGamepadProfile("Nintendo Switch Pro Controller");
  PH_CHECK(switchPro.has_value());
  if (switchPro) {
    PH_CHECK(!switchPro->hasAnalogButtons);
  }

  auto eightBit = findGamepadProfile("8BitDo Pro 2");
  PH_CHECK(eightBit.has_value());

  auto logitech = findGamepadProfile("LOGITECH F310");
  PH_CHECK(logitech.has_value());

  auto missing = findGamepadProfile("Unknown Controller");
  PH_CHECK(!missing.has_value());
}

PH_TEST("primehost.gamepad", "vendor matching") {
  auto sony = findGamepadProfile(0x054C, 0u, "");
  PH_CHECK(sony.has_value());
  if (sony) {
    PH_CHECK(sony->hasAnalogButtons);
  }

  auto nintendo = findGamepadProfile(0x057E, 0u, "");
  PH_CHECK(nintendo.has_value());
  if (nintendo) {
    PH_CHECK(!nintendo->hasAnalogButtons);
  }

  auto xbox = findGamepadProfile(0x045E, 0u, "Unknown");
  PH_CHECK(xbox.has_value());

  auto fallback = findGamepadProfile(0u, 0u, "Nintendo Switch Pro Controller");
  PH_CHECK(fallback.has_value());
}

TEST_SUITE_END();
