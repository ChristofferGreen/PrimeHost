#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frameconfig");

PH_TEST("primehost.frameconfig", "query config for invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto config = host->frameConfig(SurfaceId{12345u});
  PH_CHECK(!config.has_value());
  if (!config.has_value()) {
    PH_CHECK(config.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
