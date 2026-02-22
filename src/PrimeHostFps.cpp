#include "PrimeHost/Fps.h"

#include <algorithm>
#include <cmath>

namespace PrimeHost {
namespace {

size_t percentile_index(size_t count, double percentile) {
  if (count == 0u) {
    return 0u;
  }
  double rank = std::ceil(percentile * static_cast<double>(count));
  size_t index = 0u;
  if (rank > 0.0) {
    index = static_cast<size_t>(rank - 1.0);
  }
  if (index >= count) {
    index = count - 1u;
  }
  return index;
}

} // namespace

FpsTracker::FpsTracker(size_t sampleCapacity, std::chrono::nanoseconds reportInterval)
    : sampleCapacity_(sampleCapacity),
      reportInterval_(reportInterval),
      samples_(sampleCapacity) {}

FpsTracker::FpsTracker(std::chrono::nanoseconds reportInterval)
    : FpsTracker(120u, reportInterval) {}

void FpsTracker::framePresented() {
  auto now = std::chrono::steady_clock::now();
  if (!hasLastFrameTime_) {
    lastFrameTime_ = now;
    hasLastFrameTime_ = true;
    return;
  }
  if (sampleCapacity_ == 0u) {
    lastFrameTime_ = now;
    return;
  }
  auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastFrameTime_);
  lastFrameTime_ = now;
  samples_[sampleIndex_] = delta;
  sampleIndex_ = (sampleIndex_ + 1u) % sampleCapacity_;
  if (sampleCount_ < sampleCapacity_) {
    ++sampleCount_;
  }
}

void FpsTracker::reset() {
  sampleCount_ = 0u;
  sampleIndex_ = 0u;
  hasLastFrameTime_ = false;
  hasLastReportTime_ = false;
}

FpsStats FpsTracker::stats() const {
  if (sampleCount_ == 0u || samples_.empty()) {
    return {};
  }
  return computeFpsStats(std::span<const std::chrono::nanoseconds>(samples_.data(), sampleCount_));
}

size_t FpsTracker::sampleCapacity() const {
  return sampleCapacity_;
}

size_t FpsTracker::sampleCount() const {
  return sampleCount_;
}

bool FpsTracker::isWarmedUp() const {
  return sampleCapacity_ > 0u && sampleCount_ >= sampleCapacity_;
}

bool FpsTracker::shouldReport() {
  if (reportInterval_.count() <= 0) {
    return true;
  }
  auto now = std::chrono::steady_clock::now();
  if (!hasLastReportTime_) {
    lastReportTime_ = now;
    hasLastReportTime_ = true;
    return true;
  }
  if (now - lastReportTime_ >= reportInterval_) {
    lastReportTime_ = now;
    return true;
  }
  return false;
}

std::chrono::nanoseconds FpsTracker::reportInterval() const {
  return reportInterval_;
}

void FpsTracker::setReportInterval(std::chrono::nanoseconds interval) {
  reportInterval_ = interval;
  hasLastReportTime_ = false;
}

FpsStats computeFpsStats(std::span<const std::chrono::nanoseconds> frameTimes) {
  FpsStats stats{};
  stats.sampleCount = static_cast<uint32_t>(frameTimes.size());
  if (frameTimes.empty()) {
    return stats;
  }

  auto minTime = frameTimes[0];
  auto maxTime = frameTimes[0];
  long double sum = 0.0L;
  for (const auto& sample : frameTimes) {
    minTime = std::min(minTime, sample);
    maxTime = std::max(maxTime, sample);
    sum += static_cast<long double>(sample.count());
  }

  stats.minFrameTime = minTime;
  stats.maxFrameTime = maxTime;
  long double meanCount = sum / static_cast<long double>(frameTimes.size());
  stats.meanFrameTime = std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(meanCount));
  if (meanCount > 0.0L) {
    long double fps = 1'000'000'000.0L / meanCount;
    stats.fps = static_cast<double>(fps);
  }

  std::vector<std::chrono::nanoseconds> sorted(frameTimes.begin(), frameTimes.end());
  std::sort(sorted.begin(), sorted.end());

  stats.p50FrameTime = sorted[percentile_index(sorted.size(), 0.50)];
  stats.p95FrameTime = sorted[percentile_index(sorted.size(), 0.95)];
  stats.p99FrameTime = sorted[percentile_index(sorted.size(), 0.99)];

  return stats;
}

} // namespace PrimeHost
