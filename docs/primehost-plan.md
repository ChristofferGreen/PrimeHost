# PrimeHost Plan

## Scope
PrimeHost is the OS integration layer that provides window/surface creation, input, timing, and presentation. It feeds frame timing and input into PrimeStage/PrimeFrame/PrimeManifest and owns the platform-specific event loop integration.

## Library Hierarchy
- `PrimeManifest`: low-level renderer that draws to a framebuffer.
- `PrimeFrame`: layout + rect hierarchy that builds render batches for PrimeManifest.
- `PrimeStage`: app/UI state and widget logic that emits rect hierarchies for PrimeFrame.
- `PrimeHost`: OS integration (windows, input, timing, presentation) feeding PrimeStage.

## Target Platforms
- macOS (Apple Silicon)
- Linux (x86)
- Windows 7+ (x86)
- Android
- iOS (phone/tablet)
- watchOS

## Goals
- Low-latency presentation of a framebuffer produced by PrimeManifest.
- Consistent surface and input APIs across platforms.
- Event-driven and continuous rendering modes with explicit frame pacing.
- Minimal shared mutable state across threads.

## Phase 1: macOS (Apple Silicon)
Initial implementation targets macOS on Apple Silicon to validate the event loop, input delivery, and presentation model.
Cross-platform considerations remain part of the public API design.

### Phase 1 Scope
- Surface creation and teardown backed by Metal and `CAMetalLayer`.
- Input delivery for pointer, keyboard, text, and scroll.
- Frame pacing using `CVDisplayLink` and host-side present gating.
- External loop mode as the default.
- Event-driven frames on input/resize with a single cap bypass.

### Phase 1 Non-Goals
- Full app lifecycle management beyond required suspend/resume and foreground/background.
- Platform-specific UI conveniences (menus, accessibility, IME advanced features).
- GPU abstraction beyond Metal presentation.

## Cross-Platform Guardrails
- Keep public headers free of platform-specific types.
- Use capability flags for optional features (tearing, mailbox, extra buffers).
- Avoid macOS-only semantics leaking into `Host` or `Event` types.
- Prefer deterministic, documented mapping of presentation knobs per platform.

## Milestones (Draft)
1. Host scaffolding with macOS backend stubs and build wiring.
2. Surface creation and basic event delivery (resize + input).
3. Frame pacing and present gating with `CVDisplayLink`.
4. Presentation configuration integration (`FrameConfig`).

## Non-Goals (for now)
- GPU abstraction beyond platform presentation.
- Full app lifecycle management beyond what the OS requires.
- Cross-process rendering or remote surfaces.

## Responsibilities
- Surface/window creation and teardown.
- Input delivery (pointer/touch/keyboard/text/scroll/gamepad when present).
- Timing and frame pacing signals.
- Presentation of PrimeManifest framebuffers (with or without PrimeFrame).
- OS lifecycle events (suspend/resume, background/foreground, orientation changes).
- Gamepad auto-mapping using a curated device database.
- Audio output device selection and low-latency playback.
- Capability queries for surfaces and devices.
- Capability queries return `HostResult<T>` using `std::expected`.
- Clipboard access and text input lifecycle (IME composition).
- Cursor control and relative pointer mode.
- Window state controls (minimize/maximize/fullscreen).
- Unified timing utilities for sleeping/pacing.
- Display/monitor enumeration and properties (refresh rate, scale, color space).
- Drag-and-drop file events.
- Focus/activation events per surface.
- Host-level logging hooks.
- Native file dialogs (open/save) where appropriate.
- Haptics for supported devices (including watchOS crown/impact feedback).
- Window icon and title updates.
- Offscreen/headless surfaces for render-to-texture or CI.
- Multi-display policies and surface-to-display assignment.
- Window geometry APIs (position/size/constraints).
- Cursor shapes and custom cursor images.
- Safe-area insets for mobile/notched displays.
- HDR/EDR display capability reporting.
- Power/thermal events for pacing decisions.
- Pointer lock/capture controls.
- Clipboard formats beyond text (images/paths).
- App path discovery (data/cache/config directories).
- Permissions API (camera/mic/location).
- Idle sleep / screensaver inhibition.
- Controller LEDs / lightbar controls.
- Localization and IME language info.
- Background task/keep-alive hooks (mobile).
- System tray/menu-bar integration (desktop).
- IME composition caret/selection placement for candidate windows.

