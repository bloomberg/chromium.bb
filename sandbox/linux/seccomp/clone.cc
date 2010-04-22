// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_clone(int flags, char* stack, int* pid, int* ctid,
                            void* tls, void *wrapper_sp) {
  long long tm;
  Debug::syscall(&tm, __NR_clone, "Executing handler");
  struct {
    int       sysnum;
    long long cookie;
    Clone     clone_req;
  } __attribute__((packed)) request;
  request.sysnum               = __NR_clone;
  request.cookie               = cookie();
  request.clone_req.flags      = flags;
  request.clone_req.stack      = stack;
  request.clone_req.pid        = pid;
  request.clone_req.ctid       = ctid;
  request.clone_req.tls        = tls;

  // TODO(markus): Passing stack == 0 currently does not do the same thing
  // that the kernel would do without the sandbox. This is just going to
  // cause a crash. We should detect this case, and replace the stack pointer
  // with the correct value, instead.
  // This is complicated by the fact that we will temporarily be executing
  // both threads from the same stack. Some synchronization will be necessary.
  // Fortunately, this complication also explains why hardly anybody ever
  // does this.
  // See trusted_thread.cc for more information.
  long rc;
  if (stack == 0) {
    rc = -EINVAL;
  } else {
    // Pass along the address on the stack where syscallWrapper() stored the
    // original CPU registers. These registers will be restored in the newly
    // created thread prior to returning from the wrapped system call.
    #if defined(__x86_64__)
    memcpy(&request.clone_req.regs64, wrapper_sp,
           sizeof(request.clone_req.regs64) + sizeof(void *));
    #elif defined(__i386__)
    memcpy(&request.clone_req.regs32, wrapper_sp,
           sizeof(request.clone_req.regs32) + sizeof(void *));
    #else
    #error Unsupported target platform
    #endif

    // In order to unblock the signal mask in the newly created thread and
    // after entering Seccomp mode, we have to call sigreturn(). But that
    // requires access to a proper stack frame describing a valid signal.
    // We trigger a signal now and make sure the stack frame ends up on the
    // new stack. Our segv() handler (in sandbox.cc) does that for us.
    // See trusted_thread.cc for more details on how threads get created.
    //
    // In general we rely on the kernel for generating the signal stack
    // frame, as the exact binary format has been extended several times over
    // the course of the kernel's development. Fortunately, the kernel
    // developers treat the initial part of the stack frame as a stable part
    // of the ABI. So, we can rely on fixed, well-defined offsets for accessing
    // register values and for accessing the signal mask.
    #if defined(__x86_64__) || defined(__i386__)
    #if defined(__x86_64__)
    // Red zone compensation. The instrumented system call will remove 128
    // bytes from the thread's stack prior to returning to the original
    // call site.
    stack                   -= 128;
    request.clone_req.stack  = stack;
    #endif
    asm("int $0"
        : "=m"(request.clone_req.stack)
        : "a"(__NR_clone + 0xF000), "d"(&request.clone_req.stack)
        : "memory");
    #else
    #error Unsupported target platform
    #endif

    // Adjust the signal stack frame so that it contains the correct stack
    // pointer upon returning from sigreturn().
    #if defined(__x86_64__)
    *(char **)(request.clone_req.stack + 0xA0) = stack;
    #elif defined(__i386__)
    *(char **)(request.clone_req.stack + 0x1C) = stack;
    #else
    #error Unsupported target platform
    #endif

    SysCalls sys;
    if (write(sys, processFdPub(), &request, sizeof(request)) !=
        sizeof(request) ||
        read(sys, threadFdPub(), &rc, sizeof(rc)) != sizeof(rc)) {
      die("Failed to forward clone() request [sandbox]");
    }
  }
  Debug::elapsed(tm, __NR_clone);
  return rc;
}

bool Sandbox::process_clone(int parentMapsFd, int sandboxFd, int threadFdPub,
                            int threadFd, SecureMem::Args* mem) {
  // Read request
  Clone clone_req;
  SysCalls sys;
  if (read(sys, sandboxFd, &clone_req, sizeof(clone_req)) !=sizeof(clone_req)){
    die("Failed to read parameters for clone() [process]");
  }

  // TODO(markus): add policy restricting parameters for clone
  if ((clone_req.flags & ~CLONE_DETACHED) != (CLONE_VM|CLONE_FS|CLONE_FILES|
      CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|
      CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID)) {
    SecureMem::abandonSystemCall(threadFd, -EPERM);
    return false;
  } else {
    SecureMem::Args* newMem = getNewSecureMem();
    if (!newMem) {
      SecureMem::abandonSystemCall(threadFd, -ENOMEM);
      return false;
    } else {
      // clone() has unusual semantics. We don't want to return back into the
      // trusted thread, but instead we need to continue execution at the IP
      // where we got called initially.
      SecureMem::lockSystemCall(parentMapsFd, mem);
      mem->ret              = clone_req.ret;
      #if defined(__x86_64__)
      mem->rbp              = clone_req.regs64.rbp;
      mem->rbx              = clone_req.regs64.rbx;
      mem->rcx              = clone_req.regs64.rcx;
      mem->rdx              = clone_req.regs64.rdx;
      mem->rsi              = clone_req.regs64.rsi;
      mem->rdi              = clone_req.regs64.rdi;
      mem->r8               = clone_req.regs64.r8;
      mem->r9               = clone_req.regs64.r9;
      mem->r10              = clone_req.regs64.r10;
      mem->r11              = clone_req.regs64.r11;
      mem->r12              = clone_req.regs64.r12;
      mem->r13              = clone_req.regs64.r13;
      mem->r14              = clone_req.regs64.r14;
      mem->r15              = clone_req.regs64.r15;
      #elif defined(__i386__)
      mem->ebp              = clone_req.regs32.ebp;
      mem->edi              = clone_req.regs32.edi;
      mem->esi              = clone_req.regs32.esi;
      mem->edx              = clone_req.regs32.edx;
      mem->ecx              = clone_req.regs32.ecx;
      mem->ebx              = clone_req.regs32.ebx;
      #else
      #error Unsupported target platform
      #endif
      newMem->sequence      = 0;
      newMem->shmId         = -1;
      mem->newSecureMem     = newMem;
      mem->processFdPub     = processFdPub_;
      mem->cloneFdPub       = cloneFdPub_;

      SecureMem::sendSystemCall(threadFdPub, true, parentMapsFd, mem,
                                __NR_clone, clone_req.flags, clone_req.stack,
                                clone_req.pid, clone_req.ctid, clone_req.tls);
      return true;
    }
  }
}

} // namespace
