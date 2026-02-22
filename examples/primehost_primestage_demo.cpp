#include "PrimeHost/PrimeHost.h"
#include "PrimeStage/Render.h"
#include "PrimeStage/StudioUi.h"

#include "PrimeFrame/Events.h"
#include "PrimeFrame/Layout.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cmath>
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
constexpr uint32_t KeyF = 0x09u;
constexpr uint32_t KeyM = 0x10u;
constexpr uint32_t KeyX = 0x1Bu;
constexpr uint32_t KeyR = 0x15u;
constexpr uint32_t KeyC = 0x06u;
constexpr uint32_t KeyV = 0x19u;
constexpr uint32_t KeyO = 0x12u;
constexpr uint32_t KeyP = 0x13u;
constexpr uint32_t KeyI = 0x0Cu;

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

struct ButtonBinding {
  PrimeFrame::NodeId node{};
  PrimeFrame::PrimitiveId background = PrimeFrame::InvalidIndex;
  PrimeFrame::RectStyleToken baseStyle = 0;
  PrimeFrame::RectStyleToken hoverStyle = 0;
  PrimeFrame::RectStyleToken activeStyle = 0;
  bool pressed = false;
  bool hovered = false;
  bool* activeFlag = nullptr;
};

struct DemoUi {
  PrimeFrame::Frame frame;
  PrimeFrame::LayoutEngine layoutEngine;
  PrimeFrame::LayoutOutput layout;
  PrimeFrame::EventRouter router;
  std::vector<ButtonBinding> buttons;
  float logicalWidth = 0.0f;
  float logicalHeight = 0.0f;
  float scale = 1.0f;
  bool needsRebuild = true;
  bool layoutDirty = true;
  bool styleDirty = true;
};

struct DemoState {
  bool shareActive = false;
  bool runActive = false;
  bool publishActive = false;
};

bool pointInNode(const PrimeFrame::LayoutOutput& layout, PrimeFrame::NodeId node, float x, float y) {
  const PrimeFrame::LayoutOut* out = layout.get(node);
  if (!out || !out->visible) {
    return false;
  }
  return x >= out->absX && y >= out->absY && x <= (out->absX + out->absW) && y <= (out->absY + out->absH);
}

void applyButtonStyles(PrimeFrame::Frame& frame, std::vector<ButtonBinding>& buttons) {
  for (auto& button : buttons) {
    auto* prim = frame.getPrimitive(button.background);
    if (!prim || prim->type != PrimeFrame::PrimitiveType::Rect) {
      continue;
    }
    PrimeFrame::RectStyleToken token = button.baseStyle;
    if (button.pressed) {
      token = button.activeStyle;
    } else if (button.activeFlag && *button.activeFlag) {
      token = button.activeStyle;
    } else if (button.hovered) {
      token = button.hoverStyle;
    }
    prim->rect.token = token;
  }
}

void attachButtonCallback(PrimeFrame::Frame& frame, ButtonBinding& button, bool& styleDirty) {
  PrimeFrame::Callback cb;
  cb.onEvent = [&button, &styleDirty](PrimeFrame::Event const& event) {
    switch (event.type) {
      case PrimeFrame::EventType::PointerDown:
        button.pressed = true;
        styleDirty = true;
        return true;
      case PrimeFrame::EventType::PointerUp:
        if (button.pressed) {
          button.pressed = false;
          if (button.activeFlag) {
            *button.activeFlag = !*button.activeFlag;
          }
          styleDirty = true;
        }
        return true;
      case PrimeFrame::EventType::PointerCancel:
        if (button.pressed) {
          button.pressed = false;
          styleDirty = true;
        }
        return true;
      default:
        return false;
    }
  };
  PrimeFrame::CallbackId cbId = frame.addCallback(cb);
  if (PrimeFrame::Node* node = frame.getNode(button.node)) {
    node->callbacks = cbId;
  }
}

