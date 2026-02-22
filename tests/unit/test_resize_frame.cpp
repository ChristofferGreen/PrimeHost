#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <atomic>
#include <vector>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frame");

PH_TEST("primehost.frame", "resize triggers frame in callback mode") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  std::atomic<uint32_t> frameCount{0};
  std::atomic<uint32_t> resizeCount{0};
  Callbacks callbacks{};
  callbacks.onFrame = [&](SurfaceId id,
                          const FrameTiming&,
                          const FrameDiagnostics&) {
    if (id == surface.value()) {
      frameCount.fetch_add(1u);
    }
  };
  callbacks.onEvents = [&](const EventBatch& batch) {
    for (const auto& event : batch.events) {
      if (event.surfaceId && *event.surfaceId == surface.value() &&
          std::holds_alternative<ResizeEvent>(event.payload)) {
        resizeCount.fetch_add(1u);
      }
    }
  };
  auto callbackStatus = host->setCallbacks(callbacks);
  PH_CHECK(callbackStatus.has_value());

  FrameConfig frameConfig{};
  frameConfig.framePolicy = FramePolicy::EventDriven;
  auto frameStatus = host->setFrameConfig(surface.value(), frameConfig);
  PH_CHECK(frameStatus.has_value());

  auto resizeStatus = host->setSurfaceSize(surface.value(), 96u, 96u);
  PH_CHECK(resizeStatus.has_value());

  std::vector<Event> events(16u);
  std::vector<char> text(256u);
  EventBuffer buffer{};
  buffer.events = events;
  buffer.textBytes = text;

  for (int i = 0; i < 50 && frameCount.load() == 0u; ++i) {
    auto batch = host->pollEvents(buffer);
    if (!batch.has_value()) {
      bool allowed = batch.error().code == HostErrorCode::BufferTooSmall;
      allowed = allowed || batch.error().code == HostErrorCode::PlatformFailure;
      PH_CHECK(allowed);
      break;
    }
    sleepFor(std::chrono::milliseconds(2));
  }

  PH_CHECK(resizeCount.load() > 0u);
  PH_CHECK(frameCount.load() > 0u);

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());

  auto clearStatus = host->setCallbacks({});
  PH_CHECK(clearStatus.has_value());
}

TEST_SUITE_END();
