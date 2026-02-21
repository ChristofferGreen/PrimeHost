#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <expected>

namespace PrimeHost {

using Utf8TextView = std::string_view;

struct SurfaceId {
  uint64_t value = 0u;

  constexpr bool isValid() const { return value != 0u; }
};

constexpr bool operator==(SurfaceId a, SurfaceId b) { return a.value == b.value; }
constexpr bool operator!=(SurfaceId a, SurfaceId b) { return a.value != b.value; }

enum class PresentMode {
  LowLatency,
  Smooth,
  Uncapped,
};

enum class FramePacingSource {
  Platform,
  HostLimiter,
};

enum class FramePolicy {
  EventDriven,
  Continuous,
  Capped,
};

enum class ColorFormat {
  B8G8R8A8_UNORM,
};

enum class DeviceType {
  Mouse,
  Touch,
  Pen,
  Keyboard,
  Gamepad,
};

using PresentModeMask = uint32_t;
using ColorFormatMask = uint32_t;

enum class KeyModifier : uint8_t {
  Shift = 1u << 0u,
  Control = 1u << 1u,
  Alt = 1u << 2u,
  Super = 1u << 3u,
  CapsLock = 1u << 4u,
  NumLock = 1u << 5u,
};

using KeyModifierMask = uint8_t;

enum class HostErrorCode {
  Unknown,
  InvalidSurface,
  InvalidDevice,
  InvalidConfig,
  Unsupported,
  BufferTooSmall,
  DeviceUnavailable,
  OutOfMemory,
  PlatformFailure,
};

struct HostError {
  HostErrorCode code = HostErrorCode::Unknown;
};

using HostStatus = std::expected<void, HostError>;

template <typename T>
using HostResult = std::expected<T, HostError>;

struct HostCapabilities {
  bool supportsClipboard = false;
  bool supportsFileDialogs = false;
  bool supportsRelativePointer = false;
  bool supportsIme = false;
  bool supportsHaptics = false;
  bool supportsHeadless = false;
};

struct SurfaceCapabilities {
  bool supportsVsyncToggle = false;
  bool supportsTearing = false;
  uint32_t minBufferCount = 2u;
  uint32_t maxBufferCount = 2u;
  PresentModeMask presentModes = 0u;
  ColorFormatMask colorFormats = 0u;
};

struct DeviceCapabilities {
  DeviceType type = DeviceType::Mouse;
  bool hasPressure = false;
  bool hasTilt = false;
  bool hasTwist = false;
  bool hasDistance = false;
  bool hasRumble = false;
  bool hasAnalogButtons = false;
  uint32_t maxTouches = 0u;
};

struct DeviceInfo {
  uint32_t deviceId = 0u;
  DeviceType type = DeviceType::Mouse;
  uint16_t vendorId = 0u;
  uint16_t productId = 0u;
  Utf8TextView name;
};

struct FrameTiming {
  std::chrono::steady_clock::time_point time;
  std::chrono::nanoseconds delta{0};
  uint64_t frameIndex = 0u;
};

struct FrameDiagnostics {
  std::chrono::nanoseconds targetInterval{0};
  std::chrono::nanoseconds actualInterval{0};
  bool missedDeadline = false;
  bool wasThrottled = false;
  uint32_t droppedFrames = 0u;
};

struct FrameConfig {
  PresentMode presentMode = PresentMode::LowLatency;
  FramePolicy framePolicy = FramePolicy::EventDriven;
  FramePacingSource framePacingSource = FramePacingSource::Platform;
  ColorFormat colorFormat = ColorFormat::B8G8R8A8_UNORM;
  bool vsync = true;
  bool allowTearing = false;
  uint32_t maxFrameLatency = 1u;
  uint32_t bufferCount = 2u;
  std::optional<std::chrono::nanoseconds> frameInterval;
};

struct SurfaceConfig {
  uint32_t width = 0u;
  uint32_t height = 0u;
  bool resizable = true;
  std::optional<std::string> title;
};

enum class PointerPhase {
  Down,
  Move,
  Up,
  Cancel,
};

enum class PointerDeviceType {
  Mouse,
  Touch,
  Pen,
};

struct PointerEvent {
  uint32_t deviceId = 0u;
  uint32_t pointerId = 0u;
  PointerDeviceType deviceType = PointerDeviceType::Mouse;
  PointerPhase phase = PointerPhase::Move;
  int32_t x = 0;
  int32_t y = 0;
  std::optional<int32_t> deltaX;
  std::optional<int32_t> deltaY;
  std::optional<float> pressure;
  std::optional<float> tiltX;
  std::optional<float> tiltY;
  std::optional<float> twist;
  std::optional<float> distance;
  uint32_t buttonMask = 0u;
  bool isPrimary = true;
};

struct KeyEvent {
  uint32_t deviceId = 0u;
  uint32_t keyCode = 0u;
  KeyModifierMask modifiers = 0u;
  bool pressed = false;
  bool repeat = false;
};

struct TextSpan {
  uint32_t offset = 0u;
  uint32_t length = 0u;
};

struct TextEvent {
  uint32_t deviceId = 0u;
  TextSpan text;
};

struct ScrollEvent {
  uint32_t deviceId = 0u;
  float deltaX = 0.0f;
  float deltaY = 0.0f;
  bool isLines = false;
};

enum class GamepadButtonId : uint32_t {
  South = 0u,
  East = 1u,
  West = 2u,
  North = 3u,
  LeftBumper = 4u,
  RightBumper = 5u,
  Back = 6u,
  Start = 7u,
  Guide = 8u,
  LeftStick = 9u,
  RightStick = 10u,
  DpadUp = 11u,
  DpadDown = 12u,
  DpadLeft = 13u,
  DpadRight = 14u,
  Misc = 15u,
};

enum class GamepadAxisId : uint32_t {
  LeftX = 0u,
  LeftY = 1u,
  RightX = 2u,
  RightY = 3u,
  LeftTrigger = 4u,
  RightTrigger = 5u,
};

struct GamepadButtonEvent {
  uint32_t deviceId = 0u;
  uint32_t controlId = 0u;
  bool pressed = false;
  std::optional<float> value;
};

struct GamepadAxisEvent {
  uint32_t deviceId = 0u;
  uint32_t controlId = 0u;
  float value = 0.0f;
};

struct GamepadRumble {
  uint32_t deviceId = 0u;
  float lowFrequency = 0.0f;
  float highFrequency = 0.0f;
  std::chrono::milliseconds duration{0};
};

struct DeviceEvent {
  uint32_t deviceId = 0u;
  DeviceType deviceType = DeviceType::Mouse;
  bool connected = true;
};

using InputEvent = std::variant<PointerEvent,
                                KeyEvent,
                                TextEvent,
                                ScrollEvent,
                                GamepadButtonEvent,
                                GamepadAxisEvent,
                                DeviceEvent>;

struct ResizeEvent {
  uint32_t width = 0u;
  uint32_t height = 0u;
  float scale = 1.0f;
};

enum class LifecyclePhase {
  Created,
  Suspended,
  Resumed,
  Backgrounded,
  Foregrounded,
  Destroyed,
};

struct LifecycleEvent {
  LifecyclePhase phase = LifecyclePhase::Created;
};

struct Event {
  enum class Scope {
    Surface,
    Global,
  };

