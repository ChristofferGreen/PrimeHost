#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <variant>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.event.payload");

PH_TEST("primehost.event.payload", "event defaults") {
  Event event{};
  PH_CHECK(event.scope == Event::Scope::Surface);
  PH_CHECK(!event.surfaceId.has_value());
  PH_CHECK(std::holds_alternative<InputEvent>(event.payload));
  const auto& input = std::get<InputEvent>(event.payload);
  PH_CHECK(std::holds_alternative<PointerEvent>(input));
  const auto& pointer = std::get<PointerEvent>(input);
  PH_CHECK(pointer.deviceType == PointerDeviceType::Mouse);
  PH_CHECK(pointer.phase == PointerPhase::Move);
}

PH_TEST("primehost.event.payload", "event scope can be global") {
  Event event{};
  event.scope = Event::Scope::Global;
  PH_CHECK(event.scope == Event::Scope::Global);
}

PH_TEST("primehost.event.payload", "event surface id stored") {
  Event event{};
  event.surfaceId = SurfaceId{42u};
  PH_CHECK(event.surfaceId.has_value());
  PH_CHECK(event.surfaceId->value == 42u);
}

PH_TEST("primehost.event.payload", "resize event payload types") {
  Event event{};
  ResizeEvent resize{};
  resize.width = 1920u;
  resize.height = 1080u;
  resize.scale = 2.0f;
  event.payload = resize;

  PH_CHECK(std::holds_alternative<ResizeEvent>(event.payload));
  const auto& payload = std::get<ResizeEvent>(event.payload);
  PH_CHECK(payload.width == 1920u);
  PH_CHECK(payload.height == 1080u);
  PH_CHECK(payload.scale == 2.0f);
}

PH_TEST("primehost.event.payload", "power event payload types") {
  Event event{};
  PowerEvent power{};
  power.lowPowerModeEnabled = true;
  event.payload = power;

  PH_CHECK(std::holds_alternative<PowerEvent>(event.payload));
  const auto& payload = std::get<PowerEvent>(event.payload);
  PH_CHECK(payload.lowPowerModeEnabled.has_value());
  PH_CHECK(payload.lowPowerModeEnabled.value());
}

PH_TEST("primehost.event.payload", "thermal event payload types") {
  Event event{};
  ThermalEvent thermal{};
  thermal.state = ThermalState::Serious;
  event.payload = thermal;

  PH_CHECK(std::holds_alternative<ThermalEvent>(event.payload));
  const auto& payload = std::get<ThermalEvent>(event.payload);
  PH_CHECK(payload.state == ThermalState::Serious);
}

PH_TEST("primehost.event.payload", "lifecycle event payload types") {
  Event event{};
  LifecycleEvent lifecycle{};
  lifecycle.phase = LifecyclePhase::Foregrounded;
  event.payload = lifecycle;

  PH_CHECK(std::holds_alternative<LifecycleEvent>(event.payload));
  const auto& payload = std::get<LifecycleEvent>(event.payload);
  PH_CHECK(payload.phase == LifecyclePhase::Foregrounded);
}

PH_TEST("primehost.event.payload", "event buffer span sizes") {
  std::array<Event, 3> events{};
  std::array<char, 9> textBytes{};
  EventBuffer buffer{
      std::span<Event>(events.data(), events.size()),
      std::span<char>(textBytes.data(), textBytes.size()),
  };

  PH_CHECK(buffer.events.size() == 3u);
  PH_CHECK(buffer.textBytes.size() == 9u);
}

PH_TEST("primehost.event.payload", "event batch span sizes") {
  std::array<Event, 2> events{};
  std::array<char, 4> textBytes{};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  PH_CHECK(batch.events.size() == 2u);
  PH_CHECK(batch.textBytes.size() == 4u);
}

PH_TEST("primehost.event.payload", "event buffer span sizes") {
  std::array<Event, 3> events{};
  std::array<char, 5> textBytes{};
  EventBuffer buffer{
      std::span<Event>(events.data(), events.size()),
      std::span<char>(textBytes.data(), textBytes.size()),
  };

  PH_CHECK(buffer.events.size() == 3u);
  PH_CHECK(buffer.textBytes.size() == 5u);
}

