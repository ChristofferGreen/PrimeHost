#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.dialogs");

PH_TEST("primehost.dialogs", "file dialog buffer validation") {
  auto hostResult = createHost();
  if (!hostResult) {
    PH_CHECK(hostResult.error().code == HostErrorCode::Unsupported);
    return;
  }
  auto host = std::move(hostResult.value());

  auto caps = host->hostCapabilities();
  PH_CHECK(caps.has_value());
  if (caps) {
    PH_CHECK(caps->supportsFileDialogs);
  }

  FileDialogConfig config{};
  std::span<char> emptyBuffer{};
  auto result = host->fileDialog(config, emptyBuffer);
  PH_CHECK(!result.has_value());
  if (!result.has_value()) {
    PH_CHECK(result.error().code == HostErrorCode::BufferTooSmall);
  }

  FileDialogConfig dirConfig{};
  dirConfig.mode = FileDialogMode::OpenDirectory;
  auto dirResult = host->fileDialog(dirConfig, emptyBuffer);
  PH_CHECK(!dirResult.has_value());
  if (!dirResult.has_value()) {
    PH_CHECK(dirResult.error().code == HostErrorCode::BufferTooSmall);
  }

  std::array<TextSpan, 1> spans{};
  std::array<char, 1> buffer{};
  std::span<TextSpan> emptySpans{};
  auto multi = host->fileDialogPaths(config, emptySpans, buffer);
  PH_CHECK(!multi.has_value());
  if (!multi.has_value()) {
    PH_CHECK(multi.error().code == HostErrorCode::BufferTooSmall);
  }

  std::span<char> emptyBytes{};
  auto multiEmpty = host->fileDialogPaths(config, spans, emptyBytes);
  PH_CHECK(!multiEmpty.has_value());
  if (!multiEmpty.has_value()) {
    PH_CHECK(multiEmpty.error().code == HostErrorCode::BufferTooSmall);
  }

  const char badBytes[] = {static_cast<char>(0xFF)};
  Utf8TextView badView{badBytes, 1u};
  FileDialogConfig badConfig{};
  badConfig.allowedExtensions = std::span<const Utf8TextView>(&badView, 1u);
  std::array<char, 16> okBuffer{};
  auto invalid = host->fileDialogPaths(badConfig, spans, okBuffer);
  PH_CHECK(!invalid.has_value());
  if (!invalid.has_value()) {
    PH_CHECK(invalid.error().code == HostErrorCode::InvalidConfig);
  }

  FileDialogConfig badName{};
  badName.defaultName = badView;
  auto invalidName = host->fileDialogPaths(badName, spans, okBuffer);
  PH_CHECK(!invalidName.has_value());
  if (!invalidName.has_value()) {
    PH_CHECK(invalidName.error().code == HostErrorCode::InvalidConfig);
  }

  FileDialogConfig saveConfig{};
  saveConfig.mode = FileDialogMode::SaveFile;
  saveConfig.canCreateDirectories = false;
  saveConfig.canSelectHiddenFiles = true;
  auto saveResult = host->fileDialog(saveConfig, emptyBuffer);
  PH_CHECK(!saveResult.has_value());
  if (!saveResult.has_value()) {
    PH_CHECK(saveResult.error().code == HostErrorCode::BufferTooSmall);
  }

  FileDialogConfig invalidOpen{};
  invalidOpen.mode = FileDialogMode::Open;
  invalidOpen.allowFiles = false;
  invalidOpen.allowDirectories = false;
  auto invalidOpenResult = host->fileDialogPaths(invalidOpen, spans, okBuffer);
  PH_CHECK(!invalidOpenResult.has_value());
  if (!invalidOpenResult.has_value()) {
    PH_CHECK(invalidOpenResult.error().code == HostErrorCode::InvalidConfig);
  }

  const char dirBytes[] = {'.'};
  Utf8TextView dirExt{dirBytes, 1u};
  FileDialogConfig invalidDir{};
  invalidDir.mode = FileDialogMode::OpenDirectory;
  invalidDir.allowedExtensions = std::span<const Utf8TextView>(&dirExt, 1u);
  auto invalidDirResult = host->fileDialogPaths(invalidDir, spans, okBuffer);
  PH_CHECK(!invalidDirResult.has_value());
  if (!invalidDirResult.has_value()) {
    PH_CHECK(invalidDirResult.error().code == HostErrorCode::InvalidConfig);
  }
}

TEST_SUITE_END();
