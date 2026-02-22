#pragma once

#include "PrimeHost/Host.h"

#include <cstring>
#include <limits>

namespace PrimeHost {

struct TextBufferWriter {
  std::span<char> buffer;
  size_t offset = 0u;

  HostResult<TextSpan> append(std::string_view text) {
    constexpr size_t kMaxSpan = static_cast<size_t>(std::numeric_limits<uint32_t>::max());
    if (offset > kMaxSpan) {
      return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
    }
    if (text.size() > kMaxSpan) {
      return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
    }
    if (text.size() > kMaxSpan - offset) {
      return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
    }
    if (offset > buffer.size()) {
      return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
    }
    if (text.empty()) {
      return TextSpan{static_cast<uint32_t>(offset), 0u};
    }
    if (text.size() > buffer.size() - offset) {
      return std::unexpected(HostError{HostErrorCode::BufferTooSmall});
    }
    std::memcpy(buffer.data() + offset, text.data(), text.size());
    TextSpan span{static_cast<uint32_t>(offset), static_cast<uint32_t>(text.size())};
    offset += text.size();
    return span;
  }
};

} // namespace PrimeHost
