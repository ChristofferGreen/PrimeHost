#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVCaptureDevice.h>
#import <AVFoundation/AVMediaFormat.h>
#import <Carbon/Carbon.h>
#import <CoreLocation/CoreLocation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreHaptics/CoreHaptics.h>
#import <GameController/GameController.h>
#import <GameController/GCDualShockGamepad.h>
#import <GameController/GCDualSenseGamepad.h>
#import <ImageIO/ImageIO.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDDevice.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <Metal/Metal.h>
#import <Photos/Photos.h>
#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CAMetalLayer.h>
#import <UniformTypeIdentifiers/UTType.h>
#import <UserNotifications/UserNotifications.h>
#import <dispatch/dispatch.h>

#include "PrimeHost/Host.h"
#include "DeviceNameMatch.h"
#include "PrimeHost/FrameConfigValidation.h"
#include "PrimeHost/FrameConfigUtil.h"
#include "PrimeHost/FrameConfigDefaults.h"
#include "FrameDiagnosticsUtil.h"
#include "FrameLimiter.h"
#include "GamepadProfiles.h"
#include "TextBuffer.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace PrimeHost {
class HostMac;
} // namespace PrimeHost

@class PHWindowDelegate;
@class PHHostView;
@class PHLocationDelegate;

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

using WindowImageFn = CGImageRef (*)(CGRect, CGWindowListOption, CGWindowID, CGWindowImageOption);

