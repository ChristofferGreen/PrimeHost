#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.cursor");

PH_TEST("primehost.cursor", "cursor visibility invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto status = host->setCursorVisible(SurfaceId{777u}, false);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
