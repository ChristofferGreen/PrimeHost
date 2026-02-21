#include "PrimeHost/Audio.h"

namespace PrimeHost {

#if defined(__APPLE__)
HostResult<std::unique_ptr<AudioHost>> createAudioHostMac();
#endif

HostResult<std::unique_ptr<AudioHost>> createAudioHost() {
#if defined(__APPLE__)
  return createAudioHostMac();
#else
  return std::unexpected(HostError{HostErrorCode::Unsupported});
#endif
}

} // namespace PrimeHost
