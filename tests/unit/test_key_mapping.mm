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

static void send_key_event(NSWindow* window,
                           uint16_t keyCode,
                           NSEventModifierFlags modifiers,
                           NSString* characters,
                           NSString* charactersIgnoringModifiers) {
  NSEvent* keyEvent =
      [NSEvent keyEventWithType:NSEventTypeKeyDown
                       location:NSMakePoint(0, 0)
                  modifierFlags:modifiers
                      timestamp:0
                   windowNumber:window.windowNumber
                        context:nil
                     characters:characters
    charactersIgnoringModifiers:charactersIgnoringModifiers
                      isARepeat:NO
                        keyCode:keyCode];
  PH_REQUIRE(keyEvent != nil);
  [NSApp sendEvent:keyEvent];
  [NSApp updateWindows];
}

static uint32_t wait_for_keycode(std::atomic<uint32_t>& keyCode) {
  for (int i = 0; i < 50 && keyCode.load() == 0u; ++i) {
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.002]];
  }
  return keyCode.load();
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
  send_key_event(window,
                 kVK_LeftArrow,
                 NSEventModifierFlagFunction,
                 leftString,
                 leftString);

  PH_CHECK(wait_for_keycode(keyCode) == 0x50u);

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());

  auto clearStatus = host->setCallbacks({});
  PH_CHECK(clearStatus.has_value());
}

PH_TEST("primehost.keymap", "fn+arrow keys do not map to home/end/page") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  const std::string windowTitle = "PrimeHost Test Fn Arrow";
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

  unichar upChar = NSUpArrowFunctionKey;
  NSString* upString = [NSString stringWithCharacters:&upChar length:1];
  send_key_event(window,
                 kVK_UpArrow,
                 NSEventModifierFlagFunction,
                 upString,
                 upString);
  PH_CHECK(wait_for_keycode(keyCode) == 0x52u);

  keyCode.store(0u);
  unichar downChar = NSDownArrowFunctionKey;
  NSString* downString = [NSString stringWithCharacters:&downChar length:1];
  send_key_event(window,
                 kVK_DownArrow,
                 NSEventModifierFlagFunction,
                 downString,
                 downString);
  PH_CHECK(wait_for_keycode(keyCode) == 0x51u);

  keyCode.store(0u);
  unichar rightChar = NSRightArrowFunctionKey;
  NSString* rightString = [NSString stringWithCharacters:&rightChar length:1];
  send_key_event(window,
                 kVK_RightArrow,
                 NSEventModifierFlagFunction,
                 rightString,
                 rightString);
  PH_CHECK(wait_for_keycode(keyCode) == 0x4Fu);

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());

  auto clearStatus = host->setCallbacks({});
  PH_CHECK(clearStatus.has_value());
}

PH_TEST("primehost.keymap", "fn+home/end/page remap to navigation keys") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 64u;
  const std::string windowTitle = "PrimeHost Test Fn Nav";
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

  unichar homeChar = NSHomeFunctionKey;
  NSString* homeString = [NSString stringWithCharacters:&homeChar length:1];
  send_key_event(window,
                 kVK_LeftArrow,
                 NSEventModifierFlagFunction,
                 homeString,
                 homeString);
  PH_CHECK(wait_for_keycode(keyCode) == 0x4Au);

  keyCode.store(0u);
  unichar endChar = NSEndFunctionKey;
  NSString* endString = [NSString stringWithCharacters:&endChar length:1];
  send_key_event(window,
                 kVK_RightArrow,
                 NSEventModifierFlagFunction,
                 endString,
                 endString);
  PH_CHECK(wait_for_keycode(keyCode) == 0x4Du);

  keyCode.store(0u);
  unichar pageUpChar = NSPageUpFunctionKey;
  NSString* pageUpString = [NSString stringWithCharacters:&pageUpChar length:1];
  send_key_event(window,
                 kVK_UpArrow,
                 NSEventModifierFlagFunction,
                 pageUpString,
                 pageUpString);
  PH_CHECK(wait_for_keycode(keyCode) == 0x4Bu);

  keyCode.store(0u);
  unichar pageDownChar = NSPageDownFunctionKey;
  NSString* pageDownString = [NSString stringWithCharacters:&pageDownChar length:1];
  send_key_event(window,
                 kVK_DownArrow,
                 NSEventModifierFlagFunction,
                 pageDownString,
                 pageDownString);
  PH_CHECK(wait_for_keycode(keyCode) == 0x4Eu);

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());

  auto clearStatus = host->setCallbacks({});
  PH_CHECK(clearStatus.has_value());
}
