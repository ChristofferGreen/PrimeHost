#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <variant>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.drop");

PH_TEST("primehost.drop", "event payload types") {
  Event event{};
  event.payload = DropEvent{.count = 1u, .paths = TextSpan{0u, 4u}};
  PH_CHECK(std::holds_alternative<DropEvent>(event.payload));
}

TEST_SUITE_END();
