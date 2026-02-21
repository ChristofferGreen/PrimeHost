#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <CoreVideo/CoreVideo.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <dispatch/dispatch.h>

#include "PrimeHost/Host.h"
#include "TextBuffer.h"

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
<NSTextInputClient>
@property(nonatomic, assign) PrimeHost::HostMac* host;
@property(nonatomic, assign) uint64_t surfaceId;
- (void)setImeRect:(NSRect)rect;
@end

namespace PrimeHost {
namespace {

constexpr uint32_t kMouseDeviceId = 1u;
constexpr uint32_t kKeyboardDeviceId = 2u;

struct SurfaceState {
  SurfaceId surfaceId{};
  NSWindow* window = nullptr;
  NSView* view = nullptr;
  CAMetalLayer* layer = nullptr;
  PHWindowDelegate* delegate = nullptr;
  id commandQueue = nil;
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
  ~HostMac() override;

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
  HostStatus setImeCompositionRect(SurfaceId surfaceId,
                                   int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height) override;

  HostStatus setCallbacks(Callbacks callbacks) override;

  void handleResize(uint64_t surfaceId);
  void handlePointer(uint64_t surfaceId, PointerPhase phase, PointerDeviceType type, NSPoint point, NSEvent* event);
  void handleScroll(uint64_t surfaceId, NSEvent* event);
  void handleKey(NSEvent* event, bool pressed, bool repeat);
  void handleModifiers(NSEvent* event);
  void handleText(uint64_t surfaceId, NSString* text);
  void handleWindowClosed(uint64_t surfaceId);
  void handleDisplayLinkTick();

private:
  struct QueuedEvent {
    Event event;
    std::string text;
  };

  HostStatus presentEmptyFrame(SurfaceState& surface);
  HostResult<EventBatch> buildBatch(std::span<const QueuedEvent> events, const EventBuffer& buffer);
  void enqueueEvent(Event event, std::string text = {});
  void pumpEvents(bool wait);
  SurfaceState* findSurface(uint64_t surfaceId);
  void updateDisplayLinkState();

  NSApplication* app_ = nullptr;
  std::unordered_map<uint64_t, std::unique_ptr<SurfaceState>> surfaces_;
  std::vector<QueuedEvent> eventQueue_;
  std::vector<Event> callbackEvents_;
  std::vector<char> callbackText_;
  Callbacks callbacks_{};
  uint64_t nextSurfaceId_ = 1u;
  CVDisplayLinkRef displayLink_ = nullptr;
  std::optional<std::chrono::nanoseconds> displayInterval_{};
};

} // namespace PrimeHost

static CVReturn display_link_callback(CVDisplayLinkRef displayLink,
                                      const CVTimeStamp* now,
                                      const CVTimeStamp* outputTime,
                                      CVOptionFlags flagsIn,
                                      CVOptionFlags* flagsOut,
                                      void* context) {
  (void)displayLink;
  (void)now;
  (void)outputTime;
  (void)flagsIn;
  (void)flagsOut;
  auto* host = static_cast<PrimeHost::HostMac*>(context);
  if (!host) {
    return kCVReturnSuccess;
  }
  dispatch_async(dispatch_get_main_queue(), ^{
    host->handleDisplayLinkTick();
  });
  return kCVReturnSuccess;
}

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
  NSMutableAttributedString* markedText_;
  NSRange markedRange_;
  NSRange selectedRange_;
  NSRect imeRect_;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    markedRange_ = NSMakeRange(NSNotFound, 0);
    selectedRange_ = NSMakeRange(0, 0);
    imeRect_ = NSMakeRect(0, 0, 0, 0);
  }
  return self;
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
  [self interpretKeyEvents:@[event]];
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

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
  if (self.host) {
    NSString* text = nil;
    if ([string isKindOfClass:[NSAttributedString class]]) {
      text = [(NSAttributedString*)string string];
    } else if ([string isKindOfClass:[NSString class]]) {
      text = (NSString*)string;
    }
    if (text) {
      self.host->handleText(self.surfaceId, text);
    }
  }
  [self unmarkText];
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
  if ([string isKindOfClass:[NSAttributedString class]]) {
    markedText_ = [[NSMutableAttributedString alloc] initWithAttributedString:string];
  } else if ([string isKindOfClass:[NSString class]]) {
    markedText_ = [[NSMutableAttributedString alloc] initWithString:string];
  } else {
    markedText_ = nil;
  }
  if (markedText_) {
    markedRange_ = NSMakeRange(0, markedText_.length);
    selectedRange_ = selectedRange;
  } else {
    markedRange_ = NSMakeRange(NSNotFound, 0);
    selectedRange_ = NSMakeRange(0, 0);
  }
}

- (void)unmarkText {
  markedText_ = nil;
  markedRange_ = NSMakeRange(NSNotFound, 0);
}

