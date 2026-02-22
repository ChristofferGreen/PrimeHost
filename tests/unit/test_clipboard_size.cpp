#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.clipboard");

PH_TEST("primehost.clipboard", "clipboard size query") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto size = host->clipboardTextSize();
  PH_CHECK(size.has_value());
}

TEST_SUITE_END();
