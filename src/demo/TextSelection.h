#pragma once

#include "PrimeFrame/Frame.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string_view>
#include <vector>

namespace PrimeHost::DemoText {

struct WrappedLine {
  uint32_t start = 0u;
  uint32_t end = 0u;
  float width = 0.0f;
};

inline bool isUtf8Continuation(uint8_t value) {
  return (value & 0xC0u) == 0x80u;
}

inline uint32_t utf8Prev(std::string_view text, uint32_t index) {
  if (index == 0u) {
    return 0u;
  }
  uint32_t size = static_cast<uint32_t>(text.size());
  uint32_t i = std::min(index, size);
  if (i > 0u) {
    --i;
  }
  while (i > 0u && isUtf8Continuation(static_cast<uint8_t>(text[i]))) {
    --i;
  }
  return i;
}

inline uint32_t utf8Next(std::string_view text, uint32_t index) {
  uint32_t size = static_cast<uint32_t>(text.size());
  if (index >= size) {
    return size;
  }
  uint32_t i = index + 1u;
  while (i < size && isUtf8Continuation(static_cast<uint8_t>(text[i]))) {
    ++i;
  }
  return i;
}

template <typename MeasureFn>
inline uint32_t caretIndexForClick(std::string_view text,
                                   float paddingX,
                                   float localX,
                                   MeasureFn measure) {
  if (text.empty()) {
    return 0u;
  }
  float targetX = localX - paddingX;
  if (targetX <= 0.0f) {
    return 0u;
  }
  float totalWidth = measure(text);
  if (targetX >= totalWidth) {
    return static_cast<uint32_t>(text.size());
  }
  uint32_t prevIndex = 0u;
  float prevWidth = 0.0f;
  uint32_t index = utf8Next(text, 0u);
  while (index <= text.size()) {
    float width = measure(text.substr(0, index));
    if (width >= targetX) {
      float prevDist = targetX - prevWidth;
      float nextDist = width - targetX;
      return (prevDist <= nextDist) ? prevIndex : index;
    }
    prevIndex = index;
    prevWidth = width;
    index = utf8Next(text, index);
  }
  return static_cast<uint32_t>(text.size());
}

template <typename MeasureFn>
inline std::vector<WrappedLine> wrapTextLineRanges(std::string_view text,
                                                   float maxWidth,
                                                   PrimeFrame::WrapMode wrap,
                                                   MeasureFn measure) {
  std::vector<WrappedLine> lines;
  if (text.empty()) {
    return lines;
  }
  if (maxWidth <= 0.0f || wrap == PrimeFrame::WrapMode::None) {
    uint32_t lineStart = 0u;
    for (uint32_t i = 0u; i < text.size(); ++i) {
      if (text[i] == '\n') {
        float width = measure(text.substr(lineStart, i - lineStart));
        lines.push_back({lineStart, i, width});
        lineStart = i + 1u;
      }
    }
    float width = measure(text.substr(lineStart, text.size() - lineStart));
    lines.push_back({lineStart, static_cast<uint32_t>(text.size()), width});
    return lines;
  }

  float spaceWidth = measure(" ");
  bool wrapByChar = wrap == PrimeFrame::WrapMode::Character;
  uint32_t i = 0u;
  uint32_t lineStart = 0u;
  uint32_t lineEnd = 0u;
  float lineWidth = 0.0f;
  bool lineHasWord = false;

  auto push_line = [&](uint32_t endIndex, float width) {
    lines.push_back({lineStart, endIndex, width});
    lineStart = endIndex;
    lineEnd = endIndex;
    lineWidth = 0.0f;
    lineHasWord = false;
  };

  while (i < text.size()) {
    char ch = text[i];
    if (ch == '\n') {
      push_line(lineHasWord ? lineEnd : i, lineWidth);
      ++i;
      lineStart = i;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(ch))) {
      ++i;
      continue;
    }
    uint32_t wordStart = i;
    if (wrapByChar) {
      i = utf8Next(text, i);
    } else {
      while (i < text.size()) {
        char wordCh = text[i];
        if (wordCh == '\n' || std::isspace(static_cast<unsigned char>(wordCh))) {
          break;
        }
        ++i;
      }
    }
    uint32_t wordEnd = i;
    if (wordEnd <= wordStart) {
      ++i;
      continue;
    }
    float wordWidth = measure(text.substr(wordStart, wordEnd - wordStart));
    if (lineHasWord && lineWidth + spaceWidth + wordWidth > maxWidth) {
      push_line(lineEnd, lineWidth);
    }
    if (!lineHasWord) {
      lineStart = wordStart;
      lineEnd = wordEnd;
      lineWidth = wordWidth;
      lineHasWord = true;
    } else {
      lineEnd = wordEnd;
      lineWidth += spaceWidth + wordWidth;
    }
  }
  if (lineHasWord) {
    push_line(lineEnd, lineWidth);
  } else if (lineStart < text.size()) {
    lines.push_back({lineStart, static_cast<uint32_t>(text.size()), 0.0f});
  }
  if (lines.empty()) {
    lines.push_back({0u, static_cast<uint32_t>(text.size()), 0.0f});
  }
  return lines;
}

} // namespace PrimeHost::DemoText
