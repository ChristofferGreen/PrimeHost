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

  auto helperConfig = directoryDialogConfig("nope");
  auto helperResult = host->fileDialog(helperConfig, emptyBuffer);
  PH_CHECK(!helperResult.has_value());
  if (!helperResult.has_value()) {
    PH_CHECK(helperResult.error().code == HostErrorCode::BufferTooSmall);
  }

  auto openHelper = openFileDialogConfig("nope");
  auto openHelperResult = host->fileDialog(openHelper, emptyBuffer);
  PH_CHECK(!openHelperResult.has_value());
  if (!openHelperResult.has_value()) {
    PH_CHECK(openHelperResult.error().code == HostErrorCode::BufferTooSmall);
  }

  auto saveHelper = saveFileDialogConfig("nope", "file.txt");
  auto saveHelperResult = host->fileDialog(saveHelper, emptyBuffer);
  PH_CHECK(!saveHelperResult.has_value());
  if (!saveHelperResult.has_value()) {
    PH_CHECK(saveHelperResult.error().code == HostErrorCode::BufferTooSmall);
  }

  auto mixedHelper = openMixedDialogConfig("nope");
  auto mixedResult = host->fileDialog(mixedHelper, emptyBuffer);
  PH_CHECK(!mixedResult.has_value());
  if (!mixedResult.has_value()) {
    PH_CHECK(mixedResult.error().code == HostErrorCode::BufferTooSmall);
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

  FileDialogConfig badTypes{};
  badTypes.allowedContentTypes = std::span<const Utf8TextView>(&badView, 1u);
  auto invalidTypes = host->fileDialogPaths(badTypes, spans, okBuffer);
  PH_CHECK(!invalidTypes.has_value());
  if (!invalidTypes.has_value()) {
    PH_CHECK(invalidTypes.error().code == HostErrorCode::InvalidConfig);
  }

  const char typeBytes[] = "public.data";
  Utf8TextView typeView{typeBytes, sizeof(typeBytes) - 1u};
  const char extBytes[] = "txt";
  Utf8TextView extView{extBytes, sizeof(extBytes) - 1u};
  FileDialogConfig bothFilters{};
  bothFilters.allowedContentTypes = std::span<const Utf8TextView>(&typeView, 1u);
  bothFilters.allowedExtensions = std::span<const Utf8TextView>(&extView, 1u);
  auto invalidBoth = host->fileDialogPaths(bothFilters, spans, okBuffer);
  PH_CHECK(!invalidBoth.has_value());
  if (!invalidBoth.has_value()) {
    PH_CHECK(invalidBoth.error().code == HostErrorCode::InvalidConfig);
  }

  FileDialogConfig badName{};
  badName.defaultName = badView;
  auto invalidName = host->fileDialogPaths(badName, spans, okBuffer);
  PH_CHECK(!invalidName.has_value());
  if (!invalidName.has_value()) {
    PH_CHECK(invalidName.error().code == HostErrorCode::InvalidConfig);
  }

  FileDialogConfig badDefaultPath{};
  badDefaultPath.defaultPath = badView;
  badDefaultPath.defaultDirectoryOnly = true;
  auto invalidPath = host->fileDialogPaths(badDefaultPath, spans, okBuffer);
  PH_CHECK(!invalidPath.has_value());
  if (!invalidPath.has_value()) {
    PH_CHECK(invalidPath.error().code == HostErrorCode::InvalidConfig);
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

  FileDialogConfig invalidDirTypes{};
  invalidDirTypes.mode = FileDialogMode::OpenDirectory;
  invalidDirTypes.allowedContentTypes = std::span<const Utf8TextView>(&typeView, 1u);
  auto invalidDirTypesResult = host->fileDialogPaths(invalidDirTypes, spans, okBuffer);
  PH_CHECK(!invalidDirTypesResult.has_value());
  if (!invalidDirTypesResult.has_value()) {
    PH_CHECK(invalidDirTypesResult.error().code == HostErrorCode::InvalidConfig);
  }
}

TEST_SUITE_END();
