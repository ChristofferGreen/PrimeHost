#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <string>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surface");

PH_TEST("primehost.surface", "set title invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto status = host->setSurfaceTitle(SurfaceId{4242u}, "Test Title");
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.surface", "create surface invalid title") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  std::string badTitle;
  badTitle.push_back(static_cast<char>(0xFF));
  config.title = badTitle;
  auto surface = host->createSurface(config);
  PH_CHECK(!surface.has_value());
  if (!surface.has_value()) {
    PH_CHECK(surface.error().code == HostErrorCode::InvalidConfig);
  }
}

TEST_SUITE_END();
