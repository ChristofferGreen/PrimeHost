#include "src/SizeUtil.h"

#include "tests/unit/test_helpers.h"

#include <limits>

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.size_util");

PH_TEST("primehost.size_util", "checked u32 bounds") {
  PH_CHECK(checkedU32(0u).value() == 0u);
  PH_CHECK(checkedU32(std::numeric_limits<uint32_t>::max()).value() ==
           std::numeric_limits<uint32_t>::max());
  PH_CHECK(!checkedU32(static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1u).has_value());
}

PH_TEST("primehost.size_util", "checked multiply") {
  PH_CHECK(checkedSizeMul(0u, 5u).value() == 0u);
  PH_CHECK(checkedSizeMul(3u, 4u).value() == 12u);
  PH_CHECK(!checkedSizeMul(std::numeric_limits<size_t>::max(), 2u).has_value());
}

TEST_SUITE_END();
