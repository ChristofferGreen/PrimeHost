#include "PrimeHost/PrimeHost.h"

#include "tests/unit/test_helpers.h"

using namespace PrimeHost;

TEST_SUITE_BEGIN("primehost.types");

PH_TEST("primehost.types", "surface id validity and equality") {
  SurfaceId invalid{};
  PH_CHECK(!invalid.isValid());

  SurfaceId first{10u};
  SurfaceId second{10u};
  SurfaceId third{11u};

  PH_CHECK(first.isValid());
  PH_CHECK(first == second);
  PH_CHECK(first != third);
}

PH_TEST("primehost.types", "frame config defaults") {
  FrameConfig config{};
  PH_CHECK(config.presentMode == PresentMode::LowLatency);
  PH_CHECK(config.framePolicy == FramePolicy::EventDriven);
  PH_CHECK(config.framePacingSource == FramePacingSource::Platform);
  PH_CHECK(config.colorFormat == ColorFormat::B8G8R8A8_UNORM);
  PH_CHECK(config.vsync);
  PH_CHECK(!config.allowTearing);
  PH_CHECK(config.maxFrameLatency == 1u);
  PH_CHECK(config.bufferCount == 2u);
  PH_CHECK(!config.frameInterval.has_value());
}

PH_TEST("primehost.types", "surface config defaults") {
  SurfaceConfig config{};
  PH_CHECK(config.width == 0u);
  PH_CHECK(config.height == 0u);
  PH_CHECK(config.resizable);
  PH_CHECK(!config.headless);
  PH_CHECK(!config.title.has_value());
}

PH_TEST("primehost.types", "surface config writable") {
  SurfaceConfig config{};
  config.width = 800u;
  config.height = 600u;
  config.resizable = false;
  config.headless = true;
  config.title = std::string("Test");

  PH_CHECK(config.width == 800u);
  PH_CHECK(config.height == 600u);
  PH_CHECK(!config.resizable);
  PH_CHECK(config.headless);
  PH_CHECK(config.title.has_value());
  if (config.title) {
    PH_CHECK(config.title.value() == "Test");
  }
}

PH_TEST("primehost.types", "surface config empty title") {
  SurfaceConfig config{};
  config.title = std::string("");
  PH_CHECK(config.title.has_value());
  PH_CHECK(config.title.value().empty());
}

PH_TEST("primehost.types", "surface config flags") {
  SurfaceConfig config{};
  config.headless = true;
  config.resizable = false;
  PH_CHECK(config.headless);
  PH_CHECK(!config.resizable);
}

PH_TEST("primehost.types", "surface size defaults") {
  SurfaceSize size{};
  PH_CHECK(size.width == 0u);
  PH_CHECK(size.height == 0u);
}

PH_TEST("primehost.types", "surface size writable") {
  SurfaceSize size{};
  size.width = 1024u;
  size.height = 768u;
  PH_CHECK(size.width == 1024u);
  PH_CHECK(size.height == 768u);
}

PH_TEST("primehost.types", "surface point defaults") {
  SurfacePoint point{};
  PH_CHECK(point.x == 0);
  PH_CHECK(point.y == 0);
}

PH_TEST("primehost.types", "surface point writable") {
  SurfacePoint point{};
  point.x = -10;
  point.y = 25;
  PH_CHECK(point.x == -10);
  PH_CHECK(point.y == 25);
}

PH_TEST("primehost.types", "safe area insets defaults") {
  SafeAreaInsets insets{};
  PH_CHECK(insets.top == 0.0f);
  PH_CHECK(insets.left == 0.0f);
  PH_CHECK(insets.right == 0.0f);
  PH_CHECK(insets.bottom == 0.0f);
}

PH_TEST("primehost.types", "host capabilities defaults") {
  HostCapabilities caps{};
  PH_CHECK(!caps.supportsClipboard);
  PH_CHECK(!caps.supportsFileDialogs);
  PH_CHECK(!caps.supportsRelativePointer);
  PH_CHECK(!caps.supportsIme);
  PH_CHECK(!caps.supportsHaptics);
  PH_CHECK(!caps.supportsHeadless);
}

PH_TEST("primehost.types", "host capabilities writable") {
  HostCapabilities caps{};
  caps.supportsClipboard = true;
  caps.supportsFileDialogs = true;
  caps.supportsRelativePointer = true;
  caps.supportsIme = true;
  caps.supportsHaptics = true;
  caps.supportsHeadless = true;

  PH_CHECK(caps.supportsClipboard);
  PH_CHECK(caps.supportsFileDialogs);
  PH_CHECK(caps.supportsRelativePointer);
  PH_CHECK(caps.supportsIme);
  PH_CHECK(caps.supportsHaptics);
  PH_CHECK(caps.supportsHeadless);
}

