#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <vector>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frame");

PH_TEST("primehost.frame", "request frame invalid surface") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto status = host->requestFrame(SurfaceId{777u}, false);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    PH_CHECK(status.error().code == HostErrorCode::InvalidSurface);
  }
}

PH_TEST("primehost.frame", "request frame headless") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  config.headless = true;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  auto status = host->requestFrame(surface.value(), false);
  PH_CHECK(status.has_value());

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

PH_TEST("primehost.frame", "request frame increments frame index") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  config.headless = true;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  std::vector<uint64_t> indices;
  Callbacks callbacks{};
  callbacks.onFrame = [&indices, surface](SurfaceId id,
                                          const FrameTiming& timing,
                                          const FrameDiagnostics&) {
    if (id == surface.value()) {
      indices.push_back(timing.frameIndex);
    }
  };
  auto callbackStatus = host->setCallbacks(callbacks);
  PH_CHECK(callbackStatus.has_value());

  auto status0 = host->requestFrame(surface.value(), false);
  PH_CHECK(status0.has_value());
  auto status1 = host->requestFrame(surface.value(), false);
  PH_CHECK(status1.has_value());

  PH_CHECK(indices.size() >= 2u);
  if (indices.size() >= 2u) {
    PH_CHECK(indices[0] == 0u);
    PH_CHECK(indices[1] == 1u);
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
