#include "PrimeHost/PrimeHost.h"
#include "PrimeStage/Render.h"
#include "PrimeStage/StudioUi.h"

#include "PrimeFrame/Events.h"
#include "PrimeFrame/Layout.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace PrimeHost;
using namespace PrimeFrame;
using namespace PrimeStage;
using namespace PrimeStage::Studio;

namespace {

constexpr uint32_t KeyEscape = 0x29u;
constexpr uint32_t KeyA = 0x04u;
constexpr uint32_t KeyQ = 0x14u;
constexpr uint32_t KeyF = 0x09u;
constexpr uint32_t KeyM = 0x10u;
constexpr uint32_t KeyX = 0x1Bu;
constexpr uint32_t KeyR = 0x15u;
constexpr uint32_t KeyC = 0x06u;
constexpr uint32_t KeyV = 0x19u;
constexpr uint32_t KeyO = 0x12u;
constexpr uint32_t KeyP = 0x13u;
constexpr uint32_t KeyI = 0x0Cu;
constexpr auto SearchBlinkInterval = std::chrono::milliseconds(500);

std::string_view deviceTypeLabel(DeviceType type) {
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

std::string_view pointerTypeLabel(PointerDeviceType type) {
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

std::string_view pointerPhaseLabel(PointerPhase phase) {
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

std::optional<std::string_view> textFromSpan(const EventBatch& batch, TextSpan span) {
  if (span.length == 0u) {
    return std::string_view{};
  }
  if (span.offset + span.length > batch.textBytes.size()) {
    return std::nullopt;
  }
  return std::string_view(batch.textBytes.data() + span.offset, span.length);
}

void dumpDropPaths(const EventBatch& batch, const DropEvent& drop) {
  auto text = textFromSpan(batch, drop.paths);
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

bool isToggleKey(uint32_t keyCode) {
  switch (keyCode) {
    case KeyF:
    case KeyM:
    case KeyX:
    case KeyR:
    case KeyC:
    case KeyV:
    case KeyO:
    case KeyP:
    case KeyI:
      return true;
    default:
      return false;
  }
}

struct DemoUi {
  PrimeFrame::Frame frame;
  PrimeFrame::LayoutEngine layoutEngine;
  PrimeFrame::LayoutOutput layout;
  PrimeFrame::EventRouter router;
  PrimeFrame::FocusManager focus;
  PrimeFrame::NodeId treeViewNode{};
  PrimeFrame::NodeId searchFieldNode{};
  Host* host = nullptr;
  float cursorX = 0.0f;
  float cursorY = 0.0f;
  float logicalWidth = 0.0f;
  float logicalHeight = 0.0f;
  float scale = 1.0f;
  float scrollLineHeight = 24.0f;
  bool needsRebuild = true;
  bool layoutDirty = true;
  bool renderDirty = true;
  bool pendingTreeFocus = false;
  bool pendingSearchFocus = false;
  int pointerDownCount = 0;
  PrimeFrame::NodeId fpsNode{};
  PrimeFrame::NodeId opacityValueNode{};
  std::chrono::steady_clock::time_point lastResizeEvent{};
  std::chrono::steady_clock::time_point lastResizeLog{};
  std::chrono::steady_clock::time_point lastSlowLog{};
};

struct DemoState {
  std::vector<PrimeStage::TreeNode> treeNodes;
  float opacity = 0.85f;
  std::string opacityLabel;
  PrimeStage::TextFieldState searchField;
  std::string boardText;
  PrimeStage::SelectableTextState boardSelection;
  float treeScrollProgress = 0.0f;
};

void updateOpacityLabel(DemoState& state) {
  int percent = static_cast<int>(std::lround(state.opacity * 100.0f));
  state.opacityLabel = std::to_string(percent) + "%";
}

void ensureTreeState(DemoState& state) {
  if (!state.treeNodes.empty()) {
    return;
  }
  state.treeNodes = {
      PrimeStage::TreeNode{
          "Root",
          {
              PrimeStage::TreeNode{"World",
                                   {PrimeStage::TreeNode{"Camera"},
                                    PrimeStage::TreeNode{"Lights"},
                                    PrimeStage::TreeNode{"Environment"},
                                    PrimeStage::TreeNode{"Weather"},
                                    PrimeStage::TreeNode{"Sky"},
                                    PrimeStage::TreeNode{"Terrain"},
                                    PrimeStage::TreeNode{"Audio"},
                                    PrimeStage::TreeNode{"AI"}},
                                   true,
                                   false},
              PrimeStage::TreeNode{"UI",
                                   {PrimeStage::TreeNode{"Sidebar"},
                                    PrimeStage::TreeNode{"Toolbar", {PrimeStage::TreeNode{"Buttons"}}, false, false},
                                    PrimeStage::TreeNode{"Panels",
                                                         {PrimeStage::TreeNode{"TreeView", {}, true, true},
                                                          PrimeStage::TreeNode{"Rows"},
                                                          PrimeStage::TreeNode{"Headers"},
                                                          PrimeStage::TreeNode{"Footers"}},
                                                         true,
                                                         false},
                                    PrimeStage::TreeNode{"Dialogs",
                                                         {PrimeStage::TreeNode{"Alerts"},
                                                          PrimeStage::TreeNode{"Modals"},
                                                          PrimeStage::TreeNode{"Sheets"}},
                                                         true,
                                                         false},
                                    PrimeStage::TreeNode{"Inspector"},
                                    PrimeStage::TreeNode{"StatusBar"}},
                                   true,
                                   false},
              PrimeStage::TreeNode{"Gameplay",
                                   {PrimeStage::TreeNode{"Player"},
                                    PrimeStage::TreeNode{"Enemies",
                                                         {PrimeStage::TreeNode{"Grunt"},
                                                          PrimeStage::TreeNode{"Boss"}},
                                                         true,
                                                         false},
                                    PrimeStage::TreeNode{"Items"},
                                    PrimeStage::TreeNode{"Triggers"}},
                                   true,
                                   false},
              PrimeStage::TreeNode{"Effects",
                                   {PrimeStage::TreeNode{"Particles"},
                                    PrimeStage::TreeNode{"PostProcess"},
                                    PrimeStage::TreeNode{"Shadows"},
                                    PrimeStage::TreeNode{"Reflections"}},
                                   true,
                                   false},
              PrimeStage::TreeNode{"Audio",
                                   {PrimeStage::TreeNode{"Ambience"},
                                    PrimeStage::TreeNode{"SFX"},
                                    PrimeStage::TreeNode{"Music"}},
                                   true,
                                   false},
              PrimeStage::TreeNode{"Networking",
                                   {PrimeStage::TreeNode{"Sessions"},
                                    PrimeStage::TreeNode{"Replication"},
                                    PrimeStage::TreeNode{"Latency"}},
                                   true,
                                   false}},
          true,
          false}};
}

void clearTreeSelection(std::vector<PrimeStage::TreeNode>& nodes) {
  for (auto& node : nodes) {
    node.selected = false;
    if (!node.children.empty()) {
      clearTreeSelection(node.children);
    }
  }
}

PrimeStage::TreeNode* treeNodeForPath(std::vector<PrimeStage::TreeNode>& nodes,
                                      std::span<const uint32_t> path) {
  if (path.empty()) {
    return nullptr;
  }
  PrimeStage::TreeNode* current = nullptr;
  auto* list = &nodes;
  for (uint32_t index : path) {
    if (index >= list->size()) {
      return nullptr;
    }
    current = &(*list)[index];
    list = &current->children;
  }
  return current;
}

bool setTreeSelectionByPath(DemoState& state, std::span<const uint32_t> path) {
  PrimeStage::TreeNode* node = treeNodeForPath(state.treeNodes, path);
  if (!node) {
    return false;
  }
  bool changed = !node->selected;
  clearTreeSelection(state.treeNodes);
  node->selected = true;
  return changed;
}

bool setTreeExpandedByPath(DemoState& state, std::span<const uint32_t> path, bool expanded) {
  PrimeStage::TreeNode* node = treeNodeForPath(state.treeNodes, path);
  if (!node) {
    return false;
  }
  if (node->expanded == expanded) {
    return false;
  }
  node->expanded = expanded;
  return true;
}

enum class PerfMode {
  Off,
  Headless,
  Windowed,
};

struct PerfConfig {
  PerfMode mode = PerfMode::Off;
  bool headless = false;
  bool headlessSet = false;
  uint32_t width = 1280u;
  uint32_t height = 720u;
  uint32_t warmupFrames = 30u;
  uint32_t measureFrames = 120u;
  uint32_t timeoutMs = 0u;
  bool resizeStress = false;
  uint32_t resizeMinW = 960u;
  uint32_t resizeMinH = 540u;
  uint32_t resizeMaxW = 1600u;
  uint32_t resizeMaxH = 900u;
  uint32_t resizeIntervalMs = 16u;
  uint32_t resizeSteps = 30u;
  double minFps = 30.0;
  double maxP95Ms = 0.0;
};

struct PerfState {
  uint32_t presented = 0u;
  bool done = false;
  std::mutex mutex;
  std::condition_variable cv;
  double resizePhase = 0.0;
  int resizeDir = 1;
  std::chrono::steady_clock::time_point lastResize{};
  uint32_t lastResizeW = 0u;
  uint32_t lastResizeH = 0u;
};

bool parseUint(std::string_view value, uint32_t& out) {
  if (value.empty()) {
    return false;
  }
  uint32_t parsed = 0u;
  auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
    return false;
  }
  out = parsed;
  return true;
}

bool parseDouble(std::string_view value, double& out) {
  if (value.empty()) {
    return false;
  }
  std::string temp(value);
  char* end = nullptr;
  double parsed = std::strtod(temp.c_str(), &end);
  if (!end || *end != '\0') {
    return false;
  }
  out = parsed;
  return true;
}

bool parseSize(std::string_view value, uint32_t& outW, uint32_t& outH) {
  auto pos = value.find('x');
  if (pos == std::string_view::npos) {
    return false;
  }
  std::string_view w = value.substr(0, pos);
  std::string_view h = value.substr(pos + 1);
  return parseUint(w, outW) && parseUint(h, outH);
}

void printUsage(std::string_view name) {
  std::cout << "Usage: " << name << " [options]\n"
            << "  --perf                Run headless perf check and exit\n"
            << "  --perf-headless       Run headless perf check and exit\n"
            << "  --perf-windowed       Run windowed perf check and exit\n"
            << "  --headless            Force headless surface\n"
            << "  --windowed            Force windowed surface\n"
            << "  --size WxH            Surface size (default 1280x720)\n"
            << "  --warmup N            Warmup frames (default 30)\n"
            << "  --frames N            Measured frames (default 120)\n"
            << "  --timeout-ms N        Max time to wait (0 = auto)\n"
            << "  --resize-stress       Animate resize during perf runs\n"
            << "  --resize-min WxH      Resize min size (default 960x540)\n"
            << "  --resize-max WxH      Resize max size (default 1600x900)\n"
            << "  --resize-interval-ms N Resize interval (default 16)\n"
            << "  --resize-steps N      Steps from min->max (default 30)\n"
            << "  --min-fps N           Minimum FPS to pass (default 30)\n"
            << "  --max-p95-ms N        Maximum p95 frame time (0 disables)\n";
}

bool parseArgs(int argc, char** argv, PerfConfig& perf) {
  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);
    if (arg == "--perf" || arg == "--perf-headless") {
      perf.mode = PerfMode::Headless;
    } else if (arg == "--perf-windowed") {
      perf.mode = PerfMode::Windowed;
    } else if (arg == "--headless") {
      perf.headless = true;
      perf.headlessSet = true;
    } else if (arg == "--windowed") {
      perf.headless = false;
      perf.headlessSet = true;
    } else if (arg == "--size") {
      if (i + 1 >= argc) {
        return false;
      }
      if (!parseSize(std::string_view(argv[++i]), perf.width, perf.height)) {
        return false;
      }
    } else if (arg == "--warmup") {
      if (i + 1 >= argc) {
        return false;
      }
      if (!parseUint(std::string_view(argv[++i]), perf.warmupFrames)) {
        return false;
      }
    } else if (arg == "--frames") {
      if (i + 1 >= argc) {
        return false;
      }
      if (!parseUint(std::string_view(argv[++i]), perf.measureFrames)) {
        return false;
      }
    } else if (arg == "--timeout-ms") {
      if (i + 1 >= argc) {
        return false;
      }
      if (!parseUint(std::string_view(argv[++i]), perf.timeoutMs)) {
        return false;
      }
    } else if (arg == "--resize-stress") {
      perf.resizeStress = true;
    } else if (arg == "--resize-min") {
      if (i + 1 >= argc) {
        return false;
      }
      if (!parseSize(std::string_view(argv[++i]), perf.resizeMinW, perf.resizeMinH)) {
        return false;
      }
    } else if (arg == "--resize-max") {
      if (i + 1 >= argc) {
        return false;
      }
      if (!parseSize(std::string_view(argv[++i]), perf.resizeMaxW, perf.resizeMaxH)) {
        return false;
      }
    } else if (arg == "--resize-interval-ms") {
      if (i + 1 >= argc) {
        return false;
      }
      if (!parseUint(std::string_view(argv[++i]), perf.resizeIntervalMs)) {
        return false;
      }
    } else if (arg == "--resize-steps") {
      if (i + 1 >= argc) {
        return false;
      }
      if (!parseUint(std::string_view(argv[++i]), perf.resizeSteps)) {
        return false;
      }
    } else if (arg == "--min-fps") {
      if (i + 1 >= argc) {
        return false;
      }
      if (!parseDouble(std::string_view(argv[++i]), perf.minFps)) {
        return false;
      }
    } else if (arg == "--max-p95-ms") {
      if (i + 1 >= argc) {
        return false;
      }
      if (!parseDouble(std::string_view(argv[++i]), perf.maxP95Ms)) {
        return false;
      }
    } else if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      return false;
    } else {
      std::cout << "Unknown option: " << arg << "\n";
      printUsage(argv[0]);
      return false;
    }
  }
  return true;
}

void buildStudioUi(DemoUi& ui, DemoState& state) {
  ui.frame = PrimeFrame::Frame{};
  ui.fpsNode = PrimeFrame::NodeId{};
  ui.opacityValueNode = PrimeFrame::NodeId{};
  ui.treeViewNode = PrimeFrame::NodeId{};
  ui.searchFieldNode = PrimeFrame::NodeId{};
  ui.focus = PrimeFrame::FocusManager{};
  ensureTreeState(state);
  updateOpacityLabel(state);
  state.searchField.cursor = std::min(state.searchField.cursor,
                                      static_cast<uint32_t>(state.searchField.text.size()));
  state.searchField.selectionAnchor = std::min(state.searchField.selectionAnchor,
                                               static_cast<uint32_t>(state.searchField.text.size()));
  state.searchField.selectionStart = std::min(state.searchField.selectionStart,
                                              static_cast<uint32_t>(state.searchField.text.size()));
  state.searchField.selectionEnd = std::min(state.searchField.selectionEnd,
                                            static_cast<uint32_t>(state.searchField.text.size()));
  if (state.boardText.empty()) {
    std::string base =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
        "Sed do eiusmod tempor incididunt ut labore et dolore. "
        "Ut enim ad minim veniam, quis nostrud exercitation.";
    state.boardText = base + " " + base + " " + base;
  }
  state.boardSelection.selectionAnchor =
      std::min(state.boardSelection.selectionAnchor,
               static_cast<uint32_t>(state.boardText.size()));
  state.boardSelection.selectionStart =
      std::min(state.boardSelection.selectionStart,
               static_cast<uint32_t>(state.boardText.size()));
  state.boardSelection.selectionEnd =
      std::min(state.boardSelection.selectionEnd,
               static_cast<uint32_t>(state.boardText.size()));

  SizeSpec shellSize;
  shellSize.preferredWidth = ui.logicalWidth;
  shellSize.preferredHeight = ui.logicalHeight;
  ShellSpec shellSpec = makeShellSpec(shellSize);
  ShellLayout shell = createShell(ui.frame, shellSpec);
  float shellWidth = shellSpec.size.preferredWidth.value_or(ui.logicalWidth);
  float shellHeight = shellSpec.size.preferredHeight.value_or(ui.logicalHeight);
  float sidebarW = shellSpec.sidebarWidth;
  float inspectorW = shellSpec.inspectorWidth;
  float contentW = std::max(0.0f, shellWidth - sidebarW - inspectorW);
  float contentH = std::max(0.0f, shellHeight - shellSpec.topbarHeight - shellSpec.statusHeight);
  UiNode edgeBar = shell.topbar;
  UiNode statusBar = shell.status;
  UiNode leftRail = shell.sidebar;
  UiNode centerPane = shell.content;
  UiNode rightRail = shell.inspector;

  auto createTopbar = [&]() {
    StackSpec rowSpec;
    rowSpec.size.preferredWidth = shellWidth;
    rowSpec.size.preferredHeight = StudioDefaults::EdgeBarHeight;
    rowSpec.padding = Insets{0.0f,
                             StudioDefaults::PanelInset,
                             0.0f,
                             StudioDefaults::PanelInset};
    rowSpec.gap = StudioDefaults::PanelInset;

    UiNode row = edgeBar.createHorizontalStack(rowSpec);

    auto fixed = [](float fixedWidth, float fixedHeight) {
      SizeSpec size;
      size.preferredWidth = fixedWidth;
      size.preferredHeight = fixedHeight;
      return size;
    };

    createTextLine(row,
                   "PrimeFrame Studio",
                   TextRole::TitleBright,
                   fixed(StudioDefaults::TitleBlockWidth, StudioDefaults::ControlHeight),
                   PrimeFrame::TextAlign::Center);
    row.createDivider(rectToken(RectRole::Divider),
                      fixed(StudioDefaults::DividerThickness, StudioDefaults::ControlHeight));
    row.createSpacer(fixed(StudioDefaults::PanelInset, StudioDefaults::ControlHeight));
    {
      TextFieldSpec spec;
      spec.state = &state.searchField;
      spec.placeholder = "Search...";
      spec.size = {};
      if (!spec.size.preferredHeight.has_value() && spec.size.stretchY <= 0.0f) {
        spec.size.preferredHeight = StudioDefaults::ControlHeight;
      }
      if (!spec.size.minWidth.has_value() && spec.size.stretchX <= 0.0f) {
        spec.size.minWidth = StudioDefaults::FieldWidthL;
      }
      spec.backgroundStyle = rectToken(RectRole::Panel);
      spec.focusStyle = rectToken(RectRole::Accent);
      spec.focusStyleOverride.opacity = 0.22f;
      spec.textStyle = textToken(TextRole::BodyBright);
      spec.placeholderStyle = textToken(TextRole::BodyMuted);
      spec.cursorStyle = rectToken(RectRole::Accent);
      spec.cursorWidth = 2.0f;
      spec.selectionStyle = rectToken(RectRole::Selection);
      spec.cursorBlinkInterval = SearchBlinkInterval;
      spec.callbacks.onStateChanged = [&ui]() {
        ui.needsRebuild = true;
        ui.layoutDirty = true;
        ui.renderDirty = true;
      };
      spec.callbacks.onTextChanged = [&ui](std::string_view) {
        ui.needsRebuild = true;
        ui.layoutDirty = true;
        ui.renderDirty = true;
      };
      spec.callbacks.onRequestBlur = [&ui]() {
        if (ui.focus.clearFocus(ui.frame)) {
          ui.renderDirty = true;
        }
      };
      if (ui.host) {
        spec.clipboard.setText = [host = ui.host](std::string_view text) {
          host->setClipboardText(text);
        };
        spec.clipboard.getText = [host = ui.host]() -> std::string {
          auto size = host->clipboardTextSize();
          if (!size || size.value() == 0u) {
            return {};
          }
          std::vector<char> buffer(size.value());
          auto view = host->clipboardText(buffer);
          if (!view) {
            return {};
          }
          return std::string(view.value());
        };
      }
      UiNode searchField = row.createTextField(spec);
      ui.searchFieldNode = searchField.nodeId();
    }

    SizeSpec spacer;
    spacer.stretchX = 1.0f;
    row.createSpacer(spacer);

    UiNode share = createButton(row,
                                "Share",
                                ButtonVariant::Default,
                                {});
    UiNode run = createButton(row,
                              "Run",
                              ButtonVariant::Primary,
                              {});
  };

  auto createSidebar = [&]() {
    StackSpec columnSpec;
    columnSpec.size.preferredWidth = sidebarW;
    columnSpec.size.preferredHeight = contentH;
    columnSpec.padding = Insets{StudioDefaults::PanelInset,
                                StudioDefaults::PanelInset,
                                StudioDefaults::PanelInset,
                                StudioDefaults::PanelInset};
    columnSpec.gap = StudioDefaults::PanelInset;
    UiNode column = leftRail.createVerticalStack(columnSpec);

    PanelSpec headerSpec;
    headerSpec.rectStyle = rectToken(RectRole::PanelStrong);
    headerSpec.layout = PrimeFrame::LayoutType::HorizontalStack;
    headerSpec.size.preferredHeight = StudioDefaults::HeaderHeight;
    UiNode header = column.createPanel(headerSpec);

    SizeSpec accentSize;
    accentSize.preferredWidth = StudioDefaults::AccentThickness;
    accentSize.preferredHeight = StudioDefaults::HeaderHeight;
    header.createPanel(rectToken(RectRole::Accent), accentSize);

    SizeSpec indentSize;
    indentSize.preferredWidth = StudioDefaults::LabelIndent;
    indentSize.preferredHeight = StudioDefaults::HeaderHeight;
    header.createSpacer(indentSize);

    SizeSpec headerTextSize;
    headerTextSize.stretchX = 1.0f;
    headerTextSize.preferredHeight = StudioDefaults::HeaderHeight;
    createTextLine(header, "Scene", TextRole::BodyBright, headerTextSize);

    SizeSpec hierarchyTextSize;
    hierarchyTextSize.preferredHeight = StudioDefaults::HeaderHeight;
    createTextLine(column, "Hierarchy", TextRole::SmallMuted, hierarchyTextSize);

    PanelSpec treePanelSpec;
    treePanelSpec.rectStyle = rectToken(RectRole::Panel);
    treePanelSpec.layout = PrimeFrame::LayoutType::VerticalStack;
    treePanelSpec.size.stretchX = 1.0f;
    treePanelSpec.size.stretchY = 1.0f;
    UiNode treePanel = column.createPanel(treePanelSpec);

    Studio::TreeViewSpec treeSpec;
    treeSpec.base.size.stretchX = 1.0f;
    treeSpec.base.size.stretchY = 1.0f;
    treeSpec.base.showHeaderDivider = false;
    treeSpec.base.rowStartX = 12.0f;
    treeSpec.base.rowStartY = 4.0f;
    treeSpec.base.headerDividerY = 0.0f;
    treeSpec.base.scrollBar.inset = treeSpec.base.scrollBar.width;
    treeSpec.base.rowWidthInset = treeSpec.base.scrollBar.inset + treeSpec.base.scrollBar.width;
    treeSpec.base.rowHeight = 24.0f;
    treeSpec.base.rowGap = 2.0f;
    treeSpec.base.indent = 14.0f;
    treeSpec.base.caretBaseX = 12.0f;
    treeSpec.base.caretSize = 12.0f;
    treeSpec.base.caretInset = 2.0f;
    treeSpec.base.caretThickness = 2.5f;
    treeSpec.base.caretMaskPad = 1.0f;
    treeSpec.base.connectorThickness = 2.0f;
    treeSpec.base.linkEndInset = 0.0f;
    treeSpec.base.selectionAccentWidth = 2.0f;
    treeSpec.base.scrollBar.thumbProgress = state.treeScrollProgress;
    treeSpec.base.scrollBar.trackStyleOverride.opacity = 0.45f;
    treeSpec.base.scrollBar.thumbStyleOverride.opacity = 0.75f;
    treeSpec.base.scrollBar.trackHoverOpacity = 0.6f;
    treeSpec.base.scrollBar.thumbHoverOpacity = 0.9f;
    treeSpec.base.scrollBar.trackPressedOpacity = 0.35f;
    treeSpec.base.scrollBar.thumbPressedOpacity = 0.6f;
    treeSpec.rowRole = RectRole::PanelAlt;
    treeSpec.rowAltRole = RectRole::Panel;
    treeSpec.hoverRole = RectRole::PanelStrong;
    treeSpec.caretBackgroundRole = RectRole::PanelStrong;
    treeSpec.caretLineRole = RectRole::Accent;
    treeSpec.connectorRole = RectRole::Accent;
    treeSpec.textRole = TextRole::SmallBright;
    treeSpec.selectedTextRole = TextRole::SmallBright;
    ui.scrollLineHeight = treeSpec.base.rowHeight;
    float treePanelHeight = contentH - StudioDefaults::HeaderHeight * 2.0f -
                            StudioDefaults::PanelInset * 4.0f;
    treeSpec.base.size.preferredWidth = sidebarW - StudioDefaults::PanelInset * 2.0f;
    treeSpec.base.size.preferredHeight = std::max(0.0f, treePanelHeight);
    treeSpec.base.callbacks.onSelectionChanged = [&](const PrimeStage::TreeViewRowInfo& row) {
      if (setTreeSelectionByPath(state, row.path)) {
        ui.renderDirty = true;
      }
    };
    treeSpec.base.callbacks.onExpandedChanged = [&](const PrimeStage::TreeViewRowInfo& row, bool expanded) {
      if (setTreeExpandedByPath(state, row.path, expanded)) {
        ui.needsRebuild = true;
        ui.layoutDirty = true;
        ui.renderDirty = true;
        ui.pendingTreeFocus = true;
      }
    };
    treeSpec.base.callbacks.onScrollChanged = [&](const PrimeStage::TreeViewScrollInfo& info) {
      state.treeScrollProgress = info.progress;
      ui.layoutDirty = true;
      ui.renderDirty = true;
    };

    treeSpec.base.nodes = state.treeNodes;
    UiNode treeView = createTreeView(treePanel, treeSpec);
    ui.treeViewNode = treeView.nodeId();
  };

  auto createContent = [&]() {
    StackSpec columnSpec;
    float scrollGutterX = StudioDefaults::TableRightInset;
    float scrollGutterY = StudioDefaults::PanelInset;
    float columnWidth = std::max(0.0f, contentW - scrollGutterX);
    float columnHeight = std::max(0.0f, contentH - scrollGutterY);
    columnSpec.size.preferredWidth = columnWidth;
    columnSpec.size.preferredHeight = columnHeight;
    columnSpec.padding = Insets{StudioDefaults::SurfaceInset,
                                StudioDefaults::SectionHeaderOffsetY,
                                StudioDefaults::SurfaceInset,
                                StudioDefaults::SurfaceInset};
    columnSpec.gap = StudioDefaults::SectionGap;
    UiNode column = centerPane.createVerticalStack(columnSpec);

    float sectionWidth = columnWidth - StudioDefaults::SurfaceInset * 2.0f;
    SizeSpec overviewSize;
    overviewSize.preferredWidth = sectionWidth;
    overviewSize.preferredHeight = StudioDefaults::SectionHeaderHeight;
    createSectionHeader(column, overviewSize, "Overview", TextRole::TitleBright);

    PanelSpec boardSpec;
    boardSpec.rectStyle = rectToken(RectRole::Panel);
    boardSpec.layout = PrimeFrame::LayoutType::VerticalStack;
    boardSpec.padding = Insets{StudioDefaults::SurfaceInset,
                               StudioDefaults::PanelInset,
                               StudioDefaults::SurfaceInset,
                               StudioDefaults::PanelInset};
    boardSpec.gap = StudioDefaults::PanelInset;
    boardSpec.size.preferredWidth = sectionWidth;
    boardSpec.size.preferredHeight = StudioDefaults::PanelHeightL +
                                     StudioDefaults::ControlHeight +
                                     StudioDefaults::PanelInset;
    UiNode boardPanel = column.createPanel(boardSpec);

    float boardTextWidth = std::max(0.0f, sectionWidth - StudioDefaults::SurfaceInset * 2.0f);
    SizeSpec titleSize;
    titleSize.preferredWidth = boardTextWidth;
    titleSize.preferredHeight = StudioDefaults::TitleHeight;
    createTextLine(boardPanel, "Active Board", TextRole::SmallMuted, titleSize);

    PrimeFrame::TextStyleToken boardTextStyle = textToken(TextRole::SmallMuted);
    SelectableTextSpec selectionSpec;
    selectionSpec.state = &state.boardSelection;
    selectionSpec.text = state.boardText;
    selectionSpec.textStyle = boardTextStyle;
    selectionSpec.wrap = PrimeFrame::WrapMode::Word;
    selectionSpec.maxWidth = boardTextWidth;
    selectionSpec.selectionStyle = rectToken(RectRole::Selection);
    selectionSpec.size.preferredWidth = boardTextWidth;
    selectionSpec.callbacks.onSelectionChanged = [&ui](uint32_t, uint32_t) {
      ui.needsRebuild = true;
      ui.layoutDirty = true;
      ui.renderDirty = true;
    };
    if (ui.host) {
      selectionSpec.clipboard.setText = [host = ui.host](std::string_view text) {
        host->setClipboardText(text);
      };
    }
    boardPanel.createSelectableText(selectionSpec);

    StackSpec buttonRowSpec;
    buttonRowSpec.size.preferredWidth = boardTextWidth;
    buttonRowSpec.size.preferredHeight = StudioDefaults::ControlHeight;
    UiNode boardButtons = boardPanel.createHorizontalStack(buttonRowSpec);
    SizeSpec buttonSpacer;
    buttonSpacer.stretchX = 1.0f;
    boardButtons.createSpacer(buttonSpacer);
    SizeSpec primarySize;
    primarySize.preferredWidth = StudioDefaults::ControlWidthL;
    primarySize.preferredHeight = StudioDefaults::ControlHeight;
    createButton(boardButtons, "Primary Action", ButtonVariant::Default, primarySize);

    SizeSpec highlightSize;
    highlightSize.preferredWidth = sectionWidth;
    highlightSize.preferredHeight = StudioDefaults::HeaderHeight;
    createSectionHeader(column,
                        highlightSize,
                        "Highlights",
                        TextRole::SmallBright,
                        true,
                        StudioDefaults::HeaderDividerOffset);

    CardGridSpec cardSpec;
    cardSpec.size.preferredWidth = sectionWidth;
    cardSpec.size.preferredHeight = StudioDefaults::CardHeight;
    cardSpec.gapX = StudioDefaults::PanelInset;
    cardSpec.cardWidth = (sectionWidth - cardSpec.gapX * 2.0f) / 3.0f;
    cardSpec.cards = {
        {"Card", "Detail"},
        {"Card", "Detail"},
        {"Card", "Detail"}
    };
    createCardGrid(column, cardSpec);

    float tableWidth = sectionWidth;
    float firstColWidth = tableWidth - StudioDefaults::TableStatusOffset;
    float secondColWidth = tableWidth - firstColWidth;

    Studio::TableSpec tableSpec;
    tableSpec.base.size.preferredWidth = tableWidth;
    tableSpec.base.size.stretchY = 1.0f;
    tableSpec.base.showHeaderDividers = false;
    tableSpec.columns = {
        Studio::TableColumn{"Item", firstColWidth, TextRole::SmallBright, TextRole::SmallBright},
        Studio::TableColumn{"Status", secondColWidth, TextRole::SmallBright, TextRole::SmallMuted}
    };
    tableSpec.base.rows = {
        {"Item Row", "Ready"},
        {"Item Row", "Ready"},
        {"Item Row", "Ready"},
        {"Item Row", "Ready"},
        {"Item Row", "Ready"},
        {"Item Row", "Ready"}
    };
    createTable(column, tableSpec);

    SizeSpec tableSpacer;
    tableSpacer.preferredHeight = StudioDefaults::PanelInset;
    column.createSpacer(tableSpacer);

    StackSpec overlaySpec;
    overlaySpec.size.preferredWidth = columnWidth + scrollGutterX;
    overlaySpec.size.preferredHeight = columnHeight + scrollGutterY;
    overlaySpec.clipChildren = false;
    UiNode scrollOverlay = centerPane.createOverlay(overlaySpec);

    StackSpec vertSpec;
    vertSpec.size.preferredWidth = columnWidth + scrollGutterX;
    vertSpec.size.preferredHeight = columnHeight;
    vertSpec.gap = 0.0f;
    UiNode vertRow = scrollOverlay.createHorizontalStack(vertSpec);

    SizeSpec vertSpacer;
    vertSpacer.preferredWidth = columnWidth;
    vertSpacer.preferredHeight = columnHeight;
    vertRow.createSpacer(vertSpacer);

    ScrollHintsSpec scrollVertical;
    scrollVertical.size.preferredWidth = scrollGutterX;
    scrollVertical.size.preferredHeight = columnHeight;
    scrollVertical.showHorizontal = false;
    createScrollHints(vertRow, scrollVertical);

    StackSpec horizSpec;
    horizSpec.size.preferredWidth = columnWidth;
    horizSpec.size.preferredHeight = columnHeight + scrollGutterY;
    horizSpec.gap = 0.0f;
    UiNode horizCol = scrollOverlay.createVerticalStack(horizSpec);

    SizeSpec horizSpacer;
    horizSpacer.preferredWidth = columnWidth;
    horizSpacer.preferredHeight = columnHeight;
    horizCol.createSpacer(horizSpacer);

    ScrollHintsSpec scrollHorizontal;
    scrollHorizontal.size.preferredWidth = columnWidth;
    scrollHorizontal.size.preferredHeight = scrollGutterY;
    scrollHorizontal.showVertical = false;
    createScrollHints(horizCol, scrollHorizontal);
  };

  auto createInspector = [&]() {
    StackSpec columnSpec;
    columnSpec.size.preferredWidth = inspectorW;
    columnSpec.size.preferredHeight = contentH;
    columnSpec.padding = Insets{StudioDefaults::SurfaceInset,
                                StudioDefaults::SurfaceInset,
                                StudioDefaults::SurfaceInset,
                                StudioDefaults::SurfaceInset};
    columnSpec.gap = StudioDefaults::PanelGap;
    UiNode column = rightRail.createVerticalStack(columnSpec);

    SizeSpec headerSpacer;
    headerSpacer.preferredHeight = StudioDefaults::SectionHeaderOffsetY;
    column.createSpacer(headerSpacer);

    float sectionWidth = inspectorW - StudioDefaults::SurfaceInset * 2.0f;
    SizeSpec inspectorHeaderSize;
    inspectorHeaderSize.preferredWidth = sectionWidth;
    inspectorHeaderSize.preferredHeight = StudioDefaults::SectionHeaderHeight;
    createSectionHeader(column,
                        inspectorHeaderSize,
                        "Inspector",
                        TextRole::BodyBright);

    SizeSpec propsSize;
    propsSize.preferredWidth = sectionWidth;
    propsSize.preferredHeight = StudioDefaults::PanelHeightS;
    SectionPanelSpec propsSpec;
    propsSpec.size = propsSize;
    propsSpec.title = "Properties";
    SectionPanel propsPanel = createSectionPanel(column, propsSpec);
    float propsContentW = std::max(0.0f,
                                   propsSize.preferredWidth.value_or(0.0f) -
                                       propsSpec.contentInsetX -
                                       propsSpec.contentInsetRight);

    SizeSpec transformSize;
    transformSize.preferredWidth = sectionWidth;
    transformSize.preferredHeight = StudioDefaults::PanelHeightM + StudioDefaults::OpacityBarHeight;
    SectionPanelSpec transformSpec;
    transformSpec.size = transformSize;
    transformSpec.title = "Transform";
    SectionPanel transformPanel = createSectionPanel(column, transformSpec);
    float transformContentW = std::max(0.0f,
                                       transformSize.preferredWidth.value_or(0.0f) -
                                           transformSpec.contentInsetX -
                                           transformSpec.contentInsetRight);
    float transformContentH = std::max(0.0f,
                                       transformSize.preferredHeight.value_or(0.0f) -
                                           (transformSpec.headerInsetY +
                                            transformSpec.headerHeight +
                                            transformSpec.contentInsetY +
                                            transformSpec.contentInsetBottom));

    SizeSpec propsListSize;
    propsListSize.preferredWidth = propsContentW;
    createPropertyList(propsPanel.content,
                       propsListSize,
                       {{"Name", "SceneRoot"}, {"Tag", "Environment"}});

    StackSpec transformStackSpec;
    transformStackSpec.size.preferredWidth = transformContentW;
    transformStackSpec.size.preferredHeight = transformContentH;
    transformStackSpec.gap = StudioDefaults::PanelInset;
    UiNode transformStack = transformPanel.content.createVerticalStack(transformStackSpec);

    SizeSpec transformListSize;
    transformListSize.preferredWidth = transformContentW;
    createPropertyList(transformStack,
                       transformListSize,
                       {{"Position", "0, 0, 0"}, {"Scale", "1, 1, 1"}});

    StackSpec opacityOverlaySpec;
    opacityOverlaySpec.size.preferredWidth = transformContentW;
    opacityOverlaySpec.size.preferredHeight = StudioDefaults::OpacityBarHeight;
    UiNode opacityOverlay = transformStack.createOverlay(opacityOverlaySpec);

    SizeSpec opacityBarSize;
    opacityBarSize.preferredWidth = transformContentW;
    opacityBarSize.preferredHeight = StudioDefaults::OpacityBarHeight;
    SliderSpec opacitySlider;
    opacitySlider.size = opacityBarSize;
    opacitySlider.value = state.opacity;
    opacitySlider.trackThickness = StudioDefaults::OpacityBarHeight;
    opacitySlider.thumbSize = 0.0f;
    opacitySlider.trackStyle = rectToken(RectRole::PanelStrong);
    opacitySlider.fillStyle = rectToken(RectRole::Accent);
    opacitySlider.thumbStyle = rectToken(RectRole::PanelAlt);
    opacitySlider.focusStyle = rectToken(RectRole::Accent);
    opacitySlider.focusStyleOverride.opacity = 0.18f;
    opacitySlider.trackStyleOverride.opacity = 0.55f;
    opacitySlider.trackHoverOpacity = 0.65f;
    opacitySlider.trackPressedOpacity = 0.6f;
    opacitySlider.fillStyleOverride.opacity = 0.8f;
    opacitySlider.fillHoverOpacity = 1.0f;
    opacitySlider.fillPressedOpacity = 0.9f;
    opacitySlider.thumbStyleOverride.opacity = 0.0f;
    DemoUi* uiPtr = &ui;
    DemoState* statePtr = &state;
    opacitySlider.callbacks.onValueChanged = [uiPtr, statePtr](float next) {
      if (std::abs(statePtr->opacity - next) < 0.001f) {
        return;
      }
      statePtr->opacity = next;
      updateOpacityLabel(*statePtr);
      if (uiPtr->opacityValueNode.isValid()) {
        if (auto* node = uiPtr->frame.getNode(uiPtr->opacityValueNode)) {
          if (!node->primitives.empty()) {
            if (auto* prim = uiPtr->frame.getPrimitive(node->primitives.front())) {
              if (prim->type == PrimeFrame::PrimitiveType::Text) {
                prim->textBlock.text = statePtr->opacityLabel;
              }
            }
          }
        }
      }
    };
    opacityOverlay.createSlider(opacitySlider);

    SizeSpec opacityLabelSize;
    opacityLabelSize.preferredWidth = transformContentW;
    opacityLabelSize.preferredHeight = StudioDefaults::OpacityBarHeight;
    createTextLine(opacityOverlay,
                   "Opacity",
                   TextRole::SmallBright,
                   opacityLabelSize,
                   PrimeFrame::TextAlign::Start);
    UiNode opacityValue = createTextLine(opacityOverlay,
                                         state.opacityLabel,
                                         TextRole::SmallBright,
                                         opacityLabelSize,
                                         PrimeFrame::TextAlign::End);
    ui.opacityValueNode = opacityValue.nodeId();

    SizeSpec footerSpacer;
    footerSpacer.stretchY = 1.0f;
    column.createSpacer(footerSpacer);

    SizeSpec publishSize;
    publishSize.preferredWidth = sectionWidth;
    publishSize.stretchX = 1.0f;
    UiNode publish = createButton(column,
                                  "Publish",
                                  ButtonVariant::Primary,
                                  publishSize);
  };

  auto createStatus = [&]() {
    StackSpec rowSpec;
    rowSpec.size.preferredWidth = shellWidth;
    rowSpec.size.preferredHeight = StudioDefaults::StatusHeight;
    rowSpec.padding = Insets{StudioDefaults::SurfaceInset, 0.0f, StudioDefaults::SurfaceInset, 0.0f};
    rowSpec.gap = StudioDefaults::PanelInset;
    UiNode bar = statusBar.createHorizontalStack(rowSpec);

    SizeSpec lineSize;
    lineSize.preferredHeight = StudioDefaults::StatusHeight;
    createTextLine(bar, "Ready", TextRole::SmallMuted, lineSize);

    SizeSpec spacer;
    spacer.stretchX = 1.0f;
    bar.createSpacer(spacer);

    createTextLine(bar, "PrimeFrame Demo", TextRole::SmallMuted, lineSize);

    bar.createSpacer(spacer);

    SizeSpec fpsSize;
    fpsSize.preferredWidth = 120.0f;
    fpsSize.preferredHeight = StudioDefaults::StatusHeight;
    UiNode fpsNode = createTextLine(bar,
                                    "FPS --",
                                    TextRole::SmallMuted,
                                    fpsSize,
                                    PrimeFrame::TextAlign::End);
    ui.fpsNode = fpsNode.nodeId();
  };

  createTopbar();
  createSidebar();
  createContent();
  createInspector();
  createStatus();

  if (ui.fpsNode.isValid()) {
    auto* node = ui.frame.getNode(ui.fpsNode);
    if (node && !node->primitives.empty()) {
      auto* prim = ui.frame.getPrimitive(node->primitives.front());
      if (prim && prim->type == PrimeFrame::PrimitiveType::Text) {
        prim->textBlock.text.reserve(32);
      }
    }
  }
}

PrimeFrame::Event toPrimeFrameEvent(const PointerEvent& event) {
  PrimeFrame::Event out;
  out.pointerId = static_cast<int>(event.pointerId);
  out.x = static_cast<float>(event.x);
  out.y = static_cast<float>(event.y);
  switch (event.phase) {
    case PointerPhase::Down:
      out.type = PrimeFrame::EventType::PointerDown;
      break;
    case PointerPhase::Move:
      out.type = PrimeFrame::EventType::PointerMove;
      break;
    case PointerPhase::Up:
      out.type = PrimeFrame::EventType::PointerUp;
      break;
    case PointerPhase::Cancel:
      out.type = PrimeFrame::EventType::PointerCancel;
      break;
  }
  return out;
}

PrimeFrame::Event toPrimeFrameEvent(const KeyEvent& event) {
  PrimeFrame::Event out;
  out.type = event.pressed ? PrimeFrame::EventType::KeyDown : PrimeFrame::EventType::KeyUp;
  out.key = static_cast<int>(event.keyCode);
  out.modifiers = static_cast<uint32_t>(event.modifiers);
  return out;
}

} // namespace

