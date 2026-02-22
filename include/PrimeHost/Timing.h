#pragma once

#include <chrono>
#include <thread>

namespace PrimeHost {

using SteadyClock = std::chrono::steady_clock;

inline SteadyClock::time_point now() {
  return SteadyClock::now();
}

inline void sleepFor(std::chrono::nanoseconds duration) {
  if (duration.count() <= 0) {
    return;
  }
  std::this_thread::sleep_for(duration);
}

inline void sleepUntil(SteadyClock::time_point target) {
  auto current = SteadyClock::now();
  if (target <= current) {
    return;
  }
  std::this_thread::sleep_until(target);
}

} // namespace PrimeHost
