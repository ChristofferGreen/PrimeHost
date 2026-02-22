#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <variant>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.power");

PH_TEST("primehost.power", "event payload types") {
  Event event{};
  event.payload = PowerEvent{.lowPowerModeEnabled = true};
  PH_CHECK(std::holds_alternative<PowerEvent>(event.payload));

  event.payload = ThermalEvent{.state = ThermalState::Serious};
  PH_CHECK(std::holds_alternative<ThermalEvent>(event.payload));
}

TEST_SUITE_END();