WindowImageFn window_image_fn() {
  static WindowImageFn fn =
      reinterpret_cast<WindowImageFn>(dlsym(RTLD_DEFAULT, "CGWindowListCreateImage"));
  return fn;
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

std::string cfstring_to_utf8(CFStringRef value) {
  if (!value) {
    return {};
  }
  CFIndex length = CFStringGetLength(value);
  CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
  if (maxSize <= 0) {
    return {};
  }
  std::string buffer(static_cast<size_t>(maxSize), '\0');
  if (!CFStringGetCString(value, buffer.data(), static_cast<CFIndex>(buffer.size()), kCFStringEncodingUTF8)) {
    return {};
  }
  buffer.resize(std::strlen(buffer.c_str()));
  return buffer;
}

HostResult<NSString*> app_path_for_type(AppPathType type) {
  if (type == AppPathType::Temp) {
    NSString* temp = NSTemporaryDirectory();
    if (!temp) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    return temp;
  }
  NSSearchPathDirectory directory = NSApplicationSupportDirectory;
  bool appendPreferences = false;
  bool appendLogs = false;
  switch (type) {
    case AppPathType::UserData:
      directory = NSApplicationSupportDirectory;
      break;
    case AppPathType::Cache:
      directory = NSCachesDirectory;
      break;
    case AppPathType::Config:
      directory = NSLibraryDirectory;
      appendPreferences = true;
      break;
    case AppPathType::Logs:
      directory = NSLibraryDirectory;
      appendLogs = true;
      break;
    default:
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  NSArray<NSString*>* paths = NSSearchPathForDirectoriesInDomains(directory, NSUserDomainMask, YES);
  if (!paths || paths.count == 0) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  NSString* path = paths[0];
  if (appendPreferences) {
    path = [path stringByAppendingPathComponent:@"Preferences"];
  }
  if (appendLogs) {
    path = [path stringByAppendingPathComponent:@"Logs"];
  }
  return path;
}

NSString* utf8_to_nsstring(Utf8TextView text) {
  if (text.empty()) {
    return nil;
  }
  return [[NSString alloc] initWithBytes:text.data()
                                  length:static_cast<NSUInteger>(text.size())
                                encoding:NSUTF8StringEncoding];
}

HostResult<NSArray<NSString*>*> allowed_file_types(std::span<const Utf8TextView> extensions) {
  if (extensions.empty()) {
    return (NSArray<NSString*>*)nil;
  }
  NSMutableArray<NSString*>* types = [NSMutableArray arrayWithCapacity:extensions.size()];
  for (const auto& ext : extensions) {
    if (ext.empty()) {
      continue;
    }
    NSString* value = utf8_to_nsstring(ext);
    if (!value) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    [types addObject:value];
  }
  if (types.count == 0) {
    return (NSArray<NSString*>*)nil;
  }
  return types;
}

HostResult<NSArray<UTType*>*> allowed_content_types(std::span<const Utf8TextView> extensions) {
  if (extensions.empty()) {
    return (NSArray<UTType*>*)nil;
  }
  NSMutableArray<UTType*>* types = [NSMutableArray arrayWithCapacity:extensions.size()];
  for (const auto& ext : extensions) {
    if (ext.empty()) {
      continue;
    }
    NSString* value = utf8_to_nsstring(ext);
    if (!value) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    UTType* type = [UTType typeWithFilenameExtension:value];
    if (!type) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    [types addObject:type];
  }
  if (types.count == 0) {
    return (NSArray<UTType*>*)nil;
  }
  return types;
}

HostResult<NSArray<UTType*>*> content_types_from_identifiers(std::span<const Utf8TextView> identifiers) {
  if (identifiers.empty()) {
    return (NSArray<UTType*>*)nil;
  }
  NSMutableArray<UTType*>* types = [NSMutableArray arrayWithCapacity:identifiers.size()];
  for (const auto& entry : identifiers) {
    if (entry.empty()) {
      continue;
    }
    NSString* value = utf8_to_nsstring(entry);
    if (!value) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    UTType* type = [UTType typeWithIdentifier:value];
    if (!type) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    [types addObject:type];
  }
  if (types.count == 0) {
    return (NSArray<UTType*>*)nil;
  }
  return types;
}

PermissionStatus map_av_status(AVAuthorizationStatus status) {
  switch (status) {
    case AVAuthorizationStatusAuthorized:
      return PermissionStatus::Granted;
    case AVAuthorizationStatusDenied:
      return PermissionStatus::Denied;
    case AVAuthorizationStatusRestricted:
      return PermissionStatus::Restricted;
    case AVAuthorizationStatusNotDetermined:
    default:
      return PermissionStatus::Unknown;
  }
}

PermissionStatus map_notification_status(UNAuthorizationStatus status) {
  switch (status) {
    case UNAuthorizationStatusAuthorized:
      return PermissionStatus::Granted;
    case UNAuthorizationStatusDenied:
      return PermissionStatus::Denied;
    case UNAuthorizationStatusNotDetermined:
      return PermissionStatus::Unknown;
    case UNAuthorizationStatusProvisional:
      return PermissionStatus::Granted;
    default:
      return PermissionStatus::Unknown;
  }
}

PermissionStatus map_location_status(CLAuthorizationStatus status) {
  switch (status) {
    case kCLAuthorizationStatusAuthorizedAlways:
      return PermissionStatus::Granted;
    case kCLAuthorizationStatusDenied:
      return PermissionStatus::Denied;
    case kCLAuthorizationStatusRestricted:
      return PermissionStatus::Restricted;
    case kCLAuthorizationStatusNotDetermined:
    default:
      return PermissionStatus::Unknown;
  }
}

PermissionStatus map_photo_status(PHAuthorizationStatus status) {
  switch (status) {
    case PHAuthorizationStatusAuthorized:
      return PermissionStatus::Granted;
    case PHAuthorizationStatusDenied:
      return PermissionStatus::Denied;
    case PHAuthorizationStatusRestricted:
      return PermissionStatus::Restricted;
    case PHAuthorizationStatusNotDetermined:
    default:
      return PermissionStatus::Unknown;
  }
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

void apply_layer_config(SurfaceState& surface, const SurfaceCapabilities& caps) {
  if (!surface.layer) {
    return;
  }
  uint32_t bufferCount = effectiveBufferCount(surface.frameConfig, caps);
  if (@available(macOS 10.13, *)) {
    surface.layer.maximumDrawableCount = static_cast<NSUInteger>(bufferCount);
  }
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
  HostResult<size_t> displays(std::span<DisplayInfo> outDisplays) const override;
  HostResult<DisplayInfo> displayInfo(uint32_t displayId) const override;
  HostResult<uint32_t> surfaceDisplay(SurfaceId surfaceId) const override;
  HostStatus setSurfaceDisplay(SurfaceId surfaceId, uint32_t displayId) override;

  HostResult<SurfaceId> createSurface(const SurfaceConfig& config) override;
  HostStatus destroySurface(SurfaceId surfaceId) override;

  HostResult<EventBatch> pollEvents(const EventBuffer& buffer) override;
  HostStatus waitEvents() override;

  HostStatus requestFrame(SurfaceId surfaceId, bool bypassCap) override;
  HostStatus setFrameConfig(SurfaceId surfaceId, const FrameConfig& config) override;
  HostResult<FrameConfig> frameConfig(SurfaceId surfaceId) const override;
  HostResult<std::optional<std::chrono::nanoseconds>> displayInterval(SurfaceId surfaceId) const override;
  HostStatus setSurfaceTitle(SurfaceId surfaceId, Utf8TextView title) override;
  HostResult<SurfaceSize> surfaceSize(SurfaceId surfaceId) const override;
  HostStatus setSurfaceSize(SurfaceId surfaceId, uint32_t width, uint32_t height) override;
  HostResult<SurfacePoint> surfacePosition(SurfaceId surfaceId) const override;
  HostStatus setSurfacePosition(SurfaceId surfaceId, int32_t x, int32_t y) override;
  HostStatus setCursorVisible(SurfaceId surfaceId, bool visible) override;
  HostStatus setSurfaceMinimized(SurfaceId surfaceId, bool minimized) override;
  HostStatus setSurfaceMaximized(SurfaceId surfaceId, bool maximized) override;
  HostStatus setSurfaceFullscreen(SurfaceId surfaceId, bool fullscreen) override;
  HostResult<size_t> clipboardTextSize() const override;
  HostResult<Utf8TextView> clipboardText(std::span<char> buffer) const override;
  HostStatus setClipboardText(Utf8TextView text) override;
  HostStatus writeSurfaceScreenshot(SurfaceId surfaceId,
                                    Utf8TextView path,
                                    const ScreenshotConfig& config) override;
  HostResult<FileDialogResult> fileDialog(const FileDialogConfig& config,
                                          std::span<char> buffer) const override;
  HostResult<size_t> fileDialogPaths(const FileDialogConfig& config,
                                     std::span<TextSpan> outPaths,
                                     std::span<char> buffer) const override;
  HostResult<size_t> appPathSize(AppPathType type) const override;
  HostResult<Utf8TextView> appPath(AppPathType type, std::span<char> buffer) const override;
  HostResult<float> surfaceScale(SurfaceId surfaceId) const override;
  HostStatus setSurfaceMinSize(SurfaceId surfaceId, uint32_t width, uint32_t height) override;
  HostStatus setSurfaceMaxSize(SurfaceId surfaceId, uint32_t width, uint32_t height) override;

  HostStatus setGamepadRumble(const GamepadRumble& rumble) override;
  HostResult<PermissionStatus> checkPermission(PermissionType type) const override;
  HostResult<PermissionStatus> requestPermission(PermissionType type) override;
  HostResult<uint64_t> beginIdleSleepInhibit(Utf8TextView reason) override;
  HostStatus endIdleSleepInhibit(uint64_t token) override;
  HostStatus setGamepadLight(uint32_t deviceId, float r, float g, float b) override;
  HostResult<LocaleInfo> localeInfo() const override;
  HostResult<Utf8TextView> imeLanguageTag() const override;
  HostStatus setImeCompositionRect(SurfaceId surfaceId,
                                   int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height) override;
  HostResult<uint64_t> beginBackgroundTask(Utf8TextView reason) override;
  HostStatus endBackgroundTask(uint64_t token) override;
  HostResult<uint64_t> createTrayItem(Utf8TextView title) override;
  HostStatus updateTrayItemTitle(uint64_t trayId, Utf8TextView title) override;
  HostStatus removeTrayItem(uint64_t trayId) override;
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
  const SurfaceState* findSurface(uint64_t surfaceId) const;
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
  uint64_t nextTrayId_ = 1u;
  CVDisplayLinkRef cvDisplayLink_ = nullptr;
  std::optional<std::chrono::nanoseconds> displayInterval_{};
  bool relativePointerEnabled_ = false;
  std::optional<SurfaceId> relativePointerSurface_{};
  uint64_t nextIdleSleepToken_ = 1u;
  std::unordered_map<uint64_t, IOPMAssertionID> idleSleepAssertions_;
  std::unordered_map<uint64_t, NSStatusItem*> trayItems_;
  uint64_t nextBackgroundToken_ = 1u;
  std::unordered_map<uint64_t, id> backgroundActivities_;
  mutable std::string localeLanguage_;
  mutable std::string localeRegion_;
  mutable std::string imeLanguage_;
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

@interface PHLocationDelegate : NSObject <CLLocationManagerDelegate>
- (instancetype)initWithSemaphore:(dispatch_semaphore_t)semaphore;
- (CLAuthorizationStatus)status;
@end

@implementation PHLocationDelegate {
  dispatch_semaphore_t semaphore_;
  CLAuthorizationStatus status_;
}

- (instancetype)initWithSemaphore:(dispatch_semaphore_t)semaphore {
  self = [super init];
  if (self) {
    semaphore_ = semaphore;
    status_ = kCLAuthorizationStatusNotDetermined;
  }
  return self;
}

- (CLAuthorizationStatus)status {
  return status_;
}

- (void)locationManagerDidChangeAuthorization:(CLLocationManager*)manager {
  if (@available(macOS 11.0, *)) {
    status_ = manager.authorizationStatus;
  } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    status_ = [CLLocationManager authorizationStatus];
#pragma clang diagnostic pop
  }
  dispatch_semaphore_signal(semaphore_);
}

- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
  (void)manager;
  status_ = status;
  dispatch_semaphore_signal(semaphore_);
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
  for (const auto& entry : idleSleepAssertions_) {
    if (entry.second != kIOPMNullAssertionID) {
      IOPMAssertionRelease(entry.second);
    }
  }
  idleSleepAssertions_.clear();
  if (!trayItems_.empty()) {
    NSStatusBar* bar = [NSStatusBar systemStatusBar];
    for (const auto& entry : trayItems_) {
      [bar removeStatusItem:entry.second];
    }
    trayItems_.clear();
  }
  if (!backgroundActivities_.empty()) {
    NSProcessInfo* processInfo = [NSProcessInfo processInfo];
    for (const auto& entry : backgroundActivities_) {
      [processInfo endActivity:entry.second];
    }
    backgroundActivities_.clear();
  }
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
  caps.supportsClipboard = true;
  caps.supportsFileDialogs = true;
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
  if (outDevices.empty()) {
    return deviceOrder_.size();
  }
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

HostResult<size_t> HostMac::displays(std::span<DisplayInfo> outDisplays) const {
  uint32_t count = 0u;
  if (CGGetActiveDisplayList(0, nullptr, &count) != kCGErrorSuccess) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  if (outDisplays.empty()) {
    return static_cast<size_t>(count);
  }
  if (outDisplays.size() < count) {
    return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
  }
  std::vector<CGDirectDisplayID> ids(count);
  if (CGGetActiveDisplayList(count, ids.data(), &count) != kCGErrorSuccess) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  size_t written = 0u;
  for (CGDirectDisplayID id : ids) {
    CGRect bounds = CGDisplayBounds(id);
    DisplayInfo info{};
    info.displayId = static_cast<uint32_t>(id);
    info.x = static_cast<int32_t>(std::lround(bounds.origin.x));
    info.y = static_cast<int32_t>(std::lround(bounds.origin.y));
    info.width = static_cast<uint32_t>(std::lround(bounds.size.width));
    info.height = static_cast<uint32_t>(std::lround(bounds.size.height));
    info.scale = 1.0f;
    for (NSScreen* screen in [NSScreen screens]) {
      NSNumber* screenNumber = screen.deviceDescription[@"NSScreenNumber"];
      if (screenNumber && screenNumber.unsignedIntValue == id) {
        info.scale = static_cast<float>(screen.backingScaleFactor);
        break;
      }
    }
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(id);
    if (mode) {
      info.refreshRate = static_cast<float>(CGDisplayModeGetRefreshRate(mode));
      CGDisplayModeRelease(mode);
    }
    info.isPrimary = (id == CGMainDisplayID());
    outDisplays[written++] = info;
  }
  return written;
}

HostResult<DisplayInfo> HostMac::displayInfo(uint32_t displayId) const {
  uint32_t count = 0u;
  if (CGGetActiveDisplayList(0, nullptr, &count) != kCGErrorSuccess) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  std::vector<CGDirectDisplayID> ids(count);
  if (CGGetActiveDisplayList(count, ids.data(), &count) != kCGErrorSuccess) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  for (CGDirectDisplayID id : ids) {
    if (static_cast<uint32_t>(id) != displayId) {
      continue;
    }
    CGRect bounds = CGDisplayBounds(id);
    DisplayInfo info{};
    info.displayId = static_cast<uint32_t>(id);
    info.x = static_cast<int32_t>(std::lround(bounds.origin.x));
    info.y = static_cast<int32_t>(std::lround(bounds.origin.y));
    info.width = static_cast<uint32_t>(std::lround(bounds.size.width));
    info.height = static_cast<uint32_t>(std::lround(bounds.size.height));
    info.scale = 1.0f;
    for (NSScreen* screen in [NSScreen screens]) {
      NSNumber* screenNumber = screen.deviceDescription[@"NSScreenNumber"];
      if (screenNumber && screenNumber.unsignedIntValue == id) {
        info.scale = static_cast<float>(screen.backingScaleFactor);
        break;
      }
    }
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(id);
    if (mode) {
      info.refreshRate = static_cast<float>(CGDisplayModeGetRefreshRate(mode));
      CGDisplayModeRelease(mode);
    }
    info.isPrimary = (id == CGMainDisplayID());
    return info;
  }
  return std::unexpected(HostError{HostErrorCode::InvalidDisplay});
}

HostResult<uint32_t> HostMac::surfaceDisplay(SurfaceId surfaceId) const {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  NSScreen* screen = surface->window.screen;
  if (!screen) {
    return std::unexpected(HostError{HostErrorCode::InvalidDisplay});
  }
  NSNumber* screenNumber = screen.deviceDescription[@"NSScreenNumber"];
  if (!screenNumber) {
    return std::unexpected(HostError{HostErrorCode::InvalidDisplay});
  }
  return static_cast<uint32_t>(screenNumber.unsignedIntValue);
}

HostStatus HostMac::setSurfaceDisplay(SurfaceId surfaceId, uint32_t displayId) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  NSScreen* targetScreen = nil;
  for (NSScreen* screen in [NSScreen screens]) {
    NSNumber* screenNumber = screen.deviceDescription[@"NSScreenNumber"];
    if (screenNumber && screenNumber.unsignedIntValue == displayId) {
      targetScreen = screen;
      break;
    }
  }
  if (!targetScreen) {
    return std::unexpected(HostError{HostErrorCode::InvalidDisplay});
  }
  NSRect frame = surface->window.frame;
  NSRect visible = [targetScreen visibleFrame];
  frame.origin.x = visible.origin.x + (visible.size.width - frame.size.width) * 0.5;
  frame.origin.y = visible.origin.y + (visible.size.height - frame.size.height) * 0.5;
  [surface->window setFrame:frame display:YES];
  return {};
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
  SurfaceState* statePtr = state.get();
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

  if (statePtr) {
    auto caps = surfaceCapabilities(surfaceId);
    if (caps) {
      apply_layer_config(*statePtr, caps.value());
    }
  }

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

  std::optional<std::chrono::nanoseconds> target = surface->frameConfig.frameInterval;
  if (!target) {
    if (surface->displayInterval) {
      target = surface->displayInterval;
    } else if (displayInterval_) {
      target = displayInterval_;
    }
  }
  FrameDiagnostics diag = buildFrameDiagnostics(target, timing.delta, surface->frameConfig.framePolicy);

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
  std::optional<std::chrono::nanoseconds> defaultInterval = surface->displayInterval;
  if (!defaultInterval && displayInterval_) {
    defaultInterval = displayInterval_;
  }
  FrameConfig resolved = resolveFrameConfig(config, caps.value(), defaultInterval);
  auto status = validateFrameConfig(resolved, caps.value());
  if (!status) {
    return status;
  }
  surface->frameConfig = resolved;
  apply_layer_config(*surface, caps.value());
  updateDisplayLinkState();
  return {};
}

HostResult<FrameConfig> HostMac::frameConfig(SurfaceId surfaceId) const {
  auto* surface = findSurface(surfaceId.value);
  if (!surface) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  return surface->frameConfig;
}

HostResult<std::optional<std::chrono::nanoseconds>> HostMac::displayInterval(SurfaceId surfaceId) const {
  auto* surface = findSurface(surfaceId.value);
  if (!surface) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (surface->displayInterval) {
    return surface->displayInterval;
  }
  return displayInterval_;
}

HostStatus HostMac::setSurfaceTitle(SurfaceId surfaceId, Utf8TextView title) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  NSString* nsTitle = [[NSString alloc] initWithBytes:title.data()
                                               length:title.size()
                                             encoding:NSUTF8StringEncoding];
  if (!nsTitle) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  surface->window.title = nsTitle;
  return {};
}

HostResult<SurfaceSize> HostMac::surfaceSize(SurfaceId surfaceId) const {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  NSRect frame = [surface->window contentRectForFrameRect:surface->window.frame];
  SurfaceSize size{};
  size.width = static_cast<uint32_t>(std::lround(frame.size.width));
  size.height = static_cast<uint32_t>(std::lround(frame.size.height));
  return size;
}

HostStatus HostMac::setSurfaceSize(SurfaceId surfaceId, uint32_t width, uint32_t height) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (width == 0u || height == 0u) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  NSSize size = NSMakeSize(static_cast<CGFloat>(width), static_cast<CGFloat>(height));
  [surface->window setContentSize:size];
  return {};
}

