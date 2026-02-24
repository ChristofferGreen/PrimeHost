#import <Cocoa/Cocoa.h>

#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <atomic>
#include <vector>

using namespace PrimeHost;

static NSWindow* find_window_by_title(NSString* title) {
  if (!title) {
    return nil;
  }
  NSArray<NSWindow*>* windows = [NSApp windows];
  for (NSWindow* window in windows) {
    if ([window.title isEqualToString:title]) {
      return window;
    }
  }
  return nil;
}

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
  const std::string windowTitle = "PrimeHost Test Key Input";
  config.title = windowTitle;
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

  FrameConfig frameConfig{};
  frameConfig.framePolicy = FramePolicy::EventDriven;
  auto frameStatus = host->setFrameConfig(surface.value(), frameConfig);
  PH_CHECK(frameStatus.has_value());

  for (int i = 0; i < 10; ++i) {
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.002]];
  }
  NSString* nsTitle = [NSString stringWithUTF8String:windowTitle.c_str()];
  NSWindow* window = find_window_by_title(nsTitle);
  if (!window) {
    window = [NSApp keyWindow];
  }
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

PH_TEST("primehost.frame", "pointer input triggers frame in callback mode") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  const std::string windowTitle = "PrimeHost Test Pointer Input";
  config.title = windowTitle;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  std::atomic<uint32_t> frameCount{0};
  std::atomic<uint32_t> pointerCount{0};
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
      if (!std::holds_alternative<InputEvent>(event.payload)) {
        continue;
      }
      const auto& input = std::get<InputEvent>(event.payload);
      if (std::holds_alternative<PointerEvent>(input)) {
        pointerCount.fetch_add(1u);
      }
    }
  };
  auto callbackStatus = host->setCallbacks(callbacks);
  PH_CHECK(callbackStatus.has_value());

  FrameConfig frameConfig{};
  frameConfig.framePolicy = FramePolicy::EventDriven;
  auto frameStatus = host->setFrameConfig(surface.value(), frameConfig);
  PH_CHECK(frameStatus.has_value());

  for (int i = 0; i < 10; ++i) {
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.002]];
  }
  NSString* nsTitle = [NSString stringWithUTF8String:windowTitle.c_str()];
  NSWindow* window = find_window_by_title(nsTitle);
  if (!window) {
    window = [NSApp keyWindow];
  }
  if (!window) {
    NSArray<NSWindow*>* windows = [NSApp windows];
    if (windows.count > 0) {
      window = windows.firstObject;
    }
  }
  PH_REQUIRE(window != nil);

  window.acceptsMouseMovedEvents = YES;
  NSView* view = window.contentView;
  PH_REQUIRE(view != nil);
  NSPoint location = NSMakePoint(10, 10);
  NSEvent* moveEvent = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                          location:location
                                     modifierFlags:0
                                         timestamp:0
                                      windowNumber:window.windowNumber
                                           context:nil
                                       eventNumber:0
                                        clickCount:0
                                          pressure:0.0];
  PH_REQUIRE(moveEvent != nil);
  if ([view respondsToSelector:@selector(mouseMoved:)]) {
    [(id)view mouseMoved:moveEvent];
  } else {
    PH_CHECK(false);
  }

  for (int i = 0; i < 20 && pointerCount.load() == 0u; ++i) {
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.002]];
  }

  PH_CHECK(pointerCount.load() > 0u);
  PH_CHECK(frameCount.load() > 0u);

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());

  auto clearStatus = host->setCallbacks({});
  PH_CHECK(clearStatus.has_value());
}

TEST_SUITE_END();