- (BOOL)hasMarkedText {
  return markedRange_.location != NSNotFound && markedRange_.length > 0;
}

- (NSRange)markedRange {
  return markedRange_;
}

- (NSRange)selectedRange {
  return selectedRange_;
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText {
  return @[];
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:(NSRangePointer)actualRange {
  if (actualRange) {
    *actualRange = NSMakeRange(NSNotFound, 0);
  }
  return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
  return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
  if (actualRange) {
    *actualRange = NSMakeRange(0, 0);
  }
  return imeRect_;
}

- (void)doCommandBySelector:(SEL)selector {
  (void)selector;
}

- (void)setImeRect:(NSRect)rect {
  imeRect_ = rect;
}
@end

namespace PrimeHost {

HostMac::HostMac() {
  app_ = [NSApplication sharedApplication];
  [app_ setActivationPolicy:NSApplicationActivationPolicyRegular];
  [app_ finishLaunching];

  CVDisplayLinkCreateWithActiveCGDisplays(&displayLink_);
  if (displayLink_) {
    CVDisplayLinkSetOutputCallback(displayLink_, &display_link_callback, this);
    CVTime refresh = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(displayLink_);
    if (refresh.timeValue > 0 && refresh.timeScale > 0) {
      double seconds = static_cast<double>(refresh.timeValue) / static_cast<double>(refresh.timeScale);
      displayInterval_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::duration<double>(seconds));
    }
  }
}

HostMac::~HostMac() {
  if (displayLink_) {
    CVDisplayLinkStop(displayLink_);
    CVDisplayLinkRelease(displayLink_);
    displayLink_ = nullptr;
  }
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
  SurfaceId surfaceId{nextSurfaceId_++};

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
  view.surfaceId = surfaceId.value;
  view.wantsLayer = YES;
  view.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
  view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

  CAMetalLayer* layer = [CAMetalLayer layer];
  id device = MTLCreateSystemDefaultDevice();
  layer.device = device;
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
  delegate.surfaceId = surfaceId.value;
  window.delegate = delegate;

  [window makeKeyAndOrderFront:nil];
  [app_ activateIgnoringOtherApps:YES];

  auto state = std::make_unique<SurfaceState>();
  state->surfaceId = surfaceId;
  state->window = window;
  state->view = view;
  state->layer = layer;
  state->commandQueue = [device newCommandQueue];
  state->delegate = delegate;
  state->lastFrameTime = std::chrono::steady_clock::now();
  surfaces_.emplace(surfaceId.value, std::move(state));

  [window makeFirstResponder:view];

  Event created{};
  created.scope = Event::Scope::Surface;
  created.surfaceId = surfaceId;
  created.time = std::chrono::steady_clock::now();
  created.payload = LifecycleEvent{LifecyclePhase::Created};
  enqueueEvent(std::move(created));

  updateDisplayLinkState();
  return surfaceId;
}

HostStatus HostMac::destroySurface(SurfaceId surfaceId) {
  auto it = surfaces_.find(surfaceId.value);
  if (it == surfaces_.end()) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  NSWindow* window = it->second->window;
  it->second->window = nil;
  it->second->view = nil;
  it->second->layer = nil;
  it->second->commandQueue = nil;
  surfaces_.erase(it);
  if (window) {
    [window close];
  }
  updateDisplayLinkState();
  return {};
}

HostResult<EventBatch> HostMac::pollEvents(const EventBuffer& buffer) {
  if (buffer.events.data() == nullptr) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  pumpEvents(false);

  auto batch = buildBatch(std::span<const QueuedEvent>(eventQueue_.data(), eventQueue_.size()), buffer);
  if (!batch) {
    return std::unexpected(batch.error());
  }

  size_t consumed = batch->events.size();
  eventQueue_.erase(eventQueue_.begin(), eventQueue_.begin() + static_cast<long>(consumed));
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
    return presentEmptyFrame(*surface);
  }

  auto now = std::chrono::steady_clock::now();
  auto delta = now - surface->lastFrameTime;
  surface->lastFrameTime = now;

  FrameTiming timing{};
  timing.time = now;
  timing.delta = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
  timing.frameIndex = surface->frameIndex++;

  FrameDiagnostics diag{};
  std::optional<std::chrono::nanoseconds> target = surface->frameConfig.frameInterval;
  if (!target && displayInterval_) {
    target = displayInterval_;
  }
  if (target) {
    diag.targetInterval = *target;
    diag.actualInterval = timing.delta;
    diag.missedDeadline = timing.delta > diag.targetInterval;
  }
  diag.wasThrottled = surface->frameConfig.framePolicy == FramePolicy::Capped;

  callbacks_.onFrame(surfaceId, timing, diag);
  return {};
}

HostStatus HostMac::setFrameConfig(SurfaceId surfaceId, const FrameConfig& config) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  surface->frameConfig = config;
  updateDisplayLinkState();
  return {};
}

