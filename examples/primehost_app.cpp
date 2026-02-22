#include "PrimeHost/PrimeHost.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

using namespace PrimeHost;

namespace {

constexpr uint32_t kKeyEscape = 0x29u;
constexpr uint32_t kKeyF = 0x09u;
constexpr uint32_t kKeyM = 0x10u;
constexpr uint32_t kKeyX = 0x1Bu;
constexpr uint32_t kKeyR = 0x15u;
constexpr uint32_t kKeyC = 0x06u;
constexpr uint32_t kKeyV = 0x19u;
constexpr uint32_t kKeyO = 0x12u;
constexpr uint32_t kKeyP = 0x13u;
constexpr uint32_t kKeyI = 0x0Cu;

std::string_view device_type_label(DeviceType type) {
  switch (type) {
    case DeviceType::Mouse:
      return "mouse";
    case DeviceType::Touch:
      return "touch";
    case DeviceType::Pen:
      return "pen";
    case DeviceType::Keyboard:
      return "keyboard";
    case DeviceType::Gamepad:
      return "gamepad";
  }
  return "unknown";
}

std::string_view pointer_type_label(PointerDeviceType type) {
  switch (type) {
    case PointerDeviceType::Mouse:
      return "mouse";
    case PointerDeviceType::Touch:
      return "touch";
    case PointerDeviceType::Pen:
      return "pen";
  }
  return "unknown";
}

std::string_view pointer_phase_label(PointerPhase phase) {
  switch (phase) {
    case PointerPhase::Down:
      return "down";
    case PointerPhase::Move:
      return "move";
    case PointerPhase::Up:
      return "up";
    case PointerPhase::Cancel:
      return "cancel";
  }
  return "unknown";
}

std::optional<std::string_view> text_from_span(const EventBatch& batch, TextSpan span) {
  if (span.length == 0u) {
    return std::string_view{};
  }
  if (span.offset + span.length > batch.textBytes.size()) {
    return std::nullopt;
  }
  return std::string_view(batch.textBytes.data() + span.offset, span.length);
}

void dump_drop_paths(const EventBatch& batch, const DropEvent& drop) {
  auto text = text_from_span(batch, drop.paths);
  if (!text) {
    std::cout << "drop paths: <invalid span>\n";
    return;
  }
  std::cout << "drop paths (" << drop.count << "):";
  size_t start = 0u;
  for (uint32_t i = 0u; i < drop.count && start <= text->size(); ++i) {
    size_t end = text->find('\0', start);
    if (end == std::string_view::npos) {
      end = text->size();
    }
    std::cout << "\n  - " << text->substr(start, end - start);
    start = end + 1u;
  }
  std::cout << "\n";
}

bool is_toggle_key(uint32_t keyCode) {
  switch (keyCode) {
    case kKeyF:
    case kKeyM:
    case kKeyX:
    case kKeyR:
    case kKeyC:
    case kKeyV:
    case kKeyO:
    case kKeyP:
    case kKeyI:
      return true;
    default:
      return false;
  }
}

} // namespace

