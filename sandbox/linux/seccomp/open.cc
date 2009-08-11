#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

int Sandbox::sandbox_open(const char *pathname, int flags, mode_t mode) {
  Debug::syscall(__NR_open, "Executing handler");
  size_t len                    = strlen(pathname);
  struct Request {
    int       sysnum;
    long long cookie;
    Open      open_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->sysnum               = __NR_open;
  request->cookie               = cookie();
  request->open_req.path_length = len;
  request->open_req.flags       = flags;
  request->open_req.mode        = mode;
  memcpy(request->pathname, pathname, len);

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), request, sizeof(data)) != (int)sizeof(data) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward open() request [sandbox]");
  }
  return static_cast<int>(rc);
}

bool Sandbox::process_open(int parentProc, int sandboxFd, int threadFdPub,
                           int threadFd, SecureMem::Args* mem) {
  // Read request
  SysCalls sys;
  Open open_req;
  if (read(sys, sandboxFd, &open_req, sizeof(open_req)) != sizeof(open_req)) {
 read_parm_failed:
    die("Failed to read parameters for open() [process]");
  }
  int   rc                  = -ENAMETOOLONG;
  if (open_req.path_length >= sizeof(mem->pathname)) {
    char buf[32];
    while (open_req.path_length > 0) {
      size_t len            = open_req.path_length > sizeof(buf) ?
                              sizeof(buf) : open_req.path_length;
      ssize_t i             = read(sys, sandboxFd, buf, len);
      if (i <= 0) {
        goto read_parm_failed;
      }
      open_req.path_length -= i;
    }
    if (write(sys, threadFd, &rc, sizeof(rc)) != sizeof(rc)) {
      die("Failed to return data from open() [process]");
    }
    return false;
  }

  if ((open_req.flags & O_ACCMODE) != O_RDONLY) {
    // After locking the mutex, we can no longer abandon the system call. So,
    // perform checks before clobbering the securely shared memory.
    char tmp[open_req.path_length];
    if (read(sys, sandboxFd, tmp, open_req.path_length) !=
        (ssize_t)open_req.path_length) {
      goto read_parm_failed;
    }
    Debug::message(("Denying access to \"" + std::string(tmp) + "\"").c_str());
    SecureMem::abandonSystemCall(threadFd, -EACCES);
    return false;
  }

  SecureMem::lockSystemCall(parentProc, mem);
  if (read(sys, sandboxFd, mem->pathname, open_req.path_length) !=
      (ssize_t)open_req.path_length) {
    goto read_parm_failed;
  }
  mem->pathname[open_req.path_length] = '\000';

  // TODO(markus): Implement sandboxing policy. For now, we allow read
  // access to everything. That's probably not correct.
  Debug::message(("Allowing access to \"" + std::string(mem->pathname) +
                  "\"").c_str());

  // Tell trusted thread to open the file.
  SecureMem::sendSystemCall(threadFdPub, true, parentProc, mem, __NR_open,
                            mem->pathname - (char*)mem + (char*)mem->self,
                            open_req.flags, open_req.mode);
  return true;
}

} // namespace
