#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "PrimeHost/Host.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace PrimeHost {
class HostMac;
} // namespace PrimeHost

@class PHWindowDelegate;
@class PHHostView;

@interface PHWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) PrimeHost::HostMac* host;
@property(nonatomic, assign) uint64_t surfaceId;
@end

@interface PHHostView : NSView
@property(nonatomic, assign) PrimeHost::HostMac* host;
@property(nonatomic, assign) uint64_t surfaceId;
@end

namespace PrimeHost {
namespace {

constexpr uint32_t kMouseDeviceId = 1u;
constexpr uint32_t kKeyboardDeviceId = 2u;

struct SurfaceState {
  SurfaceId id{};
  NSWindow* window = nullptr;
  NSView* view = nullptr;
  CAMetalLayer* layer = nullptr;
  PHWindowDelegate* delegate = nullptr;
  FrameConfig frameConfig{};
  uint64_t frameIndex = 0u;
  std::chrono::steady_clock::time_point lastFrameTime{};
};

uint32_t map_modifier_flags(NSEventModifierFlags flags) {
  uint32_t result = 0u;
  if (flags & NSEventModifierFlagShift) result |= static_cast<uint32_t>(KeyModifier::Shift);
  if (flags & NSEventModifierFlagControl) result |= static_cast<uint32_t>(KeyModifier::Control);
  if (flags & NSEventModifierFlagOption) result |= static_cast<uint32_t>(KeyModifier::Alt);
  if (flags & NSEventModifierFlagCommand) result |= static_cast<uint32_t>(KeyModifier::Super);
  if (flags & NSEventModifierFlagCapsLock) result |= static_cast<uint32_t>(KeyModifier::CapsLock);
  if (flags & NSEventModifierFlagNumericPad) result |= static_cast<uint32_t>(KeyModifier::NumLock);
  return result;
}

uint32_t map_mac_keycode(uint16_t keyCode) {
  switch (keyCode) {
    case kVK_ANSI_A: return 0x04;
    case kVK_ANSI_B: return 0x05;
    case kVK_ANSI_C: return 0x06;
    case kVK_ANSI_D: return 0x07;
    case kVK_ANSI_E: return 0x08;
    case kVK_ANSI_F: return 0x09;
    case kVK_ANSI_G: return 0x0A;
    case kVK_ANSI_H: return 0x0B;
    case kVK_ANSI_I: return 0x0C;
    case kVK_ANSI_J: return 0x0D;
    case kVK_ANSI_K: return 0x0E;
    case kVK_ANSI_L: return 0x0F;
    case kVK_ANSI_M: return 0x10;
    case kVK_ANSI_N: return 0x11;
    case kVK_ANSI_O: return 0x12;
    case kVK_ANSI_P: return 0x13;
    case kVK_ANSI_Q: return 0x14;
    case kVK_ANSI_R: return 0x15;
    case kVK_ANSI_S: return 0x16;
    case kVK_ANSI_T: return 0x17;
    case kVK_ANSI_U: return 0x18;
    case kVK_ANSI_V: return 0x19;
    case kVK_ANSI_W: return 0x1A;
    case kVK_ANSI_X: return 0x1B;
    case kVK_ANSI_Y: return 0x1C;
    case kVK_ANSI_Z: return 0x1D;

    case kVK_ANSI_1: return 0x1E;
    case kVK_ANSI_2: return 0x1F;
    case kVK_ANSI_3: return 0x20;
    case kVK_ANSI_4: return 0x21;
    case kVK_ANSI_5: return 0x22;
    case kVK_ANSI_6: return 0x23;
    case kVK_ANSI_7: return 0x24;
    case kVK_ANSI_8: return 0x25;
    case kVK_ANSI_9: return 0x26;
    case kVK_ANSI_0: return 0x27;

    case kVK_Return: return 0x28;
    case kVK_Escape: return 0x29;
    case kVK_Delete: return 0x2A;
    case kVK_Tab: return 0x2B;
    case kVK_Space: return 0x2C;
    case kVK_ANSI_Minus: return 0x2D;
    case kVK_ANSI_Equal: return 0x2E;
    case kVK_ANSI_LeftBracket: return 0x2F;
    case kVK_ANSI_RightBracket: return 0x30;
    case kVK_ANSI_Backslash: return 0x31;
    case kVK_ANSI_Semicolon: return 0x33;
    case kVK_ANSI_Quote: return 0x34;
    case kVK_ANSI_Grave: return 0x35;
    case kVK_ANSI_Comma: return 0x36;
    case kVK_ANSI_Period: return 0x37;
    case kVK_ANSI_Slash: return 0x38;
    case kVK_CapsLock: return 0x39;

    case kVK_F1: return 0x3A;
    case kVK_F2: return 0x3B;
    case kVK_F3: return 0x3C;
    case kVK_F4: return 0x3D;
    case kVK_F5: return 0x3E;
    case kVK_F6: return 0x3F;
    case kVK_F7: return 0x40;
    case kVK_F8: return 0x41;
    case kVK_F9: return 0x42;
    case kVK_F10: return 0x43;
    case kVK_F11: return 0x44;
    case kVK_F12: return 0x45;
    case kVK_F13: return 0x68;
    case kVK_F14: return 0x69;
    case kVK_F15: return 0x6A;

    case kVK_Home: return 0x4A;
    case kVK_PageUp: return 0x4B;
    case kVK_ForwardDelete: return 0x4C;
    case kVK_End: return 0x4D;
    case kVK_PageDown: return 0x4E;
    case kVK_RightArrow: return 0x4F;
    case kVK_LeftArrow: return 0x50;
    case kVK_DownArrow: return 0x51;
    case kVK_UpArrow: return 0x52;

    case kVK_ANSI_Keypad0: return 0x62;
    case kVK_ANSI_Keypad1: return 0x59;
    case kVK_ANSI_Keypad2: return 0x5A;
    case kVK_ANSI_Keypad3: return 0x5B;
    case kVK_ANSI_Keypad4: return 0x5C;
    case kVK_ANSI_Keypad5: return 0x5D;
    case kVK_ANSI_Keypad6: return 0x5E;
    case kVK_ANSI_Keypad7: return 0x5F;
    case kVK_ANSI_Keypad8: return 0x60;
    case kVK_ANSI_Keypad9: return 0x61;
    case kVK_ANSI_KeypadClear: return 0x53;
    case kVK_ANSI_KeypadDecimal: return 0x63;
    case kVK_ANSI_KeypadDivide: return 0x54;
    case kVK_ANSI_KeypadMultiply: return 0x55;
    case kVK_ANSI_KeypadMinus: return 0x56;
    case kVK_ANSI_KeypadPlus: return 0x57;
    case kVK_ANSI_KeypadEnter: return 0x58;

    case kVK_Control: return 0xE0;
    case kVK_Shift: return 0xE1;
    case kVK_Option: return 0xE2;
    case kVK_Command: return 0xE3;
    case kVK_RightControl: return 0xE4;
    case kVK_RightShift: return 0xE5;
    case kVK_RightOption: return 0xE6;
    case kVK_RightCommand: return 0xE7;
    default: return 0u;
  }
}

uint32_t map_mouse_buttons(NSUInteger pressedMask) {
  uint32_t result = 0u;
  if (pressedMask & (1u << 0u)) result |= 1u << 0u; // left
  if (pressedMask & (1u << 1u)) result |= 1u << 1u; // right
  if (pressedMask & (1u << 2u)) result |= 1u << 2u; // middle
  if (pressedMask & (1u << 3u)) result |= 1u << 3u; // back
  if (pressedMask & (1u << 4u)) result |= 1u << 4u; // forward
  return result;
}

} // namespace

class HostMac : public Host {
public:
  HostMac();

