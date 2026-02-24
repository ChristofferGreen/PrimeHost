#include "PrimeHost/Host.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.surfacecaps");

PH_TEST("primehost.surfacecaps", "buffer counts can be zero") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 0u;
  caps.maxBufferCount = 0u;
  PH_CHECK(caps.minBufferCount == 0u);
  PH_CHECK(caps.maxBufferCount == 0u);
}

PH_TEST("primehost.surfacecaps", "buffer counts can be nonzero") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 1u;
  caps.maxBufferCount = 4u;
  PH_CHECK(caps.minBufferCount == 1u);
  PH_CHECK(caps.maxBufferCount == 4u);
}

PH_TEST("primehost.surfacecaps", "buffer counts can be inverted") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 4u;
  caps.maxBufferCount = 2u;
  PH_CHECK(caps.minBufferCount == 4u);
  PH_CHECK(caps.maxBufferCount == 2u);
}

PH_TEST("primehost.surfacecaps", "buffer counts can be inverted nonzero") {
  SurfaceCapabilities caps{};
  caps.minBufferCount = 5u;
  caps.maxBufferCount = 1u;
  PH_CHECK(caps.minBufferCount == 5u);
  PH_CHECK(caps.maxBufferCount == 1u);
}

PH_TEST("primehost.surfacecaps", "present and color masks writable") {
  SurfaceCapabilities caps{};
  caps.presentModes = (1u << static_cast<uint32_t>(PresentMode::LowLatency)) |
                      (1u << static_cast<uint32_t>(PresentMode::Uncapped));
  caps.colorFormats = (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM));

  PH_CHECK(caps.presentModes != 0u);
  PH_CHECK(caps.colorFormats != 0u);
}

PH_TEST("primehost.surfacecaps", "toggle support flags") {
  SurfaceCapabilities caps{};
  caps.supportsVsyncToggle = true;
  caps.supportsTearing = true;
  PH_CHECK(caps.supportsVsyncToggle);
  PH_CHECK(caps.supportsTearing);
}

PH_TEST("primehost.surfacecaps", "supports flags with masks") {
  SurfaceCapabilities caps{};
  caps.supportsVsyncToggle = true;
  caps.supportsTearing = false;
  caps.presentModes = (1u << static_cast<uint32_t>(PresentMode::Smooth));
  caps.colorFormats = (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM));

  PH_CHECK(caps.supportsVsyncToggle);
  PH_CHECK(!caps.supportsTearing);
  PH_CHECK(caps.presentModes != 0u);
  PH_CHECK(caps.colorFormats != 0u);
}

PH_TEST("primehost.surfacecaps", "mask combinations stored") {
  SurfaceCapabilities caps{};
  caps.presentModes = (1u << static_cast<uint32_t>(PresentMode::LowLatency)) |
                      (1u << static_cast<uint32_t>(PresentMode::Smooth)) |
                      (1u << static_cast<uint32_t>(PresentMode::Uncapped));
  caps.colorFormats = (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM));
  PH_CHECK(caps.presentModes != 0u);
  PH_CHECK(caps.colorFormats != 0u);
}

TEST_SUITE_END();
