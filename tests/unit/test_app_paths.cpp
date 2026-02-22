#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

#include <array>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.paths");

PH_TEST("primehost.paths", "app path queries") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  const AppPathType types[] = {
      AppPathType::UserData,
      AppPathType::Cache,
      AppPathType::Config,
      AppPathType::Logs,
      AppPathType::Temp,
  };

  for (AppPathType type : types) {
    auto size = host->appPathSize(type);
    PH_CHECK(size.has_value());
    if (!size.has_value()) {
      continue;
    }
    PH_CHECK(size.value() > 0u);
    if (size.value() == 0u) {
      continue;
    }

    std::span<char> empty{};
    auto emptyResult = host->appPath(type, empty);
    PH_CHECK(!emptyResult.has_value());
    if (!emptyResult.has_value()) {
      PH_CHECK(emptyResult.error().code == HostErrorCode::BufferTooSmall);
    }

    std::vector<char> buffer(size.value());
    auto path = host->appPath(type, buffer);
    PH_CHECK(path.has_value());
    if (!path.has_value()) {
      continue;
    }
    PH_CHECK(path->size() == size.value());

    if (size.value() > 1u) {
      std::vector<char> small(size.value() - 1u);
      auto tooSmall = host->appPath(type, small);
      PH_CHECK(!tooSmall.has_value());
      if (!tooSmall.has_value()) {
        PH_CHECK(tooSmall.error().code == HostErrorCode::BufferTooSmall);
      }
    }
  }
}

PH_TEST("primehost.paths", "invalid path type") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto invalidSize = host->appPathSize(static_cast<AppPathType>(999));
  PH_CHECK(!invalidSize.has_value());
  if (!invalidSize.has_value()) {
    PH_CHECK(invalidSize.error().code == HostErrorCode::InvalidConfig);
  }

  std::array<char, 4> buffer{};
  auto invalidPath = host->appPath(static_cast<AppPathType>(999), buffer);
  PH_CHECK(!invalidPath.has_value());
  if (!invalidPath.has_value()) {
    PH_CHECK(invalidPath.error().code == HostErrorCode::InvalidConfig);
  }
}

TEST_SUITE_END();
