#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

void* Sandbox::sandbox_mmap(void *start, size_t length, int prot, int flags,
                          int fd, off_t offset) {
  Debug::syscall(__NR_mmap, "Executing handler");
  struct {
    int       sysnum;
    long long cookie;
    MMap      mmap_req;
  } __attribute__((packed)) request;
  request.sysnum          = __NR_MMAP;
  request.cookie          = cookie();
  request.mmap_req.start  = start;
  request.mmap_req.length = length;
  request.mmap_req.prot   = prot;
  request.mmap_req.flags  = flags;
  request.mmap_req.fd     = fd;
  request.mmap_req.offset = offset;

  void* rc;
  SysCalls sys;
  if (write(sys, processFdPub(), &request, sizeof(request)) !=
      sizeof(request) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward mmap() request [sandbox]");
  }
  return rc;
}

bool Sandbox::process_mmap(int parentMapsFd, int sandboxFd, int threadFdPub,
                           int threadFd, SecureMem::Args* mem) {
  // Read request
  SysCalls sys;
  MMap mmap_req;
  if (read(sys, sandboxFd, &mmap_req, sizeof(mmap_req)) != sizeof(mmap_req)) {
    die("Failed to read parameters for mmap() [process]");
  }

  if (mmap_req.flags & MAP_FIXED) {
    // Cannot map a memory area that was part of the original memory mappings.
    void *stop = reinterpret_cast<void *>(
        (char *)mmap_req.start + mmap_req.length);
    ProtectedMap::const_iterator iter = protectedMap_.lower_bound(
        (void *)mmap_req.start);
    if (iter != protectedMap_.begin()) {
      --iter;
    }
    for (; iter != protectedMap_.end() && iter->first < stop; ++iter) {
      if (mmap_req.start < reinterpret_cast<void *>(
              reinterpret_cast<char *>(iter->first) + iter->second) &&
          stop > iter->first) {
        int rc = -EINVAL;
        SecureMem::abandonSystemCall(threadFd, rc);
        return false;
      }
    }
  }

  // All other mmap() requests are OK
  SecureMem::sendSystemCall(threadFdPub, false, -1, mem, __NR_MMAP,
                            mmap_req.start, mmap_req.length, mmap_req.prot,
                            mmap_req.flags, mmap_req.fd, mmap_req.offset);
  return true;
}

} // namespace
