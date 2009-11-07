#include "debug.h"
#include "mutex.h"
#include "sandbox_impl.h"
#include "securemem.h"

namespace playground {

void SecureMem::abandonSystemCall(int fd, int err) {
  void* rc = reinterpret_cast<void *>(err);
  if (err) {
    Debug::message("System call failed\n");
  }
  Sandbox::SysCalls sys;
  if (Sandbox::write(sys, fd, &rc, sizeof(rc)) != sizeof(rc)) {
    Sandbox::die("Failed to send system call");
  }
}

void SecureMem::dieIfParentDied(int parentMapsFd) {
  // The syscall_mutex_ should not be contended. If it is, we are either
  // experiencing a very unusual load of system calls that the sandbox is not
  // optimized for; or, more likely, the sandboxed process terminated while the
  // trusted process was in the middle of waiting for the mutex. We detect
  // this situation and terminate the trusted process.
  int alive = !lseek(parentMapsFd, 0, SEEK_SET);
  if (alive) {
    char buf;
    do {
      alive = read(parentMapsFd, &buf, 1);
    } while (alive < 0 && errno == EINTR);
  }
  if (!alive) {
    Sandbox::die();
  }
}

void SecureMem::lockSystemCall(int parentMapsFd, Args* mem) {
  while (!Mutex::lockMutex(&Sandbox::syscall_mutex_, 500)) {
    dieIfParentDied(parentMapsFd);
  }
  asm volatile(
  #if defined(__x86_64__)
      "lock; incq (%0)\n"
  #elif defined(__i386__)
      "lock; incl (%0)\n"
  #else
  #error Unsupported target platform
  #endif
      :
      : "q"(&mem->sequence)
      : "memory");
}

void SecureMem::sendSystemCallInternal(int fd, bool locked, int parentMapsFd,
                                       Args* mem, int syscallNum, void* arg1,
                                       void* arg2, void* arg3, void* arg4,
                                       void* arg5, void* arg6) {
  if (!locked) {
    asm volatile(
    #if defined(__x86_64__)
        "lock; incq (%0)\n"
    #elif defined(__i386__)
        "lock; incl (%0)\n"
    #else
    #error Unsupported target platform
    #endif
        :
        : "q"(&mem->sequence)
        : "memory");
  }
  mem->syscallNum = syscallNum;
  mem->arg1       = arg1;
  mem->arg2       = arg2;
  mem->arg3       = arg3;
  mem->arg4       = arg4;
  mem->arg5       = arg5;
  mem->arg6       = arg6;
  asm volatile(
  #if defined(__x86_64__)
      "lock; incq (%0)\n"
  #elif defined(__i386__)
      "lock; incl (%0)\n"
  #else
  #error Unsupported target platform
  #endif
      :
      : "q"(&mem->sequence)
      : "memory");
  int data = locked ? -2 : -1;
  Sandbox::SysCalls sys;
  if (Sandbox::write(sys, fd, &data, sizeof(data)) != sizeof(data)) {
    Sandbox::die("Failed to send system call");
  }
  if (parentMapsFd >= 0) {
    while (!Mutex::waitForUnlock(&Sandbox::syscall_mutex_, 500)) {
      dieIfParentDied(parentMapsFd);
    }
  }
}

} // namespace