  HostResult<HostCapabilities> hostCapabilities() const override;
  HostResult<SurfaceCapabilities> surfaceCapabilities(SurfaceId surfaceId) const override;
  HostResult<DeviceInfo> deviceInfo(uint32_t deviceId) const override;
  HostResult<DeviceCapabilities> deviceCapabilities(uint32_t deviceId) const override;
  HostResult<size_t> devices(std::span<DeviceInfo> outDevices) const override;

  HostResult<SurfaceId> createSurface(const SurfaceConfig& config) override;
  HostStatus destroySurface(SurfaceId surfaceId) override;

  HostResult<EventBatch> pollEvents(const EventBuffer& buffer) override;
  HostStatus waitEvents() override;

  HostStatus requestFrame(SurfaceId surfaceId, bool bypassCap) override;
  HostStatus setFrameConfig(SurfaceId surfaceId, const FrameConfig& config) override;

  HostStatus setGamepadRumble(const GamepadRumble& rumble) override;

  HostStatus setCallbacks(Callbacks callbacks) override;

  void handleResize(uint64_t surfaceId);
  void handlePointer(uint64_t surfaceId, PointerPhase phase, PointerDeviceType type, NSPoint point, NSEvent* event);
  void handleScroll(uint64_t surfaceId, NSEvent* event);
  void handleKey(NSEvent* event, bool pressed, bool repeat);
  void handleModifiers(NSEvent* event);
  void handleWindowClosed(uint64_t surfaceId);

private:
  void enqueueEvent(Event event);
  void pumpEvents(bool wait);
  SurfaceState* findSurface(uint64_t surfaceId);