PH_TEST("primehost.types", "surface capabilities defaults") {
  SurfaceCapabilities caps{};
  PH_CHECK(!caps.supportsVsyncToggle);
  PH_CHECK(!caps.supportsTearing);
  PH_CHECK(caps.minBufferCount == 2u);
  PH_CHECK(caps.maxBufferCount == 2u);
  PH_CHECK(caps.presentModes == 0u);
  PH_CHECK(caps.colorFormats == 0u);
}

PH_TEST("primehost.types", "surface capabilities writable") {
  SurfaceCapabilities caps{};
  caps.supportsVsyncToggle = true;
  caps.supportsTearing = true;
  caps.minBufferCount = 1u;
  caps.maxBufferCount = 3u;
  caps.presentModes = (1u << static_cast<uint32_t>(PresentMode::LowLatency));
  caps.colorFormats = (1u << static_cast<uint32_t>(ColorFormat::B8G8R8A8_UNORM));

  PH_CHECK(caps.supportsVsyncToggle);
  PH_CHECK(caps.supportsTearing);
  PH_CHECK(caps.minBufferCount == 1u);
  PH_CHECK(caps.maxBufferCount == 3u);
  PH_CHECK(caps.presentModes != 0u);
  PH_CHECK(caps.colorFormats != 0u);
}

PH_TEST("primehost.types", "device capabilities defaults") {
  DeviceCapabilities caps{};
  PH_CHECK(caps.type == DeviceType::Mouse);
  PH_CHECK(!caps.hasPressure);
  PH_CHECK(!caps.hasTilt);
  PH_CHECK(!caps.hasTwist);
  PH_CHECK(!caps.hasDistance);
  PH_CHECK(!caps.hasRumble);
  PH_CHECK(!caps.hasAnalogButtons);
  PH_CHECK(caps.maxTouches == 0u);
}

PH_TEST("primehost.types", "device capabilities writable") {
  DeviceCapabilities caps{};
  caps.type = DeviceType::Pen;
  caps.hasPressure = true;
  caps.hasTilt = true;
  caps.hasTwist = true;
  caps.hasDistance = true;
  caps.hasRumble = true;
  caps.hasAnalogButtons = true;
  caps.maxTouches = 5u;

  PH_CHECK(caps.type == DeviceType::Pen);
  PH_CHECK(caps.hasPressure);
  PH_CHECK(caps.hasTilt);
  PH_CHECK(caps.hasTwist);
  PH_CHECK(caps.hasDistance);
  PH_CHECK(caps.hasRumble);
  PH_CHECK(caps.hasAnalogButtons);
  PH_CHECK(caps.maxTouches == 5u);
}

PH_TEST("primehost.types", "device info defaults") {
  DeviceInfo info{};
  PH_CHECK(info.deviceId == 0u);
  PH_CHECK(info.type == DeviceType::Mouse);
  PH_CHECK(info.vendorId == 0u);
  PH_CHECK(info.productId == 0u);
  PH_CHECK(info.name.empty());
}

PH_TEST("primehost.types", "device info with name") {
  DeviceInfo info{};
  info.deviceId = 3u;
  info.type = DeviceType::Gamepad;
  info.vendorId = 12u;
  info.productId = 34u;
  info.name = "Test Device";

  PH_CHECK(info.deviceId == 3u);
  PH_CHECK(info.type == DeviceType::Gamepad);
  PH_CHECK(info.vendorId == 12u);
  PH_CHECK(info.productId == 34u);
  PH_CHECK(info.name == "Test Device");
}

PH_TEST("primehost.types", "display info defaults") {
  DisplayInfo info{};
  PH_CHECK(info.displayId == 0u);
  PH_CHECK(info.x == 0);
  PH_CHECK(info.y == 0);
  PH_CHECK(info.width == 0u);
  PH_CHECK(info.height == 0u);
  PH_CHECK(info.scale == 1.0f);
  PH_CHECK(info.refreshRate == 0.0f);
  PH_CHECK(!info.isPrimary);
}

PH_TEST("primehost.types", "display info writable") {
  DisplayInfo info{};
  info.displayId = 2u;
  info.x = 10;
  info.y = 20;
  info.width = 1920u;
  info.height = 1080u;
  info.scale = 2.0f;
  info.refreshRate = 60.0f;
  info.isPrimary = true;

  PH_CHECK(info.displayId == 2u);
  PH_CHECK(info.x == 10);
  PH_CHECK(info.y == 20);
  PH_CHECK(info.width == 1920u);
  PH_CHECK(info.height == 1080u);
  PH_CHECK(info.scale == 2.0f);
  PH_CHECK(info.refreshRate == 60.0f);
  PH_CHECK(info.isPrimary);
}

