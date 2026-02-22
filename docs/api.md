# PrimeHost API Surface (Draft)

This is a curated summary of the public API as defined in the headers. It is intended to remain
platform-neutral and stable across backends.

## Core Types
- `SurfaceId`: opaque surface handle.
- `FrameTiming`: monotonic time + delta for frame pacing.
- `FrameDiagnostics`: target vs actual interval plus missed/deadline signals.
- `FrameConfig`: presentation and pacing configuration per surface.
- `SurfaceConfig`: surface creation settings.
- `SurfaceSize`: logical surface size in points.
- `SurfacePoint`: logical surface position in points.
- `DisplayInfo`: display bounds, scale, refresh, and identity.
- `PermissionType`, `PermissionStatus`: permission categories and states.
- `AppPathType`: standard app path categories.
- `FileDialogMode`, `FileDialogConfig`, `FileDialogResult`: native file dialog settings and results.
- `LocaleInfo`: locale language/region tags.
- `PresentMode`, `FramePolicy`, `FramePacingSource`, `ColorFormat`: presentation enums.
- `EventBuffer` / `EventBatch`: caller-provided event storage and text buffer views.
- `HostStatus`, `HostResult<T>`, `HostError`: error reporting for host operations using `std::expected`.
Full signatures are listed in `docs/api-signatures.md`.

## Capabilities
- Surface capabilities (buffer counts, tearing support, vsync modes).
- Input capabilities (pointer types, pen pressure, gamepad rumble).
Capability queries return `HostResult<T>` (`std::expected`) values.
Device lists are filled into caller-provided spans with a `size_t` result.

## Displays
- Enumerate displays/monitors.
- Surface-to-display mapping when possible.
- Refresh rate, DPI/scale, color space, and bounds.
- Implemented: `displays`, `displayInfo`, `surfaceDisplay`, `setSurfaceDisplay` (bounds/scale/refresh; assignment policies still draft).

## Window Geometry (Draft)
- Get/set window position and size in logical pixels (points).
- Optional min/max size constraints.
- Defaults: no constraints; size/position are controlled by the OS until explicitly set.
- Implemented: `surfaceSize`, `setSurfaceSize`, `surfacePosition`, `setSurfacePosition`, `setSurfaceMinSize`, `setSurfaceMaxSize` (size/position only).
## DPI / Scale (Draft)
- Query per-surface backing scale factor for high-DPI rendering.
- Implemented: `surfaceScale`.

## Window State (Draft)
- Minimize/maximize/fullscreen controls per surface.
- Implemented: `setSurfaceMinimized`, `setSurfaceMaximized`, `setSurfaceFullscreen`.

## Cursor (Draft)
- Standard cursor shapes plus custom cursor image support.
- Defaults: system arrow cursor; cursor visible unless explicitly hidden.
- Implemented: `setCursorVisible` (visibility only).

## Safe-Area Insets (Draft)
- Per-surface safe-area insets for notches/rounded corners.
- Default insets: all zeros when not applicable.

## HDR / EDR (Draft)
- Query display HDR/EDR capability and nominal luminance ranges.
- Default: HDR disabled unless explicitly enabled per surface or display.

## Permissions (Draft)
- Camera/microphone/location/notifications/photos permission queries and requests.
- Default: unknown until queried; platforms may require prompts.
- Implemented: `checkPermission` for camera/microphone/notifications/location/photos on macOS.
- Implemented: `requestPermission` for camera/microphone/notifications/photos on macOS.

## Idle Sleep / Screensaver (Draft)
- Inhibit idle sleep or screensaver while critical work is active.
- Default: no inhibition.
- Implemented: `beginIdleSleepInhibit`, `endIdleSleepInhibit` (macOS).

## Controller LEDs / Lightbar (Draft)
- Set per-gamepad LED or lightbar color when supported.
- Default: system/driver color.
- Implemented: `setGamepadLight` (macOS 11+, requires controller light support).

## Localization / IME Language (Draft)
- Query current locale and active IME language tag.
- Default: platform locale; IME language may be empty when unavailable.
- IME composition caret/selection placement to position candidate windows.
- Implemented: `localeInfo`, `imeLanguageTag` (macOS).

## Background Tasks (Draft)
- Keep-alive/background task tokens for mobile platforms.
- Default: unsupported on desktop.
- Implemented: `beginBackgroundTask`, `endBackgroundTask` (macOS).

## System Tray / Menu Bar (Draft)
- Create/remove tray items and menu entries.
- Default: unsupported on platforms without a tray/menu bar.
- Implemented: `createTrayItem`, `updateTrayItemTitle`, `removeTrayItem` (macOS).

