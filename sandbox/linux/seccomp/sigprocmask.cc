// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

// If the sandboxed process tries to mask SIGSEGV, there is a good chance
// the process will eventually get terminated. If this is really ever a
// problem, we can hide the fact that SIGSEGV is unmasked. But I don't think
// we really need this. Masking of synchronous signals is rarely necessary.

#if defined(__NR_sigprocmask)
int Sandbox::sandbox_sigprocmask(int how, const void* set, void* old_set) {
  long long tm;
  Debug::syscall(&tm, __NR_sigprocmask, "Executing handler");

  // Access the signal mask by triggering a SEGV and modifying the signal state
  // prior to calling rt_sigreturn().
  long res = -ENOSYS;
  #if defined(__x86_64__)
  #error x86-64 does not support sigprocmask(); use rt_sigprocmask() instead
  #elif defined(__i386__)
  asm volatile(
    "push  %%ebx\n"
    "movl  %2, %%ebx\n"
    "int   $0\n"
    "pop   %%ebx\n"
    : "=a"(res)
    : "0"(__NR_sigprocmask), "ri"((long)how),
      "c"((long)set), "d"((long)old_set)
    : "esp", "memory");
  #else
  #error Unsupported target platform
  #endif

  // Update our shadow signal mask, so that we can copy it upon creation of
  // new threads.
  if (res == 0 && set != NULL) {
    SecureMem::Args* args = getSecureMem();
    switch (how) {
    case SIG_BLOCK:
      *(unsigned long long *)&args->signalMask |=  *(unsigned long long *)set;
      break;
    case SIG_UNBLOCK:
      *(unsigned long long *)&args->signalMask &= ~*(unsigned long long *)set;
      break;
    case SIG_SETMASK:
      *(unsigned long long *)&args->signalMask  =  *(unsigned long long *)set;
      break;
    default:
      break;
    }
  }

  Debug::elapsed(tm, __NR_sigprocmask);

  return (int)res;
}
#endif

#if defined(__NR_rt_sigprocmask)
int Sandbox::sandbox_rt_sigprocmask(int how, const void* set, void* old_set,
                                    size_t bytes) {
  long long tm;
  Debug::syscall(&tm, __NR_rt_sigprocmask, "Executing handler");

  // Access the signal mask by triggering a SEGV and modifying the signal state
  // prior to calling rt_sigreturn().
  long res = -ENOSYS;
  #if defined(__x86_64__)
  asm volatile(
    "movq %5, %%r10\n"
    "int $0\n"
    : "=a"(res)
    : "0"(__NR_rt_sigprocmask), "D"((long)how),
      "S"((long)set), "d"((long)old_set), "r"((long)bytes)
    : "r10", "r11", "rcx", "memory");
  #elif defined(__i386__)
  asm volatile(
    "push  %%ebx\n"
    "movl  %2, %%ebx\n"
    "int   $0\n"
    "pop   %%ebx\n"
    : "=a"(res)
    : "0"(__NR_rt_sigprocmask), "ri"((long)how),
      "c"((long)set), "d"((long)old_set), "S"((long)bytes)
    : "esp", "memory");
  #else
  #error Unsupported target platform
  #endif

  // Update our shadow signal mask, so that we can copy it upon creation of
  // new threads.
  if (res == 0 && set != NULL && bytes >= 8) {
    SecureMem::Args* args = getSecureMem();
    switch (how) {
    case SIG_BLOCK:
      *(unsigned long long *)&args->signalMask |=  *(unsigned long long *)set;
      break;
    case SIG_UNBLOCK:
      *(unsigned long long *)&args->signalMask &= ~*(unsigned long long *)set;
      break;
    case SIG_SETMASK:
      *(unsigned long long *)&args->signalMask  =  *(unsigned long long *)set;
      break;
    default:
      break;
    }
  }

  Debug::elapsed(tm, __NR_rt_sigprocmask);

  return (int)res;
}
#endif

} // namespace
