#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.presentation");

PH_TEST("primehost.presentation", "display interval invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto interval = host->displayInterval(SurfaceId{999u});
  PH_CHECK(!interval.has_value());
  if (!interval.has_value()) {
    PH_CHECK(interval.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
