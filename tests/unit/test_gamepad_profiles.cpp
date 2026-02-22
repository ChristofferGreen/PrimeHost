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
  auto xbox360 = findGamepadProfile(0x045E, 0x028E, "");
  PH_CHECK(xbox360.has_value());
  if (xbox360) {
    PH_CHECK(xbox360->hasAnalogButtons);
  }

  auto xboxOne = findGamepadProfile(0x045E, 0x02D1, "");
  PH_CHECK(xboxOne.has_value());

  auto xboxSeries = findGamepadProfile(0x045E, 0x0B12, "");
  PH_CHECK(xboxSeries.has_value());

  auto ds4 = findGamepadProfile(0x054C, 0x05C4, "");
  PH_CHECK(ds4.has_value());

  auto ds4v2 = findGamepadProfile(0x054C, 0x09CC, "");
  PH_CHECK(ds4v2.has_value());

  auto ds5 = findGamepadProfile(0x054C, 0x0CE6, "");
  PH_CHECK(ds5.has_value());

  auto switchPro = findGamepadProfile(0x057E, 0x2009, "");
  PH_CHECK(switchPro.has_value());
  if (switchPro) {
    PH_CHECK(!switchPro->hasAnalogButtons);
  }

  auto f310 = findGamepadProfile(0x046D, 0xC21D, "");
  PH_CHECK(f310.has_value());

  auto f710 = findGamepadProfile(0x046D, 0xC21F, "");
  PH_CHECK(f710.has_value());

  auto eightBitDo = findGamepadProfile(0x2DC8, 0x6000, "");
  PH_CHECK(eightBitDo.has_value());

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
