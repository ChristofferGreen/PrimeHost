# PrimeHost API Signatures (Draft)

This document lists the current public API signatures from headers plus draft signatures for planned subsystems.

## Host (from `include/PrimeHost/Host.h`)
```cpp
namespace PrimeHost {

using Utf8TextView = std::string_view;

struct SurfaceId {
  uint64_t value = 0u;
  constexpr bool isValid() const { return value != 0u; }
};

constexpr bool operator==(SurfaceId a, SurfaceId b);
constexpr bool operator!=(SurfaceId a, SurfaceId b);

enum class PresentMode { LowLatency, Smooth, Uncapped };
enum class FramePacingSource { Platform, HostLimiter };
enum class FramePolicy { EventDriven, Continuous, Capped };
enum class ColorFormat { B8G8R8A8_UNORM };
enum class CursorShape {
  Arrow,
  IBeam,
  Crosshair,
  Hand,
  ResizeLeftRight,
  ResizeUpDown,
  ResizeDiagonal,
  ResizeDiagonalReverse,
  NotAllowed,
};

enum class DeviceType { Mouse, Touch, Pen, Keyboard, Gamepad };

enum class ThermalState { Unknown, Nominal, Fair, Serious, Critical };

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
  InvalidDisplay,
  InvalidConfig,
  Unsupported,
  BufferTooSmall,
  DeviceUnavailable,
  OutOfMemory,
  PlatformFailure,
};

enum class LogLevel { Debug, Info, Warning, Error };

struct HostError { HostErrorCode code = HostErrorCode::Unknown; };

using HostStatus = std::expected<void, HostError>;
template <typename T>
using HostResult = std::expected<T, HostError>;

using LogCallback = std::function<void(LogLevel level, Utf8TextView message)>;

struct HostCapabilities {
  bool supportsClipboard = false;
  bool supportsFileDialogs = false;
  bool supportsRelativePointer = false;
  bool supportsIme = false;
  bool supportsHaptics = false;
  bool supportsHeadless = false;
};

enum class PermissionType { Camera, Microphone, Location, Photos, Notifications, ClipboardRead };

enum class PermissionStatus { Unknown, Granted, Denied, Restricted };

enum class AppPathType { UserData, Cache, Config, Logs, Temp };

enum class FileDialogMode { OpenFile, OpenDirectory, Open, SaveFile };

enum class ScreenshotScope { Surface, Window };

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

struct LocaleInfo {
  Utf8TextView languageTag;
  Utf8TextView regionTag;
};

struct DisplayInfo {
  uint32_t displayId = 0u;
  int32_t x = 0;
  int32_t y = 0;
  uint32_t width = 0u;
  uint32_t height = 0u;
  float scale = 1.0f;
  float refreshRate = 0.0f;
  bool isPrimary = false;
};

struct DisplayHdrInfo {
  bool supportsHdr = false;
  float maxEdr = 1.0f;
  float maxEdrPotential = 1.0f;
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
  bool headless = false;
  std::optional<std::string> title;
};

struct SurfaceSize {
  uint32_t width = 0u;
  uint32_t height = 0u;
};

struct SurfacePoint {
  int32_t x = 0;
  int32_t y = 0;
};

struct SafeAreaInsets {
  float top = 0.0f;
  float left = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
};

struct CursorImage {
  uint32_t width = 0u;
  uint32_t height = 0u;
  int32_t hotX = 0;
  int32_t hotY = 0;
  std::span<const uint8_t> pixels;
};

struct ImageSize {
  uint32_t width = 0u;
  uint32_t height = 0u;
};

struct ImageData {
  ImageSize size;
  std::span<const uint8_t> pixels;
};

struct FrameBuffer {
  ImageSize size;
  uint32_t stride = 0u;
  ColorFormat colorFormat = ColorFormat::B8G8R8A8_UNORM;
  float scale = 1.0f;
  uint32_t bufferIndex = 0u;
  std::span<uint8_t> pixels;
};

struct IconImage {
  ImageSize size;
  std::span<const uint8_t> pixels;
};

struct WindowIcon {
  std::span<const IconImage> images;
};

struct FileDialogConfig {
  FileDialogMode mode = FileDialogMode::OpenFile;
  std::optional<Utf8TextView> title;
  std::optional<Utf8TextView> defaultPath;
  std::optional<Utf8TextView> defaultName;
  std::span<const Utf8TextView> allowedExtensions;
  std::span<const Utf8TextView> allowedContentTypes;
  bool canCreateDirectories = true;
  bool canSelectHiddenFiles = false;
  std::optional<bool> allowFiles;
  std::optional<bool> allowDirectories;
  bool defaultDirectoryOnly = false;
};

inline FileDialogConfig directoryDialogConfig(Utf8TextView defaultPath = {}) {
  FileDialogConfig config{};
  config.mode = FileDialogMode::OpenDirectory;
  if (!defaultPath.empty()) {
    config.defaultPath = defaultPath;
    config.defaultDirectoryOnly = true;
  }
  return config;
}

inline FileDialogConfig openFileDialogConfig(Utf8TextView defaultPath = {}) {
  FileDialogConfig config{};
  config.mode = FileDialogMode::OpenFile;
  if (!defaultPath.empty()) {
    config.defaultPath = defaultPath;
  }
  return config;
}

inline FileDialogConfig saveFileDialogConfig(Utf8TextView defaultPath = {},
                                             Utf8TextView defaultName = {}) {
  FileDialogConfig config{};
  config.mode = FileDialogMode::SaveFile;
  if (!defaultPath.empty()) {
    config.defaultPath = defaultPath;
  }
  if (!defaultName.empty()) {
    config.defaultName = defaultName;
  }
  return config;
}

inline FileDialogConfig openMixedDialogConfig(Utf8TextView defaultPath = {}) {
  FileDialogConfig config{};
  config.mode = FileDialogMode::Open;
  config.allowFiles = true;
  config.allowDirectories = true;
  if (!defaultPath.empty()) {
    config.defaultPath = defaultPath;
  }
  return config;
}

struct FileDialogResult {
  bool accepted = false;
  Utf8TextView path;
};

struct TextSpan {
  uint32_t offset = 0u;
  uint32_t length = 0u;
};

struct ClipboardPathsResult {
  bool available = false;
  std::span<const TextSpan> paths;
};

struct ClipboardImageResult {
  bool available = false;
  ImageSize size;
  std::span<const uint8_t> pixels;
};

struct ScreenshotConfig {
  ScreenshotScope scope = ScreenshotScope::Surface;
  bool includeHidden = false;
};

enum class PointerPhase { Down, Move, Up, Cancel };
enum class PointerDeviceType { Mouse, Touch, Pen };

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

struct DropEvent {
  uint32_t count = 0u;
  TextSpan paths;
};

struct FocusEvent {
  bool focused = false;
};

struct PowerEvent {
  std::optional<bool> lowPowerModeEnabled;
};

struct ThermalEvent {
  ThermalState state = ThermalState::Unknown;
};

enum class LifecyclePhase { Created, Suspended, Resumed, Backgrounded, Foregrounded, Destroyed };

struct LifecycleEvent { LifecyclePhase phase = LifecyclePhase::Created; };

struct Event {
  enum class Scope { Surface, Global };
  Scope scope = Scope::Surface;
  std::optional<SurfaceId> surfaceId;
  std::chrono::steady_clock::time_point time;
  std::variant<InputEvent,
               ResizeEvent,
               DropEvent,
               FocusEvent,
               PowerEvent,
               ThermalEvent,
               LifecycleEvent> payload;
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
  virtual HostResult<size_t> displays(std::span<DisplayInfo> outDisplays) const = 0;
  virtual HostResult<DisplayInfo> displayInfo(uint32_t displayId) const = 0;
  virtual HostResult<DisplayHdrInfo> displayHdrInfo(uint32_t displayId) const = 0;
  virtual HostResult<uint32_t> surfaceDisplay(SurfaceId surfaceId) const = 0;
  virtual HostStatus setSurfaceDisplay(SurfaceId surfaceId, uint32_t displayId) = 0;

  virtual HostResult<SurfaceId> createSurface(const SurfaceConfig& config) = 0;
  virtual HostStatus destroySurface(SurfaceId surfaceId) = 0;

  virtual HostResult<EventBatch> pollEvents(const EventBuffer& buffer) = 0;
  virtual HostStatus waitEvents() = 0;

  virtual HostResult<FrameBuffer> acquireFrameBuffer(SurfaceId surfaceId) = 0;
  virtual HostStatus presentFrameBuffer(SurfaceId surfaceId, const FrameBuffer& buffer) = 0;

  virtual HostStatus requestFrame(SurfaceId surfaceId, bool bypassCap) = 0;
  virtual HostStatus setFrameConfig(SurfaceId surfaceId, const FrameConfig& config) = 0;
  virtual HostResult<FrameConfig> frameConfig(SurfaceId surfaceId) const = 0;
  virtual HostResult<std::optional<std::chrono::nanoseconds>> displayInterval(SurfaceId surfaceId) const = 0;
  virtual HostStatus setSurfaceTitle(SurfaceId surfaceId, Utf8TextView title) = 0;
  virtual HostResult<SurfaceSize> surfaceSize(SurfaceId surfaceId) const = 0;
  virtual HostStatus setSurfaceSize(SurfaceId surfaceId, uint32_t width, uint32_t height) = 0;
  virtual HostResult<SurfacePoint> surfacePosition(SurfaceId surfaceId) const = 0;
  virtual HostStatus setSurfacePosition(SurfaceId surfaceId, int32_t x, int32_t y) = 0;
  virtual HostResult<SafeAreaInsets> surfaceSafeAreaInsets(SurfaceId surfaceId) const = 0;
  virtual HostStatus setCursorShape(SurfaceId surfaceId, CursorShape shape) = 0;
  virtual HostStatus setCursorImage(SurfaceId surfaceId, const CursorImage& image) = 0;
  virtual HostStatus setCursorVisible(SurfaceId surfaceId, bool visible) = 0;
  virtual HostStatus setSurfaceIcon(SurfaceId surfaceId, const WindowIcon& icon) = 0;
  virtual HostStatus setSurfaceMinimized(SurfaceId surfaceId, bool minimized) = 0;
  virtual HostStatus setSurfaceMaximized(SurfaceId surfaceId, bool maximized) = 0;
  virtual HostStatus setSurfaceFullscreen(SurfaceId surfaceId, bool fullscreen) = 0;
  virtual HostResult<size_t> clipboardTextSize() const = 0;
  virtual HostResult<Utf8TextView> clipboardText(std::span<char> buffer) const = 0;
  virtual HostStatus setClipboardText(Utf8TextView text) = 0;
  virtual HostResult<size_t> clipboardPathsTextSize() const = 0;
  virtual HostResult<size_t> clipboardPathsCount() const = 0;
  virtual HostResult<ClipboardPathsResult> clipboardPaths(std::span<TextSpan> outPaths,
                                                         std::span<char> buffer) const = 0;
  virtual HostResult<std::optional<ImageSize>> clipboardImageSize() const = 0;
  virtual HostResult<ClipboardImageResult> clipboardImage(std::span<uint8_t> buffer) const = 0;
  virtual HostStatus setClipboardImage(const ImageData& image) = 0;
  virtual HostStatus writeSurfaceScreenshot(SurfaceId surfaceId,
                                            Utf8TextView path,
                                            const ScreenshotConfig& config = {}) = 0;
  virtual HostResult<FileDialogResult> fileDialog(const FileDialogConfig& config,
                                                   std::span<char> buffer) const = 0;
  virtual HostResult<size_t> fileDialogPaths(const FileDialogConfig& config,
                                             std::span<TextSpan> outPaths,
                                             std::span<char> buffer) const = 0;
  virtual HostResult<size_t> appPathSize(AppPathType type) const = 0;
  virtual HostResult<Utf8TextView> appPath(AppPathType type, std::span<char> buffer) const = 0;
  virtual HostResult<float> surfaceScale(SurfaceId surfaceId) const = 0;
  virtual HostStatus setSurfaceMinSize(SurfaceId surfaceId, uint32_t width, uint32_t height) = 0;
  virtual HostStatus setSurfaceMaxSize(SurfaceId surfaceId, uint32_t width, uint32_t height) = 0;

  virtual HostStatus setGamepadRumble(const GamepadRumble& rumble) = 0;
  virtual HostResult<PermissionStatus> checkPermission(PermissionType type) const = 0;
  virtual HostResult<PermissionStatus> requestPermission(PermissionType type) = 0;
  virtual HostResult<uint64_t> beginIdleSleepInhibit(Utf8TextView reason) = 0;
  virtual HostStatus endIdleSleepInhibit(uint64_t token) = 0;
  virtual HostStatus setGamepadLight(uint32_t deviceId, float r, float g, float b) = 0;
  virtual HostResult<LocaleInfo> localeInfo() const = 0;
  virtual HostResult<Utf8TextView> imeLanguageTag() const = 0;
  virtual HostStatus setImeCompositionRect(SurfaceId surfaceId,
                                           int32_t x,
                                           int32_t y,
                                           int32_t width,
                                           int32_t height) = 0;
  virtual HostResult<uint64_t> beginBackgroundTask(Utf8TextView reason) = 0;
  virtual HostStatus endBackgroundTask(uint64_t token) = 0;
  virtual HostResult<uint64_t> createTrayItem(Utf8TextView title) = 0;
  virtual HostStatus updateTrayItemTitle(uint64_t trayId, Utf8TextView title) = 0;
  virtual HostStatus removeTrayItem(uint64_t trayId) = 0;
  virtual HostStatus setRelativePointerCapture(SurfaceId surfaceId, bool enabled) = 0;
  virtual HostStatus setLogCallback(LogCallback callback) = 0;

  virtual HostStatus setCallbacks(Callbacks callbacks) = 0;
};

HostResult<std::unique_ptr<Host>> createHost();

HostStatus validateFrameConfig(const FrameConfig& config, const SurfaceCapabilities& caps);

HostStatus validateAudioStreamConfig(const AudioStreamConfig& config);

AudioStreamConfig resolveAudioStreamConfig(const AudioStreamConfig& config);

FrameConfig resolveFrameConfig(const FrameConfig& config, const SurfaceCapabilities& caps);

uint32_t effectiveBufferCount(const FrameConfig& config, const SurfaceCapabilities& caps);

} // namespace PrimeHost
```