## Non-Responsibilities
- UI layout, widget state, focus policy.
- Render batching or text shaping.
- Business logic or app state.

## Proposed API Surface (Draft)
- `Host`: create/destroy surfaces, poll or receive events, request frames.
- `SurfaceId`: opaque window/view handle.
- `Event`: tagged union of input, resize, and lifecycle events.
- `DeviceEvent`: connected/disconnected notifications for input devices.
- Audio output APIs (see `docs/audio.md`).
Additional utilities: capability queries, clipboard, cursor control, window state controls, timing utilities.
Additional utilities: window geometry, cursor shapes, safe-area insets, HDR/EDR info, power/thermal events, pointer lock, clipboard formats, app paths.
- `FrameTiming`: monotonic time + delta.
- `FrameDiagnostics`: target vs actual intervals for detecting missed frame budgets.
- `FramePolicy`: event-driven, continuous, or capped.
- `FrameConfig`: pacing config per surface.
- `FpsTracker` / `FpsStats`: utility for high-quality FPS and frame-time statistics.
Text input encoding: UTF-8 with explicit storage buffers. `TextEvent` carries a span into the `EventBatch` text buffer.
Text spans are valid for the duration of the callback in native mode, or until the next `pollEvents()` call in external mode.
All input events include a `deviceId`; pointer events unify mouse/touch/pen with optional delta/pressure/tilt data.
Gamepad input uses separate button/axis events; button events may include an optional analog value. Rumble is controlled via `setGamepadRumble`.
Gamepad mappings are configured from a curated device database (see `docs/input-devices.md`).
Input coordinate systems and ranges are defined in `docs/input-semantics.md`.
Events are explicitly scoped: `Event::Scope::Surface` requires a surface ID; `Event::Scope::Global` does not.
Draft C++ API header: `include/PrimeHost/Host.h`.

## Presentation Config
See `presentation-config.md` for the authoritative presentation knobs, frame pacing policy, and per-platform recommendations.

## Loop Modes
- `External`: app drives polling and frame requests (desktop default).
- `Native`: OS drives callbacks (mobile/watch default).
- `Hybrid`: event-driven frames on input/resize + optional cap.

## Update Loop API (Draft)
- `pollEvents(EventBuffer)` for external loop (desktop).
- `requestFrame(surface, bypassCap)` for event-driven updates.
- `setFrameConfig(surface, FrameConfig)` for pacing.
- Optional `waitEvents()` for low-power external loops.
- `setCallbacks()` for native/OS-driven delivery (callbacks receive `EventBatch`).

## Threading Model (Recommended)
- Main thread: OS events + presentation (required by many OS APIs).
- Render thread: build frame data + PrimeManifest framebuffer.
- Worker threads: optional layout/shaping/batching.
- Use double-buffered frame state and ownership transfer for the framebuffer.

## macOS Resize/Stutter Workarounds (from PathSpaceOS)
- Set `layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize`.
- Use CALayer delegate `displayLayer` to render during live resize.
- Default to CVDisplayLink; CAMetalDisplayLink blocks `nextDrawable()` in resize path.
- On resize events: sync size, request immediate frame, bypass cap once.
- Keep `layer.drawableSize` matched to framebuffer to prevent flashing.
- Avoid piling `setNeedsDisplay` dispatches (single pending flag).

## Open Questions
- Preferred render backend per platform (D3D11/Vulkan/Metal/GL).
- Tearing policy on desktop in low-latency mode.
- Surface limits for mobile/watch.
- Should PrimeHost own a render thread or leave it to the app?
- How to expose native handles (HWND/NSWindow/ANativeWindow) when needed.

## Open Decisions (Draft)
- Event model details for global events across polling vs callback modes.
- Lifecycle event mapping per platform (suspend/resume/background/foreground).
- Interaction between `FramePolicy` and `PresentMode` when they conflict.
- Definition of `FramePacingSource::HostLimiter` behavior.
- Canonical gamepad control IDs and axis/trigger mapping details.
- IME composition event shapes and data fields.
- Whether audio output lives under `Host` or a separate interface.
- Offscreen surface creation and lifecycle rules.
- Display selection policy and surface migration rules.