HostResult<SurfacePoint> HostMac::surfacePosition(SurfaceId surfaceId) const {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  NSRect frame = surface->window.frame;
  SurfacePoint point{};
  point.x = static_cast<int32_t>(std::lround(frame.origin.x));
  point.y = static_cast<int32_t>(std::lround(frame.origin.y));
  return point;
}

HostStatus HostMac::setSurfacePosition(SurfaceId surfaceId, int32_t x, int32_t y) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  NSRect frame = surface->window.frame;
  frame.origin.x = static_cast<CGFloat>(x);
  frame.origin.y = static_cast<CGFloat>(y);
  [surface->window setFrame:frame display:YES];
  return {};
}

HostStatus HostMac::setCursorVisible(SurfaceId surfaceId, bool visible) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (visible) {
    [NSCursor unhide];
  } else {
    [NSCursor hide];
  }
  return {};
}

HostStatus HostMac::setSurfaceMinimized(SurfaceId surfaceId, bool minimized) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (minimized) {
    [surface->window miniaturize:nil];
  } else {
    [surface->window deminiaturize:nil];
  }
  return {};
}

HostStatus HostMac::setSurfaceMaximized(SurfaceId surfaceId, bool maximized) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (maximized == static_cast<bool>(surface->window.zoomed)) {
    return {};
  }
  [surface->window zoom:nil];
  return {};
}

