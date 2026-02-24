#include "PrimeHost/FrameConfigValidation.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surfacemasks");

PH_TEST("primehost.surfacemasks", "maskHas works with multiple bits") {
  PresentModeMask present = 0u;
  present |= (1u << static_cast<uint32_t>(PresentMode::LowLatency));
  present |= (1u << static_cast<uint32_t>(PresentMode::Uncapped));

  PH_CHECK(maskHas(present, PresentMode::LowLatency));
  PH_CHECK(!maskHas(present, PresentMode::Smooth));
  PH_CHECK(maskHas(present, PresentMode::Uncapped));

  ColorFormatMask formats = 0u;
  formats |= (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM));
  PH_CHECK(maskHas(formats, ColorFormat::B8G8R8A8_UNORM));
}

TEST_SUITE_END();
