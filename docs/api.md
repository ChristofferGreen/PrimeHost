# PrimeHost API Surface (Draft)

This is a curated summary of the public API as defined in the headers. It is intended to remain
platform-neutral and stable across backends.

## Core Types
- `SurfaceId`: opaque surface handle.
- `FrameTiming`: monotonic time + delta for frame pacing.
- `FrameConfig`: presentation and pacing configuration per surface.
- `SurfaceConfig`: surface creation settings.
- `PresentMode`, `FramePolicy`, `FramePacingSource`, `ColorFormat`: presentation enums.

## Capabilities
- Surface capabilities (buffer counts, tearing support, vsync modes).
- Input capabilities (pointer types, pen pressure, gamepad rumble).

## Displays
- Enumerate displays/monitors.
- Surface-to-display mapping when possible.
- Refresh rate, DPI/scale, color space, and bounds.
- Surface assignment and preferred display policies (draft).

## Events
- `Event`: tagged union of input, resize, and lifecycle events.
- `DeviceEvent`: connect/disconnect notification for input devices.
- Input events: `PointerEvent`, `KeyEvent`, `TextEvent`, `ScrollEvent`, `GamepadEvent`.
- All input events carry `deviceId`.
- Pointer events unify mouse/touch/pen and include pressure/tilt/twist when available.
- Text input is UTF-8 (`std::string_view`) with a callback-lifetime guarantee.
- IME composition events (draft).
- Drag-and-drop file events (draft).
- Focus/activation events per surface (draft).

## Host Interface
- `Host::createSurface`, `destroySurface`
- `Host::pollEvents`, `waitEvents`
- `Host::requestFrame`, `setFrameConfig`
- `Host::setGamepadRumble`
- `Host::setCallbacks`
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