PH_TEST("primehost.types", "display hdr info defaults") {
  DisplayHdrInfo info{};
  PH_CHECK(!info.supportsHdr);
  PH_CHECK(info.maxEdr == 1.0f);
  PH_CHECK(info.maxEdrPotential == 1.0f);
}

PH_TEST("primehost.types", "display hdr info writable") {
  DisplayHdrInfo info{};
  info.supportsHdr = true;
  info.maxEdr = 2.5f;
  info.maxEdrPotential = 3.0f;

  PH_CHECK(info.supportsHdr);
  PH_CHECK(info.maxEdr == 2.5f);
  PH_CHECK(info.maxEdrPotential == 3.0f);
}

PH_TEST("primehost.types", "audio device info defaults") {
  AudioDeviceInfo info{};
  PH_CHECK(info.id == AudioDeviceId{});
  PH_CHECK(info.name.empty());
  PH_CHECK(!info.isDefault);
  PH_CHECK(info.preferredFormat.sampleRate == 48000u);
  PH_CHECK(info.preferredFormat.channels == 2u);
  PH_CHECK(info.preferredFormat.format == SampleFormat::Float32);
  PH_CHECK(info.preferredFormat.interleaved);
}

PH_TEST("primehost.types", "audio device info writable") {
  AudioDeviceInfo info{};
  info.id = 42u;
  info.name = "Audio Device";
  info.isDefault = true;
  info.preferredFormat.sampleRate = 44100u;
  info.preferredFormat.channels = 1u;
  info.preferredFormat.format = SampleFormat::Int16;
  info.preferredFormat.interleaved = false;

  PH_CHECK(info.id == 42u);
  PH_CHECK(info.name == "Audio Device");
  PH_CHECK(info.isDefault);
  PH_CHECK(info.preferredFormat.sampleRate == 44100u);
  PH_CHECK(info.preferredFormat.channels == 1u);
  PH_CHECK(info.preferredFormat.format == SampleFormat::Int16);
  PH_CHECK(!info.preferredFormat.interleaved);
}

PH_TEST("primehost.types", "audio device event defaults") {
  AudioDeviceEvent event{};
  PH_CHECK(event.deviceId == AudioDeviceId{});
  PH_CHECK(event.connected);
  PH_CHECK(!event.isDefault);
}

PH_TEST("primehost.types", "audio device event writable") {
  AudioDeviceEvent event{};
  event.deviceId = 7u;
  event.connected = false;
  event.isDefault = true;

  PH_CHECK(event.deviceId == 7u);
  PH_CHECK(!event.connected);
  PH_CHECK(event.isDefault);
}

PH_TEST("primehost.types", "audio callback context defaults") {
  AudioCallbackContext ctx{};
  PH_CHECK(ctx.frameIndex == 0u);
  PH_CHECK(ctx.time.time_since_epoch() == std::chrono::steady_clock::duration::zero());
  PH_CHECK(ctx.requestedFrames == 0u);
  PH_CHECK(!ctx.isUnderrun);
}

PH_TEST("primehost.types", "audio callback context writable") {
  AudioCallbackContext ctx{};
  auto now = std::chrono::steady_clock::now();
  ctx.frameIndex = 12u;
  ctx.time = now;
  ctx.requestedFrames = 256u;
  ctx.isUnderrun = true;

  PH_CHECK(ctx.frameIndex == 12u);
  PH_CHECK(ctx.time == now);
  PH_CHECK(ctx.requestedFrames == 256u);
  PH_CHECK(ctx.isUnderrun);
}

PH_TEST("primehost.types", "audio format defaults") {
  AudioFormat format{};
  PH_CHECK(format.sampleRate == 48000u);
  PH_CHECK(format.channels == 2u);
  PH_CHECK(format.format == SampleFormat::Float32);
  PH_CHECK(format.interleaved);
}

PH_TEST("primehost.types", "audio format writable") {
  AudioFormat format{};
  format.sampleRate = 44100u;
  format.channels = 1u;
  format.format = SampleFormat::Int16;
  format.interleaved = false;

  PH_CHECK(format.sampleRate == 44100u);
  PH_CHECK(format.channels == 1u);
  PH_CHECK(format.format == SampleFormat::Int16);
  PH_CHECK(!format.interleaved);
}

PH_TEST("primehost.types", "audio stream config defaults") {
  AudioStreamConfig config{};
  PH_CHECK(config.bufferFrames == 512u);
  PH_CHECK(config.periodFrames == 256u);
  PH_CHECK(config.targetLatency == std::chrono::nanoseconds(0));
  PH_CHECK(config.format.sampleRate == 48000u);
  PH_CHECK(config.format.channels == 2u);
  PH_CHECK(config.format.format == SampleFormat::Float32);
  PH_CHECK(config.format.interleaved);
}