  NSApplication* app_ = nullptr;
  std::unordered_map<uint64_t, SurfaceState> surfaces_;
  std::vector<Event> eventQueue_;
  Callbacks callbacks_{};
  uint64_t nextSurfaceId_ = 1u;
};

} // namespace PrimeHost

@implementation PHWindowDelegate
- (void)windowDidResize:(NSNotification*)notification {
  if (self.host) {
    self.host->handleResize(self.surfaceId);
  }
}

- (void)windowWillClose:(NSNotification*)notification {
  if (self.host) {
    self.host->handleWindowClosed(self.surfaceId);
  }
}
@end

@implementation PHHostView {
  NSTrackingArea* trackingArea_;
}

- (BOOL)isFlipped {
  return YES;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)updateTrackingAreas {
  if (trackingArea_) {
    [self removeTrackingArea:trackingArea_];
    trackingArea_ = nullptr;
  }
  NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect;
  trackingArea_ = [[NSTrackingArea alloc] initWithRect:NSZeroRect
                                              options:options
                                                owner:self
                                             userInfo:nil];
  [self addTrackingArea:trackingArea_];
  [super updateTrackingAreas];
}

- (void)mouseDown:(NSEvent*)event {
  if (self.host) {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    self.host->handlePointer(self.surfaceId, PrimeHost::PointerPhase::Down, PrimeHost::PointerDeviceType::Mouse, p, event);
  }
}

- (void)mouseUp:(NSEvent*)event {
  if (self.host) {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    self.host->handlePointer(self.surfaceId, PrimeHost::PointerPhase::Up, PrimeHost::PointerDeviceType::Mouse, p, event);
  }
}

- (void)mouseMoved:(NSEvent*)event {
  if (self.host) {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    self.host->handlePointer(self.surfaceId, PrimeHost::PointerPhase::Move, PrimeHost::PointerDeviceType::Mouse, p, event);
  }
}

- (void)mouseDragged:(NSEvent*)event {
  if (self.host) {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    self.host->handlePointer(self.surfaceId, PrimeHost::PointerPhase::Move, PrimeHost::PointerDeviceType::Mouse, p, event);
  }
}

- (void)rightMouseDown:(NSEvent*)event {
  if (self.host) {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    self.host->handlePointer(self.surfaceId, PrimeHost::PointerPhase::Down, PrimeHost::PointerDeviceType::Mouse, p, event);
  }
}

