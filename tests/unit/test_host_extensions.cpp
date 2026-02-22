#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.extensions");

PH_TEST("primehost.extensions", "permission and locale queries") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto permission = host->checkPermission(PermissionType::Camera);
  if (!permission) {
    PH_CHECK(permission.error().code == HostErrorCode::Unsupported);
  }

  auto request = host->requestPermission(PermissionType::Camera);
  if (!request) {
    PH_CHECK(request.error().code == HostErrorCode::Unsupported);
  }

  auto locale = host->localeInfo();
  PH_CHECK(locale.has_value());

  auto ime = host->imeLanguageTag();
  PH_CHECK(ime.has_value());
}

PH_TEST("primehost.extensions", "idle sleep inhibit") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto token = host->beginIdleSleepInhibit("PrimeHost Tests");
  if (!token) {
    bool allowed = token.error().code == HostErrorCode::Unsupported;
    allowed = allowed || token.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
    return;
  }

  PH_CHECK(token.value() != 0u);
  auto end = host->endIdleSleepInhibit(token.value());
  PH_CHECK(end.has_value());

  auto invalid = host->endIdleSleepInhibit(token.value());
  PH_CHECK(!invalid.has_value());
  if (!invalid.has_value()) {
    PH_CHECK(invalid.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.extensions", "background and tray unsupported") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto background = host->beginBackgroundTask("PrimeHost Tests");
  if (!background.has_value()) {
    bool allowed = background.error().code == HostErrorCode::Unsupported;
    allowed = allowed || background.error().code == HostErrorCode::PlatformFailure;
    PH_CHECK(allowed);
  } else {
    auto end = host->endBackgroundTask(background.value());
    PH_CHECK(end.has_value());

    auto endAgain = host->endBackgroundTask(background.value());
    PH_CHECK(!endAgain.has_value());
    if (!endAgain.has_value()) {
      PH_CHECK(endAgain.error().code == HostErrorCode::InvalidConfig);
    }
  }

  auto tray = host->createTrayItem("PrimeHost");
  if (!tray.has_value()) {
    bool allowed = tray.error().code == HostErrorCode::PlatformFailure;
    allowed = allowed || tray.error().code == HostErrorCode::Unsupported;
    PH_CHECK(allowed);
    return;
  }

  auto update = host->updateTrayItemTitle(tray.value(), "PrimeHost 2");
  PH_CHECK(update.has_value());

  auto remove = host->removeTrayItem(tray.value());
  PH_CHECK(remove.has_value());

  auto removeAgain = host->removeTrayItem(tray.value());
  PH_CHECK(!removeAgain.has_value());
  if (!removeAgain.has_value()) {
    PH_CHECK(removeAgain.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.extensions", "gamepad light invalid device") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto status = host->setGamepadLight(999u, 1.0f, 1.0f, 1.0f);
  PH_CHECK(!status.has_value());
  if (!status.has_value()) {
    bool allowed = status.error().code == HostErrorCode::InvalidDevice;
    allowed = allowed || status.error().code == HostErrorCode::Unsupported;
    PH_CHECK(allowed);
  }
}

TEST_SUITE_END();
