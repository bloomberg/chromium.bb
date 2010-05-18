// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(markus): We currently instrument the restorer functions with calls to
//               the syscallWrapper(). This prevents gdb from properly
//               creating backtraces of code that is running in signal
//               handlers. We might instead want to always override the
//               restorer with a function that contains the "magic" signature
//               but that is not executable. The SEGV handler can detect this
//               and then invoke the appropriate restorer.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

#if defined(__NR_sigaction)
long Sandbox::sandbox_sigaction(int signum, const void* a_, void* oa_) {
  const SysCalls::kernel_old_sigaction* action =
    reinterpret_cast<const SysCalls::kernel_old_sigaction*>(a_);
  SysCalls::kernel_old_sigaction* old_action =
    reinterpret_cast<SysCalls::kernel_old_sigaction*>(oa_);

  long rc = 0;
  long long tm;
  Debug::syscall(&tm, __NR_sigaction, "Executing handler");
  if (signum == SIGSEGV) {
    if (old_action) {
      old_action->sa_handler_ = sa_segv_.sa_handler_;
      old_action->sa_mask     = sa_segv_.sa_mask.sig[0];
      old_action->sa_flags    = sa_segv_.sa_flags;
      old_action->sa_restorer = sa_segv_.sa_restorer;
    }
    if (action) {
      sa_segv_.sa_handler_    = action->sa_handler_;
      sa_segv_.sa_mask.sig[0] = action->sa_mask;
      sa_segv_.sa_flags       = action->sa_flags;
      sa_segv_.sa_restorer    = action->sa_restorer;
    }
  } else {
    struct {
      int       sysnum;
      long long cookie;
      SigAction sigaction_req;
    } __attribute__((packed)) request;
    request.sysnum                   = __NR_sigaction;
    request.cookie                   = cookie();
    request.sigaction_req.sysnum     = __NR_sigaction;
    request.sigaction_req.signum     = signum;
    request.sigaction_req.action     =
      reinterpret_cast<const SysCalls::kernel_sigaction *>(action);
    request.sigaction_req.old_action =
      reinterpret_cast<const SysCalls::kernel_sigaction *>(old_action);
    request.sigaction_req.sigsetsize = 8;

    SysCalls sys;
    if (write(sys, processFdPub(), &request, sizeof(request)) !=
        sizeof(request) ||
        read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
      die("Failed to forward sigaction() request [sandbox]");
    }
  }
  Debug::elapsed(tm, __NR_sigaction);
  return rc;
}
#endif

#if defined(__NR_rt_sigaction)
#define min(a,b) ({ typeof(a) a_=(a); typeof(b) b_=(b); a_ < b_ ? a_ : b_; })
#define max(a,b) ({ typeof(a) a_=(a); typeof(b) b_=(b); a_ > b_ ? a_ : b_; })

long Sandbox::sandbox_rt_sigaction(int signum, const void* a_, void* oa_,
                                   size_t sigsetsize) {
  const SysCalls::kernel_sigaction* action =
    reinterpret_cast<const SysCalls::kernel_sigaction*>(a_);
  SysCalls::kernel_sigaction* old_action =
    reinterpret_cast<SysCalls::kernel_sigaction*>(oa_);

  long rc = 0;
  long long tm;
  Debug::syscall(&tm, __NR_rt_sigaction, "Executing handler");
  if (signum == SIGSEGV) {
    size_t theirSize = offsetof(SysCalls::kernel_sigaction, sa_mask) +
                       sigsetsize;
    if (old_action) {
      memcpy(old_action, &sa_segv_, min(sizeof(sa_segv_), theirSize));
      memset(old_action + 1, 0, max(0u, theirSize - sizeof(sa_segv_)));
    }
    if (action) {
      memcpy(&sa_segv_, action, min(sizeof(sa_segv_), theirSize));
      memset(&sa_segv_.sa_mask, 0, max(0u, 8 - sigsetsize));
    }
  } else {
    struct {
      int       sysnum;
      long long cookie;
      SigAction sigaction_req;
    } __attribute__((packed)) request;
    request.sysnum                   = __NR_rt_sigaction;
    request.cookie                   = cookie();
    request.sigaction_req.sysnum     = __NR_rt_sigaction;
    request.sigaction_req.signum     = signum;
    request.sigaction_req.action     = action;
    request.sigaction_req.old_action = old_action;
    request.sigaction_req.sigsetsize = sigsetsize;

    SysCalls sys;
    if (write(sys, processFdPub(), &request, sizeof(request)) !=
        sizeof(request) ||
        read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
      die("Failed to forward rt_sigaction() request [sandbox]");
    }
  }
  Debug::elapsed(tm, __NR_rt_sigaction);
  return rc;
}
#endif

#if defined(__NR_signal)
void* Sandbox::sandbox_signal(int signum, const void* handler) {
  struct kernel_old_sigaction sa, osa;
  sa.sa_handler_ = reinterpret_cast<void (*)(int)>(handler);
  sa.sa_flags    = SA_NODEFER | SA_RESETHAND | SA_RESTORER;
  sa.sa_mask     = 0;
  asm volatile(
      "lea  0f, %0\n"
      "jmp  1f\n"
    "0:pop  %%eax\n"
      "mov  $119, %%eax\n" // __NR_sigreturn
      "int  $0x80\n"
    "1:\n"
      : "=r"(sa.sa_restorer));
  long rc = sandbox_sigaction(signum, &sa, &osa);
  if (rc < 0) {
    return (void *)rc;
  }
  return reinterpret_cast<void *>(osa.sa_handler_);
}
#endif

bool Sandbox::process_sigaction(int parentMapsFd, int sandboxFd,
                                int threadFdPub, int threadFd,
                                SecureMem::Args* mem) {
  // We need to intercept sigaction() in order to properly rewrite calls to
  // sigaction(SEGV). While there is no security implication if we didn't do
  // so, it would end up preventing the program from running correctly as the
  // the sandbox's SEGV handler could accidentally get removed. All of this is
  // done in sandbox_{,rt_}sigaction(). But we still bounce through the
  // trusted process as that is the only way we can instrument system calls.
  // This is somewhat needlessly complicated. But as sigaction() is not a
  // performance critical system call, it is easier to do this way than to
  // extend the format of the syscall_table so that it could deal with this
  // special case.

  // Read request
  SigAction sigaction_req;
  SysCalls sys;
  if (read(sys, sandboxFd, &sigaction_req, sizeof(sigaction_req)) !=
      sizeof(sigaction_req)) {
    die("Failed to read parameters for sigaction() [process]");
  }
  if (sigaction_req.signum == SIGSEGV) {
    // This should never happen. Something went wrong when intercepting the
    // system call. This is not a security problem, but it clearly doesn't
    // make sense to let the system call pass.
    SecureMem::abandonSystemCall(threadFd, -EINVAL);
    return false;
  }
  SecureMem::sendSystemCall(threadFdPub, false, -1, mem, sigaction_req.sysnum,
                            sigaction_req.signum, sigaction_req.action,
                            sigaction_req.old_action,
                            sigaction_req.sigsetsize);
  return true;
}

} // namespace
