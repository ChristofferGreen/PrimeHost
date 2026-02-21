#include "PrimeHost/Audio.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.audio");

PH_TEST("primehost.audio", "create audio host") {
  auto result = createAudioHost();
  if (!result) {
    PH_CHECK(result.error().code == HostErrorCode::Unsupported);
    return;
  }
  PH_CHECK(result.value() != nullptr);
}

TEST_SUITE_END();
