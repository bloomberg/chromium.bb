#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

int Sandbox::sandbox_stat(const char *path, void *buf) {
  Debug::syscall(__NR_stat, "Executing handler");
  size_t len                    = strlen(path);
  struct Request {
    int       sysnum;
    long long cookie;
    Stat      stat_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->sysnum               = __NR_stat;
  request->cookie               = cookie();
  request->stat_req.sysnum      = __NR_stat;
  request->stat_req.path_length = len;
  request->stat_req.buf         = buf;
  memcpy(request->pathname, path, len);

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), request, sizeof(data)) != (int)sizeof(data) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward stat() request [sandbox]");
  }
  return static_cast<int>(rc);
}

#if defined(__NR_stat64)
int Sandbox::sandbox_stat64(const char *path, void *buf) {
  Debug::syscall(__NR_stat64, "Executing handler");
  size_t len                    = strlen(path);
  struct Request {
    int       sysnum;
    long long cookie;
    Stat      stat_req;
    char      pathname[0];
  } __attribute__((packed)) *request;
  char data[sizeof(struct Request) + len];
  request                       = reinterpret_cast<struct Request*>(data);
  request->sysnum               = __NR_stat64;
  request->cookie               = cookie();
  request->stat_req.sysnum      = __NR_stat64;
  request->stat_req.path_length = len;
  request->stat_req.buf         = buf;
  memcpy(request->pathname, path, len);

  long rc;
  SysCalls sys;
  if (write(sys, processFdPub(), request, sizeof(data)) != (int)sizeof(data) ||
      read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
    die("Failed to forward stat64() request [sandbox]");
  }
  return static_cast<int>(rc);
}
#endif

bool Sandbox::process_stat(int parentProc, int sandboxFd, int threadFdPub,
                           int threadFd, SecureMem::Args* mem) {
  // Read request
  SysCalls sys;
  Stat stat_req;
  if (read(sys, sandboxFd, &stat_req, sizeof(stat_req)) != sizeof(stat_req)) {
 read_parm_failed:
    die("Failed to read parameters for stat() [process]");
  }
  int   rc                  = -ENAMETOOLONG;
  if (stat_req.path_length >= (int)sizeof(mem->pathname)) {
    char buf[32];
    while (stat_req.path_length > 0) {
      size_t len            = stat_req.path_length > sizeof(buf) ?
                              sizeof(buf) : stat_req.path_length;
      ssize_t i             = read(sys, sandboxFd, buf, len);
      if (i <= 0) {
        goto read_parm_failed;
      }
      stat_req.path_length -= i;
    }
    if (write(sys, threadFd, &rc, sizeof(rc)) != sizeof(rc)) {
      die("Failed to return data from stat() [process]");
    }
    return false;
  }
  SecureMem::lockSystemCall(parentProc, mem);
  if (read(sys, sandboxFd, mem->pathname, stat_req.path_length) !=
      (ssize_t)stat_req.path_length) {
    goto read_parm_failed;
  }
  mem->pathname[stat_req.path_length] = '\000';

  // TODO(markus): Implement sandboxing policy
  Debug::message(("Allowing access to \"" + std::string(mem->pathname) +
                  "\"").c_str());

  // Tell trusted thread to stat the file.
  SecureMem::sendSystemCall(threadFdPub, true, parentProc, mem,
                            #if defined(__i386__)
                            stat_req.sysnum == __NR_stat64 ? __NR_stat64 :
                            #endif
                            __NR_stat,
                            mem->pathname - (char*)mem + (char*)mem->self,
                            stat_req.buf);
  return true;
}

} // namespace