HostStatus HostMac::setSurfaceFullscreen(SurfaceId surfaceId, bool fullscreen) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (fullscreen == static_cast<bool>(surface->window.styleMask & NSWindowStyleMaskFullScreen)) {
    return {};
  }
  [surface->window toggleFullScreen:nil];
  return {};
}

HostResult<size_t> HostMac::clipboardTextSize() const {
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  NSString* text = [pasteboard stringForType:NSPasteboardTypeString];
  if (!text) {
    return static_cast<size_t>(0u);
  }
  NSUInteger length = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
  return static_cast<size_t>(length);
}

HostResult<Utf8TextView> HostMac::clipboardText(std::span<char> buffer) const {
  if (buffer.empty()) {
    return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
  }
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  NSString* text = [pasteboard stringForType:NSPasteboardTypeString];
  if (!text) {
    buffer[0] = '\0';
    return Utf8TextView{buffer.data(), 0u};
  }
  NSData* data = [text dataUsingEncoding:NSUTF8StringEncoding];
  if (!data) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  if (data.length > buffer.size()) {
    return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
  }
  std::memcpy(buffer.data(), data.bytes, data.length);
  return Utf8TextView{buffer.data(), static_cast<size_t>(data.length)};
}

HostStatus HostMac::setClipboardText(Utf8TextView text) {
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  [pasteboard clearContents];
  NSString* nsText = [[NSString alloc] initWithBytes:text.data()
                                             length:text.size()
                                           encoding:NSUTF8StringEncoding];
  if (!nsText) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  if (![pasteboard setString:nsText forType:NSPasteboardTypeString]) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  return {};
}