PH_TEST("primehost.types", "audio stream config writable") {
  AudioStreamConfig config{};
  config.format.sampleRate = 44100u;
  config.format.channels = 1u;
  config.format.format = SampleFormat::Int16;
  config.format.interleaved = false;
  config.bufferFrames = 1024u;
  config.periodFrames = 512u;
  config.targetLatency = std::chrono::milliseconds(5);

  PH_CHECK(config.format.sampleRate == 44100u);
  PH_CHECK(config.format.channels == 1u);
  PH_CHECK(config.format.format == SampleFormat::Int16);
  PH_CHECK(!config.format.interleaved);
  PH_CHECK(config.bufferFrames == 1024u);
  PH_CHECK(config.periodFrames == 512u);
  PH_CHECK(config.targetLatency == std::chrono::milliseconds(5));
}

PH_TEST("primehost.types", "frame timing defaults") {
  FrameTiming timing{};
  PH_CHECK(timing.time.time_since_epoch() == std::chrono::steady_clock::duration::zero());
  PH_CHECK(timing.delta == std::chrono::nanoseconds(0));
  PH_CHECK(timing.frameIndex == 0u);
}

PH_TEST("primehost.types", "frame diagnostics defaults") {
  FrameDiagnostics diag{};
  PH_CHECK(diag.targetInterval == std::chrono::nanoseconds(0));
  PH_CHECK(diag.actualInterval == std::chrono::nanoseconds(0));
  PH_CHECK(!diag.missedDeadline);
  PH_CHECK(!diag.wasThrottled);
  PH_CHECK(diag.droppedFrames == 0u);
}

PH_TEST("primehost.types", "locale info defaults") {
  LocaleInfo info{};
  PH_CHECK(info.languageTag.empty());
  PH_CHECK(info.regionTag.empty());
}

PH_TEST("primehost.types", "locale info with tags") {
  LocaleInfo info{};
  info.languageTag = "en-US";
  info.regionTag = "US";

  PH_CHECK(info.languageTag == "en-US");
  PH_CHECK(info.regionTag == "US");
}

PH_TEST("primehost.types", "image size defaults") {
  ImageSize size{};
  PH_CHECK(size.width == 0u);
  PH_CHECK(size.height == 0u);
}

PH_TEST("primehost.types", "image data defaults") {
  ImageData data{};
  PH_CHECK(data.size.width == 0u);
  PH_CHECK(data.size.height == 0u);
  PH_CHECK(data.pixels.empty());
}

PH_TEST("primehost.types", "image data with pixels") {
  std::array<uint8_t, 4> pixels{10u, 20u, 30u, 40u};
  ImageData data{};
  data.size.width = 1u;
  data.size.height = 1u;
  data.pixels = pixels;

  PH_CHECK(data.size.width == 1u);
  PH_CHECK(data.size.height == 1u);
  PH_CHECK(data.pixels.size() == pixels.size());
}

PH_TEST("primehost.types", "image data zero size with pixels") {
  std::array<uint8_t, 4> pixels{1u, 2u, 3u, 4u};
  ImageData data{};
  data.size.width = 0u;
  data.size.height = 0u;
  data.pixels = pixels;

  PH_CHECK(data.size.width == 0u);
  PH_CHECK(data.size.height == 0u);
  PH_CHECK(data.pixels.size() == pixels.size());
}

PH_TEST("primehost.types", "cursor image defaults") {
  CursorImage image{};
  PH_CHECK(image.width == 0u);
  PH_CHECK(image.height == 0u);
  PH_CHECK(image.hotX == 0);
  PH_CHECK(image.hotY == 0);
  PH_CHECK(image.pixels.empty());
}

PH_TEST("primehost.types", "cursor image with pixels") {
  std::array<uint8_t, 4> pixels{1u, 2u, 3u, 4u};
  CursorImage image{};
  image.width = 1u;
  image.height = 1u;
  image.hotX = 0;
  image.hotY = 0;
  image.pixels = pixels;

  PH_CHECK(image.pixels.size() == pixels.size());
  PH_CHECK(image.width == 1u);
  PH_CHECK(image.height == 1u);
}

PH_TEST("primehost.types", "cursor image with hotspot") {
  std::array<uint8_t, 4> pixels{9u, 8u, 7u, 6u};
  CursorImage image{};
  image.width = 1u;
  image.height = 1u;
  image.hotX = 3;
  image.hotY = -2;
  image.pixels = pixels;

  PH_CHECK(image.hotX == 3);
  PH_CHECK(image.hotY == -2);
  PH_CHECK(image.pixels.size() == pixels.size());
}

PH_TEST("primehost.types", "cursor image zero size with pixels") {
  std::array<uint8_t, 4> pixels{11u, 12u, 13u, 14u};
  CursorImage image{};
  image.width = 0u;
  image.height = 0u;
  image.pixels = pixels;

  PH_CHECK(image.width == 0u);
  PH_CHECK(image.height == 0u);
  PH_CHECK(image.pixels.size() == pixels.size());
}

