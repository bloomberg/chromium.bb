/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/nonsfi/linux/linux_syscall_defines.h"
#include "native_client/src/nonsfi/linux/linux_syscall_structs.h"
#include "native_client/src/nonsfi/linux/linux_syscall_wrappers.h"
#include "native_client/src/nonsfi/linux/linux_syscalls.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"

/* Convert a return value of a Linux syscall to the one of an IRT call. */
static uint32_t irt_return_call(uintptr_t result) {
  if (linux_is_error_result(result))
    return -result;
  return 0;
}

static int nacl_irt_thread_create(void (*start_func)(void), void *stack,
                                  void *thread_ptr) {
  /*
   * The prototype of clone(2) is
   *
   * clone(int flags, void *stack, pid_t *ptid, void *tls, pid_t *ctid);
   *
   * See linux_syscall_wrappers.h for syscalls' calling conventions.
   */

  /*
   * We do not use CLONE_CHILD_CLEARTID as we do not want any
   * non-private futex signaling. Also, NaCl ABI does not require us
   * to signal the futex on stack_flag.
   */
  int flags = (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
               CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS);
  /*
   * Make sure we can access stack[0] and align the stack address to a
   * 16-byte boundary.
   */
  static const int kStackAlignmentMask = ~15;
  stack = (void *) (((uintptr_t) stack - sizeof(uintptr_t)) &
                    kStackAlignmentMask);
  /* We pass start_func using the stack top. */
  ((uintptr_t *) stack)[0] = (uintptr_t) start_func;

#if defined(__i386__)
  uint32_t result;
  struct linux_user_desc desc = create_linux_user_desc(
      0 /* allocate_new_entry */, thread_ptr);
  __asm__ __volatile__("int $0x80\n"
                       /*
                        * If the return value of clone is non-zero, we are
                        * in the parent thread of clone.
                        */
                       "cmp $0, %%eax\n"
                       "jne 0f\n"
                       /*
                        * In child thread. Clear the frame pointer to
                        * prevent debuggers from unwinding beyond this,
                        * pop the stack to get start_func and call it.
                        */
                       "mov $0, %%ebp\n"
                       "call *(%%esp)\n"
                       /* start_func never finishes. */
                       "hlt\n"
                       "0:\n"
                       : "=a"(result)
                       : "a"(__NR_clone), "b"(flags), "c"(stack),
                         /* We do not use CLONE_PARENT_SETTID. */
                         "d"(0),
                         "S"(&desc),
                         /* We do not use CLONE_CHILD_CLEARTID. */
                         "D"(0)
                       : "memory");
#elif defined(__arm__)
  register uint32_t result __asm__("r0");
  register uint32_t sysno __asm__("r7") = __NR_clone;
  register uint32_t a1 __asm__("r0") = flags;
  register uint32_t a2 __asm__("r1") = (uint32_t) stack;
  register uint32_t a3 __asm__("r2") = 0;  /* No CLONE_PARENT_SETTID. */
  register uint32_t a4 __asm__("r3") = (uint32_t) thread_ptr;
  register uint32_t a5 __asm__("r4") = 0;  /* No CLONE_CHILD_CLEARTID. */
  __asm__ __volatile__("svc #0\n"
                       /*
                        * If the return value of clone is non-zero, we are
                        * in the parent thread of clone.
                        */
                       "cmp r0, #0\n"
                       "bne 0f\n"
                       /*
                        * In child thread. Clear the frame pointer to
                        * prevent debuggers from unwinding beyond this,
                        * pop the stack to get start_func and call it.
                        */
                       "mov fp, #0\n"
                       "pop {r0}\n"
                       "blx r0\n"
                       /* start_func never finishes. */
                       "bkpt #0\n"
                       "0:\n"
                       : "=r"(result)
                       : "r"(sysno),
                         "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
                       : "memory");
#else
# error Unsupported architecture
#endif
  return irt_return_call(result);
}

static void nacl_irt_thread_exit(int32_t *stack_flag) {
  /*
   * We fill zero to stack_flag by ourselves instead of relying
   * on CLONE_CHILD_CLEARTID. We do everything in the following inline
   * assembly because we need to make sure we will never touch stack.
   *
   * We will set the stack pointer to zero at the beginning of the
   * assembly code just in case an async signal arrives after setting
   * *stack_flag=0 but before calling the syscall, so that any signal
   * handler crashes rather than running on a stack that has been
   * reallocated to another thread.
   */
#if defined(__i386__)
  __asm__ __volatile__("mov $0, %%esp\n"
                       "movl $0, (%[stack_flag])\n"
                       "int $0x80\n"
                       "hlt\n"
                       :: [stack_flag]"r"(stack_flag), "a"(__NR_exit));
#elif defined(__arm__)
  __asm__ __volatile__("mov sp, #0\n"
                       "mov r7, #0\n"
                       "str r7, [%[stack_flag]]\n"
                       "mov r7, %[sysno]\n"
                       "svc #0\n"
                       "bkpt #0\n"
                       :: [stack_flag]"r"(stack_flag), [sysno]"i"(__NR_exit)
                       : "r7");
#else
# error Unsupported architecture
#endif
}

static int nacl_irt_thread_nice(const int nice) {
  return 0;
}

void __nc_initialize_interfaces(void) {
  const struct nacl_irt_thread init = {
    nacl_irt_thread_create,
    nacl_irt_thread_exit,
    nacl_irt_thread_nice,
  };
  __libnacl_irt_thread = init;
}