HostStatus HostMac::setGamepadRumble(const GamepadRumble& rumble) {
  (void)rumble;
  return std::unexpected(HostError{HostErrorCode::Unsupported});
}

HostStatus HostMac::setImeCompositionRect(SurfaceId surfaceId,
                                          int32_t x,
                                          int32_t y,
                                          int32_t width,
                                          int32_t height) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->view) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  NSRect rect = NSMakeRect(static_cast<CGFloat>(x),
                           static_cast<CGFloat>(y),
                           static_cast<CGFloat>(width),
                           static_cast<CGFloat>(height));
  if ([surface->view isKindOfClass:[PHHostView class]]) {
    [(PHHostView*)surface->view setImeRect:rect];
    [[surface->view inputContext] invalidateCharacterCoordinates];
  }
  return {};
}

HostStatus HostMac::setCallbacks(Callbacks callbacks) {
  callbacks_ = std::move(callbacks);
  updateDisplayLinkState();
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

void HostMac::handleText(uint64_t surfaceId, NSString* text) {
  if (!text) {
    return;
  }
  NSData* data = [text dataUsingEncoding:NSUTF8StringEncoding];
  if (!data || data.length == 0) {
    return;
  }
  std::string utf8(static_cast<const char*>(data.bytes), data.length);

  TextEvent textEvent{};
  textEvent.deviceId = kKeyboardDeviceId;

  Event evt{};
  evt.scope = Event::Scope::Surface;
  evt.surfaceId = SurfaceId{surfaceId};
  evt.time = std::chrono::steady_clock::now();
  evt.payload = textEvent;
  enqueueEvent(std::move(evt), std::move(utf8));
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
  updateDisplayLinkState();
}

HostStatus HostMac::presentEmptyFrame(SurfaceState& surface) {
  if (!surface.layer || !surface.commandQueue) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  @autoreleasepool {
    id<CAMetalDrawable> drawable = [surface.layer nextDrawable];
    if (!drawable) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    id<MTLCommandBuffer> commandBuffer = [surface.commandQueue commandBuffer];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
  }
  return {};
}

HostResult<EventBatch> HostMac::buildBatch(std::span<const QueuedEvent> events, const EventBuffer& buffer) {
  size_t count = std::min(buffer.events.size(), events.size());
  TextBufferWriter writer{buffer.textBytes, 0u};
  for (size_t i = 0; i < count; ++i) {
    Event out = events[i].event;
    if (auto* input = std::get_if<InputEvent>(&out.payload)) {
      if (auto* text = std::get_if<TextEvent>(input)) {
        auto span = writer.append(events[i].text);
        if (!span) {
          return std::unexpected(span.error());
        }
        text->text = span.value();
      }
    }
    buffer.events[i] = out;
  }

  EventBatch batch{
      std::span<const Event>(buffer.events.data(), count),
      std::span<const char>(buffer.textBytes.data(), writer.offset),
  };
  return batch;
}

void HostMac::enqueueEvent(Event event, std::string text) {
  QueuedEvent queued{std::move(event), std::move(text)};
  if (callbacks_.onEvents) {
    callbackEvents_.clear();
    callbackText_.clear();
    callbackEvents_.resize(1);
    EventBuffer buffer{
        std::span<Event>(callbackEvents_.data(), callbackEvents_.size()),
        std::span<char>(),
    };
    if (!queued.text.empty()) {
      callbackText_.resize(queued.text.size());
      buffer.textBytes = std::span<char>(callbackText_.data(), callbackText_.size());
    }
    auto batch = buildBatch(std::span<const QueuedEvent>(&queued, 1), buffer);
    if (batch) {
      callbacks_.onEvents(*batch);
    }
    return;
  }
  eventQueue_.push_back(std::move(queued));
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
  return it->second.get();
}

void HostMac::updateDisplayLinkState() {
  if (!displayLink_) {
    return;
  }
  bool shouldRun = false;
  for (const auto& entry : surfaces_) {
    if (entry.second && entry.second->frameConfig.framePolicy == FramePolicy::Continuous) {
      shouldRun = true;
      break;
    }
  }
  if (shouldRun && !CVDisplayLinkIsRunning(displayLink_)) {
    CVDisplayLinkStart(displayLink_);
  } else if (!shouldRun && CVDisplayLinkIsRunning(displayLink_)) {
    CVDisplayLinkStop(displayLink_);
  }
}

void HostMac::handleDisplayLinkTick() {
  for (auto& entry : surfaces_) {
    if (entry.second && entry.second->frameConfig.framePolicy == FramePolicy::Continuous) {
      requestFrame(entry.second->surfaceId, false);
    }
  }
}

HostResult<std::unique_ptr<Host>> createHostMac() {
  return std::make_unique<HostMac>();
}

} // namespace PrimeHost
