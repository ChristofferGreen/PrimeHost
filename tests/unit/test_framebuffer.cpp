#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.framebuffer");

PH_TEST("primehost.framebuffer", "acquire and present headless") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 48u;
  config.headless = true;
  auto surfaceResult = host->createSurface(config);
  PH_CHECK(surfaceResult.has_value());
  if (!surfaceResult) {
    return;
  }

  SurfaceId surfaceId = surfaceResult.value();
  auto frameResult = host->acquireFrameBuffer(surfaceId);
  PH_CHECK(frameResult.has_value());
  if (!frameResult) {
    host->destroySurface(surfaceId);
    return;
  }
  FrameBuffer buffer = frameResult.value();
  PH_CHECK(buffer.size.width == config.width);
  PH_CHECK(buffer.size.height == config.height);
  PH_CHECK(buffer.stride == config.width * 4u);
  PH_CHECK(buffer.pixels.size() >= static_cast<size_t>(buffer.stride) * buffer.size.height);

  for (size_t i = 0; i < buffer.pixels.size(); ++i) {
    buffer.pixels[i] = static_cast<uint8_t>(i & 0xFFu);
  }

  auto status = host->presentFrameBuffer(surfaceId, buffer);
  PH_CHECK(status.has_value());

  host->destroySurface(surfaceId);
}

TEST_SUITE_END();