HostStatus HostMac::writeSurfaceScreenshot(SurfaceId surfaceId,
                                           Utf8TextView path,
                                           const ScreenshotConfig& config) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (path.empty()) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  NSString* nsPath = utf8_to_nsstring(path);
  if (!nsPath) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  NSURL* url = [NSURL fileURLWithPath:nsPath];
  if (!url) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }

  CGWindowID windowId = static_cast<CGWindowID>(surface->window.windowNumber);
  CGWindowListOption listOptions = kCGWindowListOptionIncludingWindow;
  if (!config.includeHidden) {
    listOptions = static_cast<CGWindowListOption>(listOptions | kCGWindowListOptionOnScreenOnly);
  }
  CGWindowImageOption imageOptions = kCGWindowImageDefault;
  if (config.scope == ScreenshotScope::Surface) {
    imageOptions = static_cast<CGWindowImageOption>(imageOptions | kCGWindowImageBoundsIgnoreFraming);
  }
  if (auto fn = window_image_fn()) {
    CGImageRef image = fn(CGRectNull, listOptions, windowId, imageOptions);
    if (!image) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    CGImageDestinationRef destination =
        CGImageDestinationCreateWithURL((__bridge CFURLRef)url,
                                        (__bridge CFStringRef)UTTypePNG.identifier,
                                        1,
                                        nullptr);
    if (!destination) {
      CGImageRelease(image);
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    CGImageDestinationAddImage(destination, image, nullptr);
    bool finalized = CGImageDestinationFinalize(destination);
    CFRelease(destination);
    CGImageRelease(image);
    if (!finalized) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    return {};
  }

  if (!config.includeHidden && (!surface->window.isVisible || surface->window.isMiniaturized)) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  NSView* targetView = surface->view;
  if (config.scope == ScreenshotScope::Window) {
    NSView* frameView = surface->window.contentView.superview;
    if (frameView) {
      targetView = frameView;
    }
  }
  if (!targetView) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  NSRect bounds = targetView.bounds;
  NSBitmapImageRep* rep = [targetView bitmapImageRepForCachingDisplayInRect:bounds];
  if (!rep) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  [targetView cacheDisplayInRect:bounds toBitmapImageRep:rep];
  NSData* data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
  if (!data) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  if (![data writeToURL:url atomically:YES]) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  return {};
}

HostResult<FileDialogResult> HostMac::fileDialog(const FileDialogConfig& config,
                                                 std::span<char> buffer) const {
  if (buffer.empty()) {
    return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
  }
  std::array<TextSpan, 1> spans{};
  auto count = fileDialogPaths(config, spans, buffer);
  if (!count) {
    return std::unexpected(count.error());
  }
  FileDialogResult result{};
  result.accepted = false;
  result.path = Utf8TextView{buffer.data(), 0u};
  if (count.value() == 0u) {
    if (!buffer.empty()) {
      buffer[0] = '\0';
    }
    return result;
  }
  result.accepted = true;
  result.path = Utf8TextView{buffer.data() + spans[0].offset, spans[0].length};
  return result;
}

