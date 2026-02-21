#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreHaptics/CoreHaptics.h>
#import <GameController/GameController.h>
#import <GameController/GCDualShockGamepad.h>
#import <GameController/GCDualSenseGamepad.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDDevice.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDUsageTables.h>
#import <Metal/Metal.h>
#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CAMetalLayer.h>
#import <dispatch/dispatch.h>

#include "PrimeHost/Host.h"
#include "DeviceNameMatch.h"
#include "FrameConfigValidation.h"
#include "FrameLimiter.h"
#include "GamepadProfiles.h"
#include "TextBuffer.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <string>
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
- (void)displayLinkTick:(CADisplayLink*)link;
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
  std::optional<std::chrono::steady_clock::time_point> lastFrameTime{};
  std::optional<std::chrono::nanoseconds> displayInterval{};
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
  CADisplayLink* viewDisplayLink = nil;
#endif
};

std::optional<GamepadProfile> match_gamepad_profile(uint16_t vendorId, uint16_t productId, std::string_view name) {
  return findGamepadProfile(vendorId, productId, name);
}

uint16_t hid_uint16_property(IOHIDDeviceRef device, CFStringRef key) {
  if (!device || !key) {
    return 0u;
  }
  CFTypeRef value = IOHIDDeviceGetProperty(device, key);
  if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) {
    return 0u;
  }
  int number = 0;
  if (!CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberIntType, &number)) {
    return 0u;
  }
  if (number < 0 || number > 0xFFFF) {
    return 0u;
  }
  return static_cast<uint16_t>(number);
}