## Power / Thermal (Draft)
- Global events for low-power mode and thermal warnings.
- Default: no events on platforms without support.

## Pointer Lock / Capture (Draft)
- Relative pointer mode and explicit capture/lock controls.
- Default: pointer lock disabled.
- API: `setRelativePointerCapture(surfaceId, enabled)`.

## Clipboard Formats (Draft)
- Text, file paths, and image data where supported.
- Default: text-only on platforms without richer formats.
- Implemented: `clipboardTextSize`, `clipboardText`, `setClipboardText` (text-only).

## File Dialogs (Draft)
- Native open/save panels.
- Optional file extension filters via `FileDialogConfig::allowedExtensions`.
- Optional UTType identifier filters via `FileDialogConfig::allowedContentTypes`.
- Optional default filename via `FileDialogConfig::defaultName`.
- `canCreateDirectories` controls save panel directory creation.
- `canSelectHiddenFiles` reveals hidden items where supported.
- `defaultDirectoryOnly` treats `defaultPath` as a directory even when it points to a file.
- Helper: `directoryDialogConfig(defaultPath)` configures directory-only selection.
- Helper: `openFileDialogConfig(defaultPath)` configures a basic open-file dialog.
- Helper: `saveFileDialogConfig(defaultPath, defaultName)` configures a basic save dialog.
- Helper: `openMixedDialogConfig(defaultPath)` configures a mixed file+directory open dialog.
- Modes: open file, open directory, open (files + directories), save file.
- `allowFiles`/`allowDirectories` override selection for `Open`.
- Implemented: `fileDialog`, `fileDialogPaths` (macOS).

Example (multi-select open):
```cpp
std::array<PrimeHost::TextSpan, 8> paths;
std::array<char, 4096> pathBytes;
PrimeHost::FileDialogConfig config{};
config.mode = PrimeHost::FileDialogMode::Open;
config.allowFiles = true;
config.allowDirectories = false;
PrimeHost::Utf8TextView txtExt{"txt"};
config.allowedExtensions = std::span<const PrimeHost::Utf8TextView>(&txtExt, 1);

auto result = host.fileDialogPaths(config, paths, pathBytes);
if (result && result.value() > 0u) {
  for (size_t i = 0; i < result.value(); ++i) {
    const auto span = paths[i];
    std::string_view path{pathBytes.data() + span.offset, span.length};
    // use path
  }
}
```

## App Paths (Draft)
- Standard directories for user data, cache, and config.
- Defaults follow platform conventions (e.g., `~/Library`, `AppData`, `~/.config`).
- App path types include `UserData`, `Cache`, `Config`, `Logs`, `Temp`.
- Implemented: `appPathSize`, `appPath` (macOS).

## Events
- `Event`: tagged union of input, resize, and lifecycle events.
- `DeviceEvent`: connect/disconnect notification for input devices.
- Input events: `PointerEvent`, `KeyEvent`, `TextEvent`, `ScrollEvent`, `GamepadButtonEvent`, `GamepadAxisEvent`.
- All input events carry `deviceId`.
- Pointer events unify mouse/touch/pen; optional fields include delta, pressure, tilt, twist, and distance.
- Gamepad buttons may include an optional analog value; axes always include a float value.
- Text input is UTF-8; `TextEvent` carries a `TextSpan` pointing into the `EventBatch` text buffer.
- Text spans are valid for the duration of the callback or until the next `pollEvents()` call.
- IME composition events (draft).
Input ranges and coordinate conventions are defined in `docs/input-semantics.md`.

Event scoping:
- `Event::scope == Surface` requires `surfaceId` to be set.
- `Event::scope == Global` requires `surfaceId` to be empty.

### TextSpan Usage (Example)
```cpp
std::array<PrimeHost::Event, 256> events;
std::array<char, 4096> textBytes;

PrimeHost::EventBuffer buffer{
    .events = events,
    .textBytes = textBytes,
};

PrimeHost::EventBatch batch = host.pollEvents(buffer);

for (const PrimeHost::Event& evt : batch.events) {
  if (auto* input = std::get_if<PrimeHost::InputEvent>(&evt.payload)) {
    if (auto* text = std::get_if<PrimeHost::TextEvent>(input)) {
      const char* base = batch.textBytes.data() + text->text.offset;
      std::string_view utf8{base, text->text.length};
      handleText(utf8);
    }
  }
}
```
- Drag-and-drop file events (draft).
- Focus/activation events per surface (draft).

