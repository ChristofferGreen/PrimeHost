# PrimeHost API Surface (Draft)

This is a curated summary of the public API as defined in the headers. It is intended to remain
platform-neutral and stable across backends.

## Core Types
- `SurfaceId`: opaque surface handle.
- `FrameTiming`: monotonic time + delta for frame pacing.
- `FrameConfig`: presentation and pacing configuration per surface.
- `SurfaceConfig`: surface creation settings.
- `PresentMode`, `FramePolicy`, `FramePacingSource`, `ColorFormat`: presentation enums.
- `EventBuffer` / `EventBatch`: caller-provided event storage and text buffer views.
- `HostStatus`, `HostResult<T>`, `HostError`: error reporting for host operations using `std::expected`.

## Capabilities
- Surface capabilities (buffer counts, tearing support, vsync modes).
- Input capabilities (pointer types, pen pressure, gamepad rumble).
Capability queries return `HostResult<T>` (`std::expected`) values.
Device lists are filled into caller-provided spans with a `size_t` result.

## Displays
- Enumerate displays/monitors.
- Surface-to-display mapping when possible.
- Refresh rate, DPI/scale, color space, and bounds.
- Surface assignment and preferred display policies (draft).

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
- `Host::hostCapabilities() -> HostResult<HostCapabilities>`
- `Host::surfaceCapabilities(SurfaceId) -> HostResult<SurfaceCapabilities>`
- `Host::deviceInfo(deviceId) -> HostResult<DeviceInfo>`
- `Host::deviceCapabilities(deviceId) -> HostResult<DeviceCapabilities>`
- `Host::devices(span<DeviceInfo>) -> HostResult<size_t>`
- `Host::createSurface(const SurfaceConfig&) -> HostResult<SurfaceId>`
- `Host::destroySurface(SurfaceId) -> HostStatus`
- `Host::pollEvents(const EventBuffer&) -> HostResult<EventBatch>` and `waitEvents()`
- `Host::requestFrame`, `setFrameConfig`
- `Host::setGamepadRumble`
- `Host::setCallbacks` (native callbacks use `EventBatch`).

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
- Native file dialogs (draft).
- Haptics for supported devices (draft).
- Window icon and title updates (draft).
- Offscreen/headless surfaces (draft).
- Clipboard read/write and IME composition events (draft).
- Cursor visibility/confine/relative pointer mode (draft).
- Window state controls (minimize/maximize/fullscreen) and DPI/scale queries (draft).
- Timing utilities for sleep/pacing (draft).
- Host-level logging callback/hook (draft).

## FPS Utility
- `FpsTracker`, `FpsStats`, `computeFpsStats`
- Internal timing, rolling sample capacity, report throttling via `shouldReport()`.

## Audio Output
Audio APIs are defined in `docs/audio.md` and are intended to be a low-level output layer only.

## Header References
- `include/PrimeHost/Host.h`
- `include/PrimeHost/Fps.h`
- `include/PrimeHost/PrimeHost.h`