std::string hid_string_property(IOHIDDeviceRef device, CFStringRef key) {
  if (!device || !key) {
    return {};
  }
  CFTypeRef value = IOHIDDeviceGetProperty(device, key);
  if (!value || CFGetTypeID(value) != CFStringGetTypeID()) {
    return {};
  }
  CFStringRef string = static_cast<CFStringRef>(value);
  char buffer[256];
  if (CFStringGetCString(string, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
    return std::string(buffer);
  }
  return {};
}

uint64_t hid_device_id(IOHIDDeviceRef device) {
  if (!device) {
    return 0u;
  }
  return static_cast<uint64_t>(CFHash(device));
}

bool hid_usage_matches(IOHIDDeviceRef device, uint32_t usagePage, uint32_t usage) {
  if (!device) {
    return false;
  }
  uint32_t deviceUsagePage = hid_uint16_property(device, CFSTR(kIOHIDPrimaryUsagePageKey));
  uint32_t deviceUsage = hid_uint16_property(device, CFSTR(kIOHIDPrimaryUsageKey));
  return deviceUsagePage == usagePage && deviceUsage == usage;
}

bool hid_is_gamepad_device(IOHIDDeviceRef device) {
  return hid_usage_matches(device, kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad) ||
         hid_usage_matches(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);
}

std::optional<std::pair<uint16_t, uint16_t>> hid_vidpid_match(
    const std::unordered_map<uint64_t, IOHIDDeviceRef>& devices,
    std::string_view name) {
  if (name.empty()) {
    return std::nullopt;
  }
  int bestScore = 0;
  std::optional<std::pair<uint16_t, uint16_t>> bestMatch;
  for (const auto& entry : devices) {
    IOHIDDeviceRef device = entry.second;
    if (!device) {
      continue;
    }
    std::string product = hid_string_property(device, CFSTR(kIOHIDProductKey));
    if (product.empty()) {
      continue;
    }
    int score = deviceNameMatchScore(name, product);
    if (score <= 0) {
      continue;
    }
    uint16_t vendorId = hid_uint16_property(device, CFSTR(kIOHIDVendorIDKey));
    uint16_t productId = hid_uint16_property(device, CFSTR(kIOHIDProductIDKey));
    if (vendorId == 0u && productId == 0u) {
      continue;
    }
    if (score > bestScore) {
      bestScore = score;
      bestMatch = std::pair<uint16_t, uint16_t>{vendorId, productId};
    }
  }
  return bestMatch;
}
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
  HostStatus setRelativePointerCapture(SurfaceId surfaceId, bool enabled) override;

  HostStatus setCallbacks(Callbacks callbacks) override;

  void handleResize(uint64_t surfaceId);
  void handlePointer(uint64_t surfaceId, PointerPhase phase, PointerDeviceType type, NSPoint point, NSEvent* event);
  void handleScroll(uint64_t surfaceId, NSEvent* event);
  void handleKey(NSEvent* event, bool pressed, bool repeat);
  void handleModifiers(NSEvent* event);
  void handleText(uint64_t surfaceId, NSString* text);
  void handleWindowClosed(uint64_t surfaceId);
  void handleDisplayLinkTick();
  void handleDisplayLinkTick(uint64_t surfaceId, CADisplayLink* link);
  void handleGamepadConnected(GCController* controller);
  void handleGamepadDisconnected(GCController* controller);
  void enqueueGamepadButton(uint32_t deviceId, uint32_t controlId, bool pressed, float value);
  void enqueueGamepadAxis(uint32_t deviceId, uint32_t controlId, float value);
  void releaseRelativePointer();
  void handleHidDeviceAttached(IOHIDDeviceRef device);
  void handleHidDeviceRemoved(IOHIDDeviceRef device);

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
  struct DeviceRecord {
    DeviceInfo info;
    DeviceCapabilities caps;
    std::string nameStorage;
  };
  std::unordered_map<uint32_t, DeviceRecord> devices_;
  std::vector<uint32_t> deviceOrder_;
  std::unordered_map<void*, uint32_t> gamepadIds_;
  std::unordered_map<uint32_t, GCController*> gamepadControllers_;
  IOHIDManagerRef hidManager_ = nullptr;
  std::unordered_map<uint64_t, IOHIDDeviceRef> hidDevicesById_;
  std::unordered_map<uint32_t, CHHapticEngine*> hapticsEngines_;
  id gamepadConnectObserver_ = nil;
  id gamepadDisconnectObserver_ = nil;
  std::vector<QueuedEvent> eventQueue_;
  std::vector<Event> callbackEvents_;
  std::vector<char> callbackText_;
  Callbacks callbacks_{};
  uint64_t nextSurfaceId_ = 1u;
  uint32_t nextDeviceId_ = 3u;
  CVDisplayLinkRef cvDisplayLink_ = nullptr;
  std::optional<std::chrono::nanoseconds> displayInterval_{};
  bool relativePointerEnabled_ = false;
  std::optional<SurfaceId> relativePointerSurface_{};
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

static void hid_device_attached(void* context, IOReturn result, void* sender, IOHIDDeviceRef device) {
  (void)result;
  (void)sender;
  auto* host = static_cast<PrimeHost::HostMac*>(context);
  if (!host || !device) {
    return;
  }
  dispatch_async(dispatch_get_main_queue(), ^{
    host->handleHidDeviceAttached(device);
  });
}

static void hid_device_removed(void* context, IOReturn result, void* sender, IOHIDDeviceRef device) {
  (void)result;
  (void)sender;
  auto* host = static_cast<PrimeHost::HostMac*>(context);
  if (!host || !device) {
    return;
  }
  dispatch_async(dispatch_get_main_queue(), ^{
    host->handleHidDeviceRemoved(device);
  });
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

- (void)displayLinkTick:(CADisplayLink*)link {
  if (self.host) {
    self.host->handleDisplayLinkTick(self.surfaceId, link);
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

  DeviceRecord mouse{};
  mouse.info.deviceId = kMouseDeviceId;
  mouse.info.type = DeviceType::Mouse;
  mouse.nameStorage = "Mouse";
  mouse.caps.type = DeviceType::Mouse;
  devices_.emplace(kMouseDeviceId, mouse);
  devices_[kMouseDeviceId].info.name = devices_[kMouseDeviceId].nameStorage;
  deviceOrder_.push_back(kMouseDeviceId);

  DeviceRecord keyboard{};
  keyboard.info.deviceId = kKeyboardDeviceId;
  keyboard.info.type = DeviceType::Keyboard;
  keyboard.nameStorage = "Keyboard";
  keyboard.caps.type = DeviceType::Keyboard;
  devices_.emplace(kKeyboardDeviceId, keyboard);
  devices_[kKeyboardDeviceId].info.name = devices_[kKeyboardDeviceId].nameStorage;
  deviceOrder_.push_back(kKeyboardDeviceId);

  auto now = std::chrono::steady_clock::now();
  for (uint32_t deviceId : deviceOrder_) {
    auto it = devices_.find(deviceId);
    if (it != devices_.end()) {
      Event event{};
      event.scope = Event::Scope::Global;
      event.time = now;
      event.payload = DeviceEvent{.deviceId = deviceId, .deviceType = it->second.info.type, .connected = true};
      enqueueEvent(std::move(event));
    }
  }

  if (@available(macOS 10.15, *)) {
    auto* center = [NSNotificationCenter defaultCenter];
    HostMac* host = this;
    hidManager_ = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (hidManager_) {
      NSDictionary* matchGamepad = @{
        @kIOHIDDeviceUsagePageKey : @(kHIDPage_GenericDesktop),
        @kIOHIDDeviceUsageKey : @(kHIDUsage_GD_GamePad)
      };
      NSDictionary* matchJoystick = @{
        @kIOHIDDeviceUsagePageKey : @(kHIDPage_GenericDesktop),
        @kIOHIDDeviceUsageKey : @(kHIDUsage_GD_Joystick)
      };
      NSArray* matches = @[matchGamepad, matchJoystick];
      IOHIDManagerSetDeviceMatchingMultiple(hidManager_, (__bridge CFArrayRef)matches);
      IOHIDManagerRegisterDeviceMatchingCallback(hidManager_, &hid_device_attached, this);
      IOHIDManagerRegisterDeviceRemovalCallback(hidManager_, &hid_device_removed, this);
      IOHIDManagerScheduleWithRunLoop(hidManager_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
      IOReturn openResult = IOHIDManagerOpen(hidManager_, kIOHIDOptionsTypeNone);
      if (openResult == kIOReturnSuccess) {
        CFSetRef devices = IOHIDManagerCopyDevices(hidManager_);
        if (devices) {
          CFIndex count = CFSetGetCount(devices);
          if (count > 0) {
            std::vector<const void*> deviceValues(static_cast<std::size_t>(count));
            CFSetGetValues(devices, deviceValues.data());
            for (const void* value : deviceValues) {
              IOHIDDeviceRef device = static_cast<IOHIDDeviceRef>(const_cast<void*>(value));
              if (hid_is_gamepad_device(device)) {
                handleHidDeviceAttached(device);
              }
            }
          }
          CFRelease(devices);
        }
      } else {
        IOHIDManagerUnscheduleFromRunLoop(hidManager_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
        CFRelease(hidManager_);
        hidManager_ = nullptr;
      }
    }

    gamepadConnectObserver_ =
        [center addObserverForName:GCControllerDidConnectNotification
                            object:nil
                             queue:[NSOperationQueue mainQueue]
                        usingBlock:^(NSNotification* note) {
                          if (auto* controller = (GCController*)note.object) {
                            host->handleGamepadConnected(controller);
                          }
                        }];
    gamepadDisconnectObserver_ =
        [center addObserverForName:GCControllerDidDisconnectNotification
                            object:nil
                             queue:[NSOperationQueue mainQueue]
                        usingBlock:^(NSNotification* note) {
                          if (auto* controller = (GCController*)note.object) {
                            host->handleGamepadDisconnected(controller);
                          }
                        }];

    for (GCController* controller in [GCController controllers]) {
      handleGamepadConnected(controller);
    }
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CVDisplayLinkCreateWithActiveCGDisplays(&cvDisplayLink_);
  if (cvDisplayLink_) {
    CVDisplayLinkSetOutputCallback(cvDisplayLink_, &display_link_callback, this);
    CVTime refresh = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(cvDisplayLink_);
    if (refresh.timeValue > 0 && refresh.timeScale > 0) {
      double seconds = static_cast<double>(refresh.timeValue) / static_cast<double>(refresh.timeScale);
      displayInterval_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::duration<double>(seconds));
    }
  }
#pragma clang diagnostic pop
}

HostMac::~HostMac() {
  releaseRelativePointer();
  if (gamepadConnectObserver_) {
    [[NSNotificationCenter defaultCenter] removeObserver:gamepadConnectObserver_];
    gamepadConnectObserver_ = nil;
  }
  if (gamepadDisconnectObserver_) {
    [[NSNotificationCenter defaultCenter] removeObserver:gamepadDisconnectObserver_];
    gamepadDisconnectObserver_ = nil;
  }
  if (hidManager_) {
    IOHIDManagerUnscheduleFromRunLoop(hidManager_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    IOHIDManagerClose(hidManager_, kIOHIDOptionsTypeNone);
    CFRelease(hidManager_);
    hidManager_ = nullptr;
  }
  for (auto& entry : hidDevicesById_) {
    if (entry.second) {
      CFRelease(entry.second);
    }
  }
  hidDevicesById_.clear();
  for (auto& entry : hapticsEngines_) {
    if (entry.second) {
      [entry.second stopWithCompletionHandler:nil];
    }
  }
  hapticsEngines_.clear();
  if (cvDisplayLink_) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CVDisplayLinkStop(cvDisplayLink_);
    CVDisplayLinkRelease(cvDisplayLink_);
#pragma clang diagnostic pop
    cvDisplayLink_ = nullptr;
  }
}

HostResult<HostCapabilities> HostMac::hostCapabilities() const {
  HostCapabilities caps{};
  caps.supportsClipboard = false;
  caps.supportsFileDialogs = false;
  caps.supportsRelativePointer = true;
  caps.supportsIme = true;
  if (@available(macOS 11.0, *)) {
    caps.supportsHaptics = true;
  } else {
    caps.supportsHaptics = false;
  }
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
  auto it = devices_.find(deviceId);
  if (it != devices_.end()) {
    return it->second.info;
  }
  return std::unexpected(HostError{HostErrorCode::InvalidDevice});
}

HostResult<DeviceCapabilities> HostMac::deviceCapabilities(uint32_t deviceId) const {
  auto it = devices_.find(deviceId);
  if (it != devices_.end()) {
    return it->second.caps;
  }
  return std::unexpected(HostError{HostErrorCode::InvalidDevice});
}

HostResult<size_t> HostMac::devices(std::span<DeviceInfo> outDevices) const {
  if (outDevices.size() < deviceOrder_.size()) {
    return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
  }
  size_t count = 0u;
  for (uint32_t deviceId : deviceOrder_) {
    auto it = devices_.find(deviceId);
    if (it != devices_.end()) {
      outDevices[count++] = it->second.info;
    }
  }
  return count;
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
  state->lastFrameTime.reset();
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
  if (@available(macOS 14.0, *)) {
    CADisplayLink* link = [view displayLinkWithTarget:view selector:@selector(displayLinkTick:)];
    if (link) {
      [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
      link.paused = YES;
      state->viewDisplayLink = link;
    }
  }
#endif
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
  if (relativePointerEnabled_ && relativePointerSurface_ && relativePointerSurface_->value == surfaceId.value) {
    releaseRelativePointer();
  }
  NSWindow* window = it->second->window;
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
  if (it->second->viewDisplayLink) {
    [it->second->viewDisplayLink invalidate];
    it->second->viewDisplayLink = nil;
  }
#endif
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
  auto* surface = findSurface(surfaceId.value);
  if (!surface) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (!callbacks_.onFrame) {
    auto now = std::chrono::steady_clock::now();
    if (!shouldPresent(surface->frameConfig.framePolicy,
                       bypassCap,
                       surface->frameConfig.frameInterval,
                       surface->lastFrameTime,
                       now)) {
      return {};
    }
    surface->lastFrameTime = now;
    return presentEmptyFrame(*surface);
  }

  auto now = std::chrono::steady_clock::now();
  if (!shouldPresent(surface->frameConfig.framePolicy,
                     bypassCap,
                     surface->frameConfig.frameInterval,
                     surface->lastFrameTime,
                     now)) {
    return {};
  }
  auto delta = surface->lastFrameTime ? now - *surface->lastFrameTime
                                      : std::chrono::steady_clock::duration::zero();
  surface->lastFrameTime = now;

  FrameTiming timing{};
  timing.time = now;
  timing.delta = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
  timing.frameIndex = surface->frameIndex++;

  FrameDiagnostics diag{};
  std::optional<std::chrono::nanoseconds> target = surface->frameConfig.frameInterval;
  if (!target) {
    if (surface->displayInterval) {
      target = surface->displayInterval;
    } else if (displayInterval_) {
      target = displayInterval_;
    }
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
  auto caps = surfaceCapabilities(surfaceId);
  if (!caps) {
    return std::unexpected(caps.error());
  }
  auto status = validateFrameConfig(config, caps.value());
  if (!status) {
    return status;
  }
  surface->frameConfig = config;
  updateDisplayLinkState();
  return {};
}

HostStatus HostMac::setGamepadRumble(const GamepadRumble& rumble) {
  auto controllerIt = gamepadControllers_.find(rumble.deviceId);
  if (controllerIt == gamepadControllers_.end()) {
    return std::unexpected(HostError{HostErrorCode::InvalidDevice});
  }

  if (@available(macOS 11.0, *)) {
    GCController* controller = controllerIt->second;
    if (!controller || !controller.haptics) {
      return std::unexpected(HostError{HostErrorCode::Unsupported});
    }

    auto engineIt = hapticsEngines_.find(rumble.deviceId);
    CHHapticEngine* engine = nil;
    if (engineIt != hapticsEngines_.end()) {
      engine = engineIt->second;
    }
    if (!engine) {
      engine = [controller.haptics createEngineWithLocality:GCHapticsLocalityDefault];
      if (!engine) {
        return std::unexpected(HostError{HostErrorCode::Unsupported});
      }
      engine.playsHapticsOnly = YES;
      [engine startWithCompletionHandler:nil];
      hapticsEngines_[rumble.deviceId] = engine;
    }

    double duration = static_cast<double>(rumble.duration.count()) / 1000.0;
    if (duration <= 0.0) {
      [engine stopWithCompletionHandler:nil];
      return {};
    }

    float intensityValue = std::clamp(std::max(rumble.lowFrequency, rumble.highFrequency), 0.0f, 1.0f);
    float sharpnessValue = std::clamp(rumble.highFrequency, 0.0f, 1.0f);

    CHHapticEventParameter* intensity =
        [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity
                                                      value:intensityValue];
    CHHapticEventParameter* sharpness =
        [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness
                                                      value:sharpnessValue];

    CHHapticEvent* event = [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                                                         parameters:@[intensity, sharpness]
                                                       relativeTime:0
                                                           duration:duration];
    NSError* error = nil;
    CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[event] parameters:@[] error:&error];
    if (!pattern || error) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern error:&error];
    if (!player || error) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    if (![player startAtTime:CHHapticTimeImmediate error:&error] || error) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    return {};
  }

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

HostStatus HostMac::setRelativePointerCapture(SurfaceId surfaceId, bool enabled) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }

  if (enabled) {
    if (relativePointerEnabled_ && relativePointerSurface_ == surfaceId) {
      return {};
    }
    if (relativePointerEnabled_) {
      releaseRelativePointer();
    }
    CGAssociateMouseAndMouseCursorPosition(false);
    CGDisplayHideCursor(kCGDirectMainDisplay);
    relativePointerEnabled_ = true;
    relativePointerSurface_ = surfaceId;
    return {};
  }

  if (!relativePointerEnabled_ || relativePointerSurface_ != surfaceId) {
    return {};
  }

  releaseRelativePointer();
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

void HostMac::handleGamepadConnected(GCController* controller) {
  if (!controller) {
    return;
  }
  void* key = (__bridge void*)controller;
  if (gamepadIds_.find(key) != gamepadIds_.end()) {
    return;
  }

  uint32_t deviceId = nextDeviceId_++;
  gamepadIds_[key] = deviceId;
  gamepadControllers_[deviceId] = controller;

  DeviceRecord record{};
  record.info.deviceId = deviceId;
  record.info.type = DeviceType::Gamepad;

  NSString* name = controller.vendorName;
  if (!name || name.length == 0) {
    name = controller.productCategory;
  }
  if (name && name.length > 0) {
    record.nameStorage = [name UTF8String];
  } else {
    record.nameStorage = "Gamepad";
  }

  std::string_view nameView = record.nameStorage;
  if (auto match = hid_vidpid_match(hidDevicesById_, nameView)) {
    record.info.vendorId = match->first;
    record.info.productId = match->second;
  }

  record.caps.type = DeviceType::Gamepad;
  if (controller.extendedGamepad) {
    record.caps.hasAnalogButtons = true;
  }
  if (controller.microGamepad) {
    record.caps.hasAnalogButtons = true;
  }
  if (auto profile = match_gamepad_profile(record.info.vendorId, record.info.productId, nameView)) {
    record.caps.hasAnalogButtons = record.caps.hasAnalogButtons || profile->hasAnalogButtons;
  }
  if (@available(macOS 11.0, *)) {
    if (controller.haptics) {
      record.caps.hasRumble = true;
    }
  }

  devices_.emplace(deviceId, std::move(record));
  devices_[deviceId].info.name = devices_[deviceId].nameStorage;
  deviceOrder_.push_back(deviceId);

  HostMac* host = this;
  auto dispatch_to_main = ^(dispatch_block_t block) {
    dispatch_async(dispatch_get_main_queue(), block);
  };

  if (controller.extendedGamepad) {
    GCExtendedGamepad* gamepad = controller.extendedGamepad;

    auto button_handler = ^(GCControllerButtonInput* button, uint32_t controlId) {
      if (!button) {
        return;
      }
      button.valueChangedHandler = ^(GCControllerButtonInput* input, float value, BOOL pressed) {
        (void)input;
        dispatch_to_main(^{
          host->enqueueGamepadButton(deviceId, controlId, pressed, value);
        });
      };
    };

    auto dpad_handler = ^(GCControllerDirectionPad* dpad) {
      if (!dpad) {
        return;
      }
      dpad.valueChangedHandler = ^(GCControllerDirectionPad* pad, float xValue, float yValue) {
        (void)xValue;
        (void)yValue;
        dispatch_to_main(^{
          host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::DpadUp), pad.up.isPressed, pad.up.value);
          host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::DpadDown), pad.down.isPressed, pad.down.value);
          host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::DpadLeft), pad.left.isPressed, pad.left.value);
          host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::DpadRight), pad.right.isPressed, pad.right.value);
        });
      };
    };

    button_handler(gamepad.buttonA, static_cast<uint32_t>(GamepadButtonId::South));
    button_handler(gamepad.buttonB, static_cast<uint32_t>(GamepadButtonId::East));
    button_handler(gamepad.buttonX, static_cast<uint32_t>(GamepadButtonId::West));
    button_handler(gamepad.buttonY, static_cast<uint32_t>(GamepadButtonId::North));
    button_handler(gamepad.leftShoulder, static_cast<uint32_t>(GamepadButtonId::LeftBumper));
    button_handler(gamepad.rightShoulder, static_cast<uint32_t>(GamepadButtonId::RightBumper));

    if (@available(macOS 10.15, *)) {
      button_handler(gamepad.buttonMenu, static_cast<uint32_t>(GamepadButtonId::Start));
      if (gamepad.buttonOptions) {
        button_handler(gamepad.buttonOptions, static_cast<uint32_t>(GamepadButtonId::Back));
      }
    }
    if (@available(macOS 11.0, *)) {
      if (gamepad.buttonHome) {
        button_handler(gamepad.buttonHome, static_cast<uint32_t>(GamepadButtonId::Guide));
      }
    }
    if (@available(macOS 10.14.1, *)) {
      if (gamepad.leftThumbstickButton) {
        button_handler(gamepad.leftThumbstickButton, static_cast<uint32_t>(GamepadButtonId::LeftStick));
      }
      if (gamepad.rightThumbstickButton) {
        button_handler(gamepad.rightThumbstickButton, static_cast<uint32_t>(GamepadButtonId::RightStick));
      }
    }

    dpad_handler(gamepad.dpad);

    gamepad.leftThumbstick.valueChangedHandler = ^(GCControllerDirectionPad* pad, float xValue, float yValue) {
      dispatch_to_main(^{
        host->enqueueGamepadAxis(deviceId, static_cast<uint32_t>(GamepadAxisId::LeftX), xValue);
        host->enqueueGamepadAxis(deviceId, static_cast<uint32_t>(GamepadAxisId::LeftY), yValue);
      });
    };

    gamepad.rightThumbstick.valueChangedHandler = ^(GCControllerDirectionPad* pad, float xValue, float yValue) {
      dispatch_to_main(^{
        host->enqueueGamepadAxis(deviceId, static_cast<uint32_t>(GamepadAxisId::RightX), xValue);
        host->enqueueGamepadAxis(deviceId, static_cast<uint32_t>(GamepadAxisId::RightY), yValue);
      });
    };

    gamepad.leftTrigger.valueChangedHandler = ^(GCControllerButtonInput* input, float value, BOOL pressed) {
      (void)input;
      (void)pressed;
      dispatch_to_main(^{
        host->enqueueGamepadAxis(deviceId, static_cast<uint32_t>(GamepadAxisId::LeftTrigger), value);
      });
    };
    gamepad.rightTrigger.valueChangedHandler = ^(GCControllerButtonInput* input, float value, BOOL pressed) {
      (void)input;
      (void)pressed;
      dispatch_to_main(^{
        host->enqueueGamepadAxis(deviceId, static_cast<uint32_t>(GamepadAxisId::RightTrigger), value);
      });
    };

    if (@available(macOS 11.0, *)) {
      if ([gamepad isKindOfClass:[GCDualShockGamepad class]]) {
        auto* dualShock = static_cast<GCDualShockGamepad*>(gamepad);
        button_handler(dualShock.touchpadButton, static_cast<uint32_t>(GamepadButtonId::Misc));
      }
    }
    if (@available(macOS 11.3, *)) {
      if ([gamepad isKindOfClass:[GCDualSenseGamepad class]]) {
        auto* dualSense = static_cast<GCDualSenseGamepad*>(gamepad);
        button_handler(dualSense.touchpadButton, static_cast<uint32_t>(GamepadButtonId::Misc));
      }
    }
  } else if (controller.microGamepad) {
    GCMicroGamepad* gamepad = controller.microGamepad;
    if (gamepad.dpad) {
      gamepad.dpad.valueChangedHandler = ^(GCControllerDirectionPad* pad, float xValue, float yValue) {
        (void)xValue;
        (void)yValue;
        dispatch_to_main(^{
          host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::DpadUp), pad.up.isPressed, pad.up.value);
          host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::DpadDown), pad.down.isPressed, pad.down.value);
          host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::DpadLeft), pad.left.isPressed, pad.left.value);
          host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::DpadRight), pad.right.isPressed, pad.right.value);
        });
      };
    }
    if (gamepad.buttonA) {
      gamepad.buttonA.valueChangedHandler = ^(GCControllerButtonInput* input, float value, BOOL pressed) {
        dispatch_to_main(^{
          host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::South), pressed, value);
        });
      };
    }
    if (gamepad.buttonX) {
      gamepad.buttonX.valueChangedHandler = ^(GCControllerButtonInput* input, float value, BOOL pressed) {
        dispatch_to_main(^{
          host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::West), pressed, value);
        });
      };
    }
    if (@available(macOS 10.15, *)) {
      if (gamepad.buttonMenu) {
        gamepad.buttonMenu.valueChangedHandler = ^(GCControllerButtonInput* input, float value, BOOL pressed) {
          dispatch_to_main(^{
            host->enqueueGamepadButton(deviceId, static_cast<uint32_t>(GamepadButtonId::Start), pressed, value);
          });
        };
      }
    }
  }

  Event event{};
  event.scope = Event::Scope::Global;
  event.time = std::chrono::steady_clock::now();
  event.payload = DeviceEvent{.deviceId = deviceId, .deviceType = DeviceType::Gamepad, .connected = true};
  enqueueEvent(std::move(event));
}

