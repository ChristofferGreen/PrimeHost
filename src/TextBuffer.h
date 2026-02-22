#pragma once

#include "PrimeHost/Host.h"

#include <cstring>

namespace PrimeHost {

struct TextBufferWriter {
  std::span<char> buffer;
  size_t offset = 0u;

  HostResult<TextSpan> append(std::string_view text) {
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