int main(int argc, char** argv) {
  PerfConfig perf{};
  if (!parseArgs(argc, argv, perf)) {
    return 2;
  }
  bool perfEnabled = perf.mode != PerfMode::Off;
  if (perf.resizeSteps == 0u) {
    perf.resizeSteps = 1u;
  }
  if (perf.resizeIntervalMs == 0u) {
    perf.resizeIntervalMs = 16u;
  }

  std::cout << "PrimeHost v" << PrimeHostVersion << " PrimeStage demo" << std::endl;

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
  config.width = perf.width;
  config.height = perf.height;
  config.resizable = true;
  if (perfEnabled && !perf.headlessSet) {
    perf.headless = (perf.mode == PerfMode::Headless);
  }
  config.headless = perf.headless;
  if (!config.headless) {
    config.title = std::string("PrimeHost Studio Demo");
  }

  auto surfaceResult = host->createSurface(config);
  if (!surfaceResult) {
    std::cerr << "Failed to create surface (" << static_cast<int>(surfaceResult.error().code) << ")\n";
    return 1;
  }
  SurfaceId surfaceId = surfaceResult.value();

  FrameConfig frameConfig{};
  if (perf.mode == PerfMode::Windowed) {
    frameConfig.framePolicy = FramePolicy::Continuous;
  } else {
    frameConfig.framePolicy = FramePolicy::EventDriven;
  }
  frameConfig.framePacingSource = FramePacingSource::Platform;
  host->setFrameConfig(surfaceId, frameConfig);
  FramePolicy baseFramePolicy = frameConfig.framePolicy;

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

  DemoState state{};
  DemoUi ui{};
  ui.host = host.get();
  ui.logicalWidth = static_cast<float>(config.width);
  ui.logicalHeight = static_cast<float>(config.height);
  ui.scale = 1.0f;

  FpsTracker fps{};
  PerfState perfState{};
  bool running = true;
  bool fullscreen = false;
  bool minimized = false;
  bool maximized = false;
  bool relativePointer = false;
  bool cursorIBeam = false;
  bool cursorIBeamActive = false;
  bool suppressEventLogs = perfEnabled;

  Host* hostPtr = host.get();
  auto setFramePolicy = [&](FramePolicy policy) {
    if (frameConfig.framePolicy == policy) {
      return;
    }
    frameConfig.framePolicy = policy;
    hostPtr->setFrameConfig(surfaceId, frameConfig);
  };
  auto markRenderDirty = [&]() {
    ui.renderDirty = true;
  };
  auto captureFocusForRebuild = [&]() {
    PrimeFrame::NodeId focused = ui.focus.focusedNode();
    if (focused == ui.treeViewNode) {
      ui.pendingTreeFocus = true;
      ui.pendingSearchFocus = false;
      return;
    }
    ui.pendingSearchFocus = (focused == ui.searchFieldNode);
  };
  auto applyPendingFocus = [&]() {
    if (ui.pendingTreeFocus && ui.treeViewNode.isValid()) {
      ui.pendingTreeFocus = false;
      auto roots = ui.frame.roots();
      if (!roots.empty()) {
        ui.focus.setActiveRoot(ui.frame, ui.layout, roots.back());
      }
      ui.focus.setFocus(ui.frame, ui.layout, ui.treeViewNode);
      return;
    }
    if (ui.pendingSearchFocus && ui.searchFieldNode.isValid()) {
      ui.pendingSearchFocus = false;
      auto roots = ui.frame.roots();
      if (!roots.empty()) {
        ui.focus.setActiveRoot(ui.frame, ui.layout, roots.back());
      }
      ui.focus.setFocus(ui.frame, ui.layout, ui.searchFieldNode);
    }
  };
  Callbacks callbacks{};
  callbacks.onFrame = [&](SurfaceId id, const FrameTiming&, const FrameDiagnostics&) {
    auto now = std::chrono::steady_clock::now();
    if (PrimeStage::updateTextFieldBlink(state.searchField, now, SearchBlinkInterval)) {
      ui.needsRebuild = true;
      ui.renderDirty = true;
    }
    bool reportFps = !perfEnabled && fps.shouldReport() && fps.sampleCount() > 0u;
    FpsStats fpsStats{};
    if (reportFps) {
      fpsStats = fps.stats();
    }

    if (perfEnabled) {
      ui.renderDirty = true;
    }
    bool needsRender = ui.renderDirty || ui.needsRebuild || ui.layoutDirty || reportFps;
    if (!needsRender) {
      return;
    }

    auto acquireStart = std::chrono::steady_clock::now();
    auto frameResult = host->acquireFrameBuffer(id);
    auto acquireEnd = std::chrono::steady_clock::now();
    if (!frameResult) {
      return;
    }
    FrameBuffer fb = frameResult.value();
    float scale = fb.scale > 0.0f ? fb.scale : 1.0f;
    float logicalW = static_cast<float>(fb.size.width) / scale;
    float logicalH = static_cast<float>(fb.size.height) / scale;
    if (std::abs(ui.logicalWidth - logicalW) > 0.5f || std::abs(ui.logicalHeight - logicalH) > 0.5f) {
      ui.logicalWidth = logicalW;
      ui.logicalHeight = logicalH;
      ui.scale = scale;
      ui.needsRebuild = true;
      ui.layoutDirty = true;
      ui.renderDirty = true;
    }

    if (ui.needsRebuild) {
      if (ui.pointerDownCount == 0) {
        captureFocusForRebuild();
        ui.router.clearAllCaptures();
        buildStudioUi(ui, state);
        ui.needsRebuild = false;
        ui.layoutDirty = true;
      }
    }

    if (ui.layoutDirty) {
      auto layoutStart = std::chrono::steady_clock::now();
      PrimeFrame::LayoutOptions options;
      options.rootWidth = ui.logicalWidth;
      options.rootHeight = ui.logicalHeight;
      ui.layoutEngine.layout(ui.frame, ui.layout, options);
      auto layoutEnd = std::chrono::steady_clock::now();
      if (perfEnabled && perf.resizeStress) {
        auto layoutMs = std::chrono::duration<double, std::milli>(layoutEnd - layoutStart).count();
        if (layoutMs > 100.0 &&
            (ui.lastSlowLog.time_since_epoch().count() == 0 ||
             now - ui.lastSlowLog > std::chrono::milliseconds(500))) {
          std::cout << "slow layout " << layoutMs << "ms\n";
          ui.lastSlowLog = now;
        }
      }
      ui.layoutDirty = false;
    }

    applyPendingFocus();

    if (reportFps) {
      if (ui.fpsNode.isValid()) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "FPS %.0f", fpsStats.fps);
        auto* node = ui.frame.getNode(ui.fpsNode);
        if (node && !node->primitives.empty()) {
          auto* prim = ui.frame.getPrimitive(node->primitives.front());
          if (prim && prim->type == PrimeFrame::PrimitiveType::Text) {
            if (prim->textBlock.text != buffer) {
              prim->textBlock.text.assign(buffer);
              ui.renderDirty = true;
            }
          }
        }
      }
    }

    PrimeStage::RenderTarget target;
    target.pixels = fb.pixels;
    target.width = fb.size.width;
    target.height = fb.size.height;
    target.stride = fb.stride;
    target.scale = scale;
    auto renderStart = std::chrono::steady_clock::now();
    bool rendered = renderFrameToTarget(ui.frame, ui.layout, target);
    auto renderEnd = std::chrono::steady_clock::now();
    if (rendered) {
      auto presentStart = std::chrono::steady_clock::now();
      host->presentFrameBuffer(id, fb);
      auto presentEnd = std::chrono::steady_clock::now();
      if (perfEnabled && perf.resizeStress) {
        auto acquireMs = std::chrono::duration<double, std::milli>(acquireEnd - acquireStart).count();
        auto renderMs = std::chrono::duration<double, std::milli>(renderEnd - renderStart).count();
        auto presentMs = std::chrono::duration<double, std::milli>(presentEnd - presentStart).count();
        if ((acquireMs > 100.0 || renderMs > 100.0 || presentMs > 100.0) &&
            (ui.lastSlowLog.time_since_epoch().count() == 0 ||
             now - ui.lastSlowLog > std::chrono::milliseconds(500))) {
          std::cout << "slow frame acquire=" << acquireMs
                    << "ms render=" << renderMs
                    << "ms present=" << presentMs
                    << "ms size=" << fb.size.width << "x" << fb.size.height
                    << "\n";
          ui.lastSlowLog = now;
        }
      }
      if (perfEnabled) {
        {
          std::lock_guard<std::mutex> lock(perfState.mutex);
          if (!perfState.done) {
            if (perfState.presented == perf.warmupFrames) {
              fps.reset();
            }
            if (perfState.presented >= perf.warmupFrames) {
              fps.framePresented();
            }
            perfState.presented++;
            perfState.cv.notify_one();
          }
        }
      } else {
        fps.framePresented();
      }
      ui.renderDirty = false;
      if (reportFps) {
        std::cout << "fps " << fpsStats.fps
                  << " p95=" << std::chrono::duration<double, std::milli>(fpsStats.p95FrameTime).count()
                  << "ms p99=" << std::chrono::duration<double, std::milli>(fpsStats.p99FrameTime).count()
                  << "ms\n";
      }
    }
  };
  callbacks.onEvents = [&](const EventBatch& batch) {
    for (const auto& event : batch.events) {
      if (auto* input = std::get_if<InputEvent>(&event.payload)) {
        if (auto* pointer = std::get_if<PointerEvent>(input)) {
          if (pointer->phase == PointerPhase::Down) {
            ui.pointerDownCount++;
          } else if (pointer->phase == PointerPhase::Up || pointer->phase == PointerPhase::Cancel) {
            ui.pointerDownCount = std::max(0, ui.pointerDownCount - 1);
          }
          ui.cursorX = static_cast<float>(pointer->x);
          ui.cursorY = static_cast<float>(pointer->y);
          if (!suppressEventLogs && pointer->phase != PointerPhase::Move) {
            std::cout << "pointer " << pointerPhaseLabel(pointer->phase)
                      << " type=" << pointerTypeLabel(pointer->deviceType)
                      << " x=" << pointer->x << " y=" << pointer->y << "\n";
          }
          if (ui.needsRebuild && ui.pointerDownCount == 0) {
            captureFocusForRebuild();
            buildStudioUi(ui, state);
            ui.needsRebuild = false;
            ui.layoutDirty = true;
            ui.renderDirty = true;
          }
          if (ui.layoutDirty) {
            PrimeFrame::LayoutOptions options;
            options.rootWidth = ui.logicalWidth;
            options.rootHeight = ui.logicalHeight;
            ui.layoutEngine.layout(ui.frame, ui.layout, options);
            ui.layoutDirty = false;
          }
          applyPendingFocus();
          bool searchFocusedBefore = state.searchField.focused;
          PrimeFrame::Event pfEvent = toPrimeFrameEvent(*pointer);
          bool handled = ui.router.dispatch(pfEvent, ui.frame, ui.layout, &ui.focus);
          if (handled) {
            ui.renderDirty = true;
            markRenderDirty();
          }
          if (searchFocusedBefore != state.searchField.focused && !perfEnabled) {
            setFramePolicy(state.searchField.focused ? FramePolicy::Continuous : baseFramePolicy);
          }
          if (pointer->phase == PointerPhase::Down) {
            if (!handled) {
              if (ui.focus.clearFocus(ui.frame)) {
                ui.renderDirty = true;
              }
            }
          }

          bool useIBeam = cursorIBeam ||
                          state.searchField.cursorHint == CursorHint::IBeam ||
                          state.boardSelection.cursorHint == CursorHint::IBeam;
          if (useIBeam != cursorIBeamActive) {
            cursorIBeamActive = useIBeam;
            hostPtr->setCursorShape(surfaceId, useIBeam ? CursorShape::IBeam : CursorShape::Arrow);
          }
        } else if (auto* key = std::get_if<KeyEvent>(input)) {
          if (ui.needsRebuild && ui.pointerDownCount == 0) {
            captureFocusForRebuild();
            buildStudioUi(ui, state);
            ui.needsRebuild = false;
            ui.layoutDirty = true;
            ui.renderDirty = true;
          }
          if (ui.layoutDirty) {
            PrimeFrame::LayoutOptions options;
            options.rootWidth = ui.logicalWidth;
            options.rootHeight = ui.logicalHeight;
            ui.layoutEngine.layout(ui.frame, ui.layout, options);
            ui.layoutDirty = false;
          }
          applyPendingFocus();
          PrimeFrame::Event keyEvent = toPrimeFrameEvent(*key);
          bool searchFocusedBefore = state.searchField.focused;
          bool handled = ui.router.dispatch(keyEvent, ui.frame, ui.layout, &ui.focus);
          if (handled) {
            ui.renderDirty = true;
            markRenderDirty();
          }
          if (searchFocusedBefore != state.searchField.focused && !perfEnabled) {
            setFramePolicy(state.searchField.focused ? FramePolicy::Continuous : baseFramePolicy);
          }
          if (handled) {
            continue;
          }
          if (key->pressed) {
            bool altPressed = (key->modifiers &
                               static_cast<KeyModifierMask>(KeyModifier::Alt)) != 0u;
            bool superPressed = (key->modifiers &
                                 static_cast<KeyModifierMask>(KeyModifier::Super)) != 0u;
            if ((altPressed || superPressed) && key->keyCode == KeyQ && !key->repeat) {
              running = false;
              continue;
            }
            if (!key->repeat) {
              if (key->keyCode == KeyEscape) {
                running = false;
              }
              if (isToggleKey(key->keyCode)) {
                markRenderDirty();
              }
              if (key->keyCode == KeyF) {
                fullscreen = !fullscreen;
                hostPtr->setSurfaceFullscreen(surfaceId, fullscreen);
              } else if (key->keyCode == KeyM) {
                minimized = !minimized;
                hostPtr->setSurfaceMinimized(surfaceId, minimized);
              } else if (key->keyCode == KeyX) {
                maximized = !maximized;
                hostPtr->setSurfaceMaximized(surfaceId, maximized);
              } else if (key->keyCode == KeyR) {
                relativePointer = !relativePointer;
                hostPtr->setRelativePointerCapture(surfaceId, relativePointer);
              } else if (key->keyCode == KeyI) {
                cursorIBeam = !cursorIBeam;
                bool useIBeam = cursorIBeam ||
                                state.searchField.cursorHint == CursorHint::IBeam ||
                                state.boardSelection.cursorHint == CursorHint::IBeam;
                cursorIBeamActive = useIBeam;
                hostPtr->setCursorShape(surfaceId, useIBeam ? CursorShape::IBeam : CursorShape::Arrow);
              } else if (key->keyCode == KeyC) {
                hostPtr->setClipboardText("PrimeHost clipboard sample");
              } else if (key->keyCode == KeyV) {
                auto size = hostPtr->clipboardTextSize();
                if (size && size.value() > 0u) {
                  std::vector<char> bufferText(size.value());
                  auto text = hostPtr->clipboardText(bufferText);
                  if (text) {
                    std::cout << "clipboard: " << text.value() << "\n";
                  }
                }
              } else if (key->keyCode == KeyO) {
                FileDialogConfig dialog{};
                dialog.mode = FileDialogMode::OpenFile;
                std::array<char, 4096> pathBuffer{};
                auto result = hostPtr->fileDialog(dialog, pathBuffer);
                if (result && result->accepted) {
                  std::cout << "selected: " << result->path << "\n";
                }
              } else if (key->keyCode == KeyP) {
                ScreenshotConfig shot{};
                shot.scope = ScreenshotScope::Surface;
                auto status = hostPtr->writeSurfaceScreenshot(surfaceId, "/tmp/primehost-shot.png", shot);
                std::cout << "screenshot: " << (status ? "ok" : "failed") << "\n";
              }
            }
          }
        } else if (auto* text = std::get_if<TextEvent>(input)) {
          auto view = textFromSpan(batch, text->text);
          if (view) {
            if (ui.needsRebuild && ui.pointerDownCount == 0) {
              captureFocusForRebuild();
              buildStudioUi(ui, state);
              ui.needsRebuild = false;
              ui.layoutDirty = true;
              ui.renderDirty = true;
            }
            if (ui.layoutDirty) {
              PrimeFrame::LayoutOptions options;
              options.rootWidth = ui.logicalWidth;
              options.rootHeight = ui.logicalHeight;
              ui.layoutEngine.layout(ui.frame, ui.layout, options);
              ui.layoutDirty = false;
            }
            applyPendingFocus();
            PrimeFrame::Event textEvent;
            textEvent.type = PrimeFrame::EventType::TextInput;
            textEvent.text = std::string(*view);
            bool handled = ui.router.dispatch(textEvent, ui.frame, ui.layout, &ui.focus);
            if (handled) {
              ui.renderDirty = true;
              markRenderDirty();
            }
          }
          if (view && !suppressEventLogs) {
            std::cout << "text: " << *view << "\n";
          }
        } else if (auto* scroll = std::get_if<ScrollEvent>(input)) {
          if (!suppressEventLogs) {
            std::cout << "scroll dx=" << scroll->deltaX << " dy=" << scroll->deltaY
                      << (scroll->isLines ? " lines" : " px") << "\n";
          }
          if (ui.needsRebuild && ui.pointerDownCount == 0) {
            captureFocusForRebuild();
            buildStudioUi(ui, state);
            ui.needsRebuild = false;
            ui.layoutDirty = true;
            ui.renderDirty = true;
          }
          if (ui.layoutDirty) {
            PrimeFrame::LayoutOptions options;
            options.rootWidth = ui.logicalWidth;
            options.rootHeight = ui.logicalHeight;
            ui.layoutEngine.layout(ui.frame, ui.layout, options);
            ui.layoutDirty = false;
          }
          applyPendingFocus();
          PrimeFrame::Event scrollEvent;
          scrollEvent.type = PrimeFrame::EventType::PointerScroll;
          scrollEvent.x = ui.cursorX;
          scrollEvent.y = ui.cursorY;
          float scale = scroll->isLines ? ui.scrollLineHeight : 1.0f;
          scrollEvent.scrollX = scroll->deltaX * scale;
          scrollEvent.scrollY = scroll->deltaY * scale;
          bool handled = ui.router.dispatch(scrollEvent, ui.frame, ui.layout, &ui.focus);
          if (handled) {
            ui.renderDirty = true;
            markRenderDirty();
          }
        } else if (auto* device = std::get_if<DeviceEvent>(input)) {
          if (!suppressEventLogs) {
            std::cout << "device " << deviceTypeLabel(device->deviceType)
                      << (device->connected ? " connected" : " disconnected")
                      << " id=" << device->deviceId << "\n";
          }
        } else if (auto* gamepad = std::get_if<GamepadButtonEvent>(input)) {
          if (!suppressEventLogs) {
            std::cout << "gamepad button id=" << gamepad->controlId
                      << " pressed=" << gamepad->pressed << "\n";
          }
        } else if (auto* axis = std::get_if<GamepadAxisEvent>(input)) {
          if (!suppressEventLogs) {
            std::cout << "gamepad axis id=" << axis->controlId << " value=" << axis->value << "\n";
          }
        }
      } else if (auto* resize = std::get_if<ResizeEvent>(&event.payload)) {
        auto now = std::chrono::steady_clock::now();
        ui.lastResizeEvent = now;
        if (!suppressEventLogs &&
            (ui.lastResizeLog.time_since_epoch().count() == 0 ||
             now - ui.lastResizeLog > std::chrono::milliseconds(500))) {
          std::cout << "resize " << resize->width << "x" << resize->height
                    << " scale=" << resize->scale << "\n";
          ui.lastResizeLog = now;
        }
        ui.logicalWidth = static_cast<float>(resize->width);
        ui.logicalHeight = static_cast<float>(resize->height);
        ui.scale = resize->scale;
        ui.needsRebuild = true;
        ui.layoutDirty = true;
        markRenderDirty();
      } else if (auto* drop = std::get_if<DropEvent>(&event.payload)) {
        if (!suppressEventLogs) {
          dumpDropPaths(batch, *drop);
        }
      } else if (auto* focus = std::get_if<FocusEvent>(&event.payload)) {
        if (!suppressEventLogs) {
          std::cout << "focus " << focus->focused << "\n";
        }
      } else if (auto* power = std::get_if<PowerEvent>(&event.payload)) {
        if (!suppressEventLogs && power->lowPowerModeEnabled.has_value()) {
          std::cout << "power lowPower=" << power->lowPowerModeEnabled.value() << "\n";
        }
      } else if (auto* thermal = std::get_if<ThermalEvent>(&event.payload)) {
        if (!suppressEventLogs) {
          std::cout << "thermal state=" << static_cast<int>(thermal->state) << "\n";
        }
      } else if (auto* lifecycle = std::get_if<LifecycleEvent>(&event.payload)) {
        if (lifecycle->phase == LifecyclePhase::Destroyed) {
          running = false;
        }
      }
    }
  };
  host->setCallbacks(callbacks);
  host->requestFrame(surfaceId, true);

  if (perfEnabled) {
    uint32_t totalFrames = perf.warmupFrames + perf.measureFrames;
    if (perf.measureFrames < 2u) {
      std::cout << "perf: --frames must be >= 2\n";
      host->destroySurface(surfaceId);
      return 2;
    }
    if (perf.resizeStress && config.headless) {
      std::cout << "perf: resize-stress requires a windowed surface\n";
      host->destroySurface(surfaceId);
      return 2;
    }
    uint32_t timeoutMs = perf.timeoutMs;
    if (timeoutMs == 0u) {
      uint32_t minMs = 5000u;
      uint32_t estimateMs = static_cast<uint32_t>(totalFrames * 1000u / 30u);
      timeoutMs = std::max(minMs, estimateMs);
    }
    bool timeout = false;
    auto resizeStep = [&](std::chrono::steady_clock::time_point now) {
      if (!perf.resizeStress) {
        return;
      }
      if (perf.resizeMaxW <= perf.resizeMinW || perf.resizeMaxH <= perf.resizeMinH) {
        return;
      }
      if (perfState.lastResize.time_since_epoch().count() == 0) {
        perfState.lastResize = now;
      }
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - perfState.lastResize);
      if (elapsed.count() < static_cast<int64_t>(perf.resizeIntervalMs)) {
        return;
      }
      perfState.lastResize = now;
      double step = 1.0 / static_cast<double>(perf.resizeSteps);
      perfState.resizePhase += step * static_cast<double>(perfState.resizeDir);
      if (perfState.resizePhase >= 1.0) {
        perfState.resizePhase = 1.0;
        perfState.resizeDir = -1;
      } else if (perfState.resizePhase <= 0.0) {
        perfState.resizePhase = 0.0;
        perfState.resizeDir = 1;
      }
      double t = perfState.resizePhase;
      uint32_t width = static_cast<uint32_t>(std::lround(
          static_cast<double>(perf.resizeMinW) +
          (static_cast<double>(perf.resizeMaxW) - static_cast<double>(perf.resizeMinW)) * t));
      uint32_t height = static_cast<uint32_t>(std::lround(
          static_cast<double>(perf.resizeMinH) +
          (static_cast<double>(perf.resizeMaxH) - static_cast<double>(perf.resizeMinH)) * t));
      if (width != perfState.lastResizeW || height != perfState.lastResizeH) {
        hostPtr->setSurfaceSize(surfaceId, width, height);
        perfState.lastResizeW = width;
        perfState.lastResizeH = height;
      }
    };
    if (perf.mode == PerfMode::Headless) {
      auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
      while (true) {
        {
          std::lock_guard<std::mutex> lock(perfState.mutex);
          if (perfState.presented >= totalFrames) {
            break;
          }
        }
        if (std::chrono::steady_clock::now() >= deadline) {
          timeout = true;
          break;
        }
        host->requestFrame(surfaceId, true);
      }
    } else {
      auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
      std::array<PrimeHost::Event, 64> events{};
      std::array<char, 2048> text{};
      EventBuffer buffer{
          std::span<PrimeHost::Event>(events.data(), events.size()),
          std::span<char>(text.data(), text.size()),
      };
      while (true) {
        auto now = std::chrono::steady_clock::now();
        {
          std::lock_guard<std::mutex> lock(perfState.mutex);
          if (perfState.presented >= totalFrames) {
            break;
          }
        }
        if (now >= deadline) {
          timeout = true;
          break;
        }
        host->pollEvents(buffer);
        resizeStep(now);
        std::unique_lock<std::mutex> lock(perfState.mutex);
        perfState.cv.wait_for(lock, std::chrono::milliseconds(10));
      }
      {
        std::lock_guard<std::mutex> lock(perfState.mutex);
        perfState.done = true;
      }
    }
    FpsStats stats{};
    {
      std::lock_guard<std::mutex> lock(perfState.mutex);
      stats = fps.stats();
    }
    double p95Ms = std::chrono::duration<double, std::milli>(stats.p95FrameTime).count();
    bool pass = stats.fps >= perf.minFps;
    if (perf.maxP95Ms > 0.0) {
      pass = pass && p95Ms <= perf.maxP95Ms;
    }
    std::cout << "perf fps=" << stats.fps
              << " p95=" << p95Ms << "ms"
              << " minFps=" << perf.minFps;
    if (perf.maxP95Ms > 0.0) {
      std::cout << " maxP95Ms=" << perf.maxP95Ms;
    }
    if (timeout) {
      std::cout << " TIMEOUT";
      pass = false;
    }
    std::cout << " " << (pass ? "PASS" : "FAIL") << "\n";
    host->destroySurface(surfaceId);
    return pass ? 0 : 2;
  }

  std::cout << "Controls: ESC quit, F fullscreen, M minimize, X maximize, R relative pointer,"
               " C copy, V paste, O open dialog, P screenshot, I toggle cursor." << std::endl;

  while (running) {
    host->waitEvents();
    if (frameConfig.framePolicy != FramePolicy::Continuous &&
        (ui.renderDirty || ui.needsRebuild || ui.layoutDirty)) {
      host->requestFrame(surfaceId, true);
    }
  }

  host->destroySurface(surfaceId);
  return 0;
}
