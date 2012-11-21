// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>
#include <errno.h>
#include <stdarg.h>

#include "sandbox/linux/seccomp-bpf/syscall.h"


namespace playground2 {

  asm(      // We need to be able to tell the kernel exactly where we made a
            // system call. The C++ compiler likes to sometimes clone or
            // inline code, which would inadvertently end up duplicating
            // the entry point.
            // "gcc" can suppress code duplication with suitable function
            // attributes, but "clang" doesn't have this ability.
            // The "clang" developer mailing list suggested that the correct
            // and portable solution is a file-scope assembly block.
            // N.B. We do mark our code as a proper function so that backtraces
            // work correctly. But we make absolutely no attempt to use the
            // ABI's calling conventions for passing arguments. We will only
            // ever be called from assembly code and thus can pick more
            // suitable calling conventions.
#if defined(__i386__)
            ".text\n"
            ".align 16, 0x90\n"
            ".type SyscallAsm, @function\n"
 "SyscallAsm:.cfi_startproc\n"
            // Check if "%eax" is negative. If so, do not attempt to make a
            // system call. Instead, compute the return address that is visible
            // to the kernel after we execute "int $0x80". This address can be
            // used as a marker that BPF code inspects.
            "test %eax, %eax\n"
            "jge  1f\n"
            // Always, make sure that our code is position-independent, or
            // address space randomization might not work on i386. This means,
            // we can't use "lea", but instead have to rely on "call/pop".
            "call 0f;   .cfi_adjust_cfa_offset  4\n"
          "0:pop  %eax; .cfi_adjust_cfa_offset -4\n"
            "addl $2f-0b, %eax\n"
            "ret\n"
            // Save register that we don't want to clobber. On i386, we need to
            // save relatively aggressively, as there are a couple or registers
            // that are used internally (e.g. %ebx for position-independent
            // code, and %ebp for the frame pointer), and as we need to keep at
            // least a few registers available for the register allocator.
          "1:push %esi; .cfi_adjust_cfa_offset 4\n"
            "push %edi; .cfi_adjust_cfa_offset 4\n"
            "push %ebx; .cfi_adjust_cfa_offset 4\n"
            "push %ebp; .cfi_adjust_cfa_offset 4\n"
            // Copy entries from the array holding the arguments into the
            // correct CPU registers.
            "movl  0(%edi), %ebx\n"
            "movl  4(%edi), %ecx\n"
            "movl  8(%edi), %edx\n"
            "movl 12(%edi), %esi\n"
            "movl 20(%edi), %ebp\n"
            "movl 16(%edi), %edi\n"
            // Enter the kernel.
            "int  $0x80\n"
            // This is our "magic" return address that the BPF filter sees.
          "2:"
            // Restore any clobbered registers that we didn't declare to the
            // compiler.
            "pop  %ebp; .cfi_adjust_cfa_offset -4\n"
            "pop  %ebx; .cfi_adjust_cfa_offset -4\n"
            "pop  %edi; .cfi_adjust_cfa_offset -4\n"
            "pop  %esi; .cfi_adjust_cfa_offset -4\n"
            "ret\n"
            ".cfi_endproc\n"
          "9:.size SyscallAsm, 9b-SyscallAsm\n"
#elif defined(__x86_64__)
            ".text\n"
            ".align 16, 0x90\n"
            ".type SyscallAsm, @function\n"
 "SyscallAsm:.cfi_startproc\n"
            // Check if "%rax" is negative. If so, do not attempt to make a
            // system call. Instead, compute the return address that is visible
            // to the kernel after we execute "syscall". This address can be
            // used as a marker that BPF code inspects.
            "test %rax, %rax\n"
            "jge  1f\n"
            // Always make sure that our code is position-independent, or the
            // linker will throw a hissy fit on x86-64.
            "call 0f;   .cfi_adjust_cfa_offset  8\n"
          "0:pop  %rax; .cfi_adjust_cfa_offset -8\n"
            "addq $2f-0b, %rax\n"
            "ret\n"
            // We declared all clobbered registers to the compiler. On x86-64,
            // there really isn't much of a problem with register pressure. So,
            // we can go ahead and directly copy the entries from the arguments
            // array into the appropriate CPU registers.
          "1:movq  0(%r12), %rdi\n"
            "movq  8(%r12), %rsi\n"
            "movq 16(%r12), %rdx\n"
            "movq 24(%r12), %r10\n"
            "movq 32(%r12), %r8\n"
            "movq 40(%r12), %r9\n"
            // Enter the kernel.
            "syscall\n"
            // This is our "magic" return address that the BPF filter sees.
          "2:ret\n"
            ".cfi_endproc\n"
          "9:.size SyscallAsm, 9b-SyscallAsm\n"
#elif defined(__arm__)
            // Throughout this file, we use the same mode (ARM vs. thumb)
            // that the C++ compiler uses. This means, when transfering control
            // from C++ to assembly code, we do not need to switch modes (e.g.
            // by using the "bx" instruction). It also means that our assembly
            // code should not be invoked directly from code that lives in
            // other compilation units, as we don't bother implementing thumb
            // interworking. That's OK, as we don't make any of the assembly
            // symbols public. They are all local to this file.
            ".text\n"
            ".align 2\n"
            ".type SyscallAsm, %function\n"
#if defined(__thumb__)
            ".thumb_func\n"
#else
            ".arm\n"
#endif
 "SyscallAsm:.fnstart\n"
            "@ args = 0, pretend = 0, frame = 8\n"
            "@ frame_needed = 1, uses_anonymous_args = 0\n"
#if defined(__thumb__)
            ".cfi_startproc\n"
            "push {r7, lr}\n"
            ".cfi_offset 14, -4\n"
            ".cfi_offset  7, -8\n"
            "mov r7, sp\n"
            ".cfi_def_cfa_register 7\n"
            ".cfi_def_cfa_offset 8\n"
#else
            "stmfd sp!, {fp, lr}\n"
            "add fp, sp, #4\n"
#endif
            // Check if "r0" is negative. If so, do not attempt to make a
            // system call. Instead, compute the return address that is visible
            // to the kernel after we execute "swi 0". This address can be
            // used as a marker that BPF code inspects.
            "cmp r0, #0\n"
            "bge 1f\n"
            "ldr r0, =2f\n"
            "b   2f\n"
            // We declared (almost) all clobbered registers to the compiler. On
            // ARM there is no particular register pressure. So, we can go
            // ahead and directly copy the entries from the arguments array
            // into the appropriate CPU registers.
          "1:ldr r5, [r6, #20]\n"
            "ldr r4, [r6, #16]\n"
            "ldr r3, [r6, #12]\n"
            "ldr r2, [r6, #8]\n"
            "ldr r1, [r6, #4]\n"
            "mov r7, r0\n"
            "ldr r0, [r6, #0]\n"
            // Enter the kernel
            "swi 0\n"
            // Restore the frame pointer. Also restore the program counter from
            // the link register; this makes us return to the caller.
#if defined(__thumb__)
          "2:pop {r7, pc}\n"
            ".cfi_endproc\n"
#else
          "2:ldmfd sp!, {fp, pc}\n"
#endif
            ".fnend\n"
          "9:.size SyscallAsm, 9b-SyscallAsm\n"
#endif
  );  // asm

intptr_t SandboxSyscall(int nr, ...) {
  // It is most convenient for the caller to pass a variadic list of arguments.
  // But this is difficult to handle in assembly code without making
  // assumptions about internal implementation details of "va_list". So, we
  // first use C code to copy all the arguments into an array, where they are
  // easily accessible to asm().
  // This is preferable over copying them into individual variables, which
  // can result in too much register pressure.
  void *args[6];
  va_list ap;

  // System calls take a system call number (typically passed in %eax or
  // %rax) and up to six arguments (passed in general-purpose CPU registers).
  //
  // On 32bit systems, all variadic arguments are passed on the stack as 32bit
  // quantities. We can use an arbitrary 32bit type to retrieve them with
  // va_arg() and then forward them to the kernel in the appropriate CPU
  // register. We do not need to know whether this is an integer or a pointer
  // value.
  //
  // On 64bit systems, variadic arguments can be either 32bit or 64bit wide,
  // which would seem to make it more important that we pass the correct type
  // to va_arg(). And we really can't know what this type is unless we have a
  // table with function signatures for all system calls.
  //
  // Fortunately, on x86-64 this is less critical. The first six function
  // arguments will be passed in CPU registers, no matter whether they were
  // named or variadic. This only leaves us with a single argument (if present)
  // that could be passed on the stack. And since x86-64 is little endian,
  // it will have the correct value both for 32bit and 64bit quantities.
  //
  // N.B. Because of how the x86-64 ABI works, it is possible that 32bit
  //   quantities will have undefined garbage bits in the upper 32 bits of a
  //   64bit register. This is relatively unlikely for the first five system
  //   call arguments, as the processor does automatic sign extensions and zero
  //   filling so frequently, there rarely is garbage in CPU registers. But it
  //   is quite likely for the last argument, which is passed on the stack.
  //   That's generally OK, because the kernel has the correct function
  //   signatures and knows to only inspect the LSB of a 32bit value.
  //   But callers must be careful in cases, where the compiler cannot tell
  //   the difference (e.g. when passing NULL to any system call, it must
  //   always be cast to a pointer type).
  //   The glibc implementation of syscall() has the exact same issues.
  //   In the unlikely event that this ever becomes a problem, we could add
  //   code that handles six-argument system calls specially. The number of
  //   system calls that take six arguments and expect a 32bit value in the
  //   sixth argument is very limited.
  va_start(ap, nr);
  args[0] = va_arg(ap, void *);
  args[1] = va_arg(ap, void *);
  args[2] = va_arg(ap, void *);
  args[3] = va_arg(ap, void *);
  args[4] = va_arg(ap, void *);
  args[5] = va_arg(ap, void *);
  va_end(ap);

  // Invoke our file-scope assembly code. The constraints have been picked
  // carefully to match what the rest of the assembly code expects in input,
  // output, and clobbered registers.
#if defined(__i386__)
  intptr_t ret = nr;
  asm volatile(
    "call SyscallAsm\n"
    // N.B. These are not the calling conventions normally used by the ABI.
    : "=a"(ret)
    : "0"(ret), "D"(args)
    : "esp", "memory", "ecx", "edx");
#elif defined(__x86_64__)
  intptr_t ret = nr;
  {
    register void **data __asm__("r12") = args;
    asm volatile(
      "call SyscallAsm\n"
      // N.B. These are not the calling conventions normally used by the ABI.
      : "=a"(ret)
      : "0"(ret), "r"(data)
      : "rsp", "memory",
        "rcx", "rdi", "rsi", "rdx", "r8", "r9", "r10", "r11");
  }
#elif defined(__arm__)
  intptr_t ret;
  {
    register intptr_t inout __asm__("r0") = nr;
    register void **data __asm__("r6") = args;
    asm volatile(
      "bl SyscallAsm\n"
      // N.B. These are not the calling conventions normally used by the ABI.
      : "=r"(inout)
      : "0"(inout), "r"(data)
      : "lr", "memory", "r1", "r2", "r3", "r4", "r5"
#if !defined(__arm__)
      // In thumb mode, we cannot use "r7" as a general purpose register, as
      // it is our frame pointer. We have to manually manage and preserve it.
      // In ARM mode, we have a dedicated frame pointer register and "r7" is
      // thus available as a general purpose register. We don't preserve it,
      // but instead mark it as clobbered.
        , "r7"
#endif
      );
    ret = inout;
  }
#else
  errno = ENOSYS;
  intptr_t ret = -1;
#endif
  return ret;
}

}  // namespace