- (void)rightMouseUp:(NSEvent*)event {
  if (self.host) {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    self.host->handlePointer(self.surfaceId, PrimeHost::PointerPhase::Up, PrimeHost::PointerDeviceType::Mouse, p, event);
  }
}

- (void)otherMouseDown:(NSEvent*)event {
  if (self.host) {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    self.host->handlePointer(self.surfaceId, PrimeHost::PointerPhase::Down, PrimeHost::PointerDeviceType::Mouse, p, event);
  }
}

- (void)otherMouseUp:(NSEvent*)event {
  if (self.host) {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    self.host->handlePointer(self.surfaceId, PrimeHost::PointerPhase::Up, PrimeHost::PointerDeviceType::Mouse, p, event);
  }
}

- (void)scrollWheel:(NSEvent*)event {
  if (self.host) {
    self.host->handleScroll(self.surfaceId, event);
  }
}

- (void)keyDown:(NSEvent*)event {
  if (self.host) {
    self.host->handleKey(event, true, event.isARepeat);
  }
}

- (void)keyUp:(NSEvent*)event {
  if (self.host) {
    self.host->handleKey(event, false, false);
  }
}

- (void)flagsChanged:(NSEvent*)event {
  if (self.host) {
    self.host->handleModifiers(event);
  }
}
@end

namespace PrimeHost {

HostMac::HostMac() {
  app_ = [NSApplication sharedApplication];
  [app_ setActivationPolicy:NSApplicationActivationPolicyRegular];
  [app_ finishLaunching];
}

HostResult<HostCapabilities> HostMac::hostCapabilities() const {
  HostCapabilities caps{};
  caps.supportsClipboard = false;
  caps.supportsFileDialogs = false;
  caps.supportsRelativePointer = false;
  caps.supportsIme = false;
  caps.supportsHaptics = false;
  caps.supportsHeadless = false;
  return caps;
}

HostResult<SurfaceCapabilities> HostMac::surfaceCapabilities(SurfaceId surfaceId) const {
  if (surfaces_.find(surfaceId.value) == surfaces_.end()) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  SurfaceCapabilities caps{};
  caps.supportsVsyncToggle = false;
  caps.supportsTearing = false;
  caps.minBufferCount = 2u;
  caps.maxBufferCount = 3u;
  caps.presentModes =
      (1u << static_cast<uint32_t>(PresentMode::LowLatency)) |
      (1u << static_cast<uint32_t>(PresentMode::Smooth)) |
      (1u << static_cast<uint32_t>(PresentMode::Uncapped));
  caps.colorFormats = (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM));
  return caps;
}

HostResult<DeviceInfo> HostMac::deviceInfo(uint32_t deviceId) const {
  DeviceInfo info{};
  if (deviceId == kMouseDeviceId) {
    info.deviceId = deviceId;
    info.type = DeviceType::Mouse;
    info.name = "Mouse";
    return info;
  }
  if (deviceId == kKeyboardDeviceId) {
    info.deviceId = deviceId;
    info.type = DeviceType::Keyboard;
    info.name = "Keyboard";
    return info;
  }
  return std::unexpected(HostError{HostErrorCode::InvalidDevice});
}

HostResult<DeviceCapabilities> HostMac::deviceCapabilities(uint32_t deviceId) const {
  DeviceCapabilities caps{};
  if (deviceId == kMouseDeviceId) {
    caps.type = DeviceType::Mouse;
    return caps;
  }
  if (deviceId == kKeyboardDeviceId) {
    caps.type = DeviceType::Keyboard;
    return caps;
  }
  return std::unexpected(HostError{HostErrorCode::InvalidDevice});
}

