#pragma once

#include "third_party/doctest.h"

#define PH_TEST(group, name) TEST_CASE(group " :: " name)
#define PH_CHECK CHECK
#define PH_REQUIRE REQUIRE
