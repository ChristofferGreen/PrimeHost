#include "PrimeHost/PrimeHost.h"

namespace PrimeHost {
#if defined(__APPLE__)
HostResult<std::unique_ptr<Host>> createHostMac();
#endif

HostResult<std::unique_ptr<Host>> createHost() {
#if defined(__APPLE__)
  return createHostMac();
#else
  return std::unexpected(HostError{HostErrorCode::Unsupported});
#endif
}

} // namespace PrimeHost