void HostMac::handleGamepadDisconnected(GCController* controller) {
  if (!controller) {
    return;
  }
  void* key = (__bridge void*)controller;
  auto it = gamepadIds_.find(key);
  if (it == gamepadIds_.end()) {
    return;
  }
  uint32_t deviceId = it->second;
  gamepadIds_.erase(it);
  gamepadControllers_.erase(deviceId);
  auto hapticsIt = hapticsEngines_.find(deviceId);
  if (hapticsIt != hapticsEngines_.end()) {
    if (hapticsIt->second) {
      [hapticsIt->second stopWithCompletionHandler:nil];
    }
    hapticsEngines_.erase(hapticsIt);
  }
  devices_.erase(deviceId);
  deviceOrder_.erase(std::remove(deviceOrder_.begin(), deviceOrder_.end(), deviceId), deviceOrder_.end());

  Event event{};
  event.scope = Event::Scope::Global;
  event.time = std::chrono::steady_clock::now();
  event.payload = DeviceEvent{.deviceId = deviceId, .deviceType = DeviceType::Gamepad, .connected = false};
  enqueueEvent(std::move(event));
}

void HostMac::handleHidDeviceAttached(IOHIDDeviceRef device) {
  if (!device) {
    return;
  }
  if (!hid_is_gamepad_device(device)) {
    return;
  }
  uint64_t id = hid_device_id(device);
  if (id == 0u || hidDevicesById_.find(id) != hidDevicesById_.end()) {
    return;
  }
  CFRetain(device);
  hidDevicesById_.emplace(id, device);
}

