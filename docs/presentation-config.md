# PrimeHost Presentation Config

## Scope
This document captures presentation configuration for PrimeHost. It focuses on low-latency, high-FPS presentation of a framebuffer produced by PrimeManifest, sometimes via PrimeFrame and sometimes via other paths.
It is the single source of truth for presentation knobs, frame pacing rules, and platform-specific presentation recommendations.

## Library Hierarchy Context
- PrimeHost sits below PrimeStage and PrimeFrame, presenting framebuffers from PrimeManifest.

## Goals
- Provide a small set of cross-platform presentation knobs.
- Keep defaults aligned with low-latency interactive UI.
- Respect platform constraints (swapchain modes, vsync, tearing).

## Core Config (Draft)
| Field | Meaning | Default |
| --- | --- | --- |
| `PresentMode` | High-level mode selector. | `LowLatency` |
| `Vsync` | Enable vsync when supported. | `true` |
| `AllowTearing` | Allow tearing when supported. | `false` |
| `MaxFrameLatency` | Max queued frames. | `1` |
| `BufferCount` | Swapchain or layer buffer count. | `2` |
| `ColorFormat` | Framebuffer color format. | `B8G8R8A8_UNORM` |
| `FramePacingSource` | Platform vs host limiter. | `Platform` |

## macOS Defaults (Phase 1)
These defaults guide the initial Apple Silicon implementation and are expected to map directly to Metal + `CAMetalLayer`.

| Setting | Default | Rationale |
| --- | --- | --- |
| `PresentMode` | `LowLatency` | Prioritize interactive responsiveness. |
| `Vsync` | `true` | Required for stable presentation on macOS. |
| `AllowTearing` | `false` | Not supported on macOS. |
| `MaxFrameLatency` | `1` | Avoid excessive queueing. |
| `BufferCount` | `2` | Lower latency; allow `3` for `Smooth`. |
| `FramePacingSource` | `Platform` | Use `CVDisplayLink` pacing. |

## PresentMode Policy (Draft)
| Mode | Policy |
| --- | --- |
| `LowLatency` | Prefer low latency over throughput; 2 buffers; vsync on; present gate enabled. |
| `Smooth` | Favor stable cadence; 3 buffers; vsync on; present gate enabled. |
| `Uncapped` | No cap; present gate optional; vsync may be off. |

## Platform Recommendations
| Platform | Backend | Swapchain/Layer | Buffer Count | Notes |
| --- | --- | --- | --- | --- |
| Windows 7+ (x86) | D3D11 + DXGI 1.1 | `DXGI_SWAP_EFFECT_DISCARD` or `SEQUENTIAL` | 2 | Flip-model unavailable; vsync on by default. |
| macOS (Apple Silicon) | Metal + `CAMetalLayer` | `CAMetalLayer` | 2 or 3 | Prefer CVDisplayLink; avoid CAMetalDisplayLink by default. |
| Linux (x86) | Vulkan (preferred) or GL | `MAILBOX` if available, else `FIFO` | 2 or 3 | Prefer mailbox for low latency. |
| Android | Vulkan or EGL | `ANativeWindow` | 2 or 3 | Use `AChoreographer` for pacing. |
| iOS (phone/tablet) | Metal + `CAMetalLayer` | `CAMetalLayer` | 2 or 3 | Vsync enforced by platform. |
| watchOS | Metal + `CAMetalLayer` | `CAMetalLayer` | 2 | Single surface; prioritize power. |

## Frame Pacing Rules
- Event-driven frames should bypass the cap once for low-latency input/resize.
- Present gating should prevent presenting more often than display interval when enabled.
- Continuous mode is optional and should be used for animation-heavy content.

## FramePolicy::Capped Defaults
- If `FramePolicy::Capped` is selected and `frameInterval` is unset or zero, PrimeHost will default
  the interval to the platform display refresh (when available).
- If no platform interval is available, the backend should leave `frameInterval` unchanged and
  rely on caller configuration.

## Latency Expectations (macOS)
- With vsync enabled and double buffering, expect up to ~1 frame of latency in the worst case.
- Rendering as late as possible before v-blank (via `CVDisplayLink`) minimizes average latency.
- Tearing is not available for normal macOS windows; lower latency comes from higher refresh rates and tighter pacing.

## Capability Mapping (Placeholder)
This table is a placeholder for describing how each platform maps or ignores configuration knobs.

| Platform | Tearing | Frame Pacing Source | Buffer Count Range |
| --- | --- | --- | --- |
| macOS | Not supported | `Platform` (`CVDisplayLink`) | 2 to 3 |
| Windows | Optional | `Platform` or `HostLimiter` | 2 to 3 |
| Linux | Optional | `Platform` or `HostLimiter` | 2 to 3 |
| Android | Not supported | `Platform` (`AChoreographer`) | 2 to 3 |
| iOS | Not supported | `Platform` (`CADisplayLink`) | 2 to 3 |
| watchOS | Not supported | `Platform` | 2 |

## API Hooks (Draft)
- `setFrameConfig(surface, FrameConfig)` applies presentation settings per surface.
- `requestFrame(surface, bypassCap)` schedules an event-driven frame.
- `pollEvents(EventBuffer)` or `setCallbacks()` drives the loop depending on mode.

## Open Questions
- Preferred render backend per platform (D3D11/Vulkan/Metal/GL).
- Tearing policy on desktop in low-latency mode.
- Surface limits for mobile/watch.
