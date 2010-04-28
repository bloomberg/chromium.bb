// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "library.h"
#include "sandbox_impl.h"
#include "syscall_table.h"

namespace playground {

// Global variables
int                           Sandbox::proc_self_maps_ = -1;
enum Sandbox::SandboxStatus   Sandbox::status_ = STATUS_UNKNOWN;
int                           Sandbox::pid_;
int                           Sandbox::processFdPub_;
int                           Sandbox::cloneFdPub_;
Sandbox::ProtectedMap         Sandbox::protectedMap_;
std::vector<SecureMem::Args*> Sandbox::secureMemPool_;

bool Sandbox::sendFd(int transport, int fd0, int fd1, const void* buf,
                     size_t len) {
  int fds[2], count                     = 0;
  if (fd0 >= 0) { fds[count++]          = fd0; }
  if (fd1 >= 0) { fds[count++]          = fd1; }
  if (!count) {
    return false;
  }
  char cmsg_buf[CMSG_SPACE(count*sizeof(int))];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  struct SysCalls::kernel_iovec  iov[2] = { { 0 } };
  struct SysCalls::kernel_msghdr msg    = { 0 };
  int dummy                             = 0;
  iov[0].iov_base                       = &dummy;
  iov[0].iov_len                        = sizeof(dummy);
  if (buf && len > 0) {
    iov[1].iov_base                     = const_cast<void *>(buf);
    iov[1].iov_len                      = len;
  }
  msg.msg_iov                           = iov;
  msg.msg_iovlen                        = (buf && len > 0) ? 2 : 1;
  msg.msg_control                       = cmsg_buf;
  msg.msg_controllen                    = CMSG_LEN(count*sizeof(int));
  struct cmsghdr *cmsg                  = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level                      = SOL_SOCKET;
  cmsg->cmsg_type                       = SCM_RIGHTS;
  cmsg->cmsg_len                        = CMSG_LEN(count*sizeof(int));
  memcpy(CMSG_DATA(cmsg), fds, count*sizeof(int));
  SysCalls sys;
  return NOINTR_SYS(sys.sendmsg(transport, &msg, 0)) ==
      (ssize_t)(sizeof(dummy) + ((buf && len > 0) ? len : 0));
}

bool Sandbox::getFd(int transport, int* fd0, int* fd1, void* buf, size_t*len) {
  int count                            = 0;
  int *err                             = NULL;
  if (fd0) {
    count++;
    err                                = fd0;
    *fd0                               = -1;
  }
  if (fd1) {
    if (!count++) {
      err                              = fd1;
    }
    *fd1                               = -1;
  }
  if (!count) {
    return false;
  }
  char cmsg_buf[CMSG_SPACE(count*sizeof(int))];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  struct SysCalls::kernel_iovec iov[2] = { { 0 } };
  struct SysCalls::kernel_msghdr msg   = { 0 };
  iov[0].iov_base                      = err;
  iov[0].iov_len                       = sizeof(int);
  if (buf && len && *len > 0) {
    iov[1].iov_base                    = buf;
    iov[1].iov_len                     = *len;
  }
  msg.msg_iov                          = iov;
  msg.msg_iovlen                       = (buf && len && *len > 0) ? 2 : 1;
  msg.msg_control                      = cmsg_buf;
  msg.msg_controllen                   = CMSG_LEN(count*sizeof(int));
  SysCalls sys;
  ssize_t bytes = NOINTR_SYS(sys.recvmsg(transport, &msg, 0));
  if (len) {
    *len                               = bytes > (int)sizeof(int) ?
                                           bytes - sizeof(int) : 0;
  }
  if (bytes != (ssize_t)(sizeof(int) + ((buf && len && *len > 0) ? *len : 0))){
    *err                               = bytes >= 0 ? 0 : -EBADF;
    return false;
  }
  if (*err) {
    // "err" is the first four bytes of the payload. If these are non-zero,
    // the sender on the other side of the socketpair sent us an errno value.
    // We don't expect to get any file handles in this case.
    return false;
  }
  struct cmsghdr *cmsg               = CMSG_FIRSTHDR(&msg);
  if ((msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) ||
      !cmsg                                    ||
      cmsg->cmsg_level != SOL_SOCKET           ||
      cmsg->cmsg_type  != SCM_RIGHTS           ||
      cmsg->cmsg_len   != CMSG_LEN(count*sizeof(int))) {
    *err                             = -EBADF;
    return false;
  }
  if (fd1) { *fd1 = ((int *)CMSG_DATA(cmsg))[--count]; }
  if (fd0) { *fd0 = ((int *)CMSG_DATA(cmsg))[--count]; }
  return true;
}

void Sandbox::setupSignalHandlers() {
  // Set SIGCHLD to SIG_DFL so that waitpid() can work
  SysCalls sys;
  struct SysCalls::kernel_sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler_ = SIG_DFL;
  sys.sigaction(SIGCHLD, &sa, NULL);

  // Set up SEGV handler for dealing with RDTSC instructions, system calls
  // that have been rewritten to use INT0, and for sigpending() emulation.
  sa.sa_sigaction_ = segv();
  sa.sa_flags      = SA_SIGINFO;
  sys.sigaction(SIGSEGV, &sa, NULL);

  // Unblock SIGSEGV and SIGCHLD
  SysCalls::kernel_sigset_t mask;
  memset(&mask, 0x00, sizeof(mask));
  mask.sig[0] |= (1 << (SIGSEGV - 1)) | (1 << (SIGCHLD - 1));
  sys.sigprocmask(SIG_UNBLOCK, &mask, 0);
}

void (*Sandbox::segv())(int signo, SysCalls::siginfo *context, void *unused) {
  void (*fnc)(int signo, SysCalls::siginfo *context, void *unused);
  asm volatile(
      "call 999f\n"
#if defined(__x86_64__)
      // Inspect instruction at the point where the segmentation fault
      // happened. If it is RDTSC, forward the request to the trusted
      // thread.
      "mov  $-3, %%r14\n"          // request for RDTSC
      "mov  0xB0(%%rsp), %%r15\n"  // %rip at time of segmentation fault
      "cmpw $0x310F, (%%r15)\n"    // RDTSC
      "jz   0f\n"
      "cmpw $0x010F, (%%r15)\n"    // RDTSCP
      "jnz  8f\n"
      "cmpb $0xF9, 2(%%r15)\n"
      "jnz  8f\n"
      "mov  $-4, %%r14\n"          // request for RDTSCP
    "0:"
#ifndef NDEBUG
      "lea  100f(%%rip), %%rdi\n"
      "call playground$debugMessage\n"
#endif
      "sub  $4, %%rsp\n"
      "push %%r14\n"
      "mov  %%gs:16, %%edi\n"      // fd  = threadFdPub
      "mov  %%rsp, %%rsi\n"        // buf = %rsp
      "mov  $4, %%edx\n"           // len = sizeof(int)
    "1:mov  $1, %%eax\n"           // NR_write
      "syscall\n"
      "cmp  %%rax, %%rdx\n"
      "jz   5f\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   1b\n"
    "2:add  $12, %%rsp\n"
      "movq $0, 0x98(%%rsp)\n"     // %rax at time of segmentation fault
      "movq $0, 0x90(%%rsp)\n"     // %rdx at time of segmentation fault
      "cmpw $0x310F, (%%r15)\n"    // RDTSC
      "jz   3f\n"
      "movq $0, 0xA0(%%rsp)\n"     // %rcx at time of segmentation fault
    "3:addq $2, 0xB0(%%rsp)\n"     // %rip at time of segmentation fault
      "cmpw $0x010F, (%%r15)\n"    // RDTSC
      "jnz  4f\n"
      "addq $1, 0xB0(%%rsp)\n"     // %rip at time of segmentation fault
    "4:ret\n"
    "5:mov  $12, %%edx\n"          // len = 3*sizeof(int)
    "6:mov  $0, %%eax\n"           // NR_read
      "syscall\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   6b\n"
      "cmp  %%rax, %%rdx\n"
      "jnz  2b\n"
      "mov  0(%%rsp), %%eax\n"
      "mov  4(%%rsp), %%edx\n"
      "mov  8(%%rsp), %%ecx\n"
      "add  $12, %%rsp\n"
      "mov  %%rdx, 0x90(%%rsp)\n"  // %rdx at time of segmentation fault
      "cmpw $0x310F, (%%r15)\n"    // RDTSC
      "jz   7f\n"
      "mov  %%rcx, 0xA0(%%rsp)\n"  // %rcx at time of segmentation fault
    "7:mov  %%rax, 0x98(%%rsp)\n"  // %rax at time of segmentation fault
      "jmp  3b\n"

      // If the instruction is INT 0, then this was probably the result
      // of playground::Library being unable to find a way to safely
      // rewrite the system call instruction. Retrieve the CPU register
      // at the time of the segmentation fault and invoke syscallWrapper().
    "8:cmpw $0x00CD, (%%r15)\n"    // INT $0x0
      "jnz  16f\n"
#ifndef NDEBUG
      "lea  200f(%%rip), %%rdi\n"
      "call playground$debugMessage\n"
#endif
      "mov  0x98(%%rsp), %%rax\n"  // %rax at time of segmentation fault
      "mov  0x70(%%rsp), %%rdi\n"  // %rdi at time of segmentation fault
      "mov  0x78(%%rsp), %%rsi\n"  // %rsi at time of segmentation fault
      "mov  0x90(%%rsp), %%rdx\n"  // %rdx at time of segmentation fault
      "mov  0x40(%%rsp), %%r10\n"  // %r10 at time of segmentation fault
      "mov  0x30(%%rsp), %%r8\n"   // %r8  at time of segmentation fault
      "mov  0x38(%%rsp), %%r9\n"   // %r9  at time of segmentation fault

      // Handle rt_sigprocmask()
      "cmp  $14, %%rax\n"          // NR_rt_sigprocmask
      "jnz  12f\n"
      "mov  $-22, %%rax\n"         // -EINVAL
      "cmp  $8, %%r10\n"           // %r10 = sigsetsize (8 bytes = 64 signals)
      "jl   7b\n"
      "mov  0x130(%%rsp), %%r10\n" // signal mask at time of segmentation fault
      "test %%rsi, %%rsi\n"        // only set mask, if set is non-NULL
      "jz   11f\n"
      "mov  0(%%rsi), %%rsi\n"
      "cmp  $0, %%rdi\n"           // %rdi = how (SIG_BLOCK)
      "jnz  9f\n"
      "or   %%rsi, 0x130(%%rsp)\n" // signal mask at time of segmentation fault
      "jmp  11f\n"
    "9:cmp  $1, %%rdi\n"           // %rdi = how (SIG_UNBLOCK)
      "jnz  10f\n"
      "xor  $-1, %%rsi\n"
      "and  %%rsi, 0x130(%%rsp)\n" // signal mask at time of segmentation fault
      "jmp  11f\n"
   "10:cmp  $2, %%rdi\n"           // %rdi = how (SIG_SETMASK)
      "jnz  7b\n"
      "mov  %%rsi, 0x130(%%rsp)\n" // signal mask at time of segmentation fault
   "11:xor  %%rax, %%rax\n"
      "test %%rdx, %%rdx\n"        // only return old mask, if set is non-NULL
      "jz   7b\n"
      "mov  %%r10, 0(%%rdx)\n"     // old_set
      "jmp  7b\n"

      // Handle rt_sigreturn()
   "12:cmp  $15, %%rax\n"          // NR_rt_sigreturn
      "jnz  14f\n"
      "mov  0xA8(%%rsp), %%rsp\n"  // %rsp at time of segmentation fault
   "13:syscall\n"                  // rt_sigreturn() is unrestricted
      "mov  $66, %%edi\n"          // rt_sigreturn() should never return
      "mov  $231, %%eax\n"         // NR_exit_group
      "jmp  13b\n"

      // Copy signal frame onto new stack. See clone.cc for details
   "14:cmp  $56+0xF000, %%rax\n"   // NR_clone + 0xF000
      "jnz  15f\n"
      "mov  0xA8(%%rsp), %%rcx\n"  // %rsp at time of segmentation fault
      "sub  %%rsp, %%rcx\n"        // %rcx = size of stack frame
      "sub  $8, %%rcx\n"           // skip return address
      "mov  %%rcx, %%rax\n"        // return size of signal stack frame
      "mov  0(%%rdx), %%rdi\n"     // stack for newly clone()'d thread
      "sub  %%rcx, %%rdi\n"        // copy onto new stack
      "mov  %%rdi, 0(%%rdx)\n"     // allocate space on new stack
      "lea  8(%%rsp), %%rsi\n"     // copy from current stack
      "cld\n"
      "rep  movsb\n"
      "jmp  7b\n"

      // Forward system call to syscallWrapper()
   "15:lea  7b(%%rip), %%rcx\n"
      "push %%rcx\n"
      "push 0xB8(%%rsp)\n"         // %rip at time of segmentation fault
      "lea  playground$syscallWrapper(%%rip), %%rcx\n"
      "jmp  *%%rcx\n"

      // This was a genuine segmentation fault. Trigger the kernel's default
      // signal disposition. The only way we can do this from seccomp mode
      // is by blocking the signal and retriggering it.
   "16:mov  $2, %%edi\n"           // stderr
      "lea  300f(%%rip), %%rsi\n"  // "Segmentation fault\n"
      "mov  $301f-300f, %%edx\n"
      "mov  $1, %%eax\n"           // NR_write
      "syscall\n"
      "orb  $4, 0x131(%%rsp)\n"    // signal mask at time of segmentation fault
      "ret\n"
#elif defined(__i386__)
      // Inspect instruction at the point where the segmentation fault
      // happened. If it is RDTSC, forward the request to the trusted
      // thread.
      "mov  $-3, %%ebx\n"          // request for RDTSC
      "mov  0xDC(%%esp), %%ebp\n"  // %eip at time of segmentation fault
      "cmpw $0x310F, (%%ebp)\n"    // RDTSC
      "jz   0f\n"
      "cmpw $0x010F, (%%ebp)\n"    // RDTSCP
      "jnz  9f\n"
      "cmpb $0xF9, 2(%%ebp)\n"
      "jnz  9f\n"
      "mov  $-4, %%ebx\n"          // request for RDTSCP
    "0:"
#ifndef NDEBUG
      "lea  100f, %%eax\n"
      "push %%eax\n"
      "call playground$debugMessage\n"
      "sub  $4, %%esp\n"
#else
      "sub  $8, %%esp\n"           // allocate buffer for receiving timestamp
#endif
      "push %%ebx\n"
      "mov  %%fs:16, %%ebx\n"      // fd  = threadFdPub
      "mov  %%esp, %%ecx\n"        // buf = %esp
      "mov  $4, %%edx\n"           // len = sizeof(int)
    "1:mov  %%edx, %%eax\n"        // NR_write
      "int  $0x80\n"
      "cmp  %%eax, %%edx\n"
      "jz   7f\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   1b\n"
    "2:add  $12, %%esp\n"          // remove temporary buffer from stack
      "xor  %%eax, %%eax\n"
      "movl $0, 0xC8(%%esp)\n"     // %edx at time of segmentation fault
      "cmpw $0x310F, (%%ebp)\n"    // RDTSC
      "jz   3f\n"
      "movl $0, 0xCC(%%esp)\n"     // %ecx at time of segmentation fault
    "3:mov  %%eax, 0xD0(%%esp)\n"  // %eax at time of segmentation fault
    "4:mov  0xDC(%%esp), %%ebp\n"  // %eip at time of segmentation fault
      "addl $2, 0xDC(%%esp)\n"     // %eip at time of segmentation fault
      "cmpw $0x010F, (%%ebp)\n"    // RDTSCP
      "jnz  5f\n"
      "addl $1, 0xDC(%%esp)\n"     // %eip at time of segmentation fault
    "5:sub  $0x1C8, %%esp\n"       // a legacy signal stack is much larger
      "mov  0x1CC(%%esp), %%eax\n" // push signal number
      "push %%eax\n"
      "lea  0x270(%%esp), %%esi\n" // copy siginfo register values
      "lea  0x4(%%esp), %%edi\n"   //     into new location
      "mov  $22, %%ecx\n"
      "cld\n"
      "rep movsl\n"
      "mov  0x2C8(%%esp), %%ebx\n" // copy first half of signal mask
      "mov  %%ebx, 0x54(%%esp)\n"
      "lea  6f, %%esi\n"           // copy "magic" restorer function
      "push %%esi\n"               // push restorer function
      "lea  0x2D4(%%esp), %%edi\n" // patch up retcode magic numbers
      "movb $2, %%cl\n"
      "rep movsl\n"
      "ret\n"                      // return to restorer function

      // The restorer function is sometimes used by gdb as a magic marker to
      // recognize signal stack frames. Don't change any of the next three
      // instructions.
    "6:pop  %%eax\n"               // remove dummy argument (signo)
      "mov  $119, %%eax\n"         // NR_sigreturn
      "int  $0x80\n"
    "7:mov  $12, %%edx\n"          // len = 3*sizeof(int)
    "8:mov  $3, %%eax\n"           // NR_read
      "int  $0x80\n"
      "cmp  $-4, %%eax\n"          // EINTR
      "jz   8b\n"
      "cmp  %%eax, %%edx\n"
      "jnz  2b\n"
      "pop  %%eax\n"
      "pop  %%edx\n"
      "pop  %%ecx\n"
      "mov  %%edx, 0xC8(%%esp)\n"  // %edx at time of segmentation fault
      "cmpw $0x310F, (%%ebp)\n"    // RDTSC
      "jz   3b\n"
      "mov  %%ecx, 0xCC(%%esp)\n"  // %ecx at time of segmentation fault
      "jmp  3b\n"

      // If the instruction is INT 0, then this was probably the result
      // of playground::Library being unable to find a way to safely
      // rewrite the system call instruction. Retrieve the CPU register
      // at the time of the segmentation fault and invoke syscallWrapper().
    "9:cmpw $0x00CD, (%%ebp)\n"    // INT $0x0
      "jnz  20f\n"
#ifndef NDEBUG
      "lea  200f, %%eax\n"
      "push %%eax\n"
      "call playground$debugMessage\n"
      "add  $0x4, %%esp\n"
#endif
      "mov  0xD0(%%esp), %%eax\n"  // %eax at time of segmentation fault
      "mov  0xC4(%%esp), %%ebx\n"  // %ebx at time of segmentation fault
      "mov  0xCC(%%esp), %%ecx\n"  // %ecx at time of segmentation fault
      "mov  0xC8(%%esp), %%edx\n"  // %edx at time of segmentation fault
      "mov  0xB8(%%esp), %%esi\n"  // %esi at time of segmentation fault
      "mov  0xB4(%%esp), %%edi\n"  // %edi at time of segmentation fault
      "mov  0xB2(%%esp), %%ebp\n"  // %ebp at time of segmentation fault

      // Handle sigprocmask() and rt_sigprocmask()
      "cmp  $175, %%eax\n"         // NR_rt_sigprocmask
      "jnz  10f\n"
      "mov  $-22, %%eax\n"         // -EINVAL
      "cmp  $8, %%esi\n"           // %esi = sigsetsize (8 bytes = 64 signals)
      "jl   3b\n"
      "jmp  11f\n"
   "10:cmp  $126, %%eax\n"         // NR_sigprocmask
      "jnz  15f\n"
      "mov  $-22, %%eax\n"
   "11:mov  0xFC(%%esp), %%edi\n"  // signal mask at time of segmentation fault
      "mov  0x100(%%esp), %%ebp\n"
      "test %%ecx, %%ecx\n"        // only set mask, if set is non-NULL
      "jz   14f\n"
      "mov  0(%%ecx), %%esi\n"
      "mov  4(%%ecx), %%ecx\n"
      "cmp  $0, %%ebx\n"           // %ebx = how (SIG_BLOCK)
      "jnz  12f\n"
      "or   %%esi, 0xFC(%%esp)\n"  // signal mask at time of segmentation fault
      "or   %%ecx, 0x100(%%esp)\n"
      "jmp  14f\n"
   "12:cmp  $1, %%ebx\n"           // %ebx = how (SIG_UNBLOCK)
      "jnz  13f\n"
      "xor  $-1, %%esi\n"
      "xor  $-1, %%ecx\n"
      "and  %%esi, 0xFC(%%esp)\n"  // signal mask at time of segmentation fault
      "and  %%ecx, 0x100(%%esp)\n"
      "jmp  14f\n"
   "13:cmp  $2, %%ebx\n"           // %ebx = how (SIG_SETMASK)
      "jnz  3b\n"
      "mov  %%esi, 0xFC(%%esp)\n"  // signal mask at time of segmentation fault
      "mov  %%ecx, 0x100(%%esp)\n"
   "14:xor  %%eax, %%eax\n"
      "test %%edx, %%edx\n"        // only return old mask, if set is non-NULL
      "jz   3b\n"
      "mov  %%edi, 0(%%edx)\n"     // old_set
      "mov  %%ebp, 4(%%edx)\n"
      "jmp  3b\n"

      // Handle sigreturn() and rt_sigreturn()
      // See syscall.cc for a discussion on how we can emulate rt_sigreturn()
      // by calling sigreturn() with a suitably adjusted stack.
   "15:cmp  $119, %%eax\n"         // NR_sigreturn
      "jnz  17f\n"
      "mov  0xC0(%%esp), %%esp\n"  // %esp at time of segmentation fault
   "16:int  $0x80\n"               // sigreturn() is unrestricted
   "17:cmp  $173, %%eax\n"         // NR_rt_sigreturn
      "jnz  18f\n"
      "mov  0xC0(%%esp), %%esp\n"  // %esp at time of segmentation fault
      "sub  $4, %%esp\n"           // add fake return address
      "jmp  4b\n"

      // Copy signal frame onto new stack. In the process, we have to convert
      // it from an RT signal frame to a legacy signal frame.
      // See clone.cc for details
   "18:cmp  $120+0xF000, %%eax\n"  // NR_clone + 0xF000
      "jnz  19f\n"
      "mov  0xC0(%%esp), %%ecx\n"  // %esp at time of segmentation fault
      "sub  %%esp, %%ecx\n"        // %ecx = size of RT stack frame
      "mov  %%ecx, %%eax\n"
      "add  $0x1C8, %%eax\n"       // adjust for size of legacy stack frame
      "sub  $0x100, %%ecx\n"
      "mov  0(%%edx), %%edi\n"     // stack for newly clone()'d thread
      "sub  %%ecx, %%edi\n"        // copy onto new stack
      "lea  0x100(%%esp), %%esi\n"
      "cld\n"
      "rep movsb\n"                // copy parts of RT stack(sigmask, FP state)
      "mov  0xF0(%%esp), %%ebx\n"  // adjust pointer to fpstate
      "sub  %%esi, %%ebx\n"
      "add  %%edi, %%ebx\n"
      "sub  %%eax, %%edi\n"
      "mov  %%edi, 0(%%edx)\n"     // allocate space on new stack
      "lea  0xA4(%%esp), %%esi\n"  // copy sigcontext from current stack
      "mov  $0x16, %%ecx\n"
      "rep  movsl\n"
      "mov  %%ebx, -0xC(%%edi)\n"  // set pointer to fpstate
      "mov  0xFC(%%esp), %%ebx\n"  // copy first half of signal mask
      "mov  %%ebx, -0x8(%%edi)\n"
      "mov  %%eax, -0x2C(%%edi)\n" // return size of stack frame in %%eax
      "addl $2, -0x20(%%edi)\n"    // adjust %eip
      "mov  0(%%edx), %%esp\n"
      "mov  $119, %%eax\n"         // NR_sigreturn
      "int  $0x80\n"

      // Forward system call to syscallWrapper()
   "19:call playground$syscallWrapper\n"
      "jmp  3b\n"

      // This was a genuine segmentation fault. Trigger the kernel's default
      // signal disposition. The only way we can do this from seccomp mode
      // is by blocking the signal and retriggering it.
   "20:mov  $2, %%ebx\n"           // stderr
      "lea  300f, %%ecx\n"         // "Segmentation fault\n"
      "mov  $301f-300f, %%edx\n"
      "mov  $4, %%eax\n"           // NR_write
      "int  $0x80\n"
      "orb  $4, 0xFD(%%esp)\n"     // signal mask at time of segmentation fault
      "jmp  4b\n"
#else
#error Unsupported target platform
#endif
      ".pushsection \".rodata\"\n"
#ifndef NDEBUG
  "100:.asciz \"RDTSC(P): Executing handler\\n\"\n"
  "200:.asciz \"INT $0x0: Executing handler\\n\"\n"
#endif
  "300:.ascii \"Segmentation fault\\n\"\n"
  "301:\n"
      ".popsection\n"
  "999:pop  %0\n"
      : "=g"(fnc)
      :
      : "memory"
#if defined(__x86_64__)
        , "rsp"
#elif defined(__i386__)
        , "esp"
#endif
  );
  return fnc;
}

SecureMem::Args* Sandbox::getSecureMem() {
  // Check trusted_thread.cc for the magic offset that gets us from the TLS
  // to the beginning of the secure memory area.
  SecureMem::Args* ret;
#if defined(__x86_64__)
  asm volatile(
    "movq %%gs:-0xE0, %0\n"
    : "=q"(ret));
#elif defined(__i386__)
  asm volatile(
    "movl %%fs:-0x58, %0\n"
    : "=r"(ret));
#else
#error Unsupported target platform
#endif
  return ret;
}

void Sandbox::snapshotMemoryMappings(int processFd, int proc_self_maps) {
  SysCalls sys;
  if (sys.lseek(proc_self_maps, 0, SEEK_SET) ||
      !sendFd(processFd, proc_self_maps, -1, NULL, 0)) {
 failure:
    die("Cannot access /proc/self/maps");
  }
  int dummy;
  if (read(sys, processFd, &dummy, sizeof(dummy)) != sizeof(dummy)) {
    goto failure;
  }
}

int Sandbox::supportsSeccompSandbox(int proc_fd) {
  if (status_ != STATUS_UNKNOWN) {
    return status_ != STATUS_UNSUPPORTED;
  }
  int fds[2];
  SysCalls sys;
  if (sys.pipe(fds)) {
    status_ = STATUS_UNSUPPORTED;
    return 0;
  }
  pid_t pid;
  switch ((pid = sys.fork())) {
    case -1:
      status_ = STATUS_UNSUPPORTED;
      return 0;
    case 0: {
      int devnull = sys.open("/dev/null", O_RDWR, 0);
      if (devnull >= 0) {
        sys.dup2(devnull, 0);
        sys.dup2(devnull, 1);
        sys.dup2(devnull, 2);
        sys.close(devnull);
      }
      if (proc_fd >= 0) {
        setProcSelfMaps(sys.openat(proc_fd, "self/maps", O_RDONLY, 0));
      }
      startSandbox();
      write(sys, fds[1], "", 1);

      // Try to tell the trusted thread to shut down the entire process in an
      // orderly fashion
      defaultSystemCallHandler(__NR_exit_group, 0, 0, 0, 0, 0, 0);

      // If that did not work (e.g. because the kernel does not know about the
      // exit_group() system call), make a direct _exit() system call instead.
      // This system call is unrestricted in seccomp mode, so it will always
      // succeed. Normally, we don't like it, because unlike exit_group() it
      // does not terminate any other thread. But since we know that
      // exit_group() exists in all kernels which support kernel-level threads,
      // this is OK we only get here for old kernels where _exit() is OK.
      sys._exit(0);
    }
    default:
      NOINTR_SYS(sys.close(fds[1]));
      char ch;
      if (read(sys, fds[0], &ch, 1) != 1) {
        status_ = STATUS_UNSUPPORTED;
      } else {
        status_ = STATUS_AVAILABLE;
      }
      int rc;
      NOINTR_SYS(sys.waitpid(pid, &rc, 0));
      NOINTR_SYS(sys.close(fds[0]));
      return status_ != STATUS_UNSUPPORTED;
  }
}

void Sandbox::setProcSelfMaps(int proc_self_maps) {
  proc_self_maps_ = proc_self_maps;
}

void Sandbox::startSandbox() {
  if (status_ == STATUS_UNSUPPORTED) {
    die("The seccomp sandbox is not supported on this computer");
  } else if (status_ == STATUS_ENABLED) {
    return;
  }

  SysCalls sys;
  if (proc_self_maps_ < 0) {
    proc_self_maps_        = sys.open("/proc/self/maps", O_RDONLY, 0);
    if (proc_self_maps_ < 0) {
      die("Cannot access \"/proc/self/maps\"");
    }
  }

  // The pid is unchanged for the entire program, so we can retrieve it once
  // and store it in a global variable.
  pid_                     = sys.getpid();

  // Block all signals, except for the RDTSC handler
  setupSignalHandlers();

  // Get socketpairs for talking to the trusted process
  int pair[4];
  if (sys.socketpair(AF_UNIX, SOCK_STREAM, 0, pair) ||
      sys.socketpair(AF_UNIX, SOCK_STREAM, 0, pair+2)) {
    die("Failed to create trusted thread");
  }
  processFdPub_            = pair[0];
  cloneFdPub_              = pair[2];
  SecureMemArgs* secureMem = createTrustedProcess(pair[0], pair[1],
                                                  pair[2], pair[3]);

  // We find all libraries that have system calls and redirect the system
  // calls to the sandbox. If we miss any system calls, the application will be
  // terminated by the kernel's seccomp code. So, from a security point of
  // view, if this code fails to identify system calls, we are still behaving
  // correctly.
  {
    Maps maps(proc_self_maps_);
    const char *libs[]     = { "ld", "libc", "librt", "libpthread", NULL };

    // Intercept system calls in the VDSO segment (if any). This has to happen
    // before intercepting system calls in any of the other libraries, as
    // the main kernel entry point might be inside of the VDSO and we need to
    // determine its address before we can compare it to jumps from inside
    // other libraries.
    for (Maps::const_iterator iter = maps.begin(); iter != maps.end(); ++iter){
      Library* library = *iter;
      if (library->isVDSO() && library->parseElf()) {
        library->makeWritable(true);
        library->patchSystemCalls();
        library->makeWritable(false);
        break;
      }
    }

    // Intercept system calls in libraries that are known to have them.
    for (Maps::const_iterator iter = maps.begin(); iter != maps.end(); ++iter){
      Library* library = *iter;
      const char* mapping = iter.name().c_str();

      // Find the actual base name of the mapped library by skipping past any
      // SPC and forward-slashes. We don't want to accidentally find matches,
      // because the directory name included part of our well-known lib names.
      //
      // Typically, prior to pruning, entries would look something like this:
      // 08:01 2289011 /lib/libc-2.7.so
      for (const char *delim = " /"; *delim; ++delim) {
        const char* skip = strrchr(mapping, *delim);
        if (skip) {
          mapping = skip + 1;
        }
      }

      for (const char **ptr = libs; *ptr; ptr++) {
        const char *name = strstr(mapping, *ptr);
        if (name == mapping) {
          char ch = name[strlen(*ptr)];
          if (ch < 'A' || (ch > 'Z' && ch < 'a') || ch > 'z') {
            if (library->parseElf()) {
              library->makeWritable(true);
              library->patchSystemCalls();
              library->makeWritable(false);
              break;
            }
          }
        }
      }
    }
  }

  // Take a snapshot of the current memory mappings. These mappings will be
  // off-limits to all future mmap(), munmap(), mremap(), and mprotect() calls.
  snapshotMemoryMappings(processFdPub_, proc_self_maps_);
  NOINTR_SYS(sys.close(proc_self_maps_));
  proc_self_maps_ = -1;

  // Creating the trusted thread enables sandboxing
  createTrustedThread(processFdPub_, cloneFdPub_, secureMem);

  // We can no longer check for sandboxing support at this point, but we also
  // know for a fact that it is available (as we just turned it on). So update
  // the status to reflect this information.
  status_ = STATUS_ENABLED;
}

} // namespace