void HostMac::handleHidDeviceRemoved(IOHIDDeviceRef device) {
  if (!device) {
    return;
  }
  uint64_t id = hid_device_id(device);
  auto it = hidDevicesById_.find(id);
  if (it == hidDevicesById_.end()) {
    return;
  }
  if (it->second) {
    CFRelease(it->second);
  }
  hidDevicesById_.erase(it);
}

void HostMac::enqueueGamepadButton(uint32_t deviceId, uint32_t controlId, bool pressed, float value) {
  GamepadButtonEvent button{};
  button.deviceId = deviceId;
  button.controlId = controlId;
  button.pressed = pressed;
  button.value = value;

  Event event{};
  event.scope = Event::Scope::Global;
  event.time = std::chrono::steady_clock::now();
  event.payload = button;
  enqueueEvent(std::move(event));
}

void HostMac::enqueueGamepadAxis(uint32_t deviceId, uint32_t controlId, float value) {
  GamepadAxisEvent axis{};
  axis.deviceId = deviceId;
  axis.controlId = controlId;
  axis.value = value;

  Event event{};
  event.scope = Event::Scope::Global;
  event.time = std::chrono::steady_clock::now();
  event.payload = axis;
  enqueueEvent(std::move(event));
}

void HostMac::releaseRelativePointer() {
  if (!relativePointerEnabled_) {
    return;
  }
  CGAssociateMouseAndMouseCursorPosition(true);
  CGDisplayShowCursor(kCGDirectMainDisplay);
  relativePointerEnabled_ = false;
  relativePointerSurface_.reset();
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
    if (relativePointerEnabled_ && relativePointerSurface_ && relativePointerSurface_->value == surfaceId) {
      releaseRelativePointer();
    }
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
    if (it->second->viewDisplayLink) {
      [it->second->viewDisplayLink invalidate];
      it->second->viewDisplayLink = nil;
    }
#endif
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
  bool shouldRunCv = false;
  for (const auto& entry : surfaces_) {
    if (!entry.second) {
      continue;
    }
    const bool wantsContinuous = entry.second->frameConfig.framePolicy == FramePolicy::Continuous;
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
    if (entry.second->viewDisplayLink) {
      entry.second->viewDisplayLink.paused = !wantsContinuous;
      continue;
    }
#endif
    if (wantsContinuous) {
      shouldRunCv = true;
    }
  }

  if (!cvDisplayLink_) {
    return;
  }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  if (shouldRunCv && !CVDisplayLinkIsRunning(cvDisplayLink_)) {
    CVDisplayLinkStart(cvDisplayLink_);
  } else if (!shouldRunCv && CVDisplayLinkIsRunning(cvDisplayLink_)) {
    CVDisplayLinkStop(cvDisplayLink_);
  }
#pragma clang diagnostic pop
}

void HostMac::handleDisplayLinkTick() {
  for (auto& entry : surfaces_) {
    if (entry.second && entry.second->frameConfig.framePolicy == FramePolicy::Continuous) {
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
      if (entry.second->viewDisplayLink) {
        continue;
      }
#endif
      requestFrame(entry.second->surfaceId, false);
    }
  }
}

void HostMac::handleDisplayLinkTick(uint64_t surfaceId, CADisplayLink* link) {
  auto* surface = findSurface(surfaceId);
  if (!surface) {
    return;
  }
  if (link) {
    CFTimeInterval duration = link.duration;
    if (duration > 0.0) {
      surface->displayInterval = std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::duration<double>(duration));
    }
  }
  if (surface->frameConfig.framePolicy == FramePolicy::Continuous) {
    requestFrame(surface->surfaceId, false);
  }
}

HostResult<std::unique_ptr<Host>> createHostMac() {
  return std::make_unique<HostMac>();
}

} // namespace PrimeHost
