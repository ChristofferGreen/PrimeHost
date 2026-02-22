#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.log");

PH_TEST("primehost.log", "set callback") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto status = host->setLogCallback([](LogLevel level, Utf8TextView message) {
    (void)level;
    (void)message;
  });
  PH_CHECK(status.has_value());

  auto clear = host->setLogCallback({});
  PH_CHECK(clear.has_value());
}

TEST_SUITE_END();