HostResult<size_t> HostMac::devices(std::span<DeviceInfo> outDevices) const {
  constexpr size_t kDeviceCount = 2u;
  if (outDevices.size() < kDeviceCount) {
    return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
  }
  outDevices[0] = DeviceInfo{.deviceId = kMouseDeviceId, .type = DeviceType::Mouse, .vendorId = 0u, .productId = 0u, .name = "Mouse"};
  outDevices[1] = DeviceInfo{.deviceId = kKeyboardDeviceId, .type = DeviceType::Keyboard, .vendorId = 0u, .productId = 0u, .name = "Keyboard"};
  return kDeviceCount;
}

HostResult<SurfaceId> HostMac::createSurface(const SurfaceConfig& config) {
  SurfaceId id{nextSurfaceId_++};

  NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
  if (config.resizable) {
    styleMask |= NSWindowStyleMaskResizable;
  }

  NSRect rect = NSMakeRect(0, 0, static_cast<CGFloat>(config.width), static_cast<CGFloat>(config.height));
  NSWindow* window = [[NSWindow alloc] initWithContentRect:rect
                                                 styleMask:styleMask
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
  if (!window) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }

  if (config.title) {
    window.title = [NSString stringWithUTF8String:config.title->c_str()];
  }

  PHHostView* view = [[PHHostView alloc] initWithFrame:rect];
  view.host = this;
  view.surfaceId = id.value;
  view.wantsLayer = YES;
  view.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
  view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

  CAMetalLayer* layer = [CAMetalLayer layer];
  layer.device = MTLCreateSystemDefaultDevice();
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  layer.framebufferOnly = YES;
  layer.contentsScale = window.backingScaleFactor;
  layer.drawableSize = CGSizeMake(rect.size.width * layer.contentsScale,
                                  rect.size.height * layer.contentsScale);
  view.layer = layer;

  window.contentView = view;
  window.acceptsMouseMovedEvents = YES;

  PHWindowDelegate* delegate = [[PHWindowDelegate alloc] init];
  delegate.host = this;
  delegate.surfaceId = id.value;
  window.delegate = delegate;

  [window makeKeyAndOrderFront:nil];
  [app_ activateIgnoringOtherApps:YES];

  SurfaceState state{};
  state.id = id;
  state.window = window;
  state.view = view;
  state.layer = layer;
  state.delegate = delegate;
  state.lastFrameTime = std::chrono::steady_clock::now();
  surfaces_.emplace(id.value, state);

  [window makeFirstResponder:view];

  Event created{};
  created.scope = Event::Scope::Surface;
  created.surfaceId = id;
  created.time = std::chrono::steady_clock::now();
  created.payload = LifecycleEvent{LifecyclePhase::Created};
  enqueueEvent(std::move(created));

  return id;
}

HostStatus HostMac::destroySurface(SurfaceId surfaceId) {
  auto it = surfaces_.find(surfaceId.value);
  if (it == surfaces_.end()) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  [it->second.window close];
  surfaces_.erase(it);
  return {};
}

HostResult<EventBatch> HostMac::pollEvents(const EventBuffer& buffer) {
  if (buffer.events.data() == nullptr) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  pumpEvents(false);

  size_t count = std::min(buffer.events.size(), eventQueue_.size());
  for (size_t i = 0; i < count; ++i) {
    buffer.events[i] = eventQueue_[i];
  }
  eventQueue_.erase(eventQueue_.begin(), eventQueue_.begin() + static_cast<long>(count));

  EventBatch batch{
      std::span<const Event>(buffer.events.data(), count),
      std::span<const char>(buffer.textBytes.data(), 0),
  };
  return batch;
}

HostStatus HostMac::waitEvents() {
  pumpEvents(true);
  return {};
}