PH_TEST("primehost.event.payload", "event batch preserves data") {
  Event event{};
  event.scope = Event::Scope::Global;
  event.surfaceId = SurfaceId{7u};
  FocusEvent focus{};
  focus.focused = true;
  event.payload = focus;

  std::array<Event, 1> events{event};
  std::array<char, 3> textBytes{'a', 'b', 'c'};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  PH_CHECK(batch.events.size() == 1u);
  PH_CHECK(batch.events[0].scope == Event::Scope::Global);
  PH_CHECK(batch.events[0].surfaceId.has_value());
  PH_CHECK(batch.events[0].surfaceId->value == 7u);
  PH_CHECK(std::holds_alternative<FocusEvent>(batch.events[0].payload));
  PH_CHECK(batch.textBytes.size() == 3u);
  PH_CHECK(batch.textBytes[0] == 'a');
  PH_CHECK(batch.textBytes[1] == 'b');
  PH_CHECK(batch.textBytes[2] == 'c');
}

PH_TEST("primehost.event.payload", "text spans reference batch bytes") {
  std::array<char, 6> textBytes{'h', 'e', 'l', 'l', 'o', '!'}; // "hello!"
  TextSpan span{};
  span.offset = 1u;
  span.length = 3u;

  std::array<TextSpan, 1> spans{span};
  ClipboardPathsResult paths{};
  paths.available = true;
  paths.paths = spans;

  Event event{};
  event.payload = DropEvent{.count = 1u, .paths = span};

  std::array<Event, 1> events{event};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  const auto& drop = std::get<DropEvent>(batch.events[0].payload);
  PH_CHECK(drop.paths.offset == 1u);
  PH_CHECK(drop.paths.length == 3u);
  PH_CHECK(batch.textBytes[drop.paths.offset] == 'e');
  PH_CHECK(batch.textBytes[drop.paths.offset + 1u] == 'l');
  PH_CHECK(batch.textBytes[drop.paths.offset + 2u] == 'l');
}

PH_TEST("primehost.event.payload", "multiple events share text buffer") {
  std::array<char, 8> textBytes{'f', 'o', 'o', '\0', 'b', 'a', 'r', '\0'};
  TextSpan first{0u, 3u};
  TextSpan second{4u, 3u};

  Event a{};
  a.payload = DropEvent{.count = 1u, .paths = first};

  Event b{};
  b.payload = DropEvent{.count = 1u, .paths = second};

  std::array<Event, 2> events{a, b};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  const auto& dropA = std::get<DropEvent>(batch.events[0].payload);
  const auto& dropB = std::get<DropEvent>(batch.events[1].payload);

  PH_CHECK(batch.textBytes[dropA.paths.offset] == 'f');
  PH_CHECK(batch.textBytes[dropA.paths.offset + 1u] == 'o');
  PH_CHECK(batch.textBytes[dropA.paths.offset + 2u] == 'o');

  PH_CHECK(batch.textBytes[dropB.paths.offset] == 'b');
  PH_CHECK(batch.textBytes[dropB.paths.offset + 1u] == 'a');
  PH_CHECK(batch.textBytes[dropB.paths.offset + 2u] == 'r');
}

PH_TEST("primehost.event.payload", "event scope and surface id propagate") {
  Event first{};
  first.scope = Event::Scope::Surface;
  first.surfaceId = SurfaceId{5u};
  first.payload = FocusEvent{.focused = true};

  Event second{};
  second.scope = Event::Scope::Global;
  second.surfaceId.reset();
  second.payload = PowerEvent{.lowPowerModeEnabled = false};

  std::array<Event, 2> events{first, second};
  std::array<char, 1> textBytes{'x'};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  PH_CHECK(batch.events[0].scope == Event::Scope::Surface);
  PH_CHECK(batch.events[0].surfaceId.has_value());
  PH_CHECK(batch.events[0].surfaceId->value == 5u);
  PH_CHECK(std::holds_alternative<FocusEvent>(batch.events[0].payload));

  PH_CHECK(batch.events[1].scope == Event::Scope::Global);
  PH_CHECK(!batch.events[1].surfaceId.has_value());
  PH_CHECK(std::holds_alternative<PowerEvent>(batch.events[1].payload));
}

