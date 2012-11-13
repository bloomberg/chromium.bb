/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_SIGNAL_HANDLER_SINGLE_STEP_STEP_TEST_COMMON_H_
#define NATIVE_CLIENT_TESTS_SIGNAL_HANDLER_SINGLE_STEP_STEP_TEST_COMMON_H_

#include <unistd.h>

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/trusted/service_runtime/arch/x86/sel_ldr_x86.h"

#if NACL_ARCH(NACL_BUILD_ARCH) != NACL_x86
# error This test uses x86 single-stepping and so is x86-only
#endif

/* Start single-stepping by setting the trap flag (bit 8). */
static INLINE void SetTrapFlag(void) {
  __asm__("pushf\n"
#if NACL_BUILD_SUBARCH == 32
          "btsl $8, (%esp)\n"
#else
          "btsq $8, (%rsp)\n"
#endif
          "popf\n");
}

/* Stop single-stepping by unsetting the trap flag (bit 8). */
static INLINE void UnsetTrapFlag(void) {
  __asm__("pushf\n"
#if NACL_BUILD_SUBARCH == 32
          "btrl $8, (%esp)\n"
#else
          "btrq $8, (%rsp)\n"
#endif
          "popf\n");
}

static INLINE uintptr_t GetTrapFlag(void) {
  uintptr_t flags;
#if NACL_BUILD_SUBARCH == 32
  __asm__("pushf\n"
          "popl %0\n" : "=r"(flags));
#else
  __asm__("pushf\n"
          "popq %0\n" : "=r"(flags));
#endif
  return (flags & NACL_X86_TRAP_FLAG) != 0;
}

static void SignalSafeWrite(const void *buf, size_t size) {
  if (write(2, buf, size) != (ssize_t) size) {
    /*
     * This error handling is largely because glibc's
     * warn_unused_result annotation requires us to do a check.
     */
    _exit(1);
  }
}

#define SignalSafeLogStringLiteral(string) \
    SignalSafeWrite(string, sizeof(string) - 1)

#endif
