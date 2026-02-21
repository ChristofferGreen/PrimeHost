# PrimeHost Input Semantics

This document defines coordinate systems, units, and ranges for PrimeHost input events.

## Coordinates and Time
- Pointer coordinates (`x`, `y`) are surface-local logical pixels (points).
- Origin is top-left, +X right, +Y down.
- Pointer deltas use the same units and sign convention.
- `ResizeEvent::scale` is the device pixel ratio for the surface.
- Event timestamps use `std::chrono::steady_clock`.

## Keyboard
- `KeyEvent::keyCode` uses USB HID Usage codes (layout-independent physical key).
- `KeyEvent::modifiers` uses `KeyModifier` bit flags.
- Text input is delivered only via `TextEvent` (UTF-8), not synthesized from key events.

## Pointer (Mouse/Touch/Pen)
- `PointerEvent::deviceType` distinguishes mouse, touch, and pen.
- `pressure` range: [0, 1], 0 means no contact.
- `tiltX` / `tiltY` in degrees, range [-90, 90].
- `twist` in degrees, range [0, 360].
- `distance` in millimeters from the surface, if supported.
- `buttonMask` uses a stable bit mapping:
  - Bit 0: left
  - Bit 1: right
  - Bit 2: middle
  - Bit 3: back
  - Bit 4: forward

## Scroll
- `deltaX` / `deltaY` represent either pixels or lines.
- `ScrollEvent::isLines = true` means deltas are in lines; otherwise pixels.

## Gamepad
- `controlId` maps to the canonical layout defined in `docs/input-devices.md`.
- Axis values are in [-1, 1]; triggers in [0, 1].
- `GamepadButtonEvent::value` (if present) is in [0, 1].