  Scope scope = Scope::Surface;
  std::optional<SurfaceId> surfaceId;
  std::chrono::steady_clock::time_point time;
  std::variant<InputEvent, ResizeEvent, LifecycleEvent> payload;
};

struct EventBuffer {
  std::span<Event> events;
  std::span<char> textBytes;
};

struct EventBatch {
  std::span<const Event> events;
  std::span<const char> textBytes;
};

struct Callbacks {
  std::function<void(const EventBatch&)> onEvents;
  std::function<void(SurfaceId, const FrameTiming&, const FrameDiagnostics&)> onFrame;
};

class Host {
public:
  virtual ~Host() = default;

  virtual HostResult<HostCapabilities> hostCapabilities() const = 0;
  virtual HostResult<SurfaceCapabilities> surfaceCapabilities(SurfaceId surfaceId) const = 0;
  virtual HostResult<DeviceInfo> deviceInfo(uint32_t deviceId) const = 0;
  virtual HostResult<DeviceCapabilities> deviceCapabilities(uint32_t deviceId) const = 0;
  virtual HostResult<size_t> devices(std::span<DeviceInfo> outDevices) const = 0;

  virtual HostResult<SurfaceId> createSurface(const SurfaceConfig& config) = 0;
  virtual HostStatus destroySurface(SurfaceId surfaceId) = 0;

  virtual HostResult<EventBatch> pollEvents(const EventBuffer& buffer) = 0;
  virtual HostStatus waitEvents() = 0;

  virtual HostStatus requestFrame(SurfaceId surfaceId, bool bypassCap) = 0;
  virtual HostStatus setFrameConfig(SurfaceId surfaceId, const FrameConfig& config) = 0;
  virtual HostResult<FrameConfig> frameConfig(SurfaceId surfaceId) const = 0;

  virtual HostStatus setGamepadRumble(const GamepadRumble& rumble) = 0;

  virtual HostStatus setImeCompositionRect(SurfaceId surfaceId,
                                           int32_t x,
                                           int32_t y,
                                           int32_t width,
                                           int32_t height) = 0;
  virtual HostStatus setRelativePointerCapture(SurfaceId surfaceId, bool enabled) = 0;

  virtual HostStatus setCallbacks(Callbacks callbacks) = 0;
};

HostResult<std::unique_ptr<Host>> createHost();

} // namespace PrimeHost
