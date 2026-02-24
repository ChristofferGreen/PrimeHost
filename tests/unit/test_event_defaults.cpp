#include "PrimeHost/Host.h"

#include "tests/unit/test_helpers.h"

#include <variant>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.event.defaults");

PH_TEST("primehost.event.defaults", "event default payload and time") {
  Event event{};
  PH_CHECK(event.scope == Event::Scope::Surface);
  PH_CHECK(!event.surfaceId.has_value());
  PH_CHECK(event.time.time_since_epoch() == std::chrono::steady_clock::duration::zero());
  PH_CHECK(std::holds_alternative<InputEvent>(event.payload));
  const auto& input = std::get<InputEvent>(event.payload);
  PH_CHECK(std::holds_alternative<PointerEvent>(input));
  const auto& pointer = std::get<PointerEvent>(input);
  PH_CHECK(pointer.phase == PointerPhase::Move);
}

PH_TEST("primehost.event.defaults", "event default pointer values") {
  Event event{};
  const auto& input = std::get<InputEvent>(event.payload);
  const auto& pointer = std::get<PointerEvent>(input);
  PH_CHECK(pointer.deviceType == PointerDeviceType::Mouse);
  PH_CHECK(pointer.x == 0);
  PH_CHECK(pointer.y == 0);
  PH_CHECK(pointer.buttonMask == 0u);
  PH_CHECK(pointer.isPrimary);
}

PH_TEST("primehost.event.defaults", "lifecycle event defaults") {
  LifecycleEvent lifecycle{};
  PH_CHECK(lifecycle.phase == LifecyclePhase::Created);
}

PH_TEST("primehost.event.defaults", "thermal event defaults") {
  ThermalEvent thermal{};
  PH_CHECK(thermal.state == ThermalState::Unknown);
}

PH_TEST("primehost.event.defaults", "focus event defaults") {
  FocusEvent focus{};
  PH_CHECK(!focus.focused);
}

PH_TEST("primehost.event.defaults", "power event defaults") {
  PowerEvent power{};
  PH_CHECK(!power.lowPowerModeEnabled.has_value());
}

PH_TEST("primehost.event.defaults", "event scope unaffected by surface id") {
  Event event{};
  event.surfaceId = SurfaceId{42u};
  PH_CHECK(event.scope == Event::Scope::Surface);
  PH_CHECK(event.surfaceId.has_value());
  PH_CHECK(event.surfaceId->value == 42u);
}

PH_TEST("primehost.event.defaults", "resize event defaults") {
  ResizeEvent resize{};
  PH_CHECK(resize.width == 0u);
  PH_CHECK(resize.height == 0u);
  PH_CHECK(resize.scale == 1.0f);
}

PH_TEST("primehost.event.defaults", "drop event defaults") {
  DropEvent drop{};
  PH_CHECK(drop.count == 0u);
  PH_CHECK(drop.paths.offset == 0u);
  PH_CHECK(drop.paths.length == 0u);
}

TEST_SUITE_END();
