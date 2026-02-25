#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <atomic>

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

TEST_SUITE_BEGIN("primehost.keymap");

PH_TEST("primehost.keymap", "fn+left arrow remains left arrow") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  const std::string windowTitle = "PrimeHost Test Key Mapping";
  config.title = windowTitle;
  auto surface = host->createSurface(config);
  if (!surface.has_value()) {
    bool allowed = surface.error().code == HostErrorCode::Unsupported;
    allowed = allowed || surface.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  std::atomic<uint32_t> keyCode{0u};
  Callbacks callbacks{};
  callbacks.onEvents = [&](const EventBatch& batch) {
    for (const auto& event : batch.events) {
      if (!std::holds_alternative<InputEvent>(event.payload)) {
        continue;
      }
      const auto& input = std::get<InputEvent>(event.payload);
      if (std::holds_alternative<KeyEvent>(input)) {
        const auto& payload = std::get<KeyEvent>(input);
        keyCode.store(payload.keyCode);
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
  PH_REQUIRE(window != nil);

  unichar leftChar = NSLeftArrowFunctionKey;
  NSString* leftString = [NSString stringWithCharacters:&leftChar length:1];
  NSEvent* keyEvent =
      [NSEvent keyEventWithType:NSEventTypeKeyDown
                       location:NSMakePoint(0, 0)
                  modifierFlags:NSEventModifierFlagFunction
                      timestamp:0
                   windowNumber:window.windowNumber
                        context:nil
                     characters:leftString
    charactersIgnoringModifiers:leftString
                      isARepeat:NO
                        keyCode:kVK_LeftArrow];
  PH_REQUIRE(keyEvent != nil);
  [NSApp sendEvent:keyEvent];
  [NSApp updateWindows];

  for (int i = 0; i < 50 && keyCode.load() == 0u; ++i) {
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.002]];
  }

  PH_CHECK(keyCode.load() == 0x50u);

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());

  auto clearStatus = host->setCallbacks({});
  PH_CHECK(clearStatus.has_value());
}
