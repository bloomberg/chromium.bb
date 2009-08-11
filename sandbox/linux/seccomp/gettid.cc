#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

int Sandbox::sandbox_gettid() {
  Debug::syscall(__NR_gettid, "Executing handler");
  return tid();
}

} // namespace
