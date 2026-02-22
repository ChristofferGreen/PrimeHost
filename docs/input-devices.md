# PrimeHost Input Devices

## Scope
This document defines how PrimeHost represents and configures input devices, with emphasis on gamepads.

## Goals
- Provide consistent control IDs across platforms for common gamepads.
- Avoid per-event allocations and minimize per-frame work.
- Keep platform-specific mapping internal to PrimeHost.

## Gamepad Database
PrimeHost should include a small, curated database of common/popular gamepads. The database is used to
auto-map platform-specific button/axis indices into PrimeHost control IDs.

### Seed List (Initial)
- Xbox 360 / Xbox One / Xbox Series controllers (XInput layout).
- Sony DualShock 4 (DS4 layout).
- Sony DualSense (DualSense layout).
- Nintendo Switch Pro Controller (Nintendo layout).
- Logitech F310/F710 when in XInput mode (maps like Xbox).
- 8BitDo controllers when set to XInput mode (maps like Xbox).

### VID/PID Table (Initial)
These are common USB VID/PID pairs for the seed list. Some controllers expose multiple IDs (USB
vs. Bluetooth) so the list is not exhaustive.

| Vendor | Model | VID | PID | Layout | Notes |
| --- | --- | --- | --- | --- | --- |
| Microsoft | Xbox 360 Controller | 0x045E | 0x028E | XInput | USB. Multiple variants exist. |
| Microsoft | Xbox One Controller | 0x045E | 0x02D1, 0x02DD, 0x02E0, 0x02EA | XInput | Common USB variants. |
| Microsoft | Xbox Series Controller | 0x045E | 0x0B12 | XInput | USB. |
| Sony | DualShock 4 | 0x054C | 0x05C4, 0x09CC | DS4 | USB variants. Bluetooth IDs differ. |
| Sony | DualSense | 0x054C | 0x0CE6 | DualSense | USB. Bluetooth IDs differ. |
| Nintendo | Switch Pro Controller | 0x057E | 0x2009 | Nintendo | USB. Bluetooth IDs differ. |
| Logitech | F310/F710 (XInput) | 0x046D | 0xC21D (F310), 0xC21F (F710) | XInput | Requires XInput mode. |
| 8BitDo | SF30 Pro (XInput) | 0x2DC8 | 0x6000 | XInput | Example model; other 8BitDo PIDs exist. |

### macOS Matching (Current)
On macOS, PrimeHost uses `GCController` and performs a case-insensitive name match to seed initial
capabilities for common controllers. When VID/PID values are available via HID, PrimeHost will
prefer those entries. The current matching tokens are:
- `xbox`
- `dualshock`
- `dualsense`
- `switch pro`
- `8bitdo`
- `f310`
- `f710`

### Required Fields
- Vendor ID (VID)
- Product ID (PID)
- Device name
- Layout (Xbox, PlayStation, Nintendo, Generic)
- Button mapping (platform index -> PrimeHost control ID)
- Axis mapping (platform index -> PrimeHost control ID)
- Optional deadzone and normalization rules

### Auto-Setup Behavior
- On device connect, match VID/PID against the database and apply the mapping.
- If no match is found, fall back to a generic mapping and expose raw indices.
- Device connect events should include whether a mapping was found.

### Control ID Guidelines
- Keep control IDs stable across platforms.
- Separate button-style controls from analog axes (triggers are axes).
- Use the same IDs for equivalent physical controls across layouts.

### Canonical Control IDs
- Buttons use `GamepadButtonId` (see `docs/input-semantics.md`).
- Axes use `GamepadAxisId` (see `docs/input-semantics.md`).

## Device Events
- `DeviceEvent` is emitted on connect/disconnect.
- `deviceId` is stable for the device lifetime.
- Device capabilities and mapping status should be queryable from the host.

## Open Questions
- JSON vs. compiled-in table for the initial database.
- Which gamepad models to include in the initial seed list.
- Whether to expose the raw platform mapping for debugging.