HostResult<size_t> HostMac::fileDialogPaths(const FileDialogConfig& config,
                                            std::span<TextSpan> outPaths,
                                            std::span<char> buffer) const {
  if (outPaths.empty() || buffer.empty()) {
    return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
  }

  NSString* title = config.title ? utf8_to_nsstring(*config.title) : nil;
  NSString* defaultPath = config.defaultPath ? utf8_to_nsstring(*config.defaultPath) : nil;
  NSString* defaultName = config.defaultName ? utf8_to_nsstring(*config.defaultName) : nil;
  if ((config.title && !title) || (config.defaultPath && !defaultPath) ||
      (config.defaultName && !defaultName)) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  NSURL* defaultDirectoryUrl = nil;
  NSString* defaultFileName = nil;
  if (defaultPath) {
    NSURL* url = [NSURL fileURLWithPath:defaultPath];
    BOOL isDir = NO;
    if ([[NSFileManager defaultManager] fileExistsAtPath:defaultPath isDirectory:&isDir] && isDir) {
      defaultDirectoryUrl = url;
    } else {
      defaultDirectoryUrl = [url URLByDeletingLastPathComponent];
      if (!config.defaultDirectoryOnly) {
        defaultFileName = url.lastPathComponent;
      }
    }
  }
  if (config.mode == FileDialogMode::Open ||
      config.mode == FileDialogMode::OpenFile ||
      config.mode == FileDialogMode::OpenDirectory) {
    bool allowFiles = config.allowFiles.value_or(config.mode != FileDialogMode::OpenDirectory);
    bool allowDirectories = config.allowDirectories.value_or(config.mode == FileDialogMode::OpenDirectory);
    if (config.mode == FileDialogMode::OpenFile) {
      allowFiles = true;
      allowDirectories = false;
    } else if (config.mode == FileDialogMode::OpenDirectory) {
      allowFiles = false;
      allowDirectories = true;
    }
    if (!allowFiles && !allowDirectories) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    if (!config.allowedExtensions.empty() && !config.allowedContentTypes.empty()) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    auto allowedTypes = allowed_file_types(config.allowedExtensions);
    if (!allowedTypes) {
      return std::unexpected(allowedTypes.error());
    }
    auto contentTypes = config.allowedContentTypes.empty()
                            ? allowed_content_types(config.allowedExtensions)
                            : content_types_from_identifiers(config.allowedContentTypes);
    if (!contentTypes) {
      return std::unexpected(contentTypes.error());
    }
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = allowFiles ? YES : NO;
    panel.canChooseDirectories = allowDirectories ? YES : NO;
    panel.allowsMultipleSelection = (outPaths.size() > 1u);
    panel.showsHiddenFiles = config.canSelectHiddenFiles ? YES : NO;
    if (title) {
      panel.title = title;
    }
    if (defaultDirectoryUrl) {
      panel.directoryURL = defaultDirectoryUrl;
    }
    if (defaultName && allowFiles && !allowDirectories) {
      panel.nameFieldStringValue = defaultName;
    } else if (defaultFileName && allowFiles && !allowDirectories) {
      panel.nameFieldStringValue = defaultFileName;
    }
    if (allowFiles && !allowDirectories) {
      if (@available(macOS 12.0, *)) {
        if (contentTypes.value()) {
          panel.allowedContentTypes = contentTypes.value();
        }
      } else {
        if (allowedTypes.value()) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
          panel.allowedFileTypes = allowedTypes.value();
#pragma clang diagnostic pop
        }
      }
    } else {
      if (!config.allowedExtensions.empty() || !config.allowedContentTypes.empty()) {
        return std::unexpected(HostError{HostErrorCode::InvalidConfig});
      }
    }
    NSInteger modal = [panel runModal];
    if (modal != NSModalResponseOK) {
      if (!buffer.empty()) {
        buffer[0] = '\0';
      }
      return static_cast<size_t>(0u);
    }
    NSArray<NSURL*>* urls = panel.URLs;
    if (!urls) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    if (static_cast<size_t>(urls.count) > outPaths.size()) {
      return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
    }
    size_t offset = 0u;
    for (NSUInteger i = 0; i < urls.count; ++i) {
      NSURL* url = urls[i];
      NSString* path = url.path;
      NSData* data = [path dataUsingEncoding:NSUTF8StringEncoding];
      if (!data) {
        return std::unexpected(HostError{HostErrorCode::PlatformFailure});
      }
      if (offset + data.length > buffer.size()) {
        return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
      }
      std::memcpy(buffer.data() + offset, data.bytes, data.length);
      outPaths[i] = TextSpan{static_cast<uint32_t>(offset),
                             static_cast<uint32_t>(data.length)};
      offset += static_cast<size_t>(data.length);
    }
    return static_cast<size_t>(urls.count);
  }

  if (config.mode == FileDialogMode::SaveFile) {
    if (!config.allowedExtensions.empty() && !config.allowedContentTypes.empty()) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    auto allowedTypes = allowed_file_types(config.allowedExtensions);
    if (!allowedTypes) {
      return std::unexpected(allowedTypes.error());
    }
    auto contentTypes = config.allowedContentTypes.empty()
                            ? allowed_content_types(config.allowedExtensions)
                            : content_types_from_identifiers(config.allowedContentTypes);
    if (!contentTypes) {
      return std::unexpected(contentTypes.error());
    }
    NSSavePanel* panel = [NSSavePanel savePanel];
    panel.canCreateDirectories = config.canCreateDirectories ? YES : NO;
    panel.showsHiddenFiles = config.canSelectHiddenFiles ? YES : NO;
    if (title) {
      panel.title = title;
    }
    if (defaultDirectoryUrl) {
      panel.directoryURL = defaultDirectoryUrl;
    }
    if (defaultFileName) {
      panel.nameFieldStringValue = defaultFileName;
    }
    if (defaultName) {
      panel.nameFieldStringValue = defaultName;
    }
    if (@available(macOS 12.0, *)) {
      if (contentTypes.value()) {
        panel.allowedContentTypes = contentTypes.value();
      }
    } else {
      if (allowedTypes.value()) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        panel.allowedFileTypes = allowedTypes.value();
#pragma clang diagnostic pop
      }
    }
    NSInteger modal = [panel runModal];
    if (modal != NSModalResponseOK) {
      if (!buffer.empty()) {
        buffer[0] = '\0';
      }
      return static_cast<size_t>(0u);
    }
    NSURL* url = panel.URL;
    if (!url) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    NSString* path = url.path;
    NSData* data = [path dataUsingEncoding:NSUTF8StringEncoding];
    if (!data) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    if (data.length > buffer.size()) {
      return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
    }
    std::memcpy(buffer.data(), data.bytes, data.length);
    outPaths[0] = TextSpan{0u, static_cast<uint32_t>(data.length)};
    return static_cast<size_t>(1u);
  }

  return std::unexpected(HostError{HostErrorCode::InvalidConfig});
}

HostResult<size_t> HostMac::appPathSize(AppPathType type) const {
  auto pathResult = app_path_for_type(type);
  if (!pathResult) {
    return std::unexpected(pathResult.error());
  }
  NSString* path = pathResult.value();
  if (!path) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  NSUInteger length = [path lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
  return static_cast<size_t>(length);
}

HostResult<Utf8TextView> HostMac::appPath(AppPathType type, std::span<char> buffer) const {
  if (buffer.empty()) {
    return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
  }
  auto pathResult = app_path_for_type(type);
  if (!pathResult) {
    return std::unexpected(pathResult.error());
  }
  NSString* path = pathResult.value();
  if (!path) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  NSData* data = [path dataUsingEncoding:NSUTF8StringEncoding];
  if (!data) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  if (data.length > buffer.size()) {
    return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
  }
  std::memcpy(buffer.data(), data.bytes, data.length);
  return Utf8TextView{buffer.data(), static_cast<size_t>(data.length)};
}

HostResult<float> HostMac::surfaceScale(SurfaceId surfaceId) const {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  return static_cast<float>(surface->window.backingScaleFactor);
}

HostStatus HostMac::setSurfaceMinSize(SurfaceId surfaceId, uint32_t width, uint32_t height) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (width == 0u || height == 0u) {
    surface->window.contentMinSize = NSMakeSize(0.0, 0.0);
    return {};
  }
  surface->window.contentMinSize = NSMakeSize(static_cast<CGFloat>(width),
                                              static_cast<CGFloat>(height));
  return {};
}