PH_TEST("primehost.event.payload", "event batch mixed payload types") {
  Event inputEvent{};
  PointerEvent pointer{};
  pointer.x = 4;
  pointer.y = 5;
  inputEvent.payload = InputEvent{pointer};

  Event resizeEvent{};
  ResizeEvent resize{};
  resize.width = 320u;
  resize.height = 240u;
  resizeEvent.payload = resize;

  Event lifecycleEvent{};
  lifecycleEvent.payload = LifecycleEvent{.phase = LifecyclePhase::Resumed};

  std::array<Event, 3> events{inputEvent, resizeEvent, lifecycleEvent};
  std::array<char, 1> textBytes{'z'};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  PH_CHECK(std::holds_alternative<InputEvent>(batch.events[0].payload));
  const auto& input = std::get<InputEvent>(batch.events[0].payload);
  PH_CHECK(std::holds_alternative<PointerEvent>(input));

  PH_CHECK(std::holds_alternative<ResizeEvent>(batch.events[1].payload));
  const auto& resizePayload = std::get<ResizeEvent>(batch.events[1].payload);
  PH_CHECK(resizePayload.width == 320u);
  PH_CHECK(resizePayload.height == 240u);

  PH_CHECK(std::holds_alternative<LifecycleEvent>(batch.events[2].payload));
  const auto& lifecycle = std::get<LifecycleEvent>(batch.events[2].payload);
  PH_CHECK(lifecycle.phase == LifecyclePhase::Resumed);
}

PH_TEST("primehost.event.payload", "event batch shared surface id with mixed scopes") {
  SurfaceId shared{9u};
  Event first{};
  first.scope = Event::Scope::Surface;
  first.surfaceId = shared;
  first.payload = FocusEvent{.focused = true};

  Event second{};
  second.scope = Event::Scope::Surface;
  second.surfaceId = shared;
  second.payload = ResizeEvent{.width = 10u, .height = 20u, .scale = 1.0f};

  Event third{};
  third.scope = Event::Scope::Global;
  third.surfaceId = shared;
  third.payload = PowerEvent{.lowPowerModeEnabled = true};

  std::array<Event, 3> events{first, second, third};
  std::array<char, 1> textBytes{'q'};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  PH_CHECK(batch.events[0].surfaceId.has_value());
  PH_CHECK(batch.events[0].surfaceId->value == shared.value);
  PH_CHECK(batch.events[1].surfaceId.has_value());
  PH_CHECK(batch.events[1].surfaceId->value == shared.value);
  PH_CHECK(batch.events[2].surfaceId.has_value());
  PH_CHECK(batch.events[2].surfaceId->value == shared.value);

  PH_CHECK(batch.events[0].scope == Event::Scope::Surface);
  PH_CHECK(batch.events[1].scope == Event::Scope::Surface);
  PH_CHECK(batch.events[2].scope == Event::Scope::Global);
}

PH_TEST("primehost.event.payload", "event batch preserves ordering") {
  Event first{};
  first.payload = ResizeEvent{.width = 1u, .height = 2u, .scale = 1.0f};

  Event second{};
  second.payload = ResizeEvent{.width = 3u, .height = 4u, .scale = 1.0f};

  Event third{};
  third.payload = ResizeEvent{.width = 5u, .height = 6u, .scale = 1.0f};

  std::array<Event, 3> events{first, second, third};
  std::array<char, 1> textBytes{'o'};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  const auto& a = std::get<ResizeEvent>(batch.events[0].payload);
  const auto& b = std::get<ResizeEvent>(batch.events[1].payload);
  const auto& c = std::get<ResizeEvent>(batch.events[2].payload);
  PH_CHECK(a.width == 1u);
  PH_CHECK(a.height == 2u);
  PH_CHECK(b.width == 3u);
  PH_CHECK(b.height == 4u);
  PH_CHECK(c.width == 5u);
  PH_CHECK(c.height == 6u);
}

PH_TEST("primehost.event.payload", "event batch scope transitions") {
  Event first{};
  first.scope = Event::Scope::Surface;
  first.surfaceId = SurfaceId{1u};
  first.payload = FocusEvent{.focused = true};

  Event second{};
  second.scope = Event::Scope::Global;
  second.surfaceId.reset();
  second.payload = PowerEvent{.lowPowerModeEnabled = true};

  Event third{};
  third.scope = Event::Scope::Surface;
  third.surfaceId = SurfaceId{2u};
  third.payload = ResizeEvent{.width = 7u, .height = 8u, .scale = 1.0f};

  std::array<Event, 3> events{first, second, third};
  std::array<char, 1> textBytes{'t'};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  PH_CHECK(batch.events[0].scope == Event::Scope::Surface);
  PH_CHECK(batch.events[0].surfaceId.has_value());
  PH_CHECK(batch.events[0].surfaceId->value == 1u);

  PH_CHECK(batch.events[1].scope == Event::Scope::Global);
  PH_CHECK(!batch.events[1].surfaceId.has_value());

  PH_CHECK(batch.events[2].scope == Event::Scope::Surface);
  PH_CHECK(batch.events[2].surfaceId.has_value());
  PH_CHECK(batch.events[2].surfaceId->value == 2u);
}

