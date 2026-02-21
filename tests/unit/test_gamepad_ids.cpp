#include "PrimeHost/Host.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.gamepad_ids");

PH_TEST("primehost.gamepad_ids", "canonical button ids") {
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::South) == 0u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::East) == 1u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::West) == 2u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::North) == 3u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::LeftBumper) == 4u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::RightBumper) == 5u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::Back) == 6u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::Start) == 7u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::Guide) == 8u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::LeftStick) == 9u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::RightStick) == 10u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::DpadUp) == 11u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::DpadDown) == 12u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::DpadLeft) == 13u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::DpadRight) == 14u);
  PH_CHECK(static_cast<uint32_t>(GamepadButtonId::Misc) == 15u);
}

PH_TEST("primehost.gamepad_ids", "canonical axis ids") {
  PH_CHECK(static_cast<uint32_t>(GamepadAxisId::LeftX) == 0u);
  PH_CHECK(static_cast<uint32_t>(GamepadAxisId::LeftY) == 1u);
  PH_CHECK(static_cast<uint32_t>(GamepadAxisId::RightX) == 2u);
  PH_CHECK(static_cast<uint32_t>(GamepadAxisId::RightY) == 3u);
  PH_CHECK(static_cast<uint32_t>(GamepadAxisId::LeftTrigger) == 4u);
  PH_CHECK(static_cast<uint32_t>(GamepadAxisId::RightTrigger) == 5u);
}

TEST_SUITE_END();
