#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.headless");

PH_TEST("primehost.headless", "create and size") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 32u;
  config.headless = true;

  auto surface = host->createSurface(config);
  if (!surface) {
    PH_CHECK(surface.error().code == HostErrorCode::Unsupported);
    return;
  }

  auto size = host->surfaceSize(surface.value());
  PH_CHECK(size.has_value());
  if (size) {
    PH_CHECK(size->width == 64u);
    PH_CHECK(size->height == 32u);
  }

  auto status = host->setSurfaceSize(surface.value(), 128u, 256u);
  PH_CHECK(status.has_value());
  if (status) {
    auto updated = host->surfaceSize(surface.value());
    PH_CHECK(updated.has_value());
    if (updated) {
      PH_CHECK(updated->width == 128u);
      PH_CHECK(updated->height == 256u);
    }
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
