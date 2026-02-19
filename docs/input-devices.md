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

### VID/PID Table (Placeholder)
Fill these once we finalize the initial database and verify platform IDs.

| Vendor | Model | VID | PID | Layout | Notes |
| --- | --- | --- | --- | --- | --- |
| Microsoft | Xbox 360 Controller | TBD | TBD | XInput | Multiple variants exist. |
| Microsoft | Xbox One Controller | TBD | TBD | XInput | Multiple variants exist. |
| Microsoft | Xbox Series Controller | TBD | TBD | XInput | Multiple variants exist. |
| Sony | DualShock 4 | TBD | TBD | DS4 | USB/Bluetooth IDs differ. |
| Sony | DualSense | TBD | TBD | DualSense | USB/Bluetooth IDs differ. |
| Nintendo | Switch Pro Controller | TBD | TBD | Nintendo | USB/Bluetooth IDs differ. |
| Logitech | F310/F710 (XInput) | TBD | TBD | XInput | Requires XInput mode. |
| 8BitDo | XInput Mode | TBD | TBD | XInput | Varies by model. |

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

## Device Events
- `DeviceEvent` is emitted on connect/disconnect.
- `deviceId` is stable for the device lifetime.
- Device capabilities and mapping status should be queryable from the host.

## Open Questions
- JSON vs. compiled-in table for the initial database.
- Which gamepad models to include in the initial seed list.
- Whether to expose the raw platform mapping for debugging.
