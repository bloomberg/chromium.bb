#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

int Sandbox::sandbox_getpid() {
  Debug::syscall(__NR_getpid, "Executing handler");
  return pid_;
}

} // namespace
