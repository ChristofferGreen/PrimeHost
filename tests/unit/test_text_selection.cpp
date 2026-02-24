#include "demo/TextSelection.h"

#include "tests/unit/test_helpers.h"

using PrimeHost::DemoText::wrapTextLineRanges;
using PrimeHost::DemoText::caretIndexForClick;

namespace {

float monoMeasure(std::string_view text) {
  return static_cast<float>(text.size());
}

} // namespace

PH_TEST("text selection", "wrap none respects newlines") {
  std::string_view text = "a\nb\n";
  auto lines = wrapTextLineRanges(text, 0.0f, PrimeFrame::WrapMode::None, monoMeasure);
  PH_REQUIRE(lines.size() == 3u);
  PH_CHECK(lines[0].start == 0u);
  PH_CHECK(lines[0].end == 1u);
  PH_CHECK(lines[1].start == 2u);
  PH_CHECK(lines[1].end == 3u);
  PH_CHECK(lines[2].start == 4u);
  PH_CHECK(lines[2].end == 4u);
}

PH_TEST("text selection", "wrap word splits on width") {
  std::string_view text = "one two three";
  auto lines = wrapTextLineRanges(text, 7.0f, PrimeFrame::WrapMode::Word, monoMeasure);
  PH_REQUIRE(lines.size() == 2u);
  PH_CHECK(text.substr(lines[0].start, lines[0].end - lines[0].start) == "one two");
  PH_CHECK(text.substr(lines[1].start, lines[1].end - lines[1].start) == "three");
}

PH_TEST("text selection", "wrap word keeps long word intact") {
  std::string_view text = "supercalifragilistic";
  auto lines = wrapTextLineRanges(text, 5.0f, PrimeFrame::WrapMode::Word, monoMeasure);
  PH_REQUIRE(lines.size() == 1u);
  PH_CHECK(lines[0].start == 0u);
  PH_CHECK(lines[0].end == text.size());
}

PH_TEST("text selection", "wrap ignores leading spaces") {
  std::string_view text = "   one two";
  auto lines = wrapTextLineRanges(text, 7.0f, PrimeFrame::WrapMode::Word, monoMeasure);
  PH_REQUIRE(lines.size() >= 1u);
  PH_CHECK(lines[0].start == 3u);
}

PH_TEST("text selection", "wrap keeps internal spaces") {
  std::string_view text = "one   two";
  auto lines = wrapTextLineRanges(text, 7.0f, PrimeFrame::WrapMode::Word, monoMeasure);
  PH_REQUIRE(lines.size() == 1u);
  PH_CHECK(lines[0].start == 0u);
  PH_CHECK(lines[0].end == text.size());
}

PH_TEST("text selection", "wrap produces ordered ranges") {
  std::string_view text = "one two three four five";
  auto lines = wrapTextLineRanges(text, 6.0f, PrimeFrame::WrapMode::Word, monoMeasure);
  PH_REQUIRE(!lines.empty());
  uint32_t lastEnd = 0u;
  for (const auto& line : lines) {
    PH_CHECK(line.start <= line.end);
    PH_CHECK(line.end <= text.size());
    PH_CHECK(line.start >= lastEnd);
    lastEnd = line.end;
  }
  PH_CHECK(lastEnd == text.size());
}

PH_TEST("text selection", "caret hit test clamps to bounds") {
  std::string_view text = "abcd";
  PH_CHECK(caretIndexForClick(text, 0.0f, -10.0f, monoMeasure) == 0u);
  PH_CHECK(caretIndexForClick(text, 0.0f, 100.0f, monoMeasure) == text.size());
}

PH_TEST("text selection", "caret hit test chooses closest boundary") {
  std::string_view text = "abcd";
  PH_CHECK(caretIndexForClick(text, 0.0f, 1.1f, monoMeasure) == 1u);
  PH_CHECK(caretIndexForClick(text, 0.0f, 1.9f, monoMeasure) == 2u);
}