HostStatus HostMac::requestFrame(SurfaceId surfaceId, bool bypassCap) {
  (void)bypassCap;
  auto* surface = findSurface(surfaceId.value);
  if (!surface) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (!callbacks_.onFrame) {
    return {};
  }

  auto now = std::chrono::steady_clock::now();
  auto delta = now - surface->lastFrameTime;
  surface->lastFrameTime = now;

  FrameTiming timing{};
  timing.time = now;
  timing.delta = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
  timing.frameIndex = surface->frameIndex++;

  FrameDiagnostics diag{};
  if (surface->frameConfig.frameInterval) {
    diag.targetInterval = *surface->frameConfig.frameInterval;
    diag.actualInterval = timing.delta;
    diag.missedDeadline = timing.delta > diag.targetInterval;
  }

  callbacks_.onFrame(surfaceId, timing, diag);
  return {};
}

HostStatus HostMac::setFrameConfig(SurfaceId surfaceId, const FrameConfig& config) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  surface->frameConfig = config;
  return {};
}

HostStatus HostMac::setGamepadRumble(const GamepadRumble& rumble) {
  (void)rumble;
  return std::unexpected(HostError{HostErrorCode::Unsupported});
}

HostStatus HostMac::setCallbacks(Callbacks callbacks) {
  callbacks_ = std::move(callbacks);
  return {};
}

void HostMac::handleResize(uint64_t surfaceId) {
  auto* surface = findSurface(surfaceId);
  if (!surface || !surface->window) {
    return;
  }

  NSView* view = surface->window.contentView;
  if (surface->layer && view) {
    CGFloat scale = surface->window.backingScaleFactor;
    NSRect bounds = view.bounds;
    surface->layer.contentsScale = scale;
    surface->layer.drawableSize = CGSizeMake(bounds.size.width * scale, bounds.size.height * scale);

    ResizeEvent resize{};
    resize.width = static_cast<uint32_t>(bounds.size.width);
    resize.height = static_cast<uint32_t>(bounds.size.height);
    resize.scale = static_cast<float>(scale);

    Event event{};
    event.scope = Event::Scope::Surface;
    event.surfaceId = SurfaceId{surfaceId};
    event.time = std::chrono::steady_clock::now();
    event.payload = resize;
    enqueueEvent(std::move(event));
  }
}

void HostMac::handlePointer(uint64_t surfaceId, PointerPhase phase, PointerDeviceType type, NSPoint point, NSEvent* event) {
  PointerEvent pointer{};
  pointer.deviceId = kMouseDeviceId;
  pointer.pointerId = 0u;
  pointer.deviceType = type;
  pointer.phase = phase;
  pointer.x = static_cast<int32_t>(std::lround(point.x));
  pointer.y = static_cast<int32_t>(std::lround(point.y));
  if (phase == PrimeHost::PointerPhase::Move) {
    pointer.deltaX = static_cast<int32_t>(std::lround(event.deltaX));
    pointer.deltaY = static_cast<int32_t>(std::lround(event.deltaY));
  }
  pointer.buttonMask = map_mouse_buttons([NSEvent pressedMouseButtons]);
  pointer.isPrimary = true;

  Event evt{};
  evt.scope = Event::Scope::Surface;
  evt.surfaceId = SurfaceId{surfaceId};
  evt.time = std::chrono::steady_clock::now();
  evt.payload = pointer;
  enqueueEvent(std::move(evt));
}

void HostMac::handleScroll(uint64_t surfaceId, NSEvent* event) {
  ScrollEvent scroll{};
  scroll.deviceId = kMouseDeviceId;
  scroll.deltaX = static_cast<float>(event.scrollingDeltaX);
  scroll.deltaY = static_cast<float>(event.scrollingDeltaY);
  scroll.isLines = !event.hasPreciseScrollingDeltas;

  Event evt{};
  evt.scope = Event::Scope::Surface;
  evt.surfaceId = SurfaceId{surfaceId};
  evt.time = std::chrono::steady_clock::now();
  evt.payload = scroll;
  enqueueEvent(std::move(evt));
}