## FPS Utility (from `include/PrimeHost/Fps.h`)
```cpp
namespace PrimeHost {

struct FpsStats {
  uint32_t sampleCount = 0u;
  std::chrono::nanoseconds minFrameTime{0};
  std::chrono::nanoseconds maxFrameTime{0};
  std::chrono::nanoseconds meanFrameTime{0};
  std::chrono::nanoseconds p50FrameTime{0};
  std::chrono::nanoseconds p95FrameTime{0};
  std::chrono::nanoseconds p99FrameTime{0};
  double fps = 0.0;
};

class FpsTracker {
public:
  explicit FpsTracker(
      size_t sampleCapacity = 120u,
      std::chrono::nanoseconds reportInterval = std::chrono::seconds(1));
  explicit FpsTracker(std::chrono::nanoseconds reportInterval);

  void framePresented();
  void reset();

  FpsStats stats() const;
  size_t sampleCapacity() const;
  size_t sampleCount() const;
  bool isWarmedUp() const;
  bool shouldReport();
  std::chrono::nanoseconds reportInterval() const;
  void setReportInterval(std::chrono::nanoseconds interval);

private:
  size_t sampleCapacity_ = 0u;
  size_t sampleCount_ = 0u;
  std::chrono::nanoseconds reportInterval_{0};
};

FpsStats computeFpsStats(std::span<const std::chrono::nanoseconds> frameTimes);

} // namespace PrimeHost
```

