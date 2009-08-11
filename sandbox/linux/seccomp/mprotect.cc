#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

int Sandbox::sandbox_mprotect(const void *addr, size_t len, int prot) {
  Debug::syscall(__NR_mprotect, "Executing handler");
  struct {
    int       sysnum;
    long long cookie;
    MProtect  mprotect_req;
  } __attribute__((packed)) request;
  request.sysnum            = __NR_mprotect;
  request.cookie            = cookie();
  request.mprotect_req.addr = addr;
  request.mprotect_req.len  = len;
  request.mprotect_req.prot = prot;

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward mprotect() request [sandbox]");
  }
  return static_cast<int>(rc);
}

bool Sandbox::process_mprotect(int parentProc, int sandboxFd, int threadFdPub,
                               int threadFd, SecureMem::Args* mem) {
  // Read request
  SysCalls sys;
  MProtect mprotect_req;
  if (read(sys, sandboxFd, &mprotect_req, sizeof(mprotect_req)) !=
      sizeof(mprotect_req)) {
    die("Failed to read parameters for mprotect() [process]");
  }

  // Cannot change permissions on any memory region that was part of the
  // original memory mappings.
  int rc = -EINVAL;
  void *stop = reinterpret_cast<void *>(
      (char *)mprotect_req.addr + mprotect_req.len);
  ProtectedMap::const_iterator iter = protectedMap_.lower_bound(
      (void *)mprotect_req.addr);
  if (iter != protectedMap_.begin()) {
    --iter;
  }
  for (; iter != protectedMap_.end() && iter->first < stop; ++iter) {
    if (mprotect_req.addr < reinterpret_cast<void *>(
            reinterpret_cast<char *>(iter->first) + iter->second) &&
        stop > iter->first) {
      SecureMem::abandonSystemCall(threadFd, rc);
      return false;
    }
  }

  // Changing permissions on memory regions that were newly mapped inside of
  // the sandbox is OK.
  SecureMem::sendSystemCall(threadFdPub, false, -1, mem, __NR_mprotect,
                            mprotect_req.addr,  mprotect_req.len,
                            mprotect_req.prot);
  return true;
}

} // namespace