void HostMac::handleKey(NSEvent* event, bool pressed, bool repeat) {
  uint32_t hid = map_mac_keycode(static_cast<uint16_t>(event.keyCode));
  if (hid == 0u) {
    return;
  }

  KeyEvent key{};
  key.deviceId = kKeyboardDeviceId;
  key.keyCode = hid;
  key.modifiers = static_cast<KeyModifierMask>(map_modifier_flags(event.modifierFlags));
  key.pressed = pressed;
  key.repeat = repeat;

  Event evt{};
  evt.scope = Event::Scope::Global;
  evt.surfaceId.reset();
  evt.time = std::chrono::steady_clock::now();
  evt.payload = key;
  enqueueEvent(std::move(evt));
}

void HostMac::handleModifiers(NSEvent* event) {
  uint32_t hid = map_mac_keycode(static_cast<uint16_t>(event.keyCode));
  if (hid == 0u) {
    return;
  }
  bool pressed = false;
  switch (event.keyCode) {
    case kVK_Shift:
    case kVK_RightShift:
      pressed = (event.modifierFlags & NSEventModifierFlagShift) != 0;
      break;
    case kVK_Control:
    case kVK_RightControl:
      pressed = (event.modifierFlags & NSEventModifierFlagControl) != 0;
      break;
    case kVK_Option:
    case kVK_RightOption:
      pressed = (event.modifierFlags & NSEventModifierFlagOption) != 0;
      break;
    case kVK_Command:
    case kVK_RightCommand:
      pressed = (event.modifierFlags & NSEventModifierFlagCommand) != 0;
      break;
    case kVK_CapsLock:
      pressed = (event.modifierFlags & NSEventModifierFlagCapsLock) != 0;
      break;
    default:
      break;
  }

  KeyEvent key{};
  key.deviceId = kKeyboardDeviceId;
  key.keyCode = hid;
  key.modifiers = static_cast<KeyModifierMask>(map_modifier_flags(event.modifierFlags));
  key.pressed = pressed;
  key.repeat = false;

  Event evt{};
  evt.scope = Event::Scope::Global;
  evt.surfaceId.reset();
  evt.time = std::chrono::steady_clock::now();
  evt.payload = key;
  enqueueEvent(std::move(evt));
}

void HostMac::handleWindowClosed(uint64_t surfaceId) {
  Event evt{};
  evt.scope = Event::Scope::Surface;
  evt.surfaceId = SurfaceId{surfaceId};
  evt.time = std::chrono::steady_clock::now();
  evt.payload = LifecycleEvent{LifecyclePhase::Destroyed};
  enqueueEvent(std::move(evt));

  auto it = surfaces_.find(surfaceId);
  if (it != surfaces_.end()) {
    surfaces_.erase(it);
  }
}

void HostMac::enqueueEvent(Event event) {
  if (callbacks_.onEvents) {
    Event local = event;
    std::array<Event, 1> events{local};
    EventBatch batch{
        std::span<const Event>(events.data(), events.size()),
        std::span<const char>(),
    };
    callbacks_.onEvents(batch);
    return;
  }
  eventQueue_.push_back(std::move(event));
}

void HostMac::pumpEvents(bool wait) {
  @autoreleasepool {
    NSDate* until = wait ? [NSDate distantFuture] : [NSDate dateWithTimeIntervalSinceNow:0];
    while (true) {
      NSEvent* event = [app_ nextEventMatchingMask:NSEventMaskAny
                                         untilDate:until
                                            inMode:NSDefaultRunLoopMode
                                           dequeue:YES];
      if (!event) {
        break;
      }
      [app_ sendEvent:event];
      [app_ updateWindows];
      if (wait) {
        break;
      }
    }
  }
}

SurfaceState* HostMac::findSurface(uint64_t surfaceId) {
  auto it = surfaces_.find(surfaceId);
  if (it == surfaces_.end()) {
    return nullptr;
  }
  return &it->second;
}

HostResult<std::unique_ptr<Host>> createHostMac() {
  return std::make_unique<HostMac>();
}

} // namespace PrimeHost
