#import <Cocoa/Cocoa.h>

#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <atomic>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frame");

PH_TEST("primehost.frame", "focus triggers frame in callback mode") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::atomic<uint64_t> focusSurface{0u};
  std::atomic<bool> sawFocus{false};
  std::atomic<bool> sawFrameForFocus{false};

  Callbacks callbacks{};
  callbacks.onEvents = [&](const EventBatch& batch) {
    for (const auto& event : batch.events) {
      if (std::holds_alternative<FocusEvent>(event.payload) && event.surfaceId) {
        focusSurface.store(event.surfaceId->value);
        sawFocus.store(true);
      }
    }
  };
  callbacks.onFrame = [&](SurfaceId id,
                          const FrameTiming&,
                          const FrameDiagnostics&) {
    if (id.value == focusSurface.load()) {
      sawFrameForFocus.store(true);
    }
  };
  auto callbackStatus = host->setCallbacks(callbacks);
  PH_CHECK(callbackStatus.has_value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  auto surfaceA = host->createSurface(config);
  if (!surfaceA.has_value()) {
    bool allowed = surfaceA.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surfaceA.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }
  auto surfaceB = host->createSurface(config);
  if (!surfaceB.has_value()) {
    bool allowed = surfaceB.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surfaceB.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    host->destroySurface(surfaceA.value());
    return;
  }

  NSArray<NSWindow*>* windows = [NSApp windows];
  for (NSWindow* window in windows) {
    if (!window) {
      continue;
    }
    [window makeKeyAndOrderFront:nil];
    for (int i = 0; i < 10; ++i) {
      [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.002]];
    }
  }

  if (sawFocus.load()) {
    PH_CHECK(sawFrameForFocus.load());
  }

  auto destroyedB = host->destroySurface(surfaceB.value());
  PH_CHECK(destroyedB.has_value());
  auto destroyedA = host->destroySurface(surfaceA.value());
  PH_CHECK(destroyedA.has_value());

  auto clearStatus = host->setCallbacks({});
  PH_CHECK(clearStatus.has_value());
}

TEST_SUITE_END();
