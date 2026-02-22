#include "PrimeHost/Host.h"

#include "src/TextBuffer.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.text_buffer");

PH_TEST("primehost.text_buffer", "append text spans") {
  std::array<char, 16> buffer{};
  TextBufferWriter writer{std::span<char>(buffer.data(), buffer.size()), 0u};

  auto span = writer.append("hi");
  PH_CHECK(span.has_value());
  PH_CHECK(span->offset == 0u);
  PH_CHECK(span->length == 2u);

  auto span2 = writer.append("world");
  PH_CHECK(span2.has_value());
  PH_CHECK(span2->offset == 2u);
  PH_CHECK(span2->length == 5u);
}

PH_TEST("primehost.text_buffer", "buffer too small") {
  std::array<char, 4> buffer{};
  TextBufferWriter writer{std::span<char>(buffer.data(), buffer.size()), 0u};

  auto span = writer.append("hello");
  PH_CHECK(!span.has_value());
  PH_CHECK(span.error().code == HostErrorCode::BufferTooSmall);
}

PH_TEST("primehost.text_buffer", "invalid offset fails") {
  std::array<char, 4> buffer{};
  TextBufferWriter writer{std::span<char>(buffer.data(), buffer.size()), 8u};

  auto span = writer.append("hi");
  PH_CHECK(!span.has_value());
  PH_CHECK(span.error().code == HostErrorCode::BufferTooSmall);
}

TEST_SUITE_END();
