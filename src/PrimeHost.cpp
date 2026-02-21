#include "PrimeHost/PrimeHost.h"

namespace PrimeHost {
namespace {
#if defined(__APPLE__)
HostResult<std::unique_ptr<Host>> createHostMac();
#endif
} // namespace

HostResult<std::unique_ptr<Host>> createHost() {
#if defined(__APPLE__)
  return createHostMac();
#else
  return std::unexpected(HostError{HostErrorCode::Unsupported});
#endif
}

} // namespace PrimeHost
