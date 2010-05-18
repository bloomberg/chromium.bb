// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox_impl.h"
#include "syscall_table.h"

namespace playground {

void Sandbox::createTrustedThread(int processFdPub, int cloneFdPub,
                                  SecureMem::Args* secureMem) {
  SecureMem::Args args                  = { { { { { 0 } } } } };
  args.self                             = &args;
  args.newSecureMem                     = secureMem;
  args.processFdPub                     = processFdPub;
  args.cloneFdPub                       = cloneFdPub;
#if defined(__x86_64__)
  asm volatile(
      "push %%rbx\n"
      "push %%rbp\n"
      "mov  %0, %%rbp\n"           // %rbp = args
      "xor  %%rbx, %%rbx\n"        // initial sequence number
      "lea  999f(%%rip), %%r15\n"  // continue in same thread

      // Signal handlers are process-wide. This means that for security
      // reasons, we cannot allow that the trusted thread ever executes any
      // signal handlers.
      // We prevent the execution of signal handlers by setting a signal
      // mask that blocks all signals. In addition, we make sure that the
      // stack pointer is invalid.
      // We cannot reset the signal mask until after we have enabled
      // Seccomp mode. Our sigprocmask() wrapper would normally do this by
      // raising a signal, modifying the signal mask in the kernel-generated
      // signal frame, and then calling sigreturn(). This presents a bit of
      // a Catch-22, as all signals are masked and we can therefore not
      // raise any signal that would allow us to generate the signal stack
      // frame.
      // Instead, we have to create the signal stack frame prior to entering
      // Seccomp mode. This incidentally also helps us to restore the
      // signal mask to the same value that it had prior to entering the
      // sandbox.
      // The signal wrapper for clone() is the second entry point into this
      // code (by means of sending an IPC to its trusted thread). It goes
      // through the same steps of creating a signal stack frame on the
      // newly created thread's stacks prior to cloning. See clone.cc for
      // details.
      "mov  $56+0xF000, %%eax\n"   // __NR_clone + 0xF000
      "mov  %%rsp, %%rcx\n"
      "int  $0\n"                  // push a signal stack frame (see clone.cc)
      "mov  %%rcx, 0xA0(%%rsp)\n"  // pop stack upon call to sigreturn()
      "mov  %%rsp, %%r9\n"
      "mov  $2, %%rdi\n"           // how     = SIG_SETMASK
      "pushq $-1\n"
      "mov  %%rsp, %%rsi\n"        // set     = full mask
      "xor  %%rdx, %%rdx\n"        // old_set = NULL
      "mov  $8, %%r10\n"           // mask all 64 signals
      "mov  $14, %%eax\n"          // NR_rt_sigprocmask
      "syscall\n"
      "xor  %%rsp, %%rsp\n"        // invalidate the stack in all trusted code
      "jmp  20f\n"                 // create trusted thread

      // TODO(markus): Coalesce the read() operations by reading into a bigger
      // buffer.

      // Parameters:
      //   *%fs: secure memory region
      //         the page following this one contains the scratch space
      //   %r13: thread's side of threadFd
      //   %r15: processFdPub

      // Local variables:
      //   %rbx: sequence number for trusted calls

      // Temporary variables:
      //   %r8: child stack
      //   %r9: system call number, child stack
      //  %rbp: secure memory of previous thread

      // Layout of secure shared memory region (c.f. securemem.h):
      //   0x00: pointer to the secure shared memory region (i.e. self)
      //   0x08: sequence number; must match %rbx
      //   0x10: call type; must match %eax, iff %eax == -1 || %eax == -2
      //   0x18: system call number; passed to syscall in %rax
      //   0x20: first argument; passed to syscall in %rdi
      //   0x28: second argument; passed to syscall in %rsi
      //   0x30: third argument; passed to syscall in %rdx
      //   0x38: fourth argument; passed to syscall in %r10
      //   0x40: fifth argument; passed to syscall in %r8
      //   0x48: sixth argument; passed to syscall in %r9
      //   0x50: stored return address for clone() system call
      //   0x58: stored %rbp value for clone() system call
      //   0x60: stored %rbx value for clone() system call
      //   0x68: stored %rcx value for clone() system call
      //   0x70: stored %rdx value for clone() system call
      //   0x78: stored %rsi value for clone() system call
      //   0x80: stored %rdi value for clone() system call
      //   0x88: stored %r8 value for clone() system call
      //   0x90: stored %r9 value for clone() system call
      //   0x98: stored %r10 value for clone() system call
      //   0xA0: stored %r11 value for clone() system call
      //   0xA8: stored %r12 value for clone() system call
      //   0xB0: stored %r13 value for clone() system call
      //   0xB8: stored %r14 value for clone() system call
      //   0xC0: stored %r15 value for clone() system call
      //   0xC8: new shared memory for clone()
      //   0xD0: processFdPub for talking to trusted process
      //   0xD4: cloneFdPub for talking to trusted process
      //   0xD8: set to non-zero, if in debugging mode
      //   0xDC: most recent SHM id returned by shmget(IPC_PRIVATE)
      //   0xE0: cookie assigned to us by the trusted process (TLS_COOKIE)
      //   0xE8: thread id (TLS_TID)
      //   0xF0: threadFdPub (TLS_THREAD_FD)
      //   0x200-0x1000: securely passed verified file name(s)

      // Layout of (untrusted) scratch space:
      //   0x00: syscall number; passed in %rax
      //   0x04: first argument; passed in %rdi
      //   0x0C: second argument; passed in %rsi
      //   0x14: third argument; passed in %rdx
      //   0x1C: fourth argument; passed in %r10
      //   0x24: fifth argument; passed in %r8
      //   0x2C: sixth argument; passed in %r9
      //   0x34: return value
      //   0x3C: RDTSCP result (%eax)
      //   0x40: RDTSCP result (%edx)
      //   0x44: RDTSCP result (%ecx)
      //   0x48: last system call (not used on x86-64)
      //   0x4C: number of consecutive calls to a time fnc (not used on x86-64)
      //   0x50: nesting level of system calls (for debugging purposes only)
      //   0x54: signal mask
      //   0x5C: in SEGV handler

      // We use the %fs register for accessing the secure read-only page, and
      // the untrusted scratch space immediately following it. The segment
      // register and the local descriptor table is set up by passing
      // appropriate arguments to clone().

    "0:xor  %%rsp, %%rsp\n"
      "mov  $2, %%ebx\n"           // %rbx  = initial sequence number

      // Read request from untrusted thread, or from trusted process. In either
      // case, the data that we read has to be considered untrusted.
      // read(threadFd, &scratch, 4)
    "1:xor  %%rax, %%rax\n"        // NR_read
      "mov  %%r13, %%rdi\n"        // fd  = threadFd
      "mov  %%fs:0x0, %%rsi\n"     // secure_mem
      "add  $0x1000, %%rsi\n"      // buf = &scratch
      "mov  $4, %%edx\n"           // len = 4
    "2:syscall\n"
      "cmp  $-4, %%rax\n"          // EINTR
      "jz   2b\n"
      "cmp  %%rdx, %%rax\n"
      "jnz  25f\n"                 // exit process

      // Retrieve system call number. It is crucial that we only dereference
      // %fs:0x1000 exactly once. Afterwards, memory becomes untrusted and
      // we must use the value that we have read the first time.
      "mov  0(%%rsi), %%eax\n"

      // If syscall number is -1, execute an unlocked system call from the
      // secure memory area
      "cmp  $-1, %%eax\n"
      "jnz  5f\n"
    "3:cmp  %%rbx, %%fs:0x8\n"
      "jne  25f\n"                 // exit process
      "cmp  %%fs:0x10, %%eax\n"
      "jne  25f\n"                 // exit process
      "mov  %%fs:0x18, %%rax\n"
      "mov  %%fs:0x20, %%rdi\n"
      "mov  %%fs:0x28, %%rsi\n"
      "mov  %%fs:0x30, %%rdx\n"
      "mov  %%fs:0x38, %%r10\n"
      "mov  %%fs:0x40, %%r8\n"
      "mov  %%fs:0x48, %%r9\n"
      "cmp  %%rbx, %%fs:0x8\n"
      "jne  25f\n"                 // exit process
      "add  $2, %%rbx\n"

      // shmget() gets some special treatment. Whenever we return from this
      // system call, we remember the most recently returned SysV shm id.
      "cmp  $29, %%eax\n"          // NR_shmget
      "jnz  4f\n"
      "syscall\n"
      "mov  %%rax, %%r8\n"
      "mov  $56, %%eax\n"          // NR_clone
      "mov  $17, %%edi\n"          // flags = SIGCHLD
      "mov  $1, %%esi\n"           // stack = 1
      "syscall\n"
      "test %%rax, %%rax\n"
      "js   25f\n"                 // exit process
      "mov  %%rax, %%rdi\n"
      "jnz  8f\n"                  // wait for child, then return result
      "mov  %%fs:0x0, %%rdi\n"     // start = secure_mem
      "mov  $4096, %%esi\n"        // len   = 4096
      "mov  $3, %%edx\n"           // prot  = PROT_READ | PROT_WRITE
      "mov  $10, %%eax\n"          // NR_mprotect
      "syscall\n"
      "mov  %%r8d, 0xDC(%%rdi)\n"  // set most recently returned SysV shm id
      "xor  %%rdi, %%rdi\n"

      // When debugging messages are enabled, warn about expensive system calls
      #ifndef NDEBUG
      "cmpw $0, %%fs:0xD8\n"       // debug mode
      "jz   27f\n"
      "mov  $1, %%eax\n"           // NR_write
      "mov  $2, %%edi\n"           // fd = stderr
      "lea  101f(%%rip), %%rsi\n"  // "This is an expensive system call"
      "mov  $102f-101f, %%edx\n"   // len = strlen(msg)
      "syscall\n"
      "xor  %%rdi, %%rdi\n"
      #endif

      "jmp  27f\n"                 // exit program, no message
    "4:syscall\n"
      "jmp  15f\n"                 // return result

      // If syscall number is -2, execute locked system call from the
      // secure memory area
    "5:jg   12f\n"
      "cmp  $-2, %%eax\n"
      "jnz  9f\n"
      "cmp  %%rbx, %%fs:0x8\n"
      "jne  25f\n"                 // exit process
      "cmp  %%eax, %%fs:0x10\n"
      "jne  25f\n"                 // exit process

      // When debugging messages are enabled, warn about expensive system calls
      #ifndef NDEBUG
      "cmpw $0, %%fs:0xD8\n"       // debug mode
      "jz   6f\n"
      "mov  $1, %%eax\n"           // NR_write
      "mov  $2, %%edi\n"           // fd = stderr
      "lea  101f(%%rip), %%rsi\n"  // "This is an expensive system call"
      "mov  $102f-101f, %%edx\n"   // len = strlen(msg)
      "syscall\n"
    "6:"
      #endif

      "mov  %%fs:0x18, %%rax\n"
      "mov  %%fs:0x20, %%rdi\n"
      "mov  %%fs:0x28, %%rsi\n"
      "mov  %%fs:0x30, %%rdx\n"
      "mov  %%fs:0x38, %%r10\n"
      "mov  %%fs:0x40, %%r8\n"
      "mov  %%fs:0x48, %%r9\n"
      "cmp  %%rbx, %%fs:0x8\n"
      "jne  25f\n"                 // exit process

      // clone() has unusual calling conventions and must be handled specially
      "cmp  $56, %%rax\n"          // NR_clone
      "jz   19f\n"

      // exit() terminates trusted thread
      "cmp  $60, %%eax\n"          // NR_exit
      "jz   18f\n"

      // Perform requested system call
      "syscall\n"

      // Unlock mutex
    "7:cmp  %%rbx, %%fs:0x8\n"
      "jne  25f\n"                 // exit process
      "add  $2, %%rbx\n"
      "mov  %%rax, %%r8\n"
      "mov  $56, %%eax\n"          // NR_clone
      "mov  $17, %%rdi\n"          // flags = SIGCHLD
      "mov  $1, %%rsi\n"           // stack = 1
      "syscall\n"
      "test %%rax, %%rax\n"
      "js   25f\n"                 // exit process
      "jz   22f\n"                 // unlock and exit
      "mov  %%rax, %%rdi\n"
    "8:xor  %%rsi, %%rsi\n"
      "xor  %%rdx, %%rdx\n"
      "xor  %%r10, %%r10\n"
      "mov  $61, %%eax\n"          // NR_wait4
      "syscall\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   8b\n"
      "mov  %%r8, %%rax\n"
      "jmp  15f\n"                 // return result

      // If syscall number is -3, read the time stamp counter
    "9:cmp  $-3, %%eax\n"
      "jnz  10f\n"
      "rdtsc\n"                    // sets %edx:%eax
      "xor  %%rcx, %%rcx\n"
      "jmp  11f\n"
    "10:cmp  $-4, %%eax\n"
      "jnz  12f\n"
      "rdtscp\n"                   // sets %edx:%eax and %ecx
   "11:add  $0x3C, %%rsi\n"
      "mov  %%eax, 0(%%rsi)\n"
      "mov  %%edx, 4(%%rsi)\n"
      "mov  %%ecx, 8(%%rsi)\n"
      "mov  $12, %%edx\n"
      "jmp  16f\n"                 // return result

      // Check in syscallTable whether this system call is unrestricted
   "12:mov  %%rax, %%r9\n"
      #ifndef NDEBUG
      "cmpw $0, %%fs:0xD8\n"       // debug mode
      "jnz  13f\n"
      #endif
      "cmp  playground$maxSyscall(%%rip), %%eax\n"
      "ja   25f\n"                 // exit process
      "shl  $4, %%rax\n"
      "lea  playground$syscallTable(%%rip), %%rdi\n"
      "add  %%rdi, %%rax\n"
      "mov  0(%%rax), %%rax\n"
      "cmp  $1, %%rax\n"
      "jne  25f\n"                 // exit process

      // Default behavior for unrestricted system calls is to just execute
      // them. Read the remaining arguments first.
   "13:mov  %%rsi, %%r8\n"
      "xor  %%rax, %%rax\n"        // NR_read
      "mov  %%r13, %%rdi\n"        // fd  = threadFd
      "add  $4, %%rsi\n"           // buf = &scratch + 4
      "mov  $48, %%edx\n"          // len = 6*sizeof(void *)
   "14:syscall\n"
      "cmp  $-4, %%rax\n"          // EINTR
      "jz   14b\n"
      "cmp  %%rdx, %%rax\n"
      "jnz  25f\n"                 // exit process
      "mov  %%r9, %%rax\n"
      "mov  0x04(%%r8), %%rdi\n"
      "mov  0x0C(%%r8), %%rsi\n"
      "mov  0x14(%%r8), %%rdx\n"
      "mov  0x1C(%%r8), %%r10\n"
      "mov  0x2C(%%r8), %%r9\n"
      "mov  0x24(%%r8), %%r8\n"
      "cmp  $231, %%rax\n"         // NR_exit_group
      "jz   27f\n"                 // exit program, no message
      "syscall\n"

      // Return result of system call to sandboxed thread
   "15:mov  %%fs:0x0, %%rsi\n"     // secure_mem
      "add  $0x1034, %%rsi\n"      // buf   = &scratch + 52
      "mov  %%rax, (%%rsi)\n"
      "mov  $8, %%edx\n"           // len   = 8
   "16:mov  %%r13, %%rdi\n"        // fd    = threadFd
      "mov  $1, %%eax\n"           // NR_write
   "17:syscall\n"
      "cmp  %%rdx, %%rax\n"
      "jz   1b\n"
      "cmp  $-4, %%rax\n"          // EINTR
      "jz   17b\n"
      "jmp  25f\n"                 // exit process

      // NR_exit:
      // Exit trusted thread after cleaning up resources
   "18:mov  %%fs:0x0, %%rsi\n"     // secure_mem
      "mov  0xF0(%%rsi), %%rdi\n"  // fd     = threadFdPub
      "mov  $3, %%eax\n"           // NR_close
      "syscall\n"
      "mov  %%rsi, %%rdi\n"        // start  = secure_mem
      "mov  $8192, %%esi\n"        // length = 8192
      "xor  %%rdx, %%rdx\n"        // prot   = PROT_NONE
      "mov  $10, %%eax\n"          // NR_mprotect
      "syscall\n"
      "mov  %%r13, %%rdi\n"        // fd     = threadFd
      "mov  $3, %%eax\n"           // NR_close
      "syscall\n"
      "mov  $56, %%eax\n"          // NR_clone
      "mov  $17, %%rdi\n"          // flags = SIGCHLD
      "mov  $1, %%rsi\n"           // stack = 1
      "syscall\n"
      "mov  %%rax, %%rdi\n"
      "test %%rax, %%rax\n"
      "js   27f\n"                 // exit process
      "jne  21f\n"                 // reap helper, exit thread
      "jmp  22f\n"                 // unlock mutex

      // NR_clone:
      // Original trusted thread calls clone() to create new nascent
      // thread. This thread is (typically) fully privileged and shares all
      // resources with the caller (i.e. the previous trusted thread),
      // and by extension it shares all resources with the sandbox'd
      // threads.
   "19:mov  %%fs:0x0, %%rbp\n"     // %rbp  = old_shared_mem
      "mov  %%rsi, %%r15\n"        // remember child stack
      "mov  $1, %%rsi\n"           // stack = 1
      "syscall\n"                  // calls NR_clone
      "cmp  $-4095, %%rax\n"       // return codes -1..-4095 are errno values
      "jae  7b\n"                  // unlock mutex, return result
      "add  $2, %%rbx\n"
      "test %%rax, %%rax\n"
      "jne  15b\n"                 // return result

      // In nascent thread, now.
      "sub  $2, %%rbx\n"

      // We want to maintain an invalid %rsp whenver we access untrusted
      // memory. This ensures that even if an attacker can trick us into
      // triggering a SIGSEGV, we will never successfully execute a signal
      // handler.
      // Signal handlers are inherently dangerous, as an attacker could trick
      // us into returning to the wrong address by adjusting the signal stack
      // right before the handler returns.
      // N.B. While POSIX is curiously silent about this, it appears that on
      // Linux, alternate signal stacks are a per-thread property. That is
      // good. It means that this security mechanism works, even if the
      // sandboxed thread manages to set up an alternate signal stack.
      //
      // TODO(markus): We currently do not support emulating calls to
      // sys_clone() with a zero (i.e. copy) stack parameter. See clone.cc
      // for a discussion on how to fix this, if this ever becomes neccessary.
      "mov  %%r15, %%r9\n"         // %r9 = child_stack
      "xor  %%r15, %%r15\n"        // Request to return from clone() when done

      // Get thread id of nascent thread
   "20:mov  $186, %%eax\n"         // NR_gettid
      "syscall\n"
      "mov  %%rax, %%r14\n"

      // Nascent thread creates socketpair() for sending requests to
      // trusted thread.
      // We can create the filehandles on the child's stack. Filehandles are
      // always treated as untrusted.
      // socketpair(AF_UNIX, SOCK_STREAM, 0, fds)
      "sub  $0x10, %%r9\n"
      "mov  %%r15, 8(%%r9)\n"      // preserve return address on child stack
      "mov  $53, %%eax\n"          // NR_socketpair
      "mov  $1, %%edi\n"           // domain = AF_UNIX
      "mov  $1, %%esi\n"           // type = SOCK_STREAM
      "xor  %%rdx, %%rdx\n"        // protocol = 0
      "mov  %%r9, %%r10\n"         // sv = child_stack
      "syscall\n"
      "test %%rax, %%rax\n"
      "jz   28f\n"

      // If things went wrong, we don't have an (easy) way of signaling
      // the parent. For our purposes, it is sufficient to fail with a
      // fatal error.
      "jmp  25f\n"                 // exit process
   "21:xor  %%rsi, %%rsi\n"
      "xor  %%rdx, %%rdx\n"
      "xor  %%r10, %%r10\n"
      "mov  $61, %%eax\n"          // NR_wait4
      "syscall\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   21b\n"
      "jmp  23f\n"                 // exit thread (no message)
   "22:lea  playground$syscall_mutex(%%rip), %%rdi\n"
      "mov  $4096, %%esi\n"
      "mov  $3, %%edx\n"           // prot = PROT_READ | PROT_WRITE
      "mov  $10, %%eax\n"          // NR_mprotect
      "syscall\n"
      "lock; addl $0x80000000, (%%rdi)\n"
      "jz   23f\n"                 // exit thread
      "mov  $1, %%edx\n"
      "mov  %%rdx, %%rsi\n"        // FUTEX_WAKE
      "mov  $202, %%eax\n"         // NR_futex
      "syscall\n"
   "23:mov  $60, %%eax\n"          // NR_exit
      "mov  $1, %%edi\n"           // status = 1
   "24:syscall\n"
   "25:mov  $1, %%eax\n"           // NR_write
      "mov  $2, %%edi\n"           // fd = stderr
      "lea  100f(%%rip), %%rsi\n"  // "Sandbox violation detected"
      "mov  $101f-100f, %%edx\n"   // len = strlen(msg)
      "syscall\n"
   "26:mov  $1, %%edi\n"
   "27:mov  $231, %%eax\n"         // NR_exit_group
      "jmp  24b\n"

      // The first page is mapped read-only for use as securely shared memory
   "28:mov  0xC8(%%rbp), %%r12\n"  // %r12 = secure shared memory
      "cmp  %%rbx, 8(%%rbp)\n"
      "jne  25b\n"                 // exit process
      "mov  $10, %%eax\n"          // NR_mprotect
      "mov  %%r12, %%rdi\n"        // addr = secure_mem
      "mov  $4096, %%esi\n"        // len  = 4096
      "mov  $1, %%edx\n"           // prot = PROT_READ
      "syscall\n"

      // The second page is used as scratch space by the trusted thread.
      // Make it writable.
      "mov  $10, %%eax\n"          // NR_mprotect
      "add  $4096, %%rdi\n"        // addr = secure_mem + 4096
      "mov  $3, %%edx\n"           // prot = PROT_READ | PROT_WRITE
      "syscall\n"

      // Call clone() to create new trusted thread().
      // clone(CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|
      //       CLONE_SYSVSEM|CLONE_UNTRACED|CLONE_SETTLS, stack, NULL, NULL,
      //       tls)
      "mov  4(%%r9), %%r13d\n"     // %r13  = threadFd (on child's stack)
      "mov  $56, %%eax\n"          // NR_clone
      "mov  $0x8D0F00, %%edi\n"    // flags = VM|FS|FILES|SIGH|THR|SYSV|UTR|TLS
      "mov  $1, %%rsi\n"           // stack = 1
      "mov  %%r12, %%r8\n"         // tls   = new_secure_mem
      "mov  0xD0(%%rbp), %%r15d\n" // %r15  = processFdPub
      "cmp  %%rbx, 8(%%rbp)\n"
      "jne  25b\n"                 // exit process
      "syscall\n"
      "test %%rax, %%rax\n"
      "js   25b\n"                 // exit process
      "jz   0b\n"                  // invoke trustedThreadFnc()

      // Copy the caller's signal mask
      "mov  0x1054(%%rbp), %%rax\n"
      "mov  %%rax, 0x1054(%%r12)\n"

      // Done creating trusted thread. We can now get ready to return to caller
      "mov  %%r9, %%r8\n"          // %r8 = child_stack
      "mov  0(%%r9), %%r9d\n"      // %r9 = threadFdPub

      // Set up thread local storage with information on how to talk to
      // trusted thread and trusted process.
      "lea  0xE0(%%r12), %%rsi\n"  // args   = &secure_mem.TLS;
      "mov  $158, %%eax\n"         // NR_arch_prctl
      "mov  $0x1001, %%edi\n"      // option = ARCH_SET_GS
      "syscall\n"
      "cmp  $-4095, %%rax\n"       // return codes -1..-4095 are errno values
      "jae  25b\n"                 // exit process

      // Check whether this is the initial thread, or a newly created one.
      // At startup we run the same code as when we create a new thread. At
      // the very top of this function, you will find that we push 999(%rip)
      // on the stack. That is the signal that we should return on the same
      // stack rather than return to where clone was called.
      "mov  8(%%r8), %%r15\n"
      "add  $0x10, %%r8\n"
      "test %%r15, %%r15\n"
      "jne  29f\n"

      // Returning from clone() into the newly created thread is special. We
      // cannot unroll the stack, as we just set up a new stack for this
      // thread. We have to explicitly restore CPU registers to the values
      // that they had when the program originally called clone().
      // We patch the register values in the signal stack frame so that we
      // can ask sigreturn() to restore all registers for us.
      "sub  $0x8, %%r8\n"
      "mov  0x50(%%rbp), %%rax\n"
      "mov  %%rax, 0x00(%%r8)\n"   // return address
      "xor  %%rax, %%rax\n"
      "mov  %%rax, 0x98(%%r8)\n"   // %rax = 0
      "mov  0x58(%%rbp), %%rax\n"
      "mov  %%rax, 0x80(%%r8)\n"   // %rbp
      "mov  0x60(%%rbp), %%rax\n"
      "mov  %%rax, 0x88(%%r8)\n"   // %rbx
      "mov  0x68(%%rbp), %%rax\n"
      "mov  %%rax, 0xA0(%%r8)\n"   // %rcx
      "mov  0x70(%%rbp), %%rax\n"
      "mov  %%rax, 0x90(%%r8)\n"   // %rdx
      "mov  0x78(%%rbp), %%rax\n"
      "mov  %%rax, 0x78(%%r8)\n"   // %rsi
      "mov  0x80(%%rbp), %%rax\n"
      "mov  %%rax, 0x70(%%r8)\n"   // %rdi
      "mov  0x88(%%rbp), %%rax\n"
      "mov  %%rax, 0x30(%%r8)\n"   // %r8
      "mov  0x90(%%rbp), %%rax\n"
      "mov  %%rax, 0x38(%%r8)\n"   // %r9
      "mov  0x98(%%rbp), %%rax\n"
      "mov  %%rax, 0x40(%%r8)\n"   // %r10
      "mov  0xA0(%%rbp), %%rax\n"
      "mov  %%rax, 0x48(%%r8)\n"   // %r11
      "mov  0xA8(%%rbp), %%rax\n"
      "mov  %%rax, 0x50(%%r8)\n"   // %r12
      "mov  0xB0(%%rbp), %%rax\n"
      "mov  %%rax, 0x58(%%r8)\n"   // %r13
      "mov  0xB8(%%rbp), %%rax\n"
      "mov  %%rax, 0x60(%%r8)\n"   // %r14
      "mov  0xC0(%%rbp), %%rax\n"
      "mov  %%rax, 0x68(%%r8)\n"   // %r15
      "cmp  %%rbx, 8(%%rbp)\n"
      "jne  25b\n"                 // exit process

      // Nascent thread launches a helper that doesn't share any of our
      // resources, except for pages mapped as MAP_SHARED.
      // clone(SIGCHLD, stack=1)
   "29:mov  $56, %%eax\n"          // NR_clone
      "mov  $17, %%rdi\n"          // flags = SIGCHLD
      "mov  $1, %%rsi\n"           // stack = 1
      "syscall\n"
      "test %%rax, %%rax\n"
      "js   25b\n"                 // exit process
      "jne  31f\n"

      // Use sendmsg() to send to the trusted process the file handles for
      // communicating with the new trusted thread. We also send the address
      // of the secure memory area (for sanity checks) and the thread id.
      "mov  0xD4(%%rbp), %%edi\n"  // transport = Sandbox::cloneFdPub()
      "cmp  %%rbx, 8(%%rbp)\n"
      "jne  25b\n"                 // exit process

      // 0x00 msg:
      //   0x00 msg_name       ($0)
      //   0x08 msg_namelen    ($0)
      //   0x10 msg_iov        (%r8 + 0x44)
      //   0x18 msg_iovlen     ($1)
      //   0x20 msg_control    (%r8 + 0x54)
      //   0x28 msg_controllen ($0x18)
      // 0x30 data:
      //   0x30 msg_flags/err  ($0)
      //   0x34 secure_mem     (%r12)
      //   0x3C threadId       (%r14d)
      //   0x40 threadFdPub    (%r9d)
      // 0x44 iov:
      //   0x44 iov_base       (%r8 + 0x30)
      //   0x4C iov_len        ($0x14)
      // 0x54 cmsg:
      //   0x54 cmsg_len       ($0x18)
      //   0x5C cmsg_level     ($1, SOL_SOCKET)
      //   0x60 cmsg_type      ($1, SCM_RIGHTS)
      //   0x64 threadFdPub    (%r9d)
      //   0x68 threadFd       (%r13d)
      // 0x6C
      "sub  $0x6C, %%r8\n"
      "xor  %%rdx, %%rdx\n"        // flags = 0
      "mov  %%rdx, 0x00(%%r8)\n"   // msg_name
      "mov  %%edx, 0x08(%%r8)\n"   // msg_namelen
      "mov  %%edx, 0x30(%%r8)\n"   // msg_flags
      "mov  $1, %%r11d\n"
      "mov  %%r11, 0x18(%%r8)\n"   // msg_iovlen
      "mov  %%r11d, 0x5C(%%r8)\n"  // cmsg_level
      "mov  %%r11d, 0x60(%%r8)\n"  // cmsg_type
      "lea  0x30(%%r8), %%r11\n"
      "mov  %%r11, 0x44(%%r8)\n"   // iov_base
      "add  $0x14, %%r11\n"
      "mov  %%r11, 0x10(%%r8)\n"   // msg_iov
      "add  $0x10, %%r11\n"
      "mov  %%r11, 0x20(%%r8)\n"   // msg_control
      "mov  $0x14, %%r11d\n"
      "mov  %%r11, 0x4C(%%r8)\n"   // iov_len
      "add  $4, %%r11d\n"
      "mov  %%r11, 0x28(%%r8)\n"   // msg_controllen
      "mov  %%r11, 0x54(%%r8)\n"   // cmsg_len
      "mov  %%r12, 0x34(%%r8)\n"   // secure_mem
      "mov  %%r14d, 0x3C(%%r8)\n"  // threadId
      "mov  %%r9d, 0x40(%%r8)\n"   // threadFdPub
      "mov  %%r9d, 0x64(%%r8)\n"   // threadFdPub
      "mov  %%r13d, 0x68(%%r8)\n"  // threadFd
      "mov  $46, %%eax\n"          // NR_sendmsg
      "mov  %%r8, %%rsi\n"         // msg
      "syscall\n"

      // Release syscall_mutex_. This signals the trusted process that
      // it can write into the original thread's secure memory again.
      "mov  $10, %%eax\n"          // NR_mprotect
      "lea  playground$syscall_mutex(%%rip), %%rdi\n"
      "mov  $4096, %%esi\n"
      "mov  $3, %%edx\n"           // PROT_READ | PROT_WRITE
      "syscall\n"
      "cmp  %%rbx, 8(%%rbp)\n"
      "jne  25b\n"                 // exit process
      "lock; addl $0x80000000, (%%rdi)\n"
      "jz   30f\n"                 // exit process (no error message)
      "mov  $1, %%edx\n"
      "mov  %%rdx, %%rsi\n"        // FUTEX_WAKE
      "mov  $202, %%eax\n"         // NR_futex
      "syscall\n"
   "30:xor  %%rdi, %%rdi\n"
      "jmp  27b\n"                 // exit process (no error message)

      // Reap helper
   "31:mov  %%rax, %%rdi\n"
   "32:lea  -4(%%r8), %%rsi\n"
      "xor  %%rdx, %%rdx\n"
      "xor  %%r10, %%r10\n"
      "mov  $61, %%eax\n"          // NR_wait4
      "syscall\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   32b\n"
      "mov  -4(%%r8), %%eax\n"
      "test %%rax, %%rax\n"
      "jnz  26b\n"                 // exit process (no error message)

      // Release privileges by entering seccomp mode.
      "mov  $157, %%eax\n"         // NR_prctl
      "mov  $22, %%edi\n"          // PR_SET_SECCOMP
      "mov  $1, %%esi\n"
      "syscall\n"
      "test %%rax, %%rax\n"
      "jnz  25b\n"                 // exit process

      // We can finally start using the stack. Signal handlers no longer pose
      // a threat to us.
      "mov  %%r8, %%rsp\n"

      // Back in the newly created sandboxed thread, wait for trusted process
      // to receive request. It is possible for an attacker to make us
      // continue even before the trusted process is done. This is OK. It'll
      // result in us putting stale values into the new thread's TLS. But that
      // data is considered untrusted anyway.
      "push %%rax\n"
      "mov  $1, %%edx\n"           // len       = 1
      "mov  %%rsp, %%rsi\n"        // buf       = %rsp
      "mov  %%r9, %%rdi\n"         // fd        = threadFdPub
   "33:xor  %%rax, %%rax\n"        // NR_read
      "syscall\n"
      "cmp  $-4, %%rax\n"          // EINTR
      "jz   33b\n"
      "cmp  %%rdx, %%rax\n"
      "jne  25b\n"                 // exit process
      "pop  %%rax\n"

      // Return to caller. We are in the new thread, now.
      "test %%r15, %%r15\n"
      "jnz  34f\n"                 // Returning to createTrustedThread()

      // Returning to the place where clone() had been called. We rely on
      // using rt_sigreturn() for restoring our registers. The caller already
      // created a signal stack frame, and we patched the register values
      // with the ones that were in effect prior to calling sandbox_clone().
      "pop %%r15\n"
   "34:mov  %%r15, 0xA8(%%rsp)\n"  // compute new %rip
      "mov  $15, %%eax\n"          // NR_rt_sigreturn
      "syscall\n"

      ".pushsection \".rodata\"\n"
  "100:.ascii \"Sandbox violation detected, program aborted\\n\"\n"
  "101:.ascii \"WARNING! This is an expensive system call\\n\"\n"
  "102:\n"
      ".popsection\n"

  "999:pop  %%rbp\n"
      "pop  %%rbx\n"
      :
      : "g"(&args)
      : "rax", "rcx", "rdx", "rdi", "rsi", "r8", "r9", "r10", "r11", "r12",
        "r13", "r14", "r15", "rsp", "memory"
#elif defined(__i386__)
  struct user_desc u;
  u.entry_number    = (typeof u.entry_number)-1;
  u.base_addr       = 0;
  u.limit           = 0xfffff;
  u.seg_32bit       = 1;
  u.contents        = 0;
  u.read_exec_only  = 0;
  u.limit_in_pages  = 1;
  u.seg_not_present = 0;
  u.useable         = 1;
  SysCalls sys;
  if (sys.set_thread_area(&u) < 0) {
    die("Cannot set up thread local storage");
  }
  asm volatile("movw %w0, %%fs"
      :
      : "q"(8*u.entry_number+3));
  asm volatile(
      "push %%ebx\n"
      "push %%ebp\n"

      // Signal handlers are process-wide. This means that for security
      // reasons, we cannot allow that the trusted thread ever executes any
      // signal handlers.
      // We prevent the execution of signal handlers by setting a signal
      // mask that blocks all signals. In addition, we make sure that the
      // stack pointer is invalid.
      // We cannot reset the signal mask until after we have enabled
      // Seccomp mode. Our sigprocmask() wrapper would normally do this by
      // raising a signal, modifying the signal mask in the kernel-generated
      // signal frame, and then calling sigreturn(). This presents a bit of
      // a Catch-22, as all signals are masked and we can therefore not
      // raise any signal that would allow us to generate the signal stack
      // frame.
      // Instead, we have to create the signal stack frame prior to entering
      // Seccomp mode. This incidentally also helps us to restore the
      // signal mask to the same value that it had prior to entering the
      // sandbox.
      // The signal wrapper for clone() is the second entry point into this
      // code (by means of sending an IPC to its trusted thread). It goes
      // through the same steps of creating a signal stack frame on the
      // newly created thread's stacks prior to cloning. See clone.cc for
      // details.
      "mov  %0, %%edi\n"           // create signal stack before accessing MMX
      "mov  $120+0xF000, %%eax\n"  // __NR_clone + 0xF000
      "mov  %%esp, %%ebp\n"
      "int  $0\n"                  // push a signal stack frame (see clone.cc)
      "mov  %%ebp, 0x1C(%%esp)\n"  // pop stack upon call to sigreturn()
      "mov  %%esp, %%ebp\n"
      "mov  $2, %%ebx\n"           // how     = SIG_SETMASK
      "pushl $-1\n"
      "pushl $-1\n"
      "mov  %%esp, %%ecx\n"        // set     = full mask
      "xor  %%edx, %%edx\n"        // old_set = NULL
      "mov  $8, %%esi\n"           // mask all 64 signals
      "mov  $175, %%eax\n"         // NR_rt_sigprocmask
      "int  $0x80\n"
      "mov  $126, %%eax\n"         // NR_sigprocmask
      "int  $0x80\n"
      "xor  %%esp, %%esp\n"        // invalidate the stack in all trusted code
      "movd %%edi, %%mm6\n"        // %mm6 = args
      "lea  999f, %%edi\n"         // continue in same thread
      "movd %%edi, %%mm3\n"
      "xor  %%edi, %%edi\n"        // initial sequence number
      "movd %%edi, %%mm2\n"
      "jmp  20f\n"                 // create trusted thread

      // TODO(markus): Coalesce the read() operations by reading into a bigger
      // buffer.

      // Parameters:
      //   %mm0: thread's side of threadFd
      //   %mm1: processFdPub
      //   %mm3: return address after creation of new trusted thread
      //   %mm5: secure memory region
      //         the page following this one contains the scratch space

      // Local variables:
      //   %mm2: sequence number for trusted calls
      //   %mm4: thread id

      // Temporary variables:
      //   %ebp: system call number
      //   %mm6: secure memory of previous thread
      //   %mm7: temporary variable for spilling data

      // Layout of secure shared memory region (c.f. securemem.h):
      //   0x00: pointer to the secure shared memory region (i.e. self)
      //   0x04: sequence number; must match %mm2
      //   0x08: call type; must match %eax, iff %eax == -1 || %eax == -2
      //   0x0C: system call number; passed to syscall in %eax
      //   0x10: first argument; passed to syscall in %ebx
      //   0x14: second argument; passed to syscall in %ecx
      //   0x18: third argument; passed to syscall in %edx
      //   0x1C: fourth argument; passed to syscall in %esi
      //   0x20: fifth argument; passed to syscall in %edi
      //   0x24: sixth argument; passed to syscall in %ebp
      //   0x28: stored return address for clone() system call
      //   0x2C: stored %ebp value for clone() system call
      //   0x30: stored %edi value for clone() system call
      //   0x34: stored %esi value for clone() system call
      //   0x38: stored %edx value for clone() system call
      //   0x3C: stored %ecx value for clone() system call
      //   0x40: stored %ebx value for clone() system call
      //   0x44: new shared memory for clone()
      //   0x48: processFdPub for talking to trusted process
      //   0x4C: cloneFdPub for talking to trusted process
      //   0x50: set to non-zero, if in debugging mode
      //   0x54: most recent SHM id returned by shmget(IPC_PRIVATE)
      //   0x58: cookie assigned to us by the trusted process (TLS_COOKIE)
      //   0x60: thread id (TLS_TID)
      //   0x68: threadFdPub (TLS_THREAD_FD)
      //   0x200-0x1000: securely passed verified file name(s)

      // Layout of (untrusted) scratch space:
      //   0x00: syscall number; passed in %eax
      //   0x04: first argument; passed in %ebx
      //   0x08: second argument; passed in %ecx
      //   0x0C: third argument; passed in %edx
      //   0x10: fourth argument; passed in %esi
      //   0x14: fifth argument; passed in %edi
      //   0x18: sixth argument; passed in %ebp
      //   0x1C: return value
      //   0x20: RDTSCP result (%eax)
      //   0x24: RDTSCP result (%edx)
      //   0x28: RDTSCP result (%ecx)
      //   0x2C: last system call (updated in syscall.cc)
      //   0x30: number of consecutive calls to a time fnc. (e.g. gettimeofday)
      //   0x34: nesting level of system calls (for debugging purposes only)
      //   0x38: signal mask
      //   0x40: in SEGV handler

    "0:xor  %%esp, %%esp\n"
      "mov  $2, %%eax\n"           // %mm2 = initial sequence number
      "movd %%eax, %%mm2\n"

      // Read request from untrusted thread, or from trusted process. In either
      // case, the data that we read has to be considered untrusted.
      // read(threadFd, &scratch, 4)
    "1:mov  $3, %%eax\n"           // NR_read
      "movd %%mm0, %%ebx\n"        // fd  = threadFd
      "movd %%mm5, %%ecx\n"        // secure_mem
      "add  $0x1000, %%ecx\n"      // buf = &scratch
      "mov  $4, %%edx\n"           // len = 4
    "2:int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   2b\n"
      "cmp  %%edx, %%eax\n"
      "jnz  25f\n"                 // exit process

      // Retrieve system call number. It is crucial that we only dereference
      // 0x1000(%mm5) exactly once. Afterwards, memory becomes untrusted and
      // we must use the value that we have read the first time.
      "mov  0(%%ecx), %%eax\n"

      // If syscall number is -1, execute an unlocked system call from the
      // secure memory area
      "cmp  $-1, %%eax\n"
      "jnz  5f\n"
    "3:movd %%mm2, %%ebp\n"
      "cmp  %%ebp, 0x4-0x1000(%%ecx)\n"
      "jne  25f\n"                 // exit process
      "cmp  0x08-0x1000(%%ecx), %%eax\n"
      "jne  25f\n"                 // exit process
      "mov  0x0C-0x1000(%%ecx), %%eax\n"
      "mov  0x10-0x1000(%%ecx), %%ebx\n"
      "mov  0x18-0x1000(%%ecx), %%edx\n"
      "mov  0x1C-0x1000(%%ecx), %%esi\n"
      "mov  0x20-0x1000(%%ecx), %%edi\n"
      "mov  0x24-0x1000(%%ecx), %%ebp\n"
      "mov  0x14-0x1000(%%ecx), %%ecx\n"
      "movd %%edi, %%mm4\n"
      "movd %%ebp, %%mm7\n"
      "movd %%mm2, %%ebp\n"
      "movd %%mm5, %%edi\n"
      "cmp  %%ebp, 4(%%edi)\n"
      "jne  25f\n"                 // exit process
      "add  $2, %%ebp\n"
      "movd %%ebp, %%mm2\n"
      "movd %%mm4, %%edi\n"
      "movd %%mm7, %%ebp\n"

      // shmget() gets some special treatment. Whenever we return from this
      // system call, we remember the most recently returned SysV shm id.
      "cmp  $117, %%eax\n"         // NR_ipc
      "jnz  4f\n"
      "cmp  $23, %%ebx\n"          // shmget()
      "jnz  4f\n"
      "int  $0x80\n"
      "mov  %%eax, %%ebp\n"
      "mov  $120, %%eax\n"         // NR_clone
      "mov  $17, %%ebx\n"          // flags = SIGCHLD
      "mov  $1, %%ecx\n"           // stack = 1
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "js   25f\n"                 // exit process
      "mov  %%eax, %%ebx\n"
      "jnz  8f\n"                  // wait for child, then return result
      "movd %%mm5, %%ebx\n"        // start = secure_mem
      "mov  $4096, %%ecx\n"        // len   = 4096
      "mov  $3, %%edx\n"           // prot  = PROT_READ | PROT_WRITE
      "mov  $125, %%eax\n"         // NR_mprotect
      "int  $0x80\n"
      "mov  %%ebp, 0x54(%%ebx)\n"  // set most recently returned SysV shm id
      "xor  %%ebx, %%ebx\n"

      // When debugging messages are enabled, warn about expensive system calls
      #ifndef NDEBUG
      "movd %%mm5, %%ecx\n"
      "cmpw $0, 0x50(%%ecx)\n"     // debug mode
      "jz   27f\n"
      "mov  $4, %%eax\n"           // NR_write
      "mov  $2, %%ebx\n"           // fd = stderr
      "lea  101f, %%ecx\n"         // "This is an expensive system call"
      "mov  $102f-101f, %%edx\n"   // len = strlen(msg)
      "int  $0x80\n"
      "xor  %%ebx, %%ebx\n"
      #endif

      "jmp  27f\n"                 // exit program, no message
    "4:int  $0x80\n"
      "jmp  15f\n"                 // return result

      // If syscall number is -2, execute locked system call from the
      // secure memory area
    "5:jg   12f\n"
      "cmp  $-2, %%eax\n"
      "jnz  9f\n"
      "movd %%mm2, %%ebp\n"
      "cmp  %%ebp, 0x4-0x1000(%%ecx)\n"
      "jne  25f\n"                 // exit process
      "cmp  %%eax, 0x8-0x1000(%%ecx)\n"
      "jne  25f\n"                 // exit process

      // When debugging messages are enabled, warn about expensive system calls
      #ifndef NDEBUG
      "cmpw $0, 0x50-0x1000(%%ecx)\n"
      "jz   6f\n"                  // debug mode
      "mov  %%ecx, %%ebp\n"
      "mov  $4, %%eax\n"           // NR_write
      "mov  $2, %%ebx\n"           // fd = stderr
      "lea  101f, %%ecx\n"         // "This is an expensive system call"
      "mov  $102f-101f, %%edx\n"   // len = strlen(msg)
      "int  $0x80\n"
      "mov  %%ebp, %%ecx\n"
   "6:"
      #endif

      "mov  0x0C-0x1000(%%ecx), %%eax\n"
      "mov  0x10-0x1000(%%ecx), %%ebx\n"
      "mov  0x18-0x1000(%%ecx), %%edx\n"
      "mov  0x1C-0x1000(%%ecx), %%esi\n"
      "mov  0x20-0x1000(%%ecx), %%edi\n"
      "mov  0x24-0x1000(%%ecx), %%ebp\n"
      "mov  0x14-0x1000(%%ecx), %%ecx\n"
      "movd %%edi, %%mm4\n"
      "movd %%ebp, %%mm7\n"
      "movd %%mm2, %%ebp\n"
      "movd %%mm5, %%edi\n"
      "cmp  %%ebp, 4(%%edi)\n"
      "jne  25f\n"                 // exit process

      // clone() has unusual calling conventions and must be handled specially
      "cmp  $120, %%eax\n"         // NR_clone
      "jz   19f\n"

      // exit() terminates trusted thread
      "cmp  $1, %%eax\n"           // NR_exit
      "jz   18f\n"

      // Perform requested system call
      "movd %%mm4, %%edi\n"
      "movd %%mm7, %%ebp\n"
      "int  $0x80\n"

      // Unlock mutex
    "7:movd %%mm2, %%ebp\n"
      "movd %%mm5, %%edi\n"
      "cmp  %%ebp, 4(%%edi)\n"
      "jne  25f\n"                 // exit process
      "add  $2, %%ebp\n"
      "movd %%ebp, %%mm2\n"
      "mov  %%eax, %%ebp\n"
      "mov  $120, %%eax\n"         // NR_clone
      "mov  $17, %%ebx\n"          // flags = SIGCHLD
      "mov  $1, %%ecx\n"           // stack = 1
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "js   25f\n"                 // exit process
      "jz   22f\n"                 // unlock and exit
      "mov  %%eax, %%ebx\n"
    "8:xor  %%ecx, %%ecx\n"
      "xor  %%edx, %%edx\n"
      "mov  $7, %%eax\n"           // NR_waitpid
      "int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   8b\n"
      "mov  %%ebp, %%eax\n"
      "jmp  15f\n"                 // return result

      // If syscall number is -3, read the time stamp counter
    "9:cmp  $-3, %%eax\n"
      "jnz  10f\n"
      "rdtsc\n"                    // sets %edx:%eax
      "xor  %%ecx, %%ecx\n"
      "jmp  11f\n"
   "10:cmp  $-4, %%eax\n"
      "jnz  12f\n"
      "rdtscp\n"                   // sets %edx:%eax and %ecx
   "11:movd %%mm5, %%ebx\n"
      "add  $0x1020, %%ebx\n"
      "mov  %%eax, 0(%%ebx)\n"
      "mov  %%edx, 4(%%ebx)\n"
      "mov  %%ecx, 8(%%ebx)\n"
      "mov  %%ebx, %%ecx\n"
      "mov  $12, %%edx\n"
      "jmp  16f\n"                 // return result

      // Check in syscallTable whether this system call is unrestricted
   "12:mov  %%eax, %%ebp\n"
      #ifndef NDEBUG
      "cmpw $0, 0x50-0x1000(%%ecx)\n"
      "jnz  13f\n"                 // debug mode
      #endif
      "cmp  playground$maxSyscall, %%eax\n"
      "ja   25f\n"                 // exit process
      "shl  $3, %%eax\n"
      "add  $playground$syscallTable, %%eax\n"
      "mov  0(%%eax), %%eax\n"
      "cmp  $1, %%eax\n"
      "jne  25f\n"                 // exit process

      // Default behavior for unrestricted system calls is to just execute
      // them. Read the remaining arguments first.
   "13:mov  $3, %%eax\n"           // NR_read
      "movd %%mm0, %%ebx\n"        // fd  = threadFd
      "add  $4, %%ecx\n"           // buf = &scratch + 4
      "mov  $24, %%edx\n"          // len = 6*sizeof(void *)
   "14:int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   14b\n"
      "cmp  %%edx, %%eax\n"
      "jnz  25f\n"                 // exit process
      "mov  %%ebp, %%eax\n"
      "mov  0x00(%%ecx), %%ebx\n"
      "mov  0x08(%%ecx), %%edx\n"
      "mov  0x0C(%%ecx), %%esi\n"
      "mov  0x10(%%ecx), %%edi\n"
      "mov  0x14(%%ecx), %%ebp\n"
      "mov  0x04(%%ecx), %%ecx\n"
      "cmp  $252, %%eax\n"         // NR_exit_group
      "jz   27f\n"                 // exit program, no message
      "int  $0x80\n"

      // Return result of system call to sandboxed thread
   "15:movd %%mm5, %%ecx\n"        // secure_mem
      "add  $0x101C, %%ecx\n"      // buf   = &scratch + 28
      "mov  %%eax, (%%ecx)\n"
      "mov  $4, %%edx\n"           // len   = 4
   "16:movd %%mm0, %%ebx\n"        // fd    = threadFd
      "mov  $4, %%eax\n"           // NR_write
   "17:int  $0x80\n"
      "cmp  %%edx, %%eax\n"
      "jz   1b\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   17b\n"
      "jmp  25f\n"                 // exit process

      // NR_exit:
      // Exit trusted thread after cleaning up resources
   "18:mov  %%edi, %%ecx\n"        // secure_mem
      "mov  0x68(%%ecx), %%ebx\n"  // fd     = threadFdPub
      "mov  $6, %%eax\n"           // NR_close
      "int  $0x80\n"
      "mov  %%ecx, %%ebx\n"        // start  = secure_mem
      "mov  $8192, %%ecx\n"        // length = 8192
      "xor  %%edx, %%edx\n"        // prot   = PROT_NONE
      "mov  $125, %%eax\n"         // NR_mprotect
      "int  $0x80\n"
      "movd %%mm0, %%ebx\n"        // fd     = threadFd
      "mov  $6, %%eax\n"           // NR_close
      "int  $0x80\n"
      "mov  $120, %%eax\n"         // NR_clone
      "mov  $17, %%ebx\n"          // flags = SIGCHLD
      "mov  $1, %%ecx\n"           // stack = 1
      "int  $0x80\n"
      "mov  %%eax, %%ebx\n"
      "test %%eax, %%eax\n"
      "js   25f\n"                 // exit process
      "jne  21f\n"                 // reap helper, exit thread
      "jmp  22f\n"                 // unlock mutex

      // NR_clone:
      // Original trusted thread calls clone() to create new nascent
      // thread. This thread is (typically) fully privileged and shares all
      // resources with the caller (i.e. the previous trusted thread),
      // and by extension it shares all resources with the sandbox'd
      // threads.
   "19:movd %%edi, %%mm6\n"        // %mm6  = old_shared_mem
      "movd %%mm4, %%edi\n"        // child_tidptr
      "mov  %%ecx, %%ebp\n"        // remember child stack
      "mov  $1, %%ecx\n"           // stack = 1
      "int  $0x80\n"               // calls NR_clone
      "cmp  $-4095, %%eax\n"       // return codes -1..-4095 are errno values
      "jae  7b\n"                  // unlock mutex, return result
      "movd %%mm2, %%edi\n"
      "add  $2, %%edi\n"
      "movd %%edi, %%mm2\n"
      "test %%eax, %%eax\n"
      "jne  15b\n"                 // return result

      // In nascent thread, now.
      "sub  $2, %%edi\n"
      "movd %%edi, %%mm2\n"

      // We want to maintain an invalid %esp whenver we access untrusted
      // memory. This ensures that even if an attacker can trick us into
      // triggering a SIGSEGV, we will never successfully execute a signal
      // handler.
      // Signal handlers are inherently dangerous, as an attacker could trick
      // us into returning to the wrong address by adjusting the signal stack
      // right before the handler returns.
      // N.B. While POSIX is curiously silent about this, it appears that on
      // Linux, alternate signal stacks are a per-thread property. That is
      // good. It means that this security mechanism works, even if the
      // sandboxed thread manages to set up an alternate signal stack.
      //
      // TODO(markus): We currently do not support emulating calls to
      // sys_clone() with a zero (i.e. copy) stack parameter. See clone.cc
      // for a discussion on how to fix this, if this ever becomes neccessary.
      "movd %%eax, %%mm3\n"        // Request to return from clone() when done

      // Get thread id of nascent thread
   "20:mov  $224, %%eax\n"         // NR_gettid
      "int  $0x80\n"
      "movd %%eax, %%mm4\n"

      // Nascent thread creates socketpair() for sending requests to
      // trusted thread.
      // We can create the filehandles on the child's stack. Filehandles are
      // always treated as untrusted.
      // socketpair(AF_UNIX, SOCK_STREAM, 0, fds)
      "mov  $102, %%eax\n"         // NR_socketcall
      "mov  $8, %%ebx\n"           // socketpair
      "sub  $8, %%ebp\n"           // sv       = child_stack
      "mov  %%ebp, -0x04(%%ebp)\n"
      "movl $0, -0x08(%%ebp)\n"    // protocol = 0
      "movl $1, -0x0C(%%ebp)\n"    // type     = SOCK_STREAM
      "movl $1, -0x10(%%ebp)\n"    // domain   = AF_UNIX
      "lea  -0x10(%%ebp), %%ecx\n"
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "jz   28f\n"

      // If things went wrong, we don't have an (easy) way of signaling
      // the parent. For our purposes, it is sufficient to fail with a
      // fatal error.
      "jmp  25f\n"                 // exit process
   "21:xor  %%ecx, %%ecx\n"
      "xor  %%edx, %%edx\n"
      "mov  $7, %%eax\n"           // NR_waitpid
      "int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   21b\n"
      "jmp  23f\n"                 // exit thread (no message)
   "22:lea  playground$syscall_mutex, %%ebx\n"
      "mov  $4096, %%ecx\n"
      "mov  $3, %%edx\n"           // prot = PROT_READ | PROT_WRITE
      "mov  $125, %%eax\n"         // NR_mprotect
      "int  $0x80\n"
      "lock; addl $0x80000000, (%%ebx)\n"
      "jz   23f\n"                 // exit thread
      "mov  $1, %%edx\n"
      "mov  %%edx, %%ecx\n"        // FUTEX_WAKE
      "mov  $240, %%eax\n"         // NR_futex
      "int  $0x80\n"
   "23:mov  $1, %%eax\n"           // NR_exit
      "mov  $1, %%ebx\n"           // status = 1
   "24:int  $0x80\n"
   "25:mov  $4, %%eax\n"           // NR_write
      "mov  $2, %%ebx\n"           // fd = stderr
      "lea  100f, %%ecx\n"         // "Sandbox violation detected"
      "mov  $101f-100f, %%edx\n"   // len = strlen(msg)
      "int  $0x80\n"
   "26:mov  $1, %%ebx\n"
   "27:mov  $252, %%eax\n"         // NR_exit_group
      "jmp  24b\n"

      // The first page is mapped read-only for use as securely shared memory
   "28:movd %%mm6, %%edi\n"        // %edi = old_shared_mem
      "mov  0x44(%%edi), %%ebx\n"  // addr = secure_mem
      "movd %%ebx, %%mm5\n"        // %mm5 = secure_mem
      "movd %%mm2, %%esi\n"
      "cmp  %%esi, 4(%%edi)\n"
      "jne  25b\n"                 // exit process
      "mov  $125, %%eax\n"         // NR_mprotect
      "mov  $4096, %%ecx\n"        // len  = 4096
      "mov  $1, %%edx\n"           // prot = PROT_READ
      "int  $0x80\n"

      // The second page is used as scratch space by the trusted thread.
      // Make it writable.
      "mov  $125, %%eax\n"         // NR_mprotect
      "add  $4096, %%ebx\n"        // addr = secure_mem + 4096
      "mov  $3, %%edx\n"           // prot = PROT_READ | PROT_WRITE
      "int  $0x80\n"

      // Call clone() to create new trusted thread().
      // clone(CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|
      //       CLONE_SYSVSEM|CLONE_UNTRACED, stack, NULL, NULL, NULL)
      "mov  4(%%ebp), %%eax\n"     // threadFd (on child's stack)
      "movd %%eax, %%mm0\n"        // %mm0  = threadFd
      "mov  $120, %%eax\n"         // NR_clone
      "mov  $0x850F00, %%ebx\n"    // flags = VM|FS|FILES|SIGH|THR|SYSV|UTR
      "mov  $1, %%ecx\n"           // stack = 1
      "movd 0x48(%%edi), %%mm1\n"  // %mm1  = processFdPub
      "cmp  %%esi, 4(%%edi)\n"
      "jne  25b\n"                 // exit process
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "js   25b\n"                 // exit process
      "jz   0b\n"                  // invoke trustedThreadFnc()

      // Set up thread local storage
      "mov  $0x51, %%eax\n"        // seg_32bit, limit_in_pages, useable
      "mov  %%eax, -0x04(%%ebp)\n"
      "mov  $0xFFFFF, %%eax\n"     // limit
      "mov  %%eax, -0x08(%%ebp)\n"
      "movd %%mm5, %%eax\n"
      "add  $0x58, %%eax\n"
      "mov  %%eax, -0x0C(%%ebp)\n" // base_addr = &secure_mem.TLS
      "mov  %%fs, %%eax\n"
      "shr  $3, %%eax\n"
      "mov  %%eax, -0x10(%%ebp)\n" // entry_number
      "mov  $243, %%eax\n"         // NR_set_thread_area
      "lea  -0x10(%%ebp), %%ebx\n"
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "jnz  25b\n"                 // exit process

      // Copy the caller's signal mask
      "movd %%mm5, %%edx\n"
      "mov  0x1038(%%edi), %%eax\n"
      "mov  %%eax, 0x1038(%%edx)\n"
      "mov  0x103C(%%edi), %%eax\n"
      "mov  %%eax, 0x103C(%%edx)\n"

      // Done creating trusted thread. We can now get ready to return to caller
      "mov  0(%%ebp), %%esi\n"     // %esi = threadFdPub
      "add  $8, %%ebp\n"

      // Check whether this is the initial thread, or a newly created one.
      // At startup we run the same code as when we create a new thread. At
      // the very top of this function, you will find that we store 999f
      // in %%mm3. That is the signal that we should return on the same
      // stack rather than return to where clone was called.
      "movd %%mm3, %%eax\n"
      "movd %%mm2, %%edx\n"
      "test %%eax, %%eax\n"
      "jne  29f\n"

      // Returning from clone() into the newly created thread is special. We
      // cannot unroll the stack, as we just set up a new stack for this
      // thread. We have to explicitly restore CPU registers to the values
      // that they had when the program originally called clone().
      // We patch the register values in the signal stack frame so that we
      // can ask sigreturn() to restore all registers for us.
      "sub  $0x4, %%ebp\n"
      "mov  0x28(%%edi), %%eax\n"
      "mov  %%eax, 0x00(%%ebp)\n"  // return address
      "xor  %%eax, %%eax\n"
      "mov  %%eax, 0x30(%%ebp)\n"  // %eax = 0
      "mov  0x2C(%%edi), %%eax\n"
      "mov  %%eax, 0x1C(%%ebp)\n"  // %ebp
      "mov  0x30(%%edi), %%eax\n"
      "mov  %%eax, 0x14(%%ebp)\n"  // %edi
      "mov  0x34(%%edi), %%eax\n"
      "mov  %%eax, 0x18(%%ebp)\n"  // %esi
      "mov  0x38(%%edi), %%eax\n"
      "mov  %%eax, 0x28(%%ebp)\n"  // %edx
      "mov  0x3C(%%edi), %%eax\n"
      "mov  %%eax, 0x2C(%%ebp)\n"  // %ecx
      "mov  0x40(%%edi), %%eax\n"
      "mov  %%eax, 0x24(%%ebp)\n"  // %ebx
      "cmp  %%edx, 4(%%edi)\n"
      "jne  25b\n"                 // exit process

      // Nascent thread launches a helper that doesn't share any of our
      // resources, except for pages mapped as MAP_SHARED.
      // clone(SIGCHLD, stack=1)
   "29:mov  $120, %%eax\n"         // NR_clone
      "mov  $17, %%ebx\n"          // flags = SIGCHLD
      "mov  $1, %%ecx\n"           // stack = 1
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "js   25b\n"                 // exit process
      "jne  31f\n"

      // Use sendmsg() to send to the trusted process the file handles for
      // communicating with the new trusted thread. We also send the address
      // of the secure memory area (for sanity checks) and the thread id.
      "cmp  %%edx, 4(%%edi)\n"
      "jne  25b\n"                 // exit process

      // 0x00 socketcall:
      //   0x00 socket         (0x4C(%edi))
      //   0x04 msg            (%ecx + 0x0C)
      //   0x08 flags          ($0)
      // 0x0C msg:
      //   0x0C msg_name       ($0)
      //   0x10 msg_namelen    ($0)
      //   0x14 msg_iov        (%ecx + 0x34)
      //   0x18 msg_iovlen     ($1)
      //   0x1C msg_control    (%ecx + 0x3C)
      //   0x20 msg_controllen ($0x14)
      // 0x24 data:
      //   0x24 msg_flags/err  ($0)
      //   0x28 secure_mem     (%mm5)
      //   0x2C threadId       (%mm4)
      //   0x30 threadFdPub    (%esi)
      // 0x34 iov:
      //   0x34 iov_base       (%ecx + 0x24)
      //   0x38 iov_len        ($0x10)
      // 0x3C cmsg:
      //   0x3C cmsg_len       ($0x14)
      //   0x40 cmsg_level     ($1, SOL_SOCKET)
      //   0x44 cmsg_type      ($1, SCM_RIGHTS)
      //   0x48 threadFdPub    (%esi)
      //   0x4C threadFd       (%mm0)
      // 0x50
      "lea  -0x50(%%ebp), %%ecx\n"
      "xor  %%eax, %%eax\n"
      "mov  %%eax, 0x08(%%ecx)\n"  // flags
      "mov  %%eax, 0x0C(%%ecx)\n"  // msg_name
      "mov  %%eax, 0x10(%%ecx)\n"  // msg_namelen
      "mov  %%eax, 0x24(%%ecx)\n"  // msg_flags
      "inc  %%eax\n"
      "mov  %%eax, 0x18(%%ecx)\n"  // msg_iovlen
      "mov  %%eax, 0x40(%%ecx)\n"  // cmsg_level
      "mov  %%eax, 0x44(%%ecx)\n"  // cmsg_type
      "movl $0x10, 0x38(%%ecx)\n"  // iov_len
      "mov  $0x14, %%eax\n"
      "mov  %%eax, 0x20(%%ecx)\n"  // msg_controllen
      "mov  %%eax, 0x3C(%%ecx)\n"  // cmsg_len
      "mov  0x4C(%%edi), %%eax\n"  // cloneFdPub
      "mov  %%eax, 0x00(%%ecx)\n"  // socket
      "lea  0x0C(%%ecx), %%eax\n"
      "mov  %%eax, 0x04(%%ecx)\n"  // msg
      "add  $0x18, %%eax\n"
      "mov  %%eax, 0x34(%%ecx)\n"  // iov_base
      "add  $0x10, %%eax\n"
      "mov  %%eax, 0x14(%%ecx)\n"  // msg_iov
      "add  $8, %%eax\n"
      "mov  %%eax, 0x1C(%%ecx)\n"  // msg_control
      "mov  %%esi, 0x30(%%ecx)\n"  // threadFdPub
      "mov  %%esi, 0x48(%%ecx)\n"  // threadFdPub
      "movd %%mm5, %%eax\n"
      "mov  %%eax, 0x28(%%ecx)\n"  // secure_mem
      "movd %%mm4, %%eax\n"
      "mov  %%eax, 0x2C(%%ecx)\n"  // threadId
      "movd %%mm0, %%eax\n"
      "mov  %%eax, 0x4C(%%ecx)\n"  // threadFd
      "mov  $16, %%ebx\n"          // sendmsg()
      "mov  $102, %%eax\n"         // NR_socketcall
      "int  $0x80\n"

      // Release syscall_mutex_. This signals the trusted process that
      // it can write into the original thread's secure memory again.
      "mov  $125, %%eax\n"         // NR_mprotect
      "lea  playground$syscall_mutex, %%ebx\n"
      "mov  $4096, %%ecx\n"
      "mov  $3, %%edx\n"           // PROT_READ | PROT_WRITE
      "int  $0x80\n"
      "movd %%mm2, %%edx\n"
      "cmp  %%edx, 0x4(%%edi)\n"
      "jnz  25b\n"                 // exit process
      "lock; addl $0x80000000, (%%ebx)\n"
      "jz   30f\n"                 // exit process (no error message)
      "mov  $1, %%edx\n"
      "mov  %%edx, %%ecx\n"        // FUTEX_WAKE
      "mov  $240, %%eax\n"         // NR_futex
      "int  $0x80\n"
   "30:xor  %%ebx, %%ebx\n"
      "jmp  27b\n"                 // exit process (no error message)

      // Reap helper
   "31:mov  %%eax, %%ebx\n"
   "32:lea  -4(%%ebp), %%ecx\n"
      "xor  %%edx, %%edx\n"
      "mov  $7, %%eax\n"           // NR_waitpid
      "int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   32b\n"
      "mov  -4(%%ebp), %%eax\n"
      "test %%eax, %%eax\n"
      "jnz  26b\n"                 // exit process (no error message)

      // Release privileges by entering seccomp mode.
   "33:mov  $172, %%eax\n"         // NR_prctl
      "mov  $22, %%ebx\n"          // PR_SET_SECCOMP
      "mov  $1, %%ecx\n"
      "int  $0x80\n"
      "test %%eax, %%eax\n"
      "jnz  25b\n"                 // exit process

      // We can finally start using the stack. Signal handlers no longer pose
      // a threat to us.
      "mov  %%ebp, %%esp\n"

      // Back in the newly created sandboxed thread, wait for trusted process
      // to receive request. It is possible for an attacker to make us
      // continue even before the trusted process is done. This is OK. It'll
      // result in us putting stale values into the new thread's TLS. But that
      // data is considered untrusted anyway.
      "push %%eax\n"
      "mov  $1, %%edx\n"           // len       = 1
      "mov  %%esp, %%ecx\n"        // buf       = %esp
      "mov  %%esi, %%ebx\n"        // fd        = threadFdPub
   "34:mov  $3, %%eax\n"           // NR_read
      "int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   34b\n"
      "cmp  %%edx, %%eax\n"
      "jne  25b\n"                 // exit process
      "pop  %%eax\n"

      // Return to caller. We are in the new thread, now.
      "movd %%mm3, %%ebx\n"
      "test %%ebx, %%ebx\n"
      "jnz  35f\n"                 // Returning to createTrustedThread()

      // Returning to the place where clone() had been called. We rely on
      // using sigreturn() for restoring our registers. The caller already
      // created a signal stack frame, and we patched the register values
      // with the ones that were in effect prior to calling sandbox_clone().
      "pop  %%ebx\n"
   "35:mov  %%ebx, 0x38(%%esp)\n"  // compute new %eip
      "mov  $119, %%eax\n"         // NR_sigreturn
      "int  $0x80\n"

      ".pushsection \".rodata\"\n"
  "100:.ascii \"Sandbox violation detected, program aborted\\n\"\n"
  "101:.ascii \"WARNING! This is an expensive system call\\n\"\n"
  "102:\n"
      ".popsection\n"

  "999:pop  %%ebp\n"
      "pop  %%ebx\n"
      :
      : "g"(&args)
      : "eax", "ecx", "edx", "edi", "esi", "esp", "memory"
#else
#error Unsupported target platform
#endif
);
}

} // namespace