PH_TEST("primehost.event.payload", "event batch mixed text spans") {
  std::array<char, 12> textBytes{
      'o', 'n', 'e', '\0',
      't', 'w', 'o', '\0',
      't', 'h', 'r', 'e',
  };
  TextSpan spanA{0u, 3u};
  TextSpan spanB{4u, 3u};
  TextSpan spanC{8u, 4u};

  Event drop{};
  drop.payload = DropEvent{.count = 1u, .paths = spanA};

  Event text{};
  TextEvent textEvent{};
  textEvent.text = spanB;
  text.payload = InputEvent{textEvent};

  Event clipboard{};
  clipboard.payload = DropEvent{.count = 1u, .paths = spanC};

  std::array<Event, 3> events{drop, text, clipboard};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  const auto& dropPayload = std::get<DropEvent>(batch.events[0].payload);
  PH_CHECK(batch.textBytes[dropPayload.paths.offset] == 'o');
  PH_CHECK(batch.textBytes[dropPayload.paths.offset + 1u] == 'n');
  PH_CHECK(batch.textBytes[dropPayload.paths.offset + 2u] == 'e');

  const auto& input = std::get<InputEvent>(batch.events[1].payload);
  const auto& textPayload = std::get<TextEvent>(input);
  PH_CHECK(batch.textBytes[textPayload.text.offset] == 't');
  PH_CHECK(batch.textBytes[textPayload.text.offset + 1u] == 'w');
  PH_CHECK(batch.textBytes[textPayload.text.offset + 2u] == 'o');

  const auto& clipPayload = std::get<DropEvent>(batch.events[2].payload);
  PH_CHECK(batch.textBytes[clipPayload.paths.offset] == 't');
  PH_CHECK(batch.textBytes[clipPayload.paths.offset + 1u] == 'h');
  PH_CHECK(batch.textBytes[clipPayload.paths.offset + 2u] == 'r');
  PH_CHECK(batch.textBytes[clipPayload.paths.offset + 3u] == 'e');
}

PH_TEST("primehost.event.payload", "shared text spans across events") {
  std::array<char, 7> textBytes{'s', 'h', 'a', 'r', 'e', 'd', '\0'};
  TextSpan shared{0u, 6u};

  Event drop{};
  drop.payload = DropEvent{.count = 1u, .paths = shared};

  Event textEvent{};
  TextEvent text{};
  text.text = shared;
  textEvent.payload = InputEvent{text};

  std::array<Event, 2> events{drop, textEvent};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  const auto& dropPayload = std::get<DropEvent>(batch.events[0].payload);
  PH_CHECK(dropPayload.paths.offset == 0u);
  PH_CHECK(dropPayload.paths.length == 6u);
  PH_CHECK(batch.textBytes[dropPayload.paths.offset] == 's');
  PH_CHECK(batch.textBytes[dropPayload.paths.offset + 5u] == 'd');

  const auto& input = std::get<InputEvent>(batch.events[1].payload);
  const auto& textPayload = std::get<TextEvent>(input);
  PH_CHECK(textPayload.text.offset == 0u);
  PH_CHECK(textPayload.text.length == 6u);
}

PH_TEST("primehost.event.payload", "shared spans with empty buffer") {
  TextSpan shared{0u, 3u};
  Event drop{};
  drop.payload = DropEvent{.count = 1u, .paths = shared};

  Event textEvent{};
  TextEvent text{};
  text.text = shared;
  textEvent.payload = InputEvent{text};

  std::array<Event, 2> events{drop, textEvent};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(),
  };

  const auto& dropPayload = std::get<DropEvent>(batch.events[0].payload);
  PH_CHECK(dropPayload.paths.offset == 0u);
  PH_CHECK(dropPayload.paths.length == 3u);

  const auto& input = std::get<InputEvent>(batch.events[1].payload);
  const auto& textPayload = std::get<TextEvent>(input);
  PH_CHECK(textPayload.text.offset == 0u);
  PH_CHECK(textPayload.text.length == 3u);
  PH_CHECK(batch.textBytes.empty());
}