void registerButton(DemoUi& ui,
                    UiNode const& node,
                    PrimeFrame::RectStyleToken baseStyle,
                    PrimeFrame::RectStyleToken hoverStyle,
                    PrimeFrame::RectStyleToken activeStyle,
                    bool* activeFlag) {
  PrimeFrame::Node* frameNode = ui.frame.getNode(node.nodeId());
  if (!frameNode || frameNode->primitives.empty()) {
    return;
  }

  ButtonBinding binding;
  binding.node = node.nodeId();
  binding.background = frameNode->primitives.front();
  binding.baseStyle = baseStyle;
  binding.hoverStyle = hoverStyle;
  binding.activeStyle = activeStyle;
  binding.activeFlag = activeFlag;
  ui.buttons.push_back(binding);
}

void buildStudioUi(DemoUi& ui, DemoState& state) {
  ui.frame = PrimeFrame::Frame{};
  ui.buttons.clear();
  ui.buttons.reserve(4);

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
    createTextField(row, "Search...", {});

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

    registerButton(ui,
                   share,
                   rectToken(RectRole::Panel),
                   rectToken(RectRole::PanelStrong),
                   rectToken(RectRole::Accent),
                   &state.shareActive);
    registerButton(ui,
                   run,
                   rectToken(RectRole::Accent),
                   rectToken(RectRole::Selection),
                   rectToken(RectRole::Selection),
                   &state.runActive);
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

    Studio::TreeNode treeRoot{
        "Root",
        {
            Studio::TreeNode{"World",
                     {Studio::TreeNode{"Camera"}, Studio::TreeNode{"Lights"}, Studio::TreeNode{"Environment"}},
                     true,
                     false},
            Studio::TreeNode{"UI",
                     {
                         Studio::TreeNode{"Sidebar"},
                         Studio::TreeNode{"Toolbar", {Studio::TreeNode{"Buttons"}}, false, false},
                         Studio::TreeNode{"Panels",
                                  {Studio::TreeNode{"TreeView", {}, true, true}, Studio::TreeNode{"Rows"}},
                                  true,
                                  false}
                     },
                     true,
                     false}
        },
        true,
        false};

    treeSpec.base.nodes = {treeRoot};
    createTreeView(treePanel, treeSpec);
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
    createParagraph(boardPanel,
                    "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"
                    "Sed do eiusmod tempor incididunt ut labore et dolore.\n"
                    "Ut enim ad minim veniam, quis nostrud exercitation.",
                    TextRole::SmallMuted,
                    paragraphSize);

    StackSpec buttonRowSpec;
    buttonRowSpec.size.preferredWidth = boardTextWidth;
    buttonRowSpec.size.preferredHeight = StudioDefaults::ControlHeight;
    UiNode boardButtons = boardPanel.createHorizontalStack(buttonRowSpec);
    SizeSpec buttonSpacer;
    buttonSpacer.stretchX = 1.0f;
    boardButtons.createSpacer(buttonSpacer);
    createButton(boardButtons, "Primary Action", ButtonVariant::Primary, {});

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
    createProgressBar(opacityOverlay, opacityBarSize, 0.85f);

    PropertyListSpec opacitySpec;
    opacitySpec.size.preferredWidth = transformContentW;
    opacitySpec.size.preferredHeight = StudioDefaults::OpacityBarHeight;
    opacitySpec.rowHeight = StudioDefaults::OpacityBarHeight;
    opacitySpec.rowGap = 0.0f;
    opacitySpec.labelRole = TextRole::SmallBright;
    opacitySpec.valueRole = TextRole::SmallBright;
    opacitySpec.rows = {{"Opacity", "85%"}};
    createPropertyList(opacityOverlay, opacitySpec);

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

    registerButton(ui,
                   publish,
                   rectToken(RectRole::Accent),
                   rectToken(RectRole::Selection),
                   rectToken(RectRole::Selection),
                   &state.publishActive);
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

    SizeSpec barSpacer;
    barSpacer.stretchX = 1.0f;
    bar.createSpacer(barSpacer);

    createTextLine(bar, "PrimeFrame Demo", TextRole::SmallMuted, lineSize);
  };

  createTopbar();
  createSidebar();
  createContent();
  createInspector();
  createStatus();

  for (auto& button : ui.buttons) {
    attachButtonCallback(ui.frame, button, ui.styleDirty);
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

int main() {
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
  config.width = 1280u;
  config.height = 720u;
  config.resizable = true;
  config.title = std::string("PrimeHost Studio Demo");

  auto surfaceResult = host->createSurface(config);
  if (!surfaceResult) {
    std::cerr << "Failed to create surface (" << static_cast<int>(surfaceResult.error().code) << ")\n";
    return 1;
  }
  SurfaceId surfaceId = surfaceResult.value();

  FrameConfig frameConfig{};
  frameConfig.framePolicy = FramePolicy::Continuous;
  frameConfig.framePacingSource = FramePacingSource::Platform;
  host->setFrameConfig(surfaceId, frameConfig);

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

  std::array<PrimeHost::Event, 256> events{};
  std::array<char, 8192> textBytes{};
  EventBuffer buffer{
      std::span<PrimeHost::Event>(events.data(), events.size()),
      std::span<char>(textBytes.data(), textBytes.size()),
  };

  DemoState state{};
  DemoUi ui{};
  ui.logicalWidth = static_cast<float>(config.width);
  ui.logicalHeight = static_cast<float>(config.height);
  ui.scale = 1.0f;

  std::mutex uiMutex;
  FpsTracker fps{};
  bool running = true;
  bool fullscreen = false;
  bool minimized = false;
  bool maximized = false;
  bool relativePointer = false;
  bool cursorIBeam = false;

  Callbacks callbacks{};
  callbacks.onFrame = [&](SurfaceId id, const FrameTiming&, const FrameDiagnostics&) {
    std::lock_guard<std::mutex> lock(uiMutex);
    auto frameResult = host->acquireFrameBuffer(id);
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
    }

    if (ui.needsRebuild) {
      ui.router.clearAllCaptures();
      buildStudioUi(ui, state);
      ui.needsRebuild = false;
      ui.layoutDirty = true;
      ui.styleDirty = true;
    }

    if (ui.layoutDirty) {
      PrimeFrame::LayoutOptions options;
      options.rootWidth = ui.logicalWidth;
      options.rootHeight = ui.logicalHeight;
      ui.layoutEngine.layout(ui.frame, ui.layout, options);
      ui.layoutDirty = false;
    }

    if (ui.styleDirty) {
      applyButtonStyles(ui.frame, ui.buttons);
      ui.styleDirty = false;
    }

    PrimeStage::RenderTarget target;
    target.pixels = fb.pixels;
    target.width = fb.size.width;
    target.height = fb.size.height;
    target.stride = fb.stride;
    target.scale = scale;
    if (renderFrameToTarget(ui.frame, ui.layout, target)) {
      host->presentFrameBuffer(id, fb);
      fps.framePresented();
      if (fps.shouldReport() && fps.sampleCount() > 0u) {
        auto stats = fps.stats();
        std::cout << "fps " << stats.fps
                  << " p95=" << std::chrono::duration<double, std::milli>(stats.p95FrameTime).count()
                  << "ms p99=" << std::chrono::duration<double, std::milli>(stats.p99FrameTime).count()
                  << "ms\n";
      }
    }
  };
  host->setCallbacks(callbacks);

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

    std::lock_guard<std::mutex> lock(uiMutex);

    for (const auto& event : batch.events) {
      if (auto* input = std::get_if<InputEvent>(&event.payload)) {
        if (auto* pointer = std::get_if<PointerEvent>(input)) {
          if (pointer->phase != PointerPhase::Move) {
            std::cout << "pointer " << pointerPhaseLabel(pointer->phase)
                      << " type=" << pointerTypeLabel(pointer->deviceType)
                      << " x=" << pointer->x << " y=" << pointer->y << "\n";
          }
          if (ui.needsRebuild) {
            buildStudioUi(ui, state);
            ui.needsRebuild = false;
            ui.layoutDirty = true;
          }
          if (ui.layoutDirty) {
            PrimeFrame::LayoutOptions options;
            options.rootWidth = ui.logicalWidth;
            options.rootHeight = ui.logicalHeight;
            ui.layoutEngine.layout(ui.frame, ui.layout, options);
            ui.layoutDirty = false;
          }
          PrimeFrame::Event pfEvent = toPrimeFrameEvent(*pointer);
          ui.router.dispatch(pfEvent, ui.frame, ui.layout, nullptr);

          if (pointer->deviceType == PointerDeviceType::Mouse && pointer->phase == PointerPhase::Move) {
            bool changed = false;
            float px = static_cast<float>(pointer->x);
            float py = static_cast<float>(pointer->y);
            for (auto& button : ui.buttons) {
              bool inside = pointInNode(ui.layout, button.node, px, py);
              if (button.hovered != inside) {
                button.hovered = inside;
                changed = true;
              }
            }
            if (changed) {
              ui.styleDirty = true;
            }
          }
          if (ui.styleDirty) {
            applyButtonStyles(ui.frame, ui.buttons);
            ui.styleDirty = false;
          }
        } else if (auto* key = std::get_if<KeyEvent>(input)) {
          if (key->pressed && !key->repeat) {
            if (key->keyCode == KeyEscape) {
              running = false;
            }
            if (isToggleKey(key->keyCode)) {
              ui.styleDirty = true;
            }
            if (key->keyCode == KeyF) {
              fullscreen = !fullscreen;
              host->setSurfaceFullscreen(surfaceId, fullscreen);
            } else if (key->keyCode == KeyM) {
              minimized = !minimized;
              host->setSurfaceMinimized(surfaceId, minimized);
            } else if (key->keyCode == KeyX) {
              maximized = !maximized;
              host->setSurfaceMaximized(surfaceId, maximized);
            } else if (key->keyCode == KeyR) {
              relativePointer = !relativePointer;
              host->setRelativePointerCapture(surfaceId, relativePointer);
            } else if (key->keyCode == KeyI) {
              cursorIBeam = !cursorIBeam;
              host->setCursorShape(surfaceId, cursorIBeam ? CursorShape::IBeam : CursorShape::Arrow);
            } else if (key->keyCode == KeyC) {
              host->setClipboardText("PrimeHost clipboard sample");
            } else if (key->keyCode == KeyV) {
              auto size = host->clipboardTextSize();
              if (size && size.value() > 0u) {
                std::vector<char> bufferText(size.value());
                auto text = host->clipboardText(bufferText);
                if (text) {
                  std::cout << "clipboard: " << text.value() << "\n";
                }
              }
            } else if (key->keyCode == KeyO) {
              FileDialogConfig dialog{};
              dialog.mode = FileDialogMode::OpenFile;
              std::array<char, 4096> pathBuffer{};
              auto result = host->fileDialog(dialog, pathBuffer);
              if (result && result->accepted) {
                std::cout << "selected: " << result->path << "\n";
              }
            } else if (key->keyCode == KeyP) {
              ScreenshotConfig shot{};
              shot.scope = ScreenshotScope::Surface;
              auto status = host->writeSurfaceScreenshot(surfaceId, "/tmp/primehost-shot.png", shot);
              std::cout << "screenshot: " << (status ? "ok" : "failed") << "\n";
            }
          }
        } else if (auto* text = std::get_if<TextEvent>(input)) {
          auto view = textFromSpan(batch, text->text);
          if (view) {
            std::cout << "text: " << *view << "\n";
          }
        } else if (auto* scroll = std::get_if<ScrollEvent>(input)) {
          std::cout << "scroll dx=" << scroll->deltaX << " dy=" << scroll->deltaY
                    << (scroll->isLines ? " lines" : " px") << "\n";
        } else if (auto* device = std::get_if<DeviceEvent>(input)) {
          std::cout << "device " << deviceTypeLabel(device->deviceType)
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
        ui.logicalWidth = static_cast<float>(resize->width);
        ui.logicalHeight = static_cast<float>(resize->height);
        ui.scale = resize->scale;
        ui.needsRebuild = true;
        ui.layoutDirty = true;
      } else if (auto* drop = std::get_if<DropEvent>(&event.payload)) {
        dumpDropPaths(batch, *drop);
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
  }

  host->destroySurface(surfaceId);
  return 0;
}
