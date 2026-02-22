#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.devices");

PH_TEST("primehost.devices", "count query") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());
  auto countResult = host->devices({});
  PH_CHECK(countResult.has_value());
  if (!countResult) {
    return;
  }
  PH_CHECK(countResult.value() >= 2u);

  std::vector<DeviceInfo> devices(countResult.value());
  auto filled = host->devices(devices);
  PH_CHECK(filled.has_value());
  if (filled) {
    PH_CHECK(filled.value() == devices.size());
  }

  if (!devices.empty()) {
    auto info = host->deviceInfo(devices[0].deviceId);
    PH_CHECK(info.has_value());
    if (info) {
      PH_CHECK(info->deviceId == devices[0].deviceId);
      PH_CHECK(info->type == devices[0].type);
    }

    auto caps = host->deviceCapabilities(devices[0].deviceId);
    PH_CHECK(caps.has_value());
    if (caps) {
      PH_CHECK(caps->type == devices[0].type);
    }
  }

  if (countResult.value() > 0u) {
    std::vector<DeviceInfo> tooSmall(countResult.value() - 1u);
    auto tooSmallResult = host->devices(tooSmall);
    PH_CHECK(!tooSmallResult.has_value());
    if (!tooSmallResult.has_value()) {
      PH_CHECK(tooSmallResult.error().code == HostErrorCode::BufferTooSmall);
    }
  }
}

PH_TEST("primehost.devices", "invalid device queries") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto info = host->deviceInfo(99999u);
  PH_CHECK(!info.has_value());
  if (!info.has_value()) {
    PH_CHECK(info.error().code == HostErrorCode::InvalidDevice);
  }

  auto caps = host->deviceCapabilities(99999u);
  PH_CHECK(!caps.has_value());
  if (!caps.has_value()) {
    PH_CHECK(caps.error().code == HostErrorCode::InvalidDevice);
  }
}

PH_TEST("primehost.devices", "pen device capabilities") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto countResult = host->devices({});
  PH_REQUIRE(countResult.has_value());
  std::vector<DeviceInfo> devices(countResult.value());
  auto filled = host->devices(devices);
  PH_REQUIRE(filled.has_value());

  bool foundPen = false;
  for (const auto& device : devices) {
    if (device.type != DeviceType::Pen) {
      continue;
    }
    foundPen = true;
    auto caps = host->deviceCapabilities(device.deviceId);
    PH_CHECK(caps.has_value());
    if (caps) {
      PH_CHECK(caps->hasPressure);
      PH_CHECK(caps->hasTilt);
      PH_CHECK(caps->hasTwist);
    }
  }

  PH_CHECK(foundPen);
}

TEST_SUITE_END();