PH_TEST("primehost.event.payload", "resize and drop payload values") {
  ResizeEvent resize{};
  resize.width = 640u;
  resize.height = 480u;
  resize.scale = 2.0f;

  DropEvent drop{};
  drop.count = 2u;
  drop.paths = TextSpan{2u, 5u};

  Event a{};
  a.payload = resize;
  Event b{};
  b.payload = drop;

  PH_CHECK(std::holds_alternative<ResizeEvent>(a.payload));
  const auto& resizePayload = std::get<ResizeEvent>(a.payload);
  PH_CHECK(resizePayload.width == 640u);
  PH_CHECK(resizePayload.height == 480u);
  PH_CHECK(resizePayload.scale == 2.0f);

  PH_CHECK(std::holds_alternative<DropEvent>(b.payload));
  const auto& dropPayload = std::get<DropEvent>(b.payload);
  PH_CHECK(dropPayload.count == 2u);
  PH_CHECK(dropPayload.paths.offset == 2u);
  PH_CHECK(dropPayload.paths.length == 5u);
}

PH_TEST("primehost.event.payload", "text spans with empty buffer") {
  TextSpan span{2u, 4u};
  DropEvent drop{};
  drop.count = 1u;
  drop.paths = span;

  Event event{};
  event.payload = drop;

  std::array<Event, 1> events{event};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(),
  };

  const auto& payload = std::get<DropEvent>(batch.events[0].payload);
  PH_CHECK(payload.paths.offset == 2u);
  PH_CHECK(payload.paths.length == 4u);
  PH_CHECK(batch.textBytes.empty());
}

PH_TEST("primehost.event.payload", "text event with empty buffer") {
  TextSpan span{1u, 2u};
  TextEvent text{};
  text.text = span;

  Event event{};
  event.payload = InputEvent{text};

  std::array<Event, 1> events{event};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(),
  };

  const auto& input = std::get<InputEvent>(batch.events[0].payload);
  const auto& textPayload = std::get<TextEvent>(input);
  PH_CHECK(textPayload.text.offset == 1u);
  PH_CHECK(textPayload.text.length == 2u);
  PH_CHECK(batch.textBytes.empty());
}

PH_TEST("primehost.event.payload", "mixed payloads with empty buffer") {
  TextSpan span{0u, 3u};
  TextEvent text{};
  text.text = span;

  Event textEvent{};
  textEvent.payload = InputEvent{text};

  Event focusEvent{};
  focusEvent.payload = FocusEvent{.focused = true};

  Event resizeEvent{};
  resizeEvent.payload = ResizeEvent{.width = 10u, .height = 20u, .scale = 1.0f};

  std::array<Event, 3> events{textEvent, focusEvent, resizeEvent};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(),
  };

  const auto& input = std::get<InputEvent>(batch.events[0].payload);
  const auto& textPayload = std::get<TextEvent>(input);
  PH_CHECK(textPayload.text.offset == 0u);
  PH_CHECK(textPayload.text.length == 3u);
  PH_CHECK(batch.textBytes.empty());

  PH_CHECK(std::holds_alternative<FocusEvent>(batch.events[1].payload));
  PH_CHECK(std::holds_alternative<ResizeEvent>(batch.events[2].payload));
}

PH_TEST("primehost.event.payload", "text span zero length") {
  std::array<char, 4> textBytes{'z', 'e', 'r', 'o'};
  TextSpan span{1u, 0u};
  DropEvent drop{};
  drop.count = 1u;
  drop.paths = span;

  Event event{};
  event.payload = drop;

  std::array<Event, 1> events{event};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  const auto& payload = std::get<DropEvent>(batch.events[0].payload);
  PH_CHECK(payload.paths.offset == 1u);
  PH_CHECK(payload.paths.length == 0u);
  PH_CHECK(batch.textBytes.size() == textBytes.size());
}

PH_TEST("primehost.event.payload", "multiple zero-length spans") {
  std::array<char, 6> textBytes{'a', 'b', 'c', 'd', 'e', 'f'};
  TextSpan spanA{0u, 0u};
  TextSpan spanB{3u, 0u};

  Event first{};
  first.payload = DropEvent{.count = 1u, .paths = spanA};

  Event second{};
  second.payload = DropEvent{.count = 1u, .paths = spanB};

  std::array<Event, 2> events{first, second};
  EventBatch batch{
      std::span<const Event>(events.data(), events.size()),
      std::span<const char>(textBytes.data(), textBytes.size()),
  };

  const auto& payloadA = std::get<DropEvent>(batch.events[0].payload);
  const auto& payloadB = std::get<DropEvent>(batch.events[1].payload);
  PH_CHECK(payloadA.paths.length == 0u);
  PH_CHECK(payloadB.paths.length == 0u);
  PH_CHECK(payloadA.paths.offset == 0u);
  PH_CHECK(payloadB.paths.offset == 3u);
}

TEST_SUITE_END();
