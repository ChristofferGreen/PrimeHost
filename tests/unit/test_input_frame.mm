#import <Cocoa/Cocoa.h>

#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <atomic>
#include <vector>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.frame");

PH_TEST("primehost.frame", "key input triggers frame in callback mode") {
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
  std::atomic<uint32_t> keyCount{0};
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
      if (std::holds_alternative<InputEvent>(event.payload)) {
        const auto& input = std::get<InputEvent>(event.payload);
        if (std::holds_alternative<KeyEvent>(input)) {
          keyCount.fetch_add(1u);
        }
      }
    }
  };
  auto callbackStatus = host->setCallbacks(callbacks);
  PH_CHECK(callbackStatus.has_value());

  for (int i = 0; i < 10; ++i) {
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.002]];
  }
  NSWindow* window = [NSApp keyWindow];
  if (!window) {
    NSArray<NSWindow*>* windows = [NSApp windows];
    if (windows.count > 0) {
      window = windows.firstObject;
    }
  }
  PH_REQUIRE(window != nil);
  NSEvent* keyEvent = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                      location:NSMakePoint(0, 0)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:window.windowNumber
                                       context:nil
                                    characters:@"a"
                   charactersIgnoringModifiers:@"a"
                                     isARepeat:NO
                                       keyCode:0];
  PH_REQUIRE(keyEvent != nil);
  [NSApp sendEvent:keyEvent];
  [NSApp updateWindows];

  for (int i = 0; i < 20 && frameCount.load() == 0u; ++i) {
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.002]];
  }

  PH_CHECK(keyCount.load() > 0u);
  PH_CHECK(frameCount.load() > 0u);

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());

  auto clearStatus = host->setCallbacks({});
  PH_CHECK(clearStatus.has_value());
}

TEST_SUITE_END();
