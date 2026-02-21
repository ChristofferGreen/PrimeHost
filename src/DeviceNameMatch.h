#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace PrimeHost {
namespace detail {

inline std::string normalizeDeviceName(std::string_view name) {
  std::string normalized;
  normalized.reserve(name.size());
  bool pendingSpace = false;
  for (char ch : name) {
    unsigned char value = static_cast<unsigned char>(ch);
    if (std::isalnum(value)) {
      if (pendingSpace && !normalized.empty()) {
        normalized.push_back(' ');
      }
      normalized.push_back(static_cast<char>(std::tolower(value)));
      pendingSpace = false;
    } else {
      pendingSpace = true;
    }
  }
  return normalized;
}

inline int countSharedTokens(const std::string& left, const std::string& right) {
  if (left.empty() || right.empty()) {
    return 0;
  }
  int shared = 0;
  std::size_t start = 0;
  while (start < left.size()) {
    std::size_t end = left.find(' ', start);
    if (end == std::string::npos) {
      end = left.size();
    }
    std::size_t length = end - start;
    if (length >= 3) {
      std::string_view token(left.data() + start, length);
      if (right.find(token) != std::string::npos) {
        shared += 1;
      }
    }
    start = end + 1;
  }
  return shared;
}

} // namespace detail

inline int deviceNameMatchScore(std::string_view candidate, std::string_view reference) {
  if (candidate.empty() || reference.empty()) {
    return 0;
  }
  std::string normalizedCandidate = detail::normalizeDeviceName(candidate);
  std::string normalizedReference = detail::normalizeDeviceName(reference);
  if (normalizedCandidate.empty() || normalizedReference.empty()) {
    return 0;
  }
  if (normalizedCandidate == normalizedReference) {
    return 100;
  }
  if (normalizedCandidate.size() < normalizedReference.size()) {
    if (normalizedReference.find(normalizedCandidate) != std::string::npos) {
      return 80;
    }
  } else if (normalizedCandidate.find(normalizedReference) != std::string::npos) {
    return 80;
  }
  int sharedTokens = detail::countSharedTokens(normalizedCandidate, normalizedReference);
  return sharedTokens * 10;
}

inline bool deviceNameMatches(std::string_view candidate, std::string_view reference) {
  return deviceNameMatchScore(candidate, reference) > 0;
}

} // namespace PrimeHost
