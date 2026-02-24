#include "PrimeHost/PrimeHost.h"
#include "PrimeStage/Render.h"
#include "PrimeStage/StudioUi.h"

#include "PrimeFrame/Events.h"
#include "PrimeFrame/Layout.h"
#include "PrimeFrame/Theme.h"
#include "PrimeStage/TextSelection.h"

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
#include <string>
#include <string_view>
#include <vector>

using namespace PrimeHost;
using namespace PrimeFrame;
using namespace PrimeStage;
using namespace PrimeStage::Studio;

namespace {

constexpr uint32_t KeyEscape = 0x29u;
constexpr uint32_t KeyReturn = 0x28u;
constexpr uint32_t KeyBackspace = 0x2Au;
constexpr uint32_t KeyTab = 0x2Bu;
constexpr uint32_t KeyA = 0x04u;
constexpr uint32_t KeyQ = 0x14u;
constexpr uint32_t KeyDelete = 0x4Cu;
constexpr uint32_t KeyEnd = 0x4Du;
constexpr uint32_t KeyRight = 0x4Fu;
constexpr uint32_t KeyLeft = 0x50u;
constexpr uint32_t KeyDown = 0x51u;
constexpr uint32_t KeyUp = 0x52u;
constexpr uint32_t KeyHome = 0x4Au;
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
  PrimeFrame::NodeId treeViewNode{};
  PrimeFrame::NodeId searchFieldNode{};
  PrimeFrame::NodeId boardTextNode{};
  PrimeStage::TreeViewSpec treeViewSpec{};
  std::vector<PrimeStage::TextSelectionLine> boardTextLines;
  float boardLineHeight = 0.0f;
  struct TreeRowRef {
    PrimeStage::TreeNode* node = nullptr;
    int depth = 0;
    bool hasChildren = false;
  };
  std::vector<TreeRowRef> treeRows;
  float logicalWidth = 0.0f;
  float logicalHeight = 0.0f;
  float scale = 1.0f;
  bool needsRebuild = true;
  bool layoutDirty = true;
  bool renderDirty = true;
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
  std::string searchText;
  uint32_t searchCursor = 0u;
  bool searchFocused = false;
  bool searchHover = false;
  bool searchCursorVisible = false;
  bool searchSelecting = false;
  uint32_t searchSelectionAnchor = 0u;
  uint32_t searchSelectionStart = 0u;
  uint32_t searchSelectionEnd = 0u;
  int searchPointerId = -1;
  std::chrono::steady_clock::time_point nextSearchBlink{};
  std::string boardText;
  bool boardFocused = false;
  bool boardHover = false;
  bool boardSelecting = false;
  uint32_t boardSelectionAnchor = 0u;
  uint32_t boardSelectionStart = 0u;
  uint32_t boardSelectionEnd = 0u;
  int boardPointerId = -1;
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
                                    PrimeStage::TreeNode{"Environment"}},
                                   true,
                                   false},
              PrimeStage::TreeNode{"UI",
                                   {PrimeStage::TreeNode{"Sidebar"},
                                    PrimeStage::TreeNode{"Toolbar", {PrimeStage::TreeNode{"Buttons"}}, false, false},
                                    PrimeStage::TreeNode{"Panels",
                                                         {PrimeStage::TreeNode{"TreeView", {}, true, true},
                                                          PrimeStage::TreeNode{"Rows"}},
                                                         true,
                                                         false}},
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

void flattenTree(std::vector<PrimeStage::TreeNode>& nodes,
                 int depth,
                 std::vector<DemoUi::TreeRowRef>& out) {
  for (auto& node : nodes) {
    DemoUi::TreeRowRef row;
    row.node = &node;
    row.depth = depth;
    row.hasChildren = !node.children.empty();
    out.push_back(row);
    if (node.expanded && !node.children.empty()) {
      flattenTree(node.children, depth + 1, out);
    }
  }
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

bool pointInNode(const PrimeFrame::LayoutOutput& layout, PrimeFrame::NodeId node, float x, float y) {
  const PrimeFrame::LayoutOut* out = layout.get(node);
  if (!out || !out->visible) {
    return false;
  }
  return x >= out->absX && y >= out->absY && x <= (out->absX + out->absW) && y <= (out->absY + out->absH);
}

bool handleTreeClick(DemoUi& ui, DemoState& state, float px, float py) {
  if (!ui.treeViewNode.isValid() || ui.treeRows.empty()) {
    return false;
  }
  const PrimeFrame::LayoutOut* treeOut = ui.layout.get(ui.treeViewNode);
  if (!treeOut || !treeOut->visible) {
    return false;
  }
  if (px < treeOut->absX || px > treeOut->absX + treeOut->absW ||
      py < treeOut->absY || py > treeOut->absY + treeOut->absH) {
    return false;
  }
  const PrimeStage::TreeViewSpec& spec = ui.treeViewSpec;
  float localX = px - treeOut->absX;
  float localY = py - treeOut->absY;
  float rowSpan = spec.rowHeight + spec.rowGap;
  if (rowSpan <= 0.0f) {
    return false;
  }
  float rowY = localY - spec.rowStartY;
  if (rowY < 0.0f) {
    return false;
  }
  int index = static_cast<int>(rowY / rowSpan);
  if (index < 0 || index >= static_cast<int>(ui.treeRows.size())) {
    return false;
  }
  float withinRow = rowY - static_cast<float>(index) * rowSpan;
  if (withinRow > spec.rowHeight) {
    return false;
  }
  auto& row = ui.treeRows[static_cast<size_t>(index)];
  if (!row.node) {
    return false;
  }
  float indent = row.depth > 0 ? spec.indent * static_cast<float>(row.depth) : 0.0f;
  float glyphX = spec.caretBaseX + indent;
  float glyphY = spec.rowStartY + static_cast<float>(index) * rowSpan +
                 (spec.rowHeight - spec.caretSize) * 0.5f;
  bool onCaret = row.hasChildren &&
                 localX >= glyphX && localX <= glyphX + spec.caretSize &&
                 localY >= glyphY && localY <= glyphY + spec.caretSize;
  if (onCaret) {
    row.node->expanded = !row.node->expanded;
    return true;
  }
  if (!row.node->selected) {
    clearTreeSelection(state.treeNodes);
    row.node->selected = true;
    return true;
  }
  return false;
}

float resolveLineHeight(PrimeFrame::Frame& frame, PrimeFrame::TextStyleToken style) {
  PrimeFrame::Theme const* theme = frame.getTheme(PrimeFrame::DefaultThemeId);
  if (!theme) {
    return 0.0f;
  }
  PrimeFrame::ResolvedTextStyle resolved = PrimeFrame::resolveTextStyle(*theme, style, {});
  float lineHeight = resolved.lineHeight > 0.0f ? resolved.lineHeight : resolved.size * 1.2f;
  return lineHeight;
}

bool hasSearchSelection(const DemoState& state, uint32_t& start, uint32_t& end) {
  start = std::min(state.searchSelectionStart, state.searchSelectionEnd);
  end = std::max(state.searchSelectionStart, state.searchSelectionEnd);
  return start != end;
}

void clearSearchSelection(DemoState& state, uint32_t cursor) {
  state.searchSelectionAnchor = cursor;
  state.searchSelectionStart = cursor;
  state.searchSelectionEnd = cursor;
  state.searchSelecting = false;
  state.searchPointerId = -1;
}

bool hasBoardSelection(const DemoState& state, uint32_t& start, uint32_t& end) {
  start = std::min(state.boardSelectionStart, state.boardSelectionEnd);
  end = std::max(state.boardSelectionStart, state.boardSelectionEnd);
  return start != end;
}

void clearBoardSelection(DemoState& state, uint32_t cursor) {
  state.boardSelectionAnchor = cursor;
  state.boardSelectionStart = cursor;
  state.boardSelectionEnd = cursor;
  state.boardSelecting = false;
  state.boardPointerId = -1;
}

void buildStudioUi(DemoUi& ui, DemoState& state) {
  ui.frame = PrimeFrame::Frame{};
  ui.fpsNode = PrimeFrame::NodeId{};
  ui.opacityValueNode = PrimeFrame::NodeId{};
  ui.treeViewNode = PrimeFrame::NodeId{};
  ui.searchFieldNode = PrimeFrame::NodeId{};
  ui.boardTextNode = PrimeFrame::NodeId{};
  ui.boardTextLines.clear();
  ui.boardLineHeight = 0.0f;
  ui.treeRows.clear();
  ensureTreeState(state);
  updateOpacityLabel(state);
  state.searchCursor = std::min(state.searchCursor, static_cast<uint32_t>(state.searchText.size()));
  state.searchSelectionStart = std::min(state.searchSelectionStart,
                                        static_cast<uint32_t>(state.searchText.size()));
  state.searchSelectionEnd = std::min(state.searchSelectionEnd,
                                      static_cast<uint32_t>(state.searchText.size()));
  state.searchSelectionAnchor = std::min(state.searchSelectionAnchor,
                                         static_cast<uint32_t>(state.searchText.size()));
  if (state.boardText.empty()) {
    std::string base =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
        "Sed do eiusmod tempor incididunt ut labore et dolore. "
        "Ut enim ad minim veniam, quis nostrud exercitation.";
    state.boardText = base + " " + base + " " + base;
  }
  state.boardSelectionStart = std::min(state.boardSelectionStart,
                                       static_cast<uint32_t>(state.boardText.size()));
  state.boardSelectionEnd = std::min(state.boardSelectionEnd,
                                     static_cast<uint32_t>(state.boardText.size()));
  state.boardSelectionAnchor = std::min(state.boardSelectionAnchor,
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
      spec.text = state.searchText;
      spec.placeholder = "Search...";
      spec.size = {};
      if (!spec.size.preferredHeight.has_value() && spec.size.stretchY <= 0.0f) {
        spec.size.preferredHeight = StudioDefaults::ControlHeight;
      }
      if (!spec.size.minWidth.has_value() && spec.size.stretchX <= 0.0f) {
        spec.size.minWidth = StudioDefaults::FieldWidthL;
      }
      spec.backgroundStyle = rectToken(RectRole::Panel);
      spec.textStyle = textToken(TextRole::BodyBright);
      spec.placeholderStyle = textToken(TextRole::BodyMuted);
      spec.showCursor = state.searchFocused && state.searchCursorVisible;
      spec.cursorIndex = state.searchCursor;
      spec.cursorStyle = rectToken(RectRole::Accent);
      spec.cursorWidth = 2.0f;
      spec.selectionStart = state.searchSelectionStart;
      spec.selectionEnd = state.searchSelectionEnd;
      spec.selectionStyle = rectToken(RectRole::Selection);
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
    treeSpec.base.rowWidthInset = 28.0f;
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
    treeSpec.rowRole = RectRole::PanelAlt;
    treeSpec.rowAltRole = RectRole::Panel;
    treeSpec.caretBackgroundRole = RectRole::PanelStrong;
    treeSpec.caretLineRole = RectRole::Accent;
    treeSpec.connectorRole = RectRole::Accent;
    treeSpec.textRole = TextRole::SmallBright;
    treeSpec.selectedTextRole = TextRole::SmallBright;
    float treePanelHeight = contentH - StudioDefaults::HeaderHeight * 2.0f -
                            StudioDefaults::PanelInset * 4.0f;
    treeSpec.base.size.preferredWidth = sidebarW - StudioDefaults::PanelInset * 2.0f;
    treeSpec.base.size.preferredHeight = std::max(0.0f, treePanelHeight);

    treeSpec.base.nodes = state.treeNodes;
    UiNode treeView = createTreeView(treePanel, treeSpec);
    ui.treeViewNode = treeView.nodeId();
    ui.treeViewSpec = treeSpec.base;
    ui.treeRows.clear();
    flattenTree(state.treeNodes, 0, ui.treeRows);
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

    SizeSpec paragraphSize;
    paragraphSize.preferredWidth = boardTextWidth;
    PrimeFrame::TextStyleToken boardTextStyle = textToken(TextRole::SmallMuted);
    ui.boardLineHeight = resolveLineHeight(ui.frame, boardTextStyle);
    ui.boardTextLines = PrimeStage::wrapTextLineRanges(ui.frame,
                                                       boardTextStyle,
                                                       state.boardText,
                                                       boardTextWidth,
                                                       PrimeFrame::WrapMode::Word);
    float paragraphHeight = ui.boardLineHeight *
                            static_cast<float>(std::max<size_t>(1u, ui.boardTextLines.size()));
    StackSpec paragraphOverlaySpec;
    paragraphOverlaySpec.size.preferredWidth = boardTextWidth;
    paragraphOverlaySpec.size.preferredHeight = paragraphHeight;
    paragraphOverlaySpec.clipChildren = true;
    UiNode paragraphOverlay = boardPanel.createOverlay(paragraphOverlaySpec);
    ui.boardTextNode = paragraphOverlay.nodeId();

    StackSpec selectionColumnSpec;
    selectionColumnSpec.size.preferredWidth = boardTextWidth;
    selectionColumnSpec.size.preferredHeight = paragraphHeight;
    selectionColumnSpec.gap = 0.0f;
    UiNode selectionColumn = paragraphOverlay.createVerticalStack(selectionColumnSpec);
    uint32_t selectionStart = 0u;
    uint32_t selectionEnd = 0u;
    bool hasSelection = hasBoardSelection(state, selectionStart, selectionEnd);
    for (size_t lineIndex = 0; lineIndex < ui.boardTextLines.size(); ++lineIndex) {
      const auto& line = ui.boardTextLines[lineIndex];
      StackSpec lineSpec;
      lineSpec.size.preferredWidth = boardTextWidth;
      lineSpec.size.preferredHeight = ui.boardLineHeight;
      lineSpec.gap = 0.0f;
      UiNode lineRow = selectionColumn.createHorizontalStack(lineSpec);
      float leftWidth = 0.0f;
      float selectWidth = 0.0f;
      if (hasSelection && selectionEnd > line.start && selectionStart < line.end) {
        uint32_t localStart = std::max(selectionStart, line.start) - line.start;
        uint32_t localEnd = std::min(selectionEnd, line.end) - line.start;
        std::string_view lineText(state.boardText.data() + line.start,
                                  line.end - line.start);
        leftWidth = PrimeStage::measureTextWidth(ui.frame, boardTextStyle, lineText.substr(0, localStart));
        float rightWidth = PrimeStage::measureTextWidth(ui.frame, boardTextStyle, lineText.substr(0, localEnd));
        selectWidth = std::max(0.0f, rightWidth - leftWidth);
      }
      if (leftWidth > 0.0f) {
        SizeSpec leftSize;
        leftSize.preferredWidth = leftWidth;
        leftSize.preferredHeight = ui.boardLineHeight;
        lineRow.createSpacer(leftSize);
      }
      if (selectWidth > 0.0f) {
        SizeSpec selectSize;
        selectSize.preferredWidth = selectWidth;
        selectSize.preferredHeight = ui.boardLineHeight;
        lineRow.createPanel(rectToken(RectRole::Selection), selectSize);
      }
      SizeSpec fillSize;
      fillSize.stretchX = 1.0f;
      fillSize.preferredHeight = ui.boardLineHeight;
      lineRow.createSpacer(fillSize);
    }

    paragraphSize.preferredHeight = paragraphHeight;
    ParagraphSpec paragraphSpec;
    paragraphSpec.text = state.boardText;
    paragraphSpec.textStyle = boardTextStyle;
    paragraphSpec.maxWidth = boardTextWidth;
    paragraphSpec.size = paragraphSize;
    paragraphOverlay.createParagraph(paragraphSpec);

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
  Callbacks callbacks{};
  callbacks.onFrame = [&](SurfaceId id, const FrameTiming&, const FrameDiagnostics&) {
    auto now = std::chrono::steady_clock::now();
    bool blinkChanged = false;
    if (state.searchFocused) {
      if (state.nextSearchBlink.time_since_epoch().count() == 0) {
        state.searchCursorVisible = true;
        state.nextSearchBlink = now + SearchBlinkInterval;
        blinkChanged = true;
      } else if (now >= state.nextSearchBlink) {
        state.searchCursorVisible = !state.searchCursorVisible;
        state.nextSearchBlink = now + SearchBlinkInterval;
        blinkChanged = true;
      }
    } else if (state.searchCursorVisible || state.nextSearchBlink.time_since_epoch().count() != 0) {
      state.searchCursorVisible = false;
      state.nextSearchBlink = {};
      blinkChanged = true;
    }
    if (blinkChanged) {
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
      ui.router.clearAllCaptures();
      buildStudioUi(ui, state);
      ui.needsRebuild = false;
      ui.layoutDirty = true;
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
    auto now = std::chrono::steady_clock::now();
    auto markSearchDirty = [&]() {
      ui.needsRebuild = true;
      ui.layoutDirty = true;
      ui.renderDirty = true;
      markRenderDirty();
    };
    auto markBoardDirty = [&]() {
      ui.needsRebuild = true;
      ui.layoutDirty = true;
      ui.renderDirty = true;
      markRenderDirty();
    };
    auto resetSearchBlink = [&]() {
      state.searchCursorVisible = true;
      state.nextSearchBlink = now + SearchBlinkInterval;
    };
    auto setSearchFocus = [&](bool focused, std::optional<uint32_t> cursorOverride) {
      bool focusChanged = state.searchFocused != focused;
      if (!focusChanged && !cursorOverride.has_value()) {
        return;
      }
      state.searchFocused = focused;
      if (focused) {
        if (cursorOverride.has_value()) {
          state.searchCursor = std::min(cursorOverride.value(),
                                        static_cast<uint32_t>(state.searchText.size()));
        } else if (focusChanged) {
          state.searchCursor = static_cast<uint32_t>(state.searchText.size());
        }
        clearSearchSelection(state, state.searchCursor);
        resetSearchBlink();
      } else {
        state.searchCursorVisible = false;
        state.nextSearchBlink = {};
        state.searchSelecting = false;
        state.searchPointerId = -1;
      }
      markSearchDirty();
      if (focusChanged && !perfEnabled) {
        setFramePolicy(focused ? FramePolicy::Continuous : baseFramePolicy);
      }
    };
    auto setBoardFocus = [&](bool focused, std::optional<uint32_t> cursorOverride) {
      bool focusChanged = state.boardFocused != focused;
      if (!focusChanged && !cursorOverride.has_value()) {
        return;
      }
      state.boardFocused = focused;
      if (focused) {
        uint32_t cursor = cursorOverride.value_or(state.boardSelectionAnchor);
        cursor = std::min(cursor, static_cast<uint32_t>(state.boardText.size()));
        clearBoardSelection(state, cursor);
      } else {
        state.boardSelecting = false;
        state.boardPointerId = -1;
      }
      markBoardDirty();
    };
    for (const auto& event : batch.events) {
      if (auto* input = std::get_if<InputEvent>(&event.payload)) {
        if (auto* pointer = std::get_if<PointerEvent>(input)) {
          if (!suppressEventLogs && pointer->phase != PointerPhase::Move) {
            std::cout << "pointer " << pointerPhaseLabel(pointer->phase)
                      << " type=" << pointerTypeLabel(pointer->deviceType)
                      << " x=" << pointer->x << " y=" << pointer->y << "\n";
          }
          if (ui.needsRebuild) {
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
          PrimeFrame::Event pfEvent = toPrimeFrameEvent(*pointer);
          bool handled = ui.router.dispatch(pfEvent, ui.frame, ui.layout, nullptr);
          if (handled) {
            ui.renderDirty = true;
            markRenderDirty();
          }

          bool hitSearch = false;
          bool hitBoard = false;
          if (pointer->phase == PointerPhase::Move ||
              pointer->phase == PointerPhase::Down ||
              pointer->phase == PointerPhase::Up) {
            float px = static_cast<float>(pointer->x);
            float py = static_cast<float>(pointer->y);
            hitSearch = ui.searchFieldNode.isValid() && pointInNode(ui.layout, ui.searchFieldNode, px, py);
            hitBoard = ui.boardTextNode.isValid() && pointInNode(ui.layout, ui.boardTextNode, px, py);
            if (hitSearch != state.searchHover || hitBoard != state.boardHover) {
              state.searchHover = hitSearch;
              state.boardHover = hitBoard;
              bool useIBeam = hitSearch || hitBoard || cursorIBeam;
              hostPtr->setCursorShape(surfaceId, useIBeam ? CursorShape::IBeam : CursorShape::Arrow);
            }
          }
          if (pointer->phase == PointerPhase::Down) {
            float px = static_cast<float>(pointer->x);
            float py = static_cast<float>(pointer->y);
            if (hitSearch) {
              const PrimeFrame::LayoutOut* searchOut = ui.layout.get(ui.searchFieldNode);
              float localX = searchOut ? (px - searchOut->absX) : 0.0f;
              uint32_t cursorIndex = PrimeStage::caretIndexForClick(ui.frame,
                                                                    textToken(TextRole::BodyBright),
                                                                    state.searchText,
                                                                    16.0f,
                                                                    localX);
              setSearchFocus(true, cursorIndex);
              state.searchSelecting = true;
              state.searchPointerId = static_cast<int>(pointer->pointerId);
              state.searchSelectionAnchor = cursorIndex;
              state.searchSelectionStart = cursorIndex;
              state.searchSelectionEnd = cursorIndex;
              state.searchCursor = cursorIndex;
              markSearchDirty();
              setBoardFocus(false, std::nullopt);
            } else if (hitBoard) {
              setSearchFocus(false, std::nullopt);
              const PrimeFrame::LayoutOut* boardOut = ui.layout.get(ui.boardTextNode);
              float localX = boardOut ? (px - boardOut->absX) : 0.0f;
              float localY = boardOut ? (py - boardOut->absY) : 0.0f;
              uint32_t cursorIndex = 0u;
              if (!ui.boardTextLines.empty() && ui.boardLineHeight > 0.0f) {
                int lineIndex = static_cast<int>(localY / ui.boardLineHeight);
                lineIndex = std::clamp(lineIndex, 0, static_cast<int>(ui.boardTextLines.size() - 1u));
                const auto& line = ui.boardTextLines[static_cast<size_t>(lineIndex)];
                std::string_view lineText(state.boardText.data() + line.start,
                                          line.end - line.start);
                cursorIndex = line.start + PrimeStage::caretIndexForClick(ui.frame,
                                                                          textToken(TextRole::SmallMuted),
                                                                          lineText,
                                                                          0.0f,
                                                                          localX);
              }
              setBoardFocus(true, cursorIndex);
              state.boardSelecting = true;
              state.boardPointerId = static_cast<int>(pointer->pointerId);
              state.boardSelectionAnchor = cursorIndex;
              state.boardSelectionStart = cursorIndex;
              state.boardSelectionEnd = cursorIndex;
              markBoardDirty();
            } else if (state.searchFocused) {
              setSearchFocus(false, std::nullopt);
              setBoardFocus(false, std::nullopt);
            } else if (state.boardFocused) {
              setBoardFocus(false, std::nullopt);
            }
            if (handleTreeClick(ui, state, px, py)) {
              ui.needsRebuild = true;
              ui.layoutDirty = true;
              ui.renderDirty = true;
              markRenderDirty();
            }
          } else if (pointer->phase == PointerPhase::Move) {
            if (state.searchSelecting &&
                state.searchPointerId == static_cast<int>(pointer->pointerId) &&
                (pointer->buttonMask & 0x1u) != 0u) {
              const PrimeFrame::LayoutOut* searchOut = ui.layout.get(ui.searchFieldNode);
              float px = static_cast<float>(pointer->x);
              float localX = searchOut ? (px - searchOut->absX) : 0.0f;
              uint32_t cursorIndex = PrimeStage::caretIndexForClick(ui.frame,
                                                                    textToken(TextRole::BodyBright),
                                                                    state.searchText,
                                                                    16.0f,
                                                                    localX);
              state.searchCursor = cursorIndex;
              state.searchSelectionStart = state.searchSelectionAnchor;
              state.searchSelectionEnd = cursorIndex;
              resetSearchBlink();
              markSearchDirty();
            }
            if (state.boardSelecting &&
                state.boardPointerId == static_cast<int>(pointer->pointerId) &&
                (pointer->buttonMask & 0x1u) != 0u) {
              const PrimeFrame::LayoutOut* boardOut = ui.layout.get(ui.boardTextNode);
              float px = static_cast<float>(pointer->x);
              float localX = boardOut ? (px - boardOut->absX) : 0.0f;
              float localY = boardOut ? (static_cast<float>(pointer->y) - boardOut->absY) : 0.0f;
              uint32_t cursorIndex = 0u;
              if (!ui.boardTextLines.empty() && ui.boardLineHeight > 0.0f) {
                int lineIndex = static_cast<int>(localY / ui.boardLineHeight);
                lineIndex = std::clamp(lineIndex, 0, static_cast<int>(ui.boardTextLines.size() - 1u));
                const auto& line = ui.boardTextLines[static_cast<size_t>(lineIndex)];
                std::string_view lineText(state.boardText.data() + line.start,
                                          line.end - line.start);
                cursorIndex = line.start + PrimeStage::caretIndexForClick(ui.frame,
                                                                          textToken(TextRole::SmallMuted),
                                                                          lineText,
                                                                          0.0f,
                                                                          localX);
              }
              state.boardSelectionStart = state.boardSelectionAnchor;
              state.boardSelectionEnd = cursorIndex;
              markBoardDirty();
            }
          } else if (pointer->phase == PointerPhase::Up || pointer->phase == PointerPhase::Cancel) {
            if (state.searchPointerId == static_cast<int>(pointer->pointerId)) {
              state.searchSelecting = false;
              state.searchPointerId = -1;
            }
            if (state.boardPointerId == static_cast<int>(pointer->pointerId)) {
              state.boardSelecting = false;
              state.boardPointerId = -1;
            }
          }
        } else if (auto* key = std::get_if<KeyEvent>(input)) {
          if (key->pressed) {
            bool altPressed = (key->modifiers &
                               static_cast<KeyModifierMask>(KeyModifier::Alt)) != 0u;
            bool superPressed = (key->modifiers &
                                 static_cast<KeyModifierMask>(KeyModifier::Super)) != 0u;
            if ((altPressed || superPressed) && key->keyCode == KeyQ && !key->repeat) {
              running = false;
              continue;
            }
            if (state.searchFocused) {
              bool hasSelection = false;
              uint32_t selectionStart = 0u;
              uint32_t selectionEnd = 0u;
              hasSelection = hasSearchSelection(state, selectionStart, selectionEnd);
              auto deleteSelection = [&]() -> bool {
                if (!hasSelection) {
                  return false;
                }
                state.searchText.erase(selectionStart, selectionEnd - selectionStart);
                state.searchCursor = selectionStart;
                clearSearchSelection(state, state.searchCursor);
                return true;
              };
              bool isShortcut = (key->modifiers &
                                 static_cast<KeyModifierMask>(KeyModifier::Super)) != 0u ||
                                (key->modifiers &
                                 static_cast<KeyModifierMask>(KeyModifier::Control)) != 0u;
              bool shiftPressed = (key->modifiers &
                                   static_cast<KeyModifierMask>(KeyModifier::Shift)) != 0u;
              if (isShortcut && !key->repeat) {
                if (key->keyCode == KeyA) {
                  state.searchSelectionStart = 0u;
                  state.searchSelectionEnd = static_cast<uint32_t>(state.searchText.size());
                  state.searchCursor = state.searchSelectionEnd;
                  resetSearchBlink();
                  markSearchDirty();
                  continue;
                }
                if (key->keyCode == KeyC) {
                  if (hasSelection) {
                    hostPtr->setClipboardText(
                        std::string_view(state.searchText.data() + selectionStart,
                                         selectionEnd - selectionStart));
                  }
                  continue;
                }
                if (key->keyCode == KeyX) {
                  if (hasSelection) {
                    hostPtr->setClipboardText(
                        std::string_view(state.searchText.data() + selectionStart,
                                         selectionEnd - selectionStart));
                    deleteSelection();
                    resetSearchBlink();
                    markSearchDirty();
                  }
                  continue;
                }
                if (key->keyCode == KeyV) {
                  auto size = hostPtr->clipboardTextSize();
                  if (size && size.value() > 0u) {
                    std::vector<char> bufferText(size.value());
                    auto text = hostPtr->clipboardText(bufferText);
                    if (text) {
                      std::string paste(text.value());
                      if (!paste.empty()) {
                        deleteSelection();
                        uint32_t cursor = std::min(state.searchCursor,
                                                   static_cast<uint32_t>(state.searchText.size()));
                        state.searchText.insert(cursor, paste);
                        state.searchCursor = cursor + static_cast<uint32_t>(paste.size());
                        clearSearchSelection(state, state.searchCursor);
                        resetSearchBlink();
                        markSearchDirty();
                      }
                    }
                  }
                  continue;
                }
              }
              bool changed = false;
              bool keepSelection = false;
              uint32_t cursor = std::min(state.searchCursor, static_cast<uint32_t>(state.searchText.size()));
              switch (key->keyCode) {
                case KeyEscape:
                  setSearchFocus(false, std::nullopt);
                  continue;
                case KeyLeft:
                  if (shiftPressed) {
                    if (!hasSelection) {
                      state.searchSelectionAnchor = cursor;
                    }
                  cursor = PrimeStage::utf8Prev(state.searchText, cursor);
                    state.searchSelectionStart = state.searchSelectionAnchor;
                    state.searchSelectionEnd = cursor;
                    keepSelection = true;
                  } else {
                  cursor = PrimeStage::utf8Prev(state.searchText, cursor);
                    clearSearchSelection(state, cursor);
                  }
                  break;
                case KeyRight:
                  if (shiftPressed) {
                    if (!hasSelection) {
                      state.searchSelectionAnchor = cursor;
                    }
                  cursor = PrimeStage::utf8Next(state.searchText, cursor);
                    state.searchSelectionStart = state.searchSelectionAnchor;
                    state.searchSelectionEnd = cursor;
                    keepSelection = true;
                  } else {
                  cursor = PrimeStage::utf8Next(state.searchText, cursor);
                    clearSearchSelection(state, cursor);
                  }
                  break;
                case KeyHome:
                  if (shiftPressed) {
                    if (!hasSelection) {
                      state.searchSelectionAnchor = cursor;
                    }
                    cursor = 0u;
                    state.searchSelectionStart = state.searchSelectionAnchor;
                    state.searchSelectionEnd = cursor;
                    keepSelection = true;
                  } else {
                    cursor = 0u;
                    clearSearchSelection(state, cursor);
                  }
                  break;
                case KeyEnd:
                  if (shiftPressed) {
                    if (!hasSelection) {
                      state.searchSelectionAnchor = cursor;
                    }
                    cursor = static_cast<uint32_t>(state.searchText.size());
                    state.searchSelectionStart = state.searchSelectionAnchor;
                    state.searchSelectionEnd = cursor;
                    keepSelection = true;
                  } else {
                    cursor = static_cast<uint32_t>(state.searchText.size());
                    clearSearchSelection(state, cursor);
                  }
                  break;
                case KeyBackspace:
                  if (deleteSelection()) {
                    changed = true;
                    cursor = state.searchCursor;
                  } else if (cursor > 0u) {
                    uint32_t start = PrimeStage::utf8Prev(state.searchText, cursor);
                    state.searchText.erase(start, cursor - start);
                    cursor = start;
                    changed = true;
                  }
                  break;
                case KeyDelete:
                  if (deleteSelection()) {
                    changed = true;
                    cursor = state.searchCursor;
                  } else if (cursor < static_cast<uint32_t>(state.searchText.size())) {
                    uint32_t end = PrimeStage::utf8Next(state.searchText, cursor);
                    state.searchText.erase(cursor, end - cursor);
                    changed = true;
                  }
                  break;
                case KeyTab:
                  setSearchFocus(false, std::nullopt);
                  continue;
                case KeyReturn:
                  continue;
                default:
                  continue;
              }
              if (cursor != state.searchCursor || changed) {
                state.searchCursor = cursor;
                if (!keepSelection) {
                  clearSearchSelection(state, cursor);
                }
                resetSearchBlink();
                markSearchDirty();
              }
              continue;
            }
            if (state.boardFocused) {
              bool isShortcut = (key->modifiers &
                                 static_cast<KeyModifierMask>(KeyModifier::Super)) != 0u ||
                                (key->modifiers &
                                 static_cast<KeyModifierMask>(KeyModifier::Control)) != 0u;
              if (isShortcut && !key->repeat) {
                if (key->keyCode == KeyA) {
                  state.boardSelectionStart = 0u;
                  state.boardSelectionEnd = static_cast<uint32_t>(state.boardText.size());
                  markBoardDirty();
                  continue;
                }
                if (key->keyCode == KeyC) {
                  uint32_t selectionStart = 0u;
                  uint32_t selectionEnd = 0u;
                  if (hasBoardSelection(state, selectionStart, selectionEnd)) {
                    hostPtr->setClipboardText(
                        std::string_view(state.boardText.data() + selectionStart,
                                         selectionEnd - selectionStart));
                  }
                  continue;
                }
              }
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
                bool useIBeam = cursorIBeam || state.searchHover || state.boardHover;
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
          if (state.searchFocused && view) {
            std::string filtered;
            filtered.reserve(view->size());
            for (char ch : *view) {
              if (ch != '\n' && ch != '\r') {
                filtered.push_back(ch);
              }
            }
            if (!filtered.empty()) {
              uint32_t selectionStart = 0u;
              uint32_t selectionEnd = 0u;
              bool hasSelection = hasSearchSelection(state, selectionStart, selectionEnd);
              if (hasSelection) {
                state.searchText.erase(selectionStart, selectionEnd - selectionStart);
                state.searchCursor = selectionStart;
                clearSearchSelection(state, state.searchCursor);
              }
              uint32_t cursor = std::min(state.searchCursor, static_cast<uint32_t>(state.searchText.size()));
              state.searchText.insert(cursor, filtered);
              state.searchCursor = cursor + static_cast<uint32_t>(filtered.size());
              clearSearchSelection(state, state.searchCursor);
              resetSearchBlink();
              markSearchDirty();
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
