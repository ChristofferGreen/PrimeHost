#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <variant>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.focus");

PH_TEST("primehost.focus", "event payload types") {
  Event event{};
  event.payload = FocusEvent{.focused = true};
  PH_CHECK(std::holds_alternative<FocusEvent>(event.payload));
}

TEST_SUITE_END();
