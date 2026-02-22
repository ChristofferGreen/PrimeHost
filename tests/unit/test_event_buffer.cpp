#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.events");

PH_TEST("primehost.events", "pollEvents invalid buffer") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  EventBuffer buffer{};
  auto batch = host->pollEvents(buffer);
  PH_CHECK(!batch.has_value());
  if (!batch.has_value()) {
    PH_CHECK(batch.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.events", "pollEvents invalid text buffer") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  std::array<Event, 1> events{};
  std::span<char> textBytes(static_cast<char*>(nullptr), 4u);
  EventBuffer buffer{
      std::span<Event>(events.data(), events.size()),
      textBytes,
  };
  auto batch = host->pollEvents(buffer);
  PH_CHECK(!batch.has_value());
  if (!batch.has_value()) {
    PH_CHECK(batch.error().code == HostErrorCode::InvalidConfig);
  }
}

TEST_SUITE_END();
