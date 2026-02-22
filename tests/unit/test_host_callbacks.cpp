#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.callbacks");

PH_TEST("primehost.callbacks", "set callbacks") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  Callbacks callbacks{};
  bool called = false;
  callbacks.onEvents = [&](const EventBatch& batch) {
    called = true;
    PH_CHECK(batch.events.size() == 1u);
  };

  auto status = host->setCallbacks(callbacks);
  PH_CHECK(status.has_value());

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

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
  PH_CHECK(called);

  auto clearStatus = host->setCallbacks({});
  PH_CHECK(clearStatus.has_value());
}

TEST_SUITE_END();
