#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

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

TEST_SUITE_END();