int main() {
  std::cout << "PrimeHost v" << PrimeHostVersion << " example" << std::endl;

  auto hostResult = createHost();
  if (!hostResult) {
    std::cerr << "PrimeHost unavailable (" << static_cast<int>(hostResult.error().code) << ")\n";
    return 1;
  }
  auto host = std::move(hostResult.value());

  host->setLogCallback([](LogLevel level, Utf8TextView message) {
    std::cout << "[host " << static_cast<int>(level) << "] " << message << "\n";
  });

  auto caps = host->hostCapabilities();
  if (caps) {
    std::cout << "caps: clipboard=" << caps->supportsClipboard
              << " fileDialogs=" << caps->supportsFileDialogs
              << " relativePointer=" << caps->supportsRelativePointer
              << " ime=" << caps->supportsIme
              << " haptics=" << caps->supportsHaptics
              << " headless=" << caps->supportsHeadless << "\n";
  }

  SurfaceConfig config{};
  config.width = 1280u;
  config.height = 720u;
  config.resizable = true;
  config.title = std::string("PrimeHost Example");

  auto surfaceResult = host->createSurface(config);
  if (!surfaceResult) {
    std::cerr << "Failed to create surface (" << static_cast<int>(surfaceResult.error().code) << ")\n";
    return 1;
  }
  SurfaceId surfaceId = surfaceResult.value();

  auto surfaceCaps = host->surfaceCapabilities(surfaceId);
  if (surfaceCaps) {
    std::cout << "surface: buffers=" << surfaceCaps->minBufferCount << "-" << surfaceCaps->maxBufferCount
              << " vsyncToggle=" << surfaceCaps->supportsVsyncToggle
              << " tearing=" << surfaceCaps->supportsTearing << "\n";
  }

  auto displayInterval = host->displayInterval(surfaceId);
  if (displayInterval && displayInterval->has_value()) {
    double hz = 1.0 / std::chrono::duration<double>(displayInterval->value()).count();
    std::cout << "display interval: " << hz << " Hz\n";
  }

  host->setCursorShape(surfaceId, CursorShape::Arrow);

  std::array<Event, 256> events{};
  std::array<char, 8192> textBytes{};
  EventBuffer buffer{
      std::span<Event>(events.data(), events.size()),
      std::span<char>(textBytes.data(), textBytes.size()),
  };

  bool running = true;
  bool fullscreen = false;
  bool minimized = false;
  bool maximized = false;
  bool relativePointer = false;
  bool cursorIBeam = false;
  FpsTracker fps{};

  std::cout << "Controls: ESC quit, F fullscreen, M minimize, X maximize, R relative pointer,"
               " C copy, V paste, O open dialog, P screenshot, I toggle cursor." << std::endl;

  while (running) {
    host->waitEvents();

    auto batchResult = host->pollEvents(buffer);
    if (!batchResult) {
      std::cerr << "pollEvents failed (" << static_cast<int>(batchResult.error().code) << ")\n";
      continue;
    }

    const auto& batch = batchResult.value();
    bool needsFrame = false;
    bool bypassCap = false;

    for (const auto& event : batch.events) {
      if (auto* input = std::get_if<InputEvent>(&event.payload)) {
        if (auto* pointer = std::get_if<PointerEvent>(input)) {
          if (pointer->phase != PointerPhase::Move) {
            std::cout << "pointer " << pointer_phase_label(pointer->phase)
                      << " type=" << pointer_type_label(pointer->deviceType)
                      << " x=" << pointer->x << " y=" << pointer->y << "\n";
          }
          needsFrame = true;
          bypassCap = true;
        } else if (auto* key = std::get_if<KeyEvent>(input)) {
          if (key->pressed && !key->repeat) {
            if (key->keyCode == kKeyEscape) {
              running = false;
            }
            if (is_toggle_key(key->keyCode)) {
              needsFrame = true;
              bypassCap = true;
            }
            if (key->keyCode == kKeyF) {
              fullscreen = !fullscreen;
              host->setSurfaceFullscreen(surfaceId, fullscreen);
            } else if (key->keyCode == kKeyM) {
              minimized = !minimized;
              host->setSurfaceMinimized(surfaceId, minimized);
            } else if (key->keyCode == kKeyX) {
              maximized = !maximized;
              host->setSurfaceMaximized(surfaceId, maximized);
            } else if (key->keyCode == kKeyR) {
              relativePointer = !relativePointer;
              host->setRelativePointerCapture(surfaceId, relativePointer);
            } else if (key->keyCode == kKeyI) {
              cursorIBeam = !cursorIBeam;
              host->setCursorShape(surfaceId, cursorIBeam ? CursorShape::IBeam : CursorShape::Arrow);
            } else if (key->keyCode == kKeyC) {
              host->setClipboardText("PrimeHost clipboard sample");
            } else if (key->keyCode == kKeyV) {
              auto size = host->clipboardTextSize();
              if (size && size.value() > 0u) {
                std::vector<char> bufferText(size.value());
                auto text = host->clipboardText(bufferText);
                if (text) {
                  std::cout << "clipboard: " << text.value() << "\n";
                }
              }
            } else if (key->keyCode == kKeyO) {
              FileDialogConfig dialog{};
              dialog.mode = FileDialogMode::OpenFile;
              std::array<char, 4096> pathBuffer{};
              auto result = host->fileDialog(dialog, pathBuffer);
              if (result && result->accepted) {
                std::cout << "selected: " << result->path << "\n";
              }
            } else if (key->keyCode == kKeyP) {
              ScreenshotConfig shot{};
              shot.scope = ScreenshotScope::Surface;
              auto status = host->writeSurfaceScreenshot(surfaceId, "/tmp/primehost-shot.png", shot);
              std::cout << "screenshot: " << (status ? "ok" : "failed") << "\n";
            }
          }
        } else if (auto* text = std::get_if<TextEvent>(input)) {
          auto view = text_from_span(batch, text->text);
          if (view) {
            std::cout << "text: " << *view << "\n";
          }
          needsFrame = true;
        } else if (auto* scroll = std::get_if<ScrollEvent>(input)) {
          std::cout << "scroll dx=" << scroll->deltaX << " dy=" << scroll->deltaY
                    << (scroll->isLines ? " lines" : " px") << "\n";
          needsFrame = true;
          bypassCap = true;
        } else if (auto* device = std::get_if<DeviceEvent>(input)) {
          std::cout << "device " << device_type_label(device->deviceType)
                    << (device->connected ? " connected" : " disconnected")
                    << " id=" << device->deviceId << "\n";
        } else if (auto* gamepad = std::get_if<GamepadButtonEvent>(input)) {
          std::cout << "gamepad button id=" << gamepad->controlId
                    << " pressed=" << gamepad->pressed << "\n";
        } else if (auto* axis = std::get_if<GamepadAxisEvent>(input)) {
          std::cout << "gamepad axis id=" << axis->controlId << " value=" << axis->value << "\n";
        }
      } else if (auto* resize = std::get_if<ResizeEvent>(&event.payload)) {
        std::cout << "resize " << resize->width << "x" << resize->height
                  << " scale=" << resize->scale << "\n";
        needsFrame = true;
        bypassCap = true;
      } else if (auto* drop = std::get_if<DropEvent>(&event.payload)) {
        dump_drop_paths(batch, *drop);
        needsFrame = true;
      } else if (auto* focus = std::get_if<FocusEvent>(&event.payload)) {
        std::cout << "focus " << focus->focused << "\n";
      } else if (auto* power = std::get_if<PowerEvent>(&event.payload)) {
        if (power->lowPowerModeEnabled.has_value()) {
          std::cout << "power lowPower=" << power->lowPowerModeEnabled.value() << "\n";
        }
      } else if (auto* thermal = std::get_if<ThermalEvent>(&event.payload)) {
        std::cout << "thermal state=" << static_cast<int>(thermal->state) << "\n";
      } else if (auto* lifecycle = std::get_if<LifecycleEvent>(&event.payload)) {
        if (lifecycle->phase == LifecyclePhase::Destroyed) {
          running = false;
        }
      }
    }

    if (needsFrame) {
      auto status = host->requestFrame(surfaceId, bypassCap);
      if (status) {
        fps.framePresented();
        if (fps.shouldReport() && fps.sampleCount() > 0u) {
          auto stats = fps.stats();
          std::cout << "fps " << stats.fps
                    << " p95=" << std::chrono::duration<double, std::milli>(stats.p95FrameTime).count()
                    << "ms p99=" << std::chrono::duration<double, std::milli>(stats.p99FrameTime).count()
                    << "ms\n";
        }
      }
    }
  }

  host->destroySurface(surfaceId);
  return 0;
}