HostStatus HostMac::setSurfaceMaxSize(SurfaceId surfaceId, uint32_t width, uint32_t height) {
  auto* surface = findSurface(surfaceId.value);
  if (!surface || !surface->window) {
    return std::unexpected(HostError{HostErrorCode::InvalidSurface});
  }
  if (width == 0u || height == 0u) {
    surface->window.contentMaxSize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
    return {};
  }
  surface->window.contentMaxSize = NSMakeSize(static_cast<CGFloat>(width),
                                              static_cast<CGFloat>(height));
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

HostResult<PermissionStatus> HostMac::checkPermission(PermissionType type) const {
  switch (type) {
    case PermissionType::Camera: {
      AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
      return map_av_status(status);
    }
    case PermissionType::Microphone: {
      AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
      return map_av_status(status);
    }
    case PermissionType::Notifications:
      if (@available(macOS 10.14, *)) {
        NSBundle* bundle = [NSBundle mainBundle];
        if (!bundle || !bundle.bundleIdentifier) {
          return std::unexpected(HostError{HostErrorCode::Unsupported});
        }
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        __block UNAuthorizationStatus status = UNAuthorizationStatusNotDetermined;
        [[UNUserNotificationCenter currentNotificationCenter]
            getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings* settings) {
              status = settings.authorizationStatus;
              dispatch_semaphore_signal(semaphore);
            }];
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        return map_notification_status(status);
      }
      return std::unexpected(HostError{HostErrorCode::Unsupported});
    case PermissionType::Location:
      if (@available(macOS 10.15, *)) {
        NSBundle* bundle = [NSBundle mainBundle];
        if (!bundle || !bundle.bundleIdentifier) {
          return std::unexpected(HostError{HostErrorCode::Unsupported});
        }
        CLLocationManager* manager = [[CLLocationManager alloc] init];
        CLAuthorizationStatus status = kCLAuthorizationStatusNotDetermined;
        if (@available(macOS 11.0, *)) {
          status = manager.authorizationStatus;
        } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
          status = [CLLocationManager authorizationStatus];
#pragma clang diagnostic pop
        }
        return map_location_status(status);
      }
      return std::unexpected(HostError{HostErrorCode::Unsupported});
    case PermissionType::Photos:
      if (@available(macOS 10.13, *)) {
        NSBundle* bundle = [NSBundle mainBundle];
        if (!bundle || !bundle.bundleIdentifier) {
          return std::unexpected(HostError{HostErrorCode::Unsupported});
        }
        PHAuthorizationStatus status = [PHPhotoLibrary authorizationStatus];
        return map_photo_status(status);
      }
      return std::unexpected(HostError{HostErrorCode::Unsupported});
    default:
      return std::unexpected(HostError{HostErrorCode::Unsupported});
  }
}

HostResult<PermissionStatus> HostMac::requestPermission(PermissionType type) {
  NSString* mediaType = nil;
  switch (type) {
    case PermissionType::Camera:
      mediaType = AVMediaTypeVideo;
      break;
    case PermissionType::Microphone:
      mediaType = AVMediaTypeAudio;
      break;
    case PermissionType::Notifications:
      if (@available(macOS 10.14, *)) {
        NSBundle* bundle = [NSBundle mainBundle];
        if (!bundle || !bundle.bundleIdentifier) {
          return std::unexpected(HostError{HostErrorCode::Unsupported});
        }
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        __block bool granted = false;
        __block NSError* nsError = nil;
        [[UNUserNotificationCenter currentNotificationCenter]
            requestAuthorizationWithOptions:(UNAuthorizationOptionAlert |
                                             UNAuthorizationOptionSound |
                                             UNAuthorizationOptionBadge)
                          completionHandler:^(BOOL ok, NSError* error) {
                            granted = ok;
                            nsError = error;
                            dispatch_semaphore_signal(semaphore);
                          }];
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        if (nsError) {
          return std::unexpected(HostError{HostErrorCode::PlatformFailure});
        }
        return granted ? PermissionStatus::Granted : PermissionStatus::Denied;
      }
      return std::unexpected(HostError{HostErrorCode::Unsupported});
    case PermissionType::Location:
      if (@available(macOS 10.15, *)) {
        NSBundle* bundle = [NSBundle mainBundle];
        if (!bundle || !bundle.bundleIdentifier) {
          return std::unexpected(HostError{HostErrorCode::Unsupported});
        }
        NSString* usage = [bundle objectForInfoDictionaryKey:@"NSLocationWhenInUseUsageDescription"];
        if (!usage) {
          return std::unexpected(HostError{HostErrorCode::Unsupported});
        }
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        PHLocationDelegate* delegate = [[PHLocationDelegate alloc] initWithSemaphore:semaphore];
        CLLocationManager* manager = [[CLLocationManager alloc] init];
        manager.delegate = delegate;
        [manager requestWhenInUseAuthorization];
        dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(5) * NSEC_PER_SEC);
        if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
          return std::unexpected(HostError{HostErrorCode::PlatformFailure});
        }
        return map_location_status([delegate status]);
      }
      return std::unexpected(HostError{HostErrorCode::Unsupported});
    case PermissionType::Photos:
      if (@available(macOS 10.13, *)) {
        NSBundle* bundle = [NSBundle mainBundle];
        if (!bundle || !bundle.bundleIdentifier) {
          return std::unexpected(HostError{HostErrorCode::Unsupported});
        }
        NSString* usage = [bundle objectForInfoDictionaryKey:@"NSPhotoLibraryUsageDescription"];
        if (!usage) {
          return std::unexpected(HostError{HostErrorCode::Unsupported});
        }
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        __block PHAuthorizationStatus status = PHAuthorizationStatusNotDetermined;
        if (@available(macOS 11.0, *)) {
          [PHPhotoLibrary requestAuthorizationForAccessLevel:PHAccessLevelReadWrite
                                                    handler:^(PHAuthorizationStatus authStatus) {
                                                      status = authStatus;
                                                      dispatch_semaphore_signal(semaphore);
                                                    }];
        } else {
          [PHPhotoLibrary requestAuthorization:^(PHAuthorizationStatus authStatus) {
            status = authStatus;
            dispatch_semaphore_signal(semaphore);
          }];
        }
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        return map_photo_status(status);
      }
      return std::unexpected(HostError{HostErrorCode::Unsupported});
    default:
      return std::unexpected(HostError{HostErrorCode::Unsupported});
  }

  AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:mediaType];
  if (status != AVAuthorizationStatusNotDetermined) {
    return map_av_status(status);
  }

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block bool granted = false;
  [AVCaptureDevice requestAccessForMediaType:mediaType
                           completionHandler:^(BOOL ok) {
                             granted = ok;
                             dispatch_semaphore_signal(semaphore);
                           }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  return granted ? PermissionStatus::Granted : PermissionStatus::Denied;
}

