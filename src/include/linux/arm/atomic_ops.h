/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_INCLUDE_LINUX_ARM_ATOMIC_OPS_LINUX_ARM_H_
#define NATIVE_CLIENT_SRC_INCLUDE_LINUX_ARM_ATOMIC_OPS_LINUX_ARM_H_ 1

// Used only by trusted code.  Untrusted code uses gcc intrinsics.

#include "native_client/src/include/portability.h"
#include <stdint.h>

typedef int32_t Atomic32;

static INLINE Atomic32 CompareAndSwap(volatile Atomic32* ptr,
                                      Atomic32 old_value,
                                      Atomic32 new_value) {
  Atomic32 oldval, res;
  do {
    __asm__ __volatile__(
        "ldrex   %1, [%3]\n"
        "mov     %0, #0\n"
        "teq     %1, %4\n"
        /*
         * The if-then block affects the following four instructions.
         * But we only have one we want to predicate.  So we have to
         * add three nops when compiling for Thumb.
         */
        "itt     eq\n"
        "strexeq %0, %5, [%3]\n"
#ifdef __thumb__
        "nop\n"
        "nop\n"
        "nop\n"
#endif
        : "=&r" (res), "=&r" (oldval), "+Qo" (*ptr)
        : "r" (ptr), "Ir" (old_value), "r" (new_value)
        : "cc", "memory");
  } while (res);
  return oldval;
}

static INLINE Atomic32 AtomicExchange(volatile Atomic32* ptr,
                                      Atomic32 new_value) {
  Atomic32 tmp, old;

  __asm__ __volatile__(
      "1:\n"
      "ldrex %1, [%2]\n"
      "strex %0, %3, [%2]\n"
      "teq %0, #0\n"
      "bne 1b"
      : "=&r" (tmp), "=&r" (old)
      : "r" (ptr), "r" (new_value)
      : "cc", "memory");

  return old;
}

static INLINE Atomic32 AtomicIncrement(volatile Atomic32* ptr,
                                       Atomic32 increment) {
  Atomic32 tmp, res;

  __asm__ __volatile__(
      "1:\n"
      "ldrex %1, [%2]\n"
      "add %1, %1, %3\n"
      "strex %0, %1, [%2]\n"
      "cmp %0, #0\n"
      "bne 1b"
      : "=&r" (tmp),    // %0
        "=&r"(res)      // %1
      : "r" (ptr),      // %2
        "r"(increment)  // %3
      : "cc", "memory");
  return res;
}

#endif  /* NATIVE_CLIENT_SRC_INCLUDE_LINUX_ARM_ATOMIC_OPS_LINUX_ARM_H_ */
