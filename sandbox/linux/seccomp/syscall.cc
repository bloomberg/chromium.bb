// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"
#include "syscall_table.h"

namespace playground {

// TODO(markus): change this into a function that returns the address of the assembly code. If that isn't possible for sandbox_clone, then move that function into a *.S file
asm(
    ".pushsection .text, \"ax\", @progbits\n"

    // This is the special wrapper for the clone() system call. The code
    // relies on the stack layout of the system call wrapper (c.f. below). It
    // passes the stack pointer as an additional argument to sandbox__clone(),
    // so that upon starting the child, register values can be restored and
    // the child can start executing at the correct IP, instead of trying to
    // run in the trusted thread.
    "playground$sandbox_clone:"
    ".globl playground$sandbox_clone\n"
    ".type playground$sandbox_clone, @function\n"
    #if defined(__x86_64__)
    // Skip the 8 byte return address into the system call wrapper. The
    // following bytes are the saved register values that we need to restore
    // upon return from clone() in the new thread.
    "lea 8(%rsp), %r9\n"
    "jmp playground$sandbox__clone\n"
    #elif defined(__i386__)
    // As i386 passes function arguments on the stack, we need to skip a few
    // more values before we can get to the saved registers.
    "lea 28(%esp), %eax\n"
    "mov %eax, 24(%esp)\n"
    "jmp playground$sandbox__clone\n"
    #else
    #error Unsupported target platform
    #endif
    ".size playground$sandbox_clone, .-playground$sandbox_clone\n"


    // This is the wrapper which is called by the untrusted code, trying to
    // make a system call.
    "playground$syscallWrapper:"
    ".internal playground$syscallWrapper\n"
    ".globl playground$syscallWrapper\n"
    ".type playground$syscallWrapper, @function\n"
    #if defined(__x86_64__)
    // Save all registers
    "push %rbp\n"
    "mov  %rsp, %rbp\n"
    "push %rbx\n"
    "push %rcx\n"
    "push %rdx\n"
    "push %rsi\n"
    "push %rdi\n"
    "push %r8\n"
    "push %r9\n"
    "push %r10\n"
    "push %r11\n"
    "push %r12\n"
    "push %r13\n"
    "push %r14\n"
    "push %r15\n"

    // Convert from syscall calling conventions to C calling conventions.
    // System calls have a subtly different register ordering than the user-
    // space x86-64 ABI.
    "mov %r10, %rcx\n"

    // Check range of system call
    "cmp playground$maxSyscall(%rip), %eax\n"
    "ja  1f\n"

    // Retrieve function call from system call table (c.f. syscall_table.c).
    // We have three different types of entries; zero for denied system calls,
    // that should be handled by the defaultSystemCallHandler(); minus one
    // for unrestricted system calls that need to be forwarded to the trusted
    // thread; and function pointers to specific handler functions.
    "mov %rax, %r10\n"
    "shl $4, %r10\n"
    "lea playground$syscallTable(%rip), %r11\n"
    "add %r11, %r10\n"
    "mov 0(%r10), %r10\n"

    // Jump to function if non-null and not UNRESTRICTED_SYSCALL, otherwise
    // jump to fallback handler.
    "cmp $1, %r10\n"
    "jbe 1f\n"
    "call *%r10\n"
  "0:"

    // Restore CPU registers, except for %rax which was set by the system call.
    "pop %r15\n"
    "pop %r14\n"
    "pop %r13\n"
    "pop %r12\n"
    "pop %r11\n"
    "pop %r10\n"
    "pop %r9\n"
    "pop %r8\n"
    "pop %rdi\n"
    "pop %rsi\n"
    "pop %rdx\n"
    "pop %rcx\n"
    "pop %rbx\n"
    "pop %rbp\n"

    // Remove fake return address. This is added in the patching code in
    // library.cc and it makes stack traces a little cleaner.
    "add $8, %rsp\n"

    // Return to caller
    "ret\n"

  "1:"
    // If we end up calling a specific handler, we don't need to know the
    // system call number. However, in the generic case, we do. Shift
    // registers so that the system call number becomes visible as the
    // first function argument.
    "push %r9\n"
    "mov  %r8, %r9\n"
    "mov  %rcx, %r8\n"
    "mov  %rdx, %rcx\n"
    "mov  %rsi, %rdx\n"
    "mov  %rdi, %rsi\n"
    "mov  %rax, %rdi\n"

    // Call default handler.
    "call playground$defaultSystemCallHandler\n"
    "pop  %r9\n"
    "jmp 0b\n"
    #elif defined(__i386__)
    // Preserve all registers
    "push %ebx\n"
    "push %ecx\n"
    "push %edx\n"
    "push %esi\n"
    "push %edi\n"
    "push %ebp\n"

    // Convert from syscall calling conventions to C calling conventions
    "push %ebp\n"
    "push %edi\n"
    "push %esi\n"
    "push %edx\n"
    "push %ecx\n"
    "push %ebx\n"
    "push %eax\n"

    // Check range of system call
    "cmp playground$maxSyscall, %eax\n"
    "ja  5f\n"

    // We often have long sequences of calls to gettimeofday(). This is
    // needlessly expensive. Coalesce them into a single call.
    //
    // We keep track of state in TLS storage that we can access through
    // the %fs segment register. See trusted_thread.cc for the exact
    // memory layout.
    //
    // TODO(markus): maybe, we should proactively call gettimeofday() and
    //               clock_gettime(), whenever we talk to the trusted thread?
    //               or maybe, if we have recently seen requests to compute
    //               the time. There might be a repeated pattern of those.
    "cmp  $78, %eax\n"             // __NR_gettimeofday
    "jnz  2f\n"
    "cmp  %eax, %fs:0x102C-0x58\n" // last system call
    "jnz  0f\n"

    // This system call and the last system call prior to this one both are
    // calls to gettimeofday(). Try to avoid making the new call and just
    // return the same result as in the previous call.
    // Just in case the caller is spinning on the result from gettimeofday(),
    // every so often, call the actual system call.
    "decl %fs:0x1030-0x58\n"       // countdown calls to gettimofday()
    "jz   0f\n"

    // Atomically read the 64bit word representing last-known timestamp and
    // return it to the caller. On x86-32 this is a little more complicated and
    // requires the use of the cmpxchg8b instruction.
    "mov  %ebx, %eax\n"
    "mov  %ecx, %edx\n"
    "lock; cmpxchg8b 100f\n"
    "mov  %eax, 0(%ebx)\n"
    "mov  %edx, 4(%ebx)\n"
    "xor  %eax, %eax\n"
    "add  $28, %esp\n"
    "jmp  4f\n"

    // This is a call to gettimeofday(), but we don't have a valid cached
    // result, yet.
  "0:mov  %eax, %fs:0x102C-0x58\n" // remember syscall number
    "movl $500, %fs:0x1030-0x58\n" // make system call, each 500 invocations
    "call playground$defaultSystemCallHandler\n"

    // Returned from gettimeofday(). Remember return value, in case the
    // application calls us again right away.
    // Again, this has to happen atomically and requires cmpxchg8b.
    "mov 4(%ebx), %ecx\n"
    "mov 0(%ebx), %ebx\n"
    "mov 100f, %eax\n"
    "mov 101f, %edx\n"
  "1:lock; cmpxchg8b 100f\n"
    "jnz 1b\n"
    "xor %eax, %eax\n"
    "jmp 6f\n"

    // Remember the number of the last system call made. We deliberately do
    // not remember calls to gettid(), as we have often seen long sequences
    // of calls to just gettimeofday() and gettid(). In that situation, we
    // would still like to coalesce the gettimeofday() calls.
  "2:cmp $224, %eax\n"             // __NR_gettid
    "jz  3f\n"
    "mov  %eax, %fs:0x102C-0x58\n" // remember syscall number

    // Retrieve function call from system call table (c.f. syscall_table.c).
    // We have three different types of entries; zero for denied system calls,
    // that should be handled by the defaultSystemCallHandler(); minus one
    // for unrestricted system calls that need to be forwarded to the trusted
    // thread; and function pointers to specific handler functions.
  "3:shl  $3, %eax\n"
    "lea  playground$syscallTable, %ebx\n"
    "add  %ebx, %eax\n"
    "mov  0(%eax), %eax\n"

    // Jump to function if non-null and not UNRESTRICTED_SYSCALL, otherwise
    // jump to fallback handler.
    "cmp  $1, %eax\n"
    "jbe  5f\n"
    "add  $4, %esp\n"
    "call *%eax\n"
    "add  $24, %esp\n"

    // Restore CPU registers, except for %eax which was set by the system call.
  "4:pop  %ebp\n"
    "pop  %edi\n"
    "pop  %esi\n"
    "pop  %edx\n"
    "pop  %ecx\n"
    "pop  %ebx\n"

    // Return to caller
    "ret\n"

    // Call default handler.
  "5:call playground$defaultSystemCallHandler\n"
  "6:add  $28, %esp\n"
    "jmp 4b\n"

    ".pushsection \".bss\"\n"
    ".balign 8\n"
"100:.byte 0, 0, 0, 0\n"
"101:.byte 0, 0, 0, 0\n"
    ".popsection\n"

    #else
    #error Unsupported target platform
    #endif
    ".size playground$syscallWrapper, .-playground$syscallWrapper\n"
    ".popsection\n"
);


void* Sandbox::defaultSystemCallHandler(int syscallNum, void* arg0, void* arg1,
                                        void* arg2, void* arg3, void* arg4,
                                        void* arg5) {
  // TODO(markus): The following comment is currently not true, we do intercept these system calls. Try to fix that.

  // We try to avoid intercepting read(), write(), and sigreturn(), as
  // these system calls are not restricted in Seccomp mode. But depending on
  // the exact instruction sequence in libc, we might not be able to reliably
  // filter out these system calls at the time when we instrument the code.
  SysCalls  sys;
  long      rc;
  long long tm;
  switch (syscallNum) {
    case __NR_read:
      Debug::syscall(&tm, syscallNum, "Allowing unrestricted system call");
      rc             = sys.read((long)arg0, arg1, (size_t)arg2);
      break;
    case __NR_write:
      Debug::syscall(&tm, syscallNum, "Allowing unrestricted system call");
      rc             = sys.write((long)arg0, arg1, (size_t)arg2);
      break;
    case __NR_rt_sigreturn:
      Debug::syscall(&tm, syscallNum, "Allowing unrestricted system call");
      rc             = sys.rt_sigreturn((unsigned long)arg0);
      break;
    default:
      if (Debug::isEnabled()) {
        // In debug mode, prevent stderr from being closed
        if (syscallNum == __NR_close && arg0 == (void *)2)
          return 0;
      }

      if ((unsigned)syscallNum <= maxSyscall &&
          syscallTable[syscallNum].handler == UNRESTRICTED_SYSCALL) {
        Debug::syscall(&tm, syscallNum, "Allowing unrestricted system call");
     perform_unrestricted:
        struct {
          int          sysnum;
          void*        unrestricted_req[6];
        } __attribute__((packed)) request = {
          syscallNum, { arg0, arg1, arg2, arg3, arg4, arg5 } };

        int   thread = threadFdPub();
        void* rc;
        if (write(sys, thread, &request, sizeof(request)) != sizeof(request) ||
            read(sys, thread, &rc, sizeof(rc)) != sizeof(rc)) {
          die("Failed to forward unrestricted system call");
        }
        Debug::elapsed(tm, syscallNum);
        return rc;
      } else if (Debug::isEnabled()) {
        Debug::syscall(&tm, syscallNum,
                       "In production mode, this call would be disallowed");
        goto perform_unrestricted;
      } else {
        return (void *)-ENOSYS;
      }
  }
  if (rc < 0) {
    rc               = -sys.my_errno;
  }
  Debug::elapsed(tm, syscallNum);
  return (void *)rc;
}

} // namespace
