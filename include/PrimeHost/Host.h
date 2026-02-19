#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

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

struct FrameTiming {
  std::chrono::steady_clock::time_point time;
  std::chrono::nanoseconds delta{0};
  uint64_t frameIndex = 0u;
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
  int32_t deltaX = 0;
  int32_t deltaY = 0;
  float pressure = 0.0f;
  float tiltX = 0.0f;
  float tiltY = 0.0f;
  float twist = 0.0f;
  float distance = 0.0f;
  uint32_t buttonMask = 0u;
  bool isPrimary = true;
};

struct KeyEvent {
  uint32_t deviceId = 0u;
  uint32_t keyCode = 0u;
  bool pressed = false;
  bool repeat = false;
};

struct TextEvent {
  uint32_t deviceId = 0u;
  Utf8TextView text;
};

struct ScrollEvent {
  uint32_t deviceId = 0u;
  float deltaX = 0.0f;
  float deltaY = 0.0f;
};

enum class GamepadEventType {
  Button,
  Axis,
};

struct GamepadEvent {
  uint32_t deviceId = 0u;
  GamepadEventType type = GamepadEventType::Button;
  uint32_t controlId = 0u;
  float value = 0.0f;
  bool pressed = false;
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

using InputEvent = std::variant<PointerEvent, KeyEvent, TextEvent, ScrollEvent, GamepadEvent, DeviceEvent>;

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
  std::chrono::steady_clock::time_point time;
  SurfaceId surfaceId;
  std::variant<InputEvent, ResizeEvent, LifecycleEvent> payload;
};

struct Callbacks {
  std::function<void(const Event&)> onEvent;
  std::function<void(SurfaceId, const FrameTiming&)> onFrame;
};

class Host {
public:
  virtual ~Host() = default;

  virtual std::optional<SurfaceId> createSurface(const SurfaceConfig& config) = 0;
  virtual void destroySurface(SurfaceId surfaceId) = 0;

  virtual size_t pollEvents(std::span<Event> outEvents) = 0;
  virtual void waitEvents() = 0;

  virtual void requestFrame(SurfaceId surfaceId, bool bypassCap) = 0;
  virtual void setFrameConfig(SurfaceId surfaceId, const FrameConfig& config) = 0;

  virtual void setGamepadRumble(const GamepadRumble& rumble) = 0;

  virtual void setCallbacks(Callbacks callbacks) = 0;
};

} // namespace PrimeHost
