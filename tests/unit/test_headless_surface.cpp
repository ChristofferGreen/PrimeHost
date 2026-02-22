#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.headless");

PH_TEST("primehost.headless", "create and size") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  SurfaceConfig config{};
  config.width = 64u;
  config.height = 32u;
  config.headless = true;

  auto surface = host->createSurface(config);
  if (!surface) {
    PH_CHECK(surface.error().code == HostErrorCode::Unsupported);
    return;
  }

  auto size = host->surfaceSize(surface.value());
  PH_CHECK(size.has_value());
  if (size) {
    PH_CHECK(size->width == 64u);
    PH_CHECK(size->height == 32u);
  }

  auto position = host->surfacePosition(surface.value());
  PH_CHECK(position.has_value());
  if (position) {
    PH_CHECK(position->x == 0);
    PH_CHECK(position->y == 0);
  }

  auto setPos = host->setSurfacePosition(surface.value(), 10, 20);
  PH_CHECK(setPos.has_value());

  auto titleStatus = host->setSurfaceTitle(surface.value(), "Headless");
  PH_CHECK(titleStatus.has_value());

  auto insets = host->surfaceSafeAreaInsets(surface.value());
  PH_CHECK(insets.has_value());
  if (insets) {
    PH_CHECK(insets->top == 0.0f);
    PH_CHECK(insets->left == 0.0f);
    PH_CHECK(insets->right == 0.0f);
    PH_CHECK(insets->bottom == 0.0f);
  }

  auto minimized = host->setSurfaceMinimized(surface.value(), true);
  PH_CHECK(minimized.has_value());
  auto maximized = host->setSurfaceMaximized(surface.value(), true);
  PH_CHECK(maximized.has_value());
  auto fullscreen = host->setSurfaceFullscreen(surface.value(), true);
  PH_CHECK(fullscreen.has_value());

  auto minSize = host->setSurfaceMinSize(surface.value(), 16u, 16u);
  PH_CHECK(minSize.has_value());
  auto maxSize = host->setSurfaceMaxSize(surface.value(), 1024u, 768u);
  PH_CHECK(maxSize.has_value());

  auto status = host->setSurfaceSize(surface.value(), 128u, 256u);
  PH_CHECK(status.has_value());
  if (status) {
    auto updated = host->surfaceSize(surface.value());
    PH_CHECK(updated.has_value());
    if (updated) {
      PH_CHECK(updated->width == 128u);
      PH_CHECK(updated->height == 256u);
    }
  }

  auto destroyed = host->destroySurface(surface.value());
  PH_CHECK(destroyed.has_value());
}

TEST_SUITE_END();
