#ifndef SYSCALL_TABLE_H__
#define SYSCALL_TABLE_H__

#include <sys/types.h>

#ifdef __cplusplus
#include "securemem.h"
extern "C" {
namespace playground {
#define SecureMemArgs SecureMem::Args
#else
#define SecureMemArgs void
#define bool          int
#endif
  #define UNRESTRICTED_SYSCALL ((void *)1)

  struct SyscallTable {
    void   *handler;
    bool  (*trustedProcess)(int parentProc, int sandboxFd, int threadFdPub,
                            int threadFd, SecureMemArgs* mem);
  };
  extern const struct SyscallTable syscallTable[]
    asm("playground$syscallTable")
#if defined(__x86_64__)
    __attribute__((visibility("internal")))
#endif
    ;
  extern const unsigned maxSyscall
    asm("playground$maxSyscall")
#if defined(__x86_64__)
    __attribute__((visibility("internal")))
#endif
    ;
#ifdef __cplusplus
} // namespace
}
#endif

#endif // SYSCALL_TABLE_H__