HostResult<uint64_t> HostMac::beginIdleSleepInhibit(Utf8TextView reason) {
  CFStringRef reasonRef = nullptr;
  bool releaseReason = false;
  if (!reason.empty()) {
    reasonRef = CFStringCreateWithBytes(kCFAllocatorDefault,
                                        reinterpret_cast<const UInt8*>(reason.data()),
                                        static_cast<CFIndex>(reason.size()),
                                        kCFStringEncodingUTF8,
                                        false);
    releaseReason = (reasonRef != nullptr);
  }
  if (!reasonRef) {
    reasonRef = CFSTR("PrimeHost Idle Sleep");
  }
  IOPMAssertionID assertionId = kIOPMNullAssertionID;
  IOReturn result = IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
                                                kIOPMAssertionLevelOn,
                                                reasonRef,
                                                &assertionId);
  if (releaseReason) {
    CFRelease(reasonRef);
  }
  if (result != kIOReturnSuccess || assertionId == kIOPMNullAssertionID) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }

  uint64_t token = nextIdleSleepToken_++;
  if (token == 0u) {
    token = nextIdleSleepToken_++;
  }
  idleSleepAssertions_.emplace(token, assertionId);
  return token;
}

HostStatus HostMac::endIdleSleepInhibit(uint64_t token) {
  auto it = idleSleepAssertions_.find(token);
  if (it == idleSleepAssertions_.end()) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  if (IOPMAssertionRelease(it->second) != kIOReturnSuccess) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  idleSleepAssertions_.erase(it);
  return {};
}

HostStatus HostMac::setGamepadLight(uint32_t deviceId, float r, float g, float b) {
  auto controllerIt = gamepadControllers_.find(deviceId);
  if (controllerIt == gamepadControllers_.end()) {
    return std::unexpected(HostError{HostErrorCode::InvalidDevice});
  }
  if (@available(macOS 11.0, *)) {
    GCController* controller = controllerIt->second;
    if (!controller || !controller.light) {
      return std::unexpected(HostError{HostErrorCode::Unsupported});
    }
    float red = std::clamp(r, 0.0f, 1.0f);
    float green = std::clamp(g, 0.0f, 1.0f);
    float blue = std::clamp(b, 0.0f, 1.0f);
    controller.light.color = [[GCColor alloc] initWithRed:red green:green blue:blue];
    return {};
  }
  return std::unexpected(HostError{HostErrorCode::Unsupported});
}

HostResult<LocaleInfo> HostMac::localeInfo() const {
  NSLocale* locale = [NSLocale autoupdatingCurrentLocale];
  NSString* language = [locale objectForKey:NSLocaleLanguageCode];
  NSString* region = [locale objectForKey:NSLocaleCountryCode];
  const char* languageBytes = language ? [language UTF8String] : nullptr;
  const char* regionBytes = region ? [region UTF8String] : nullptr;
  localeLanguage_ = languageBytes ? languageBytes : "";
  localeRegion_ = regionBytes ? regionBytes : "";
  LocaleInfo info{};
  info.languageTag = localeLanguage_;
  info.regionTag = localeRegion_;
  return info;
}

HostResult<Utf8TextView> HostMac::imeLanguageTag() const {
  imeLanguage_.clear();
  TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
  if (source) {
    CFArrayRef languages = static_cast<CFArrayRef>(
        TISGetInputSourceProperty(source, kTISPropertyInputSourceLanguages));
    if (languages && CFArrayGetCount(languages) > 0) {
      CFTypeRef value = CFArrayGetValueAtIndex(languages, 0);
      if (value && CFGetTypeID(value) == CFStringGetTypeID()) {
        imeLanguage_ = cfstring_to_utf8(static_cast<CFStringRef>(value));
      }
    }
    CFRelease(source);
  }
  return Utf8TextView{imeLanguage_};
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

HostResult<uint64_t> HostMac::beginBackgroundTask(Utf8TextView reason) {
  NSString* nsReason = nil;
  if (!reason.empty()) {
    nsReason = [[NSString alloc] initWithBytes:reason.data()
                                        length:static_cast<NSUInteger>(reason.size())
                                      encoding:NSUTF8StringEncoding];
  }
  if (!nsReason) {
    nsReason = @"PrimeHost Background Task";
  }
  NSProcessInfo* processInfo = [NSProcessInfo processInfo];
  id activity = [processInfo beginActivityWithOptions:NSActivityUserInitiated reason:nsReason];
  if (!activity) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  uint64_t token = nextBackgroundToken_++;
  if (token == 0u) {
    token = nextBackgroundToken_++;
  }
  backgroundActivities_.emplace(token, activity);
  return token;
}

HostStatus HostMac::endBackgroundTask(uint64_t token) {
  auto it = backgroundActivities_.find(token);
  if (it == backgroundActivities_.end()) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  [[NSProcessInfo processInfo] endActivity:it->second];
  backgroundActivities_.erase(it);
  return {};
}

HostResult<uint64_t> HostMac::createTrayItem(Utf8TextView title) {
  NSStatusItem* item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
  if (!item) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  if (!title.empty()) {
    NSString* nsTitle = [[NSString alloc] initWithBytes:title.data()
                                                 length:static_cast<NSUInteger>(title.size())
                                               encoding:NSUTF8StringEncoding];
    if (!nsTitle) {
      return std::unexpected(HostError{HostErrorCode::InvalidConfig});
    }
    if (!item.button) {
      return std::unexpected(HostError{HostErrorCode::PlatformFailure});
    }
    item.button.title = nsTitle;
  }
  uint64_t trayId = nextTrayId_++;
  if (trayId == 0u) {
    trayId = nextTrayId_++;
  }
  trayItems_[trayId] = item;
  return trayId;
}

HostStatus HostMac::updateTrayItemTitle(uint64_t trayId, Utf8TextView title) {
  auto it = trayItems_.find(trayId);
  if (it == trayItems_.end()) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  NSString* nsTitle = [[NSString alloc] initWithBytes:title.data()
                                               length:static_cast<NSUInteger>(title.size())
                                             encoding:NSUTF8StringEncoding];
  if (!nsTitle) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  NSStatusItem* item = it->second;
  if (!item.button) {
    return std::unexpected(HostError{HostErrorCode::PlatformFailure});
  }
  item.button.title = nsTitle;
  return {};
}

HostStatus HostMac::removeTrayItem(uint64_t trayId) {
  auto it = trayItems_.find(trayId);
  if (it == trayItems_.end()) {
    return std::unexpected(HostError{HostErrorCode::InvalidConfig});
  }
  [[NSStatusBar systemStatusBar] removeStatusItem:it->second];
  trayItems_.erase(it);
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

const SurfaceState* HostMac::findSurface(uint64_t surfaceId) const {
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
