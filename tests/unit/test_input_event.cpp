#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <variant>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.input");

PH_TEST("primehost.input", "pointer event defaults") {
  PointerEvent pointer{};
  PH_CHECK(pointer.deviceType == PointerDeviceType::Mouse);
  PH_CHECK(pointer.phase == PointerPhase::Move);
  PH_CHECK(pointer.x == 0);
  PH_CHECK(pointer.y == 0);
  PH_CHECK(pointer.buttonMask == 0u);
  PH_CHECK(pointer.isPrimary);
  PH_CHECK(!pointer.deltaX.has_value());
  PH_CHECK(!pointer.deltaY.has_value());
  PH_CHECK(!pointer.pressure.has_value());
  PH_CHECK(!pointer.tiltX.has_value());
  PH_CHECK(!pointer.tiltY.has_value());
  PH_CHECK(!pointer.twist.has_value());
  PH_CHECK(!pointer.distance.has_value());
}

PH_TEST("primehost.input", "key event defaults") {
  KeyEvent key{};
  PH_CHECK(key.deviceId == 0u);
  PH_CHECK(key.keyCode == 0u);
  PH_CHECK(key.modifiers == 0u);
  PH_CHECK(!key.pressed);
  PH_CHECK(!key.repeat);
}

PH_TEST("primehost.input", "text event defaults") {
  TextEvent text{};
  PH_CHECK(text.deviceId == 0u);
  PH_CHECK(text.text.offset == 0u);
  PH_CHECK(text.text.length == 0u);
}

PH_TEST("primehost.input", "scroll event defaults") {
  ScrollEvent scroll{};
  PH_CHECK(scroll.deviceId == 0u);
  PH_CHECK(scroll.deltaX == 0.0f);
  PH_CHECK(scroll.deltaY == 0.0f);
  PH_CHECK(!scroll.isLines);
}

PH_TEST("primehost.input", "gamepad button event defaults") {
  GamepadButtonEvent button{};
  PH_CHECK(button.deviceId == 0u);
  PH_CHECK(button.controlId == 0u);
  PH_CHECK(!button.pressed);
  PH_CHECK(!button.value.has_value());
}

PH_TEST("primehost.input", "gamepad axis event defaults") {
  GamepadAxisEvent axis{};
  PH_CHECK(axis.deviceId == 0u);
  PH_CHECK(axis.controlId == 0u);
  PH_CHECK(axis.value == 0.0f);
}

PH_TEST("primehost.input", "device event defaults") {
  DeviceEvent device{};
  PH_CHECK(device.deviceId == 0u);
  PH_CHECK(device.deviceType == DeviceType::Mouse);
  PH_CHECK(device.connected);
}

PH_TEST("primehost.input", "pointer event payload types") {
  Event event{};
  PointerEvent pointer{};
  pointer.x = 12;
  pointer.y = -4;
  event.payload = InputEvent{pointer};

  PH_CHECK(std::holds_alternative<InputEvent>(event.payload));
  const auto& input = std::get<InputEvent>(event.payload);
  PH_CHECK(std::holds_alternative<PointerEvent>(input));
  const auto& payload = std::get<PointerEvent>(input);
  PH_CHECK(payload.x == 12);
  PH_CHECK(payload.y == -4);
}

PH_TEST("primehost.input", "key event payload types") {
  Event event{};
  KeyEvent key{};
  key.keyCode = 42u;
  key.pressed = true;
  event.payload = InputEvent{key};

  PH_CHECK(std::holds_alternative<InputEvent>(event.payload));
  const auto& input = std::get<InputEvent>(event.payload);
  PH_CHECK(std::holds_alternative<KeyEvent>(input));
  const auto& payload = std::get<KeyEvent>(input);
  PH_CHECK(payload.keyCode == 42u);
  PH_CHECK(payload.pressed);
}

PH_TEST("primehost.input", "text event payload types") {
  Event event{};
  TextEvent text{};
  text.text = TextSpan{3u, 7u};
  event.payload = InputEvent{text};

  PH_CHECK(std::holds_alternative<InputEvent>(event.payload));
  const auto& input = std::get<InputEvent>(event.payload);
  PH_CHECK(std::holds_alternative<TextEvent>(input));
  const auto& payload = std::get<TextEvent>(input);
  PH_CHECK(payload.text.offset == 3u);
  PH_CHECK(payload.text.length == 7u);
}

PH_TEST("primehost.input", "scroll event payload types") {
  Event event{};
  ScrollEvent scroll{};
  scroll.deltaX = 1.5f;
  scroll.deltaY = -2.5f;
  event.payload = InputEvent{scroll};

  PH_CHECK(std::holds_alternative<InputEvent>(event.payload));
  const auto& input = std::get<InputEvent>(event.payload);
  PH_CHECK(std::holds_alternative<ScrollEvent>(input));
  const auto& payload = std::get<ScrollEvent>(input);
  PH_CHECK(payload.deltaX == 1.5f);
  PH_CHECK(payload.deltaY == -2.5f);
}

PH_TEST("primehost.input", "gamepad button event payload types") {
  Event event{};
  GamepadButtonEvent button{};
  button.controlId = static_cast<uint32_t>(GamepadButtonId::South);
  button.pressed = true;
  event.payload = InputEvent{button};

  PH_CHECK(std::holds_alternative<InputEvent>(event.payload));
  const auto& input = std::get<InputEvent>(event.payload);
  PH_CHECK(std::holds_alternative<GamepadButtonEvent>(input));
  const auto& payload = std::get<GamepadButtonEvent>(input);
  PH_CHECK(payload.controlId == static_cast<uint32_t>(GamepadButtonId::South));
  PH_CHECK(payload.pressed);
}

PH_TEST("primehost.input", "gamepad axis event payload types") {
  Event event{};
  GamepadAxisEvent axis{};
  axis.controlId = static_cast<uint32_t>(GamepadAxisId::LeftX);
  axis.value = 0.25f;
  event.payload = InputEvent{axis};

  PH_CHECK(std::holds_alternative<InputEvent>(event.payload));
  const auto& input = std::get<InputEvent>(event.payload);
  PH_CHECK(std::holds_alternative<GamepadAxisEvent>(input));
  const auto& payload = std::get<GamepadAxisEvent>(input);
  PH_CHECK(payload.controlId == static_cast<uint32_t>(GamepadAxisId::LeftX));
  PH_CHECK(payload.value == 0.25f);
}

PH_TEST("primehost.input", "device event payload types") {
  Event event{};
  DeviceEvent device{};
  device.deviceId = 7u;
  device.connected = false;
  event.payload = InputEvent{device};

  PH_CHECK(std::holds_alternative<InputEvent>(event.payload));
  const auto& input = std::get<InputEvent>(event.payload);
  PH_CHECK(std::holds_alternative<DeviceEvent>(input));
  const auto& payload = std::get<DeviceEvent>(input);
  PH_CHECK(payload.deviceId == 7u);
  PH_CHECK(!payload.connected);
}

TEST_SUITE_END();