PH_TEST("primehost.types", "frame buffer defaults") {
  FrameBuffer buffer{};
  PH_CHECK(buffer.size.width == 0u);
  PH_CHECK(buffer.size.height == 0u);
  PH_CHECK(buffer.stride == 0u);
  PH_CHECK(buffer.colorFormat == ColorFormat::B8G8R8A8_UNORM);
  PH_CHECK(buffer.scale == 1.0f);
  PH_CHECK(buffer.bufferIndex == 0u);
  PH_CHECK(buffer.pixels.empty());
}

PH_TEST("primehost.types", "frame buffer with pixels") {
  std::array<uint8_t, 8> pixels{1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
  FrameBuffer buffer{};
  buffer.size.width = 2u;
  buffer.size.height = 1u;
  buffer.stride = 8u;
  buffer.scale = 2.0f;
  buffer.bufferIndex = 3u;
  buffer.pixels = pixels;

  PH_CHECK(buffer.size.width == 2u);
  PH_CHECK(buffer.size.height == 1u);
  PH_CHECK(buffer.stride == 8u);
  PH_CHECK(buffer.scale == 2.0f);
  PH_CHECK(buffer.bufferIndex == 3u);
  PH_CHECK(buffer.pixels.size() == pixels.size());
}

PH_TEST("primehost.types", "frame buffer zero size with pixels") {
  std::array<uint8_t, 4> pixels{9u, 8u, 7u, 6u};
  FrameBuffer buffer{};
  buffer.size.width = 0u;
  buffer.size.height = 0u;
  buffer.pixels = pixels;

  PH_CHECK(buffer.size.width == 0u);
  PH_CHECK(buffer.size.height == 0u);
  PH_CHECK(buffer.pixels.size() == pixels.size());
}

PH_TEST("primehost.types", "icon image defaults") {
  IconImage icon{};
  PH_CHECK(icon.size.width == 0u);
  PH_CHECK(icon.size.height == 0u);
  PH_CHECK(icon.pixels.empty());
}

PH_TEST("primehost.types", "icon image with pixels") {
  std::array<uint8_t, 4> pixels{7u, 8u, 9u, 10u};
  IconImage icon{};
  icon.size.width = 1u;
  icon.size.height = 1u;
  icon.pixels = pixels;

  PH_CHECK(icon.size.width == 1u);
  PH_CHECK(icon.size.height == 1u);
  PH_CHECK(icon.pixels.size() == pixels.size());
}

PH_TEST("primehost.types", "window icon defaults") {
  WindowIcon icon{};
  PH_CHECK(icon.images.empty());
}

PH_TEST("primehost.types", "window icon with images") {
  std::array<uint8_t, 4> pixels{255u, 0u, 0u, 255u};
  IconImage image{};
  image.size.width = 1u;
  image.size.height = 1u;
  image.pixels = pixels;

  std::array<IconImage, 1> images{image};
  WindowIcon icon{};
  icon.images = images;

  PH_CHECK(!icon.images.empty());
  PH_CHECK(icon.images.size() == 1u);
  PH_CHECK(icon.images[0].size.width == 1u);
  PH_CHECK(icon.images[0].size.height == 1u);
  PH_CHECK(icon.images[0].pixels.size() == pixels.size());
}

PH_TEST("primehost.types", "window icon with multiple images") {
  std::array<uint8_t, 4> pixelsA{1u, 2u, 3u, 4u};
  std::array<uint8_t, 4> pixelsB{5u, 6u, 7u, 8u};

  IconImage first{};
  first.size.width = 1u;
  first.size.height = 1u;
  first.pixels = pixelsA;

  IconImage second{};
  second.size.width = 1u;
  second.size.height = 1u;
  second.pixels = pixelsB;

  std::array<IconImage, 2> images{first, second};
  WindowIcon icon{};
  icon.images = images;

  PH_CHECK(icon.images.size() == 2u);
  PH_CHECK(icon.images[0].pixels.size() == pixelsA.size());
  PH_CHECK(icon.images[1].pixels.size() == pixelsB.size());
}

PH_TEST("primehost.types", "window icon with different sizes") {
  std::array<uint8_t, 4> pixelsA{1u, 1u, 1u, 1u};
  std::array<uint8_t, 16> pixelsB{
      2u, 2u, 2u, 2u,
      3u, 3u, 3u, 3u,
      4u, 4u, 4u, 4u,
      5u, 5u, 5u, 5u,
  };

  IconImage first{};
  first.size.width = 1u;
  first.size.height = 1u;
  first.pixels = pixelsA;

  IconImage second{};
  second.size.width = 2u;
  second.size.height = 2u;
  second.pixels = pixelsB;

  std::array<IconImage, 2> images{first, second};
  WindowIcon icon{};
  icon.images = images;

  PH_CHECK(icon.images[0].size.width == 1u);
  PH_CHECK(icon.images[0].size.height == 1u);
  PH_CHECK(icon.images[1].size.width == 2u);
  PH_CHECK(icon.images[1].size.height == 2u);
  PH_CHECK(icon.images[1].pixels.size() == pixelsB.size());
}

PH_TEST("primehost.types", "file dialog config defaults") {
  FileDialogConfig config{};
  PH_CHECK(config.mode == FileDialogMode::OpenFile);
  PH_CHECK(!config.title.has_value());
  PH_CHECK(!config.defaultPath.has_value());
  PH_CHECK(!config.defaultName.has_value());
  PH_CHECK(config.allowedExtensions.empty());
  PH_CHECK(config.allowedContentTypes.empty());
  PH_CHECK(config.canCreateDirectories);
  PH_CHECK(!config.canSelectHiddenFiles);
  PH_CHECK(!config.allowFiles.has_value());
  PH_CHECK(!config.allowDirectories.has_value());
  PH_CHECK(!config.defaultDirectoryOnly);
}

PH_TEST("primehost.types", "file dialog helper defaults") {
  auto directory = directoryDialogConfig();
  PH_CHECK(directory.mode == FileDialogMode::OpenDirectory);
  PH_CHECK(!directory.defaultPath.has_value());
  PH_CHECK(!directory.defaultDirectoryOnly);

  auto openFile = openFileDialogConfig();
  PH_CHECK(openFile.mode == FileDialogMode::OpenFile);
  PH_CHECK(!openFile.defaultPath.has_value());

  auto saveFile = saveFileDialogConfig();
  PH_CHECK(saveFile.mode == FileDialogMode::SaveFile);
  PH_CHECK(!saveFile.defaultPath.has_value());
  PH_CHECK(!saveFile.defaultName.has_value());

  auto openMixed = openMixedDialogConfig();
  PH_CHECK(openMixed.mode == FileDialogMode::Open);
  PH_CHECK(openMixed.allowFiles.has_value());
  PH_CHECK(openMixed.allowDirectories.has_value());
  if (openMixed.allowFiles) {
    PH_CHECK(openMixed.allowFiles.value());
  }
  if (openMixed.allowDirectories) {
    PH_CHECK(openMixed.allowDirectories.value());
  }
  PH_CHECK(!openMixed.defaultPath.has_value());
}

PH_TEST("primehost.types", "file dialog helper with defaults") {
  auto directory = directoryDialogConfig("path");
  PH_CHECK(directory.defaultPath.has_value());
  PH_CHECK(directory.defaultPath.value() == "path");
  PH_CHECK(directory.defaultDirectoryOnly);

  auto openFile = openFileDialogConfig("open");
  PH_CHECK(openFile.defaultPath.has_value());
  PH_CHECK(openFile.defaultPath.value() == "open");

  auto saveFile = saveFileDialogConfig("save", "name");
  PH_CHECK(saveFile.defaultPath.has_value());
  PH_CHECK(saveFile.defaultPath.value() == "save");
  PH_CHECK(saveFile.defaultName.has_value());
  PH_CHECK(saveFile.defaultName.value() == "name");

  auto openMixed = openMixedDialogConfig("mixed");
  PH_CHECK(openMixed.defaultPath.has_value());
  PH_CHECK(openMixed.defaultPath.value() == "mixed");
}

PH_TEST("primehost.types", "file dialog result defaults") {
  FileDialogResult result{};
  PH_CHECK(!result.accepted);
  PH_CHECK(result.path.empty());
}

PH_TEST("primehost.types", "text span defaults") {
  TextSpan span{};
  PH_CHECK(span.offset == 0u);
  PH_CHECK(span.length == 0u);
}

PH_TEST("primehost.types", "clipboard paths result defaults") {
  ClipboardPathsResult result{};
  PH_CHECK(!result.available);
  PH_CHECK(result.paths.empty());
}

PH_TEST("primehost.types", "clipboard paths result with spans") {
  std::array<TextSpan, 2> spans{TextSpan{1u, 3u}, TextSpan{4u, 2u}};
  ClipboardPathsResult result{};
  result.available = true;
  result.paths = spans;

  PH_CHECK(result.available);
  PH_CHECK(result.paths.size() == 2u);
  PH_CHECK(result.paths[0].offset == 1u);
  PH_CHECK(result.paths[0].length == 3u);
  PH_CHECK(result.paths[1].offset == 4u);
  PH_CHECK(result.paths[1].length == 2u);
}

PH_TEST("primehost.types", "clipboard image result defaults") {
  ClipboardImageResult result{};
  PH_CHECK(!result.available);
  PH_CHECK(result.size.width == 0u);
  PH_CHECK(result.size.height == 0u);
  PH_CHECK(result.pixels.empty());
}

PH_TEST("primehost.types", "clipboard image result with pixels") {
  std::array<uint8_t, 4> pixels{1u, 2u, 3u, 4u};
  ClipboardImageResult result{};
  result.available = true;
  result.size.width = 1u;
  result.size.height = 1u;
  result.pixels = pixels;

  PH_CHECK(result.available);
  PH_CHECK(result.size.width == 1u);
  PH_CHECK(result.size.height == 1u);
  PH_CHECK(result.pixels.size() == pixels.size());
}

PH_TEST("primehost.types", "screenshot config defaults") {
  ScreenshotConfig config{};
  PH_CHECK(config.scope == ScreenshotScope::Surface);
  PH_CHECK(!config.includeHidden);
}

PH_TEST("primehost.types", "screenshot config writable") {
  ScreenshotConfig config{};
  config.scope = ScreenshotScope::Window;
  config.includeHidden = true;
  PH_CHECK(config.scope == ScreenshotScope::Window);
  PH_CHECK(config.includeHidden);
}

PH_TEST("primehost.types", "audio callbacks defaults") {
  AudioCallbacks callbacks{};
  PH_CHECK(!callbacks.onDeviceEvent);
}

PH_TEST("primehost.types", "audio callbacks writable") {
  AudioCallbacks callbacks{};
  callbacks.onDeviceEvent = [](const AudioDeviceEvent&) {};
  PH_CHECK(static_cast<bool>(callbacks.onDeviceEvent));
}

PH_TEST("primehost.types", "callbacks defaults") {
  Callbacks callbacks{};
  PH_CHECK(!callbacks.onEvents);
  PH_CHECK(!callbacks.onFrame);
}

PH_TEST("primehost.types", "callbacks writable") {
  Callbacks callbacks{};
  callbacks.onEvents = [](const EventBatch&) {};
  callbacks.onFrame = [](SurfaceId, const FrameTiming&, const FrameDiagnostics&) {};
  PH_CHECK(static_cast<bool>(callbacks.onEvents));
  PH_CHECK(static_cast<bool>(callbacks.onFrame));
}

PH_TEST("primehost.types", "gamepad rumble defaults") {
  GamepadRumble rumble{};
  PH_CHECK(rumble.deviceId == 0u);
  PH_CHECK(rumble.lowFrequency == 0.0f);
  PH_CHECK(rumble.highFrequency == 0.0f);
  PH_CHECK(rumble.duration == std::chrono::milliseconds(0));
}

PH_TEST("primehost.types", "gamepad rumble writable") {
  GamepadRumble rumble{};
  rumble.deviceId = 5u;
  rumble.lowFrequency = 0.25f;
  rumble.highFrequency = 0.75f;
  rumble.duration = std::chrono::milliseconds(250);

  PH_CHECK(rumble.deviceId == 5u);
  PH_CHECK(rumble.lowFrequency == 0.25f);
  PH_CHECK(rumble.highFrequency == 0.75f);
  PH_CHECK(rumble.duration == std::chrono::milliseconds(250));
}

PH_TEST("primehost.types", "gamepad button event defaults") {
  GamepadButtonEvent event{};
  PH_CHECK(event.deviceId == 0u);
  PH_CHECK(event.controlId == 0u);
  PH_CHECK(!event.pressed);
  PH_CHECK(!event.value.has_value());
}

PH_TEST("primehost.types", "gamepad button event writable") {
  GamepadButtonEvent event{};
  event.deviceId = 2u;
  event.controlId = static_cast<uint32_t>(GamepadButtonId::South);
  event.pressed = true;
  event.value = 0.5f;
  PH_CHECK(event.deviceId == 2u);
  PH_CHECK(event.controlId == static_cast<uint32_t>(GamepadButtonId::South));
  PH_CHECK(event.pressed);
  PH_CHECK(event.value.has_value());
  PH_CHECK(event.value.value() == 0.5f);
}

PH_TEST("primehost.types", "gamepad axis event defaults") {
  GamepadAxisEvent event{};
  PH_CHECK(event.deviceId == 0u);
  PH_CHECK(event.controlId == 0u);
  PH_CHECK(event.value == 0.0f);
}

PH_TEST("primehost.types", "gamepad axis event writable") {
  GamepadAxisEvent event{};
  event.deviceId = 3u;
  event.controlId = static_cast<uint32_t>(GamepadAxisId::LeftX);
  event.value = -0.75f;
  PH_CHECK(event.deviceId == 3u);
  PH_CHECK(event.controlId == static_cast<uint32_t>(GamepadAxisId::LeftX));
  PH_CHECK(event.value == -0.75f);
}

PH_TEST("primehost.types", "device event defaults") {
  DeviceEvent event{};
  PH_CHECK(event.deviceId == 0u);
  PH_CHECK(event.deviceType == DeviceType::Mouse);
  PH_CHECK(event.connected);
}

PH_TEST("primehost.types", "device event writable") {
  DeviceEvent event{};
  event.deviceId = 9u;
  event.deviceType = DeviceType::Gamepad;
  event.connected = false;
  PH_CHECK(event.deviceId == 9u);
  PH_CHECK(event.deviceType == DeviceType::Gamepad);
  PH_CHECK(!event.connected);
}

PH_TEST("primehost.types", "pointer event writable") {
  PointerEvent event{};
  event.deviceId = 1u;
  event.pointerId = 2u;
  event.deviceType = PointerDeviceType::Pen;
  event.phase = PointerPhase::Down;
  event.x = 12;
  event.y = -4;
  event.deltaX = 3;
  event.deltaY = -2;
  event.pressure = 0.4f;
  event.tiltX = 0.1f;
  event.tiltY = -0.2f;
  event.twist = 5.0f;
  event.distance = 0.8f;
  event.buttonMask = 2u;
  event.isPrimary = false;

  PH_CHECK(event.deviceId == 1u);
  PH_CHECK(event.pointerId == 2u);
  PH_CHECK(event.deviceType == PointerDeviceType::Pen);
  PH_CHECK(event.phase == PointerPhase::Down);
  PH_CHECK(event.x == 12);
  PH_CHECK(event.y == -4);
  PH_CHECK(event.deltaX.has_value());
  PH_CHECK(event.deltaX.value() == 3);
  PH_CHECK(event.deltaY.has_value());
  PH_CHECK(event.deltaY.value() == -2);
  PH_CHECK(event.pressure.has_value());
  PH_CHECK(event.pressure.value() == 0.4f);
  PH_CHECK(event.tiltX.has_value());
  PH_CHECK(event.tiltX.value() == 0.1f);
  PH_CHECK(event.tiltY.has_value());
  PH_CHECK(event.tiltY.value() == -0.2f);
  PH_CHECK(event.twist.has_value());
  PH_CHECK(event.twist.value() == 5.0f);
  PH_CHECK(event.distance.has_value());
  PH_CHECK(event.distance.value() == 0.8f);
  PH_CHECK(event.buttonMask == 2u);
  PH_CHECK(!event.isPrimary);
}

PH_TEST("primehost.types", "key event writable") {
  KeyEvent event{};
  event.deviceId = 3u;
  event.keyCode = 42u;
  event.modifiers = 7u;
  event.pressed = true;
  event.repeat = true;

  PH_CHECK(event.deviceId == 3u);
  PH_CHECK(event.keyCode == 42u);
  PH_CHECK(event.modifiers == 7u);
  PH_CHECK(event.pressed);
  PH_CHECK(event.repeat);
}

PH_TEST("primehost.types", "text event writable") {
  TextEvent event{};
  event.deviceId = 4u;
  event.text = TextSpan{2u, 5u};
  PH_CHECK(event.deviceId == 4u);
  PH_CHECK(event.text.offset == 2u);
  PH_CHECK(event.text.length == 5u);
}

PH_TEST("primehost.types", "scroll event writable") {
  ScrollEvent event{};
  event.deviceId = 5u;
  event.deltaX = 1.5f;
  event.deltaY = -2.5f;
  event.isLines = true;
  PH_CHECK(event.deviceId == 5u);
  PH_CHECK(event.deltaX == 1.5f);
  PH_CHECK(event.deltaY == -2.5f);
  PH_CHECK(event.isLines);
}

PH_TEST("primehost.types", "host error defaults") {
  HostError error{};
  PH_CHECK(error.code == HostErrorCode::Unknown);
}

PH_TEST("primehost.types", "host status ok and error") {
  HostStatus okStatus{};
  PH_CHECK(okStatus.has_value());

  HostStatus errorStatus = std::unexpected(HostError{HostErrorCode::InvalidConfig});
  PH_CHECK(!errorStatus.has_value());
  if (!errorStatus.has_value()) {
    PH_CHECK(errorStatus.error().code == HostErrorCode::InvalidConfig);
  }
}

PH_TEST("primehost.types", "host result ok and error") {
  HostResult<int> okResult{42};
  PH_CHECK(okResult.has_value());
  if (okResult.has_value()) {
    PH_CHECK(okResult.value() == 42);
  }

  HostResult<int> errorResult = std::unexpected(HostError{HostErrorCode::InvalidSurface});
  PH_CHECK(!errorResult.has_value());
  if (!errorResult.has_value()) {
    PH_CHECK(errorResult.error().code == HostErrorCode::InvalidSurface);
  }
}

TEST_SUITE_END();
