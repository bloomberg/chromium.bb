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
    "ja  1f\n"

    // Retrieve function call from system call table (c.f. syscall_table.c).
    // We have three different types of entries; zero for denied system calls,
    // that should be handled by the defaultSystemCallHandler(); minus one
    // for unrestricted system calls that need to be forwarded to the trusted
    // thread; and function pointers to specific handler functions.
    "shl  $3, %eax\n"
    "lea  playground$syscallTable, %ebx\n"
    "add  %ebx, %eax\n"
    "mov  0(%eax), %eax\n"

    // Jump to function if non-null and not UNRESTRICTED_SYSCALL, otherwise
    // jump to fallback handler.
    "cmp  $1, %eax\n"
    "jbe  1f\n"
    "add  $4, %esp\n"
    "call *%eax\n"
    "add  $24, %esp\n"
  "0:"

    // Restore CPU registers, except for %eax which was set by the system call.
    "pop  %ebp\n"
    "pop  %edi\n"
    "pop  %esi\n"
    "pop  %edx\n"
    "pop  %ecx\n"
    "pop  %ebx\n"

    // Return to caller
    "ret\n"

  "1:"
    // Call default handler.
    "push $2f\n"
    "push $playground$defaultSystemCallHandler\n"
    "ret\n"
  "2:add  $28, %esp\n"
    "jmp 0b\n"

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
  SysCalls sys;
  long     rc;
  switch (syscallNum) {
    case __NR_read:
      Debug::syscall(syscallNum, "Allowing unrestricted system call");
      rc             = sys.read((long)arg0, arg1, (size_t)arg2);
      break;
    case __NR_write:
      Debug::syscall(syscallNum, "Allowing unrestricted system call");
      rc             = sys.write((long)arg0, arg1, (size_t)arg2);
      break;
    case __NR_rt_sigreturn:
      Debug::syscall(syscallNum, "Allowing unrestricted system call");
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
        Debug::syscall(syscallNum, "Allowing unrestricted system call");
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
        return rc;
      } else if (Debug::isEnabled()) {
        Debug::syscall(syscallNum,
                       "In production mode, this call would be disallowed");
        goto perform_unrestricted;
      } else {
        return (void *)-ENOSYS;
      }
  }
  if (rc < 0) {
    rc               = -sys.my_errno;
  }
  return (void *)rc;
}

} // namespace