## Host Interface
- `createHost() -> HostResult<std::unique_ptr<Host>>`
- `Host::hostCapabilities() -> HostResult<HostCapabilities>`
- `Host::surfaceCapabilities(SurfaceId) -> HostResult<SurfaceCapabilities>`
- `Host::deviceInfo(deviceId) -> HostResult<DeviceInfo>`
- `Host::deviceCapabilities(deviceId) -> HostResult<DeviceCapabilities>`
- `Host::devices(span<DeviceInfo>) -> HostResult<size_t>`
- `Host::displays(span<DisplayInfo>) -> HostResult<size_t>`
- `Host::displayInfo(displayId) -> HostResult<DisplayInfo>`
- `Host::surfaceDisplay(surfaceId) -> HostResult<uint32_t>`
- `Host::setSurfaceDisplay(surfaceId, displayId) -> HostStatus`
- `Host::createSurface(const SurfaceConfig&) -> HostResult<SurfaceId>`
- `Host::destroySurface(SurfaceId) -> HostStatus`
- `Host::pollEvents(const EventBuffer&) -> HostResult<EventBatch>` and `waitEvents()`
- `Host::requestFrame`, `setFrameConfig`, `frameConfig`, `displayInterval`, `setSurfaceTitle`, `surfaceSize`, `setSurfaceSize`, `surfacePosition`, `setSurfacePosition`, `setCursorVisible`, `setSurfaceMinimized`, `setSurfaceMaximized`, `setSurfaceFullscreen`, `clipboardTextSize`, `clipboardText`, `setClipboardText`, `surfaceScale`, `setSurfaceMinSize`, `setSurfaceMaxSize`
- `Host::appPathSize`, `appPath`
- `Host::fileDialog`, `fileDialogPaths`
- `Host::setGamepadRumble`
- `Host::checkPermission`, `requestPermission`
- `Host::beginIdleSleepInhibit`, `endIdleSleepInhibit`
- `Host::setGamepadLight`
- `Host::localeInfo`, `imeLanguageTag`
- `Host::beginBackgroundTask`, `endBackgroundTask`
- `Host::createTrayItem`, `updateTrayItemTitle`, `removeTrayItem`
- `Host::setCallbacks` (native callbacks use `EventBatch`; frame callbacks include `FrameDiagnostics`).

## Validation Helpers
- `validateFrameConfig(const FrameConfig&, const SurfaceCapabilities&)` (see `PrimeHost/FrameConfigValidation.h`).
- `validateAudioStreamConfig(const AudioStreamConfig&)` (see `PrimeHost/AudioConfigValidation.h`).

## Default Helpers
- `resolveAudioStreamConfig(const AudioStreamConfig&)` (see `PrimeHost/AudioConfigDefaults.h`).
- `resolveFrameConfig(const FrameConfig&, const SurfaceCapabilities&)` (see `PrimeHost/FrameConfigDefaults.h`).
- `effectiveBufferCount(const FrameConfig&, const SurfaceCapabilities&)` (see `PrimeHost/FrameConfigUtil.h`).

### Error Handling (Example)
```cpp
auto caps = host.surfaceCapabilities(surfaceId);
if (!caps) {
  return;
}

auto batch = host.pollEvents(buffer);
if (!batch) {
  return;
}

for (const auto& evt : batch->events) {
  // ...
}
```
- Haptics for supported devices (draft).
- Window icon updates (draft).
- Offscreen/headless surfaces (draft).
- Clipboard read/write and IME composition events (draft).
- Cursor visibility/confine/relative pointer mode (draft).
- Window state controls and DPI/scale queries (draft).
- Timing utilities for sleep/pacing (draft).
- Host-level logging callback/hook (draft).
- Window geometry APIs (draft).
- Custom cursor images (draft).
- Safe-area insets (draft).
- HDR/EDR reporting (draft).
- Power/thermal events (draft).
- Pointer lock/capture (draft).
- Clipboard formats beyond text (draft).
- App path discovery (draft).
- Permissions API (draft).
- Idle sleep/screen saver inhibition (draft).
- Controller LEDs/lightbar (draft).
- Localization/IME language info (draft).
- Background task hooks (draft).
- System tray/menu-bar integration (draft).
- IME composition caret/selection placement (draft).

## FPS Utility
- `FpsTracker`, `FpsStats`, `computeFpsStats`
- Internal timing, rolling sample capacity, report throttling via `shouldReport()`.

## Audio Output
Audio APIs are defined in `docs/audio.md` and are intended to be a low-level output layer only.

## Header References
- `include/PrimeHost/Host.h`
- `include/PrimeHost/Fps.h`
- `include/PrimeHost/PrimeHost.h`
