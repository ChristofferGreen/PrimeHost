#pragma once

#include <chrono>
#include <cstddef>
#include <span>

namespace PrimeHost {

struct FpsStats {
  uint32_t sampleCount = 0u;
  std::chrono::nanoseconds minFrameTime{0};
  std::chrono::nanoseconds maxFrameTime{0};
  std::chrono::nanoseconds meanFrameTime{0};
  std::chrono::nanoseconds p50FrameTime{0};
  std::chrono::nanoseconds p95FrameTime{0};
  std::chrono::nanoseconds p99FrameTime{0};
  double fps = 0.0;
};

class FpsTracker {
public:
  explicit FpsTracker(
      size_t sampleCapacity = 120u,
      std::chrono::nanoseconds reportInterval = std::chrono::seconds(1));
  explicit FpsTracker(std::chrono::nanoseconds reportInterval);

  void framePresented();
  void reset();

  FpsStats stats() const;
  size_t sampleCapacity() const;
  size_t sampleCount() const;
  bool isWarmedUp() const;
  bool shouldReport();
  std::chrono::nanoseconds reportInterval() const;
  void setReportInterval(std::chrono::nanoseconds interval);

private:
  size_t sampleCapacity_ = 0u;
  size_t sampleCount_ = 0u;
  std::chrono::nanoseconds reportInterval_{0};
};

FpsStats computeFpsStats(std::span<const std::chrono::nanoseconds> frameTimes);

} // namespace PrimeHost
