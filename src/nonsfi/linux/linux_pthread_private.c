/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/nonsfi/linux/linux_syscall_defines.h"
#include "native_client/src/nonsfi/linux/linux_syscall_structs.h"
#include "native_client/src/nonsfi/linux/linux_syscall_wrappers.h"
#include "native_client/src/nonsfi/linux/linux_sys_private.h"
#include "native_client/src/public/linux_syscalls/sched.h"
#include "native_client/src/public/linux_syscalls/sys/syscall.h"
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
   * We do not use CLONE_CHILD_CLEARTID as we do not want any
   * non-private futex signaling. Also, NaCl ABI does not require us
   * to signal the futex on stack_flag.
   */
  int flags = (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
               CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS);
  /*
   * linux_clone_wrapper expects start_func's type is "int (*)(void *)".
   * Although |start_func| has type "void (*)(void)", the type mismatching
   * will not cause a problem. Passing a dummy |arg| (= 0) does nothing there.
   * Also, start_func will never return.
   */
  return irt_return_call(linux_clone_wrapper(
      (uintptr_t) start_func, /* arg */ 0, flags, stack,
      /* ptid */ NULL, thread_ptr, /* ctid */ NULL));
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
