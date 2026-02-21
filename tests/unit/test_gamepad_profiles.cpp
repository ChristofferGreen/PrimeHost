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

TEST_SUITE_END();
