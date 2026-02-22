#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.smoke");

PH_TEST("primehost.smoke", "version constant") {
  PH_CHECK(PrimeHostVersion == 1u);
}

PH_TEST("primehost.smoke", "host capabilities") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());
  auto caps = host->hostCapabilities();
  PH_CHECK(caps.has_value());
  if (!caps) {
    return;
  }
  PH_CHECK(caps->supportsRelativePointer);
  PH_CHECK(caps->supportsClipboard);

  SurfaceId invalidSurface{999u};
  auto status = host->setRelativePointerCapture(invalidSurface, true);
  PH_CHECK(!status.has_value());
  PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
}

TEST_SUITE_END();