## Timing Utility (from `include/PrimeHost/Timing.h`)
```cpp
namespace PrimeHost {

using SteadyClock = std::chrono::steady_clock;

SteadyClock::time_point now();

void sleepFor(std::chrono::nanoseconds duration);

void sleepUntil(SteadyClock::time_point target);

} // namespace PrimeHost
```

## Audio Output (Draft, from `docs/audio.md`)
```cpp
namespace PrimeHost {

using AudioDeviceId = uint64_t;

enum class SampleFormat {
  Float32,
  Int16,
};

struct AudioFormat {
  uint32_t sampleRate = 48000;
  uint16_t channels = 2;
  SampleFormat format = SampleFormat::Float32;
  bool interleaved = true;
};

struct AudioStreamConfig {
  AudioFormat format{};
  uint32_t bufferFrames = 512;
  uint32_t periodFrames = 256;
  std::chrono::nanoseconds targetLatency{0};
};

struct AudioDeviceInfo {
  AudioDeviceId id{};
  Utf8TextView name;
  bool isDefault = false;
  AudioFormat preferredFormat{};
};

struct AudioCallbackContext {
  uint64_t frameIndex = 0;
  std::chrono::steady_clock::time_point time;
  uint32_t requestedFrames = 0;
  bool isUnderrun = false;
};

struct AudioDeviceEvent {
  AudioDeviceId deviceId = 0;
  bool connected = true;
  bool isDefault = false;
};

struct AudioCallbacks {
  std::function<void(const AudioDeviceEvent&)> onDeviceEvent;
};

using AudioCallback = void (*)(std::span<float> interleaved,
                               const AudioCallbackContext& ctx,
                               void* userData);

class AudioHost {
public:
  virtual ~AudioHost() = default;

  virtual HostResult<size_t> outputDevices(std::span<AudioDeviceInfo> outDevices) const = 0;
  virtual HostResult<AudioDeviceInfo> outputDeviceInfo(AudioDeviceId deviceId) const = 0;
  virtual HostResult<AudioDeviceId> defaultOutputDevice() const = 0;

  virtual HostStatus openStream(AudioDeviceId deviceId,
                                const AudioStreamConfig& config,
                                AudioCallback callback,
                                void* userData) = 0;

  virtual HostStatus startStream() = 0;
  virtual HostStatus stopStream() = 0;
  virtual HostStatus closeStream() = 0;

  virtual HostResult<AudioStreamConfig> activeConfig() const = 0;

  virtual HostStatus setCallbacks(AudioCallbacks callbacks) = 0;
};

HostResult<std::unique_ptr<AudioHost>> createAudioHost();

} // namespace PrimeHost
```

## Host Extensions (Draft)
These extension methods are part of `Host` and may return `HostErrorCode::Unsupported`
on platforms that do not implement them.
