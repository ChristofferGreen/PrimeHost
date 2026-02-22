#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.lifecycle");

PH_TEST("primehost.lifecycle", "headless destroy emits destroyed") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 32u;
  config.height = 32u;
  config.headless = true;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  std::array<Event, 32> events{};
  std::array<char, 256> text{};
  EventBuffer buffer{events, text};

  for (;;) {
    auto batch = host->pollEvents(buffer);
    PH_REQUIRE(batch.has_value());
    if (batch->events.empty()) {
      break;
    }
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());

  bool sawDestroyed = false;
  for (;;) {
    auto batch = host->pollEvents(buffer);
    PH_REQUIRE(batch.has_value());
    for (const auto& evt : batch->events) {
      auto* lifecycle = std::get_if<LifecycleEvent>(&evt.payload);
      if (!lifecycle) {
        continue;
      }
      if (lifecycle->phase == LifecyclePhase::Destroyed &&
          evt.surfaceId && evt.surfaceId->value == surface.value().value) {
        sawDestroyed = true;
        break;
      }
    }
    if (sawDestroyed || batch->events.empty()) {
      break;
    }
  }

  PH_CHECK(sawDestroyed);
}

TEST_SUITE_END();
