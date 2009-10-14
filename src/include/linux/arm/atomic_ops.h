/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef NATIVE_CLIENT_SRC_INCLUDE_LINUX_ARM_ATOMIC_OPS_LINUX_ARM_H_
#define NATIVE_CLIENT_SRC_INCLUDE_LINUX_ARM_ATOMIC_OPS_LINUX_ARM_H_ 1

#include <stdint.h>

typedef intptr_t AtomicWord;
typedef int32_t Atomic32;

#ifdef NATIVE_CLIENT_USE_KERNEL_ATOMIC_OPS

/*
 * The Linux Kernel provides an atomic compare-exchange instruction sequence
 * at a published place in memory, for use by user-space code.  Unfortunately
 * there does not appear to be a runtime API for this, so we provide one
 * ourselves!
 *
 * This indirection allows us to use the same atomic operation code on ARMv4
 * and later, and takes into account processor errata (e.g. the fact that SWP
 * creates cache inconsistencies on the StrongARM family).  In the worst case
 * this may invoke a syscall.
 *
 * Note that this involves a call into high memory, and will only work from
 * trusted code!  Untrusted code currently has no portable atomic op story.
 *
 * For more information see arch/arm/kernel/entry-armv.S in the 2.6.x kernels.
 */
typedef int (__kernel_cmpxchg_t)(AtomicWord oldval, AtomicWord newval,
                                 volatile AtomicWord* ptr);
#define __kernel_cmpxchg (*(__kernel_cmpxchg_t *)0xFFFF0FC0)

static inline AtomicWord CompareAndSwap(volatile AtomicWord* ptr,
                                        AtomicWord old_value,
                                        AtomicWord new_value) {
  (void) __kernel_cmpxchg(old_value, new_value, ptr);
  return old_value;
}

static inline AtomicWord AtomicExchange(volatile AtomicWord *ptr,
                                        AtomicWord new_value) {
  /*
   * Implemented in terms of cmpxchg for simplicity and portability -- would
   * be more efficient on ARMv6+ if we used inline assembly.
   */
  int r = 0;
  AtomicWord before;
  do {
    before = *ptr;
    r = __kernel_cmpxchg(before, new_value, ptr);
  } while (r == 0);
  return before;
}

static inline AtomicWord AtomicIncrement(volatile AtomicWord* ptr,
                                         AtomicWord increment) {
  /*
   * Implemented in terms of cmpxchg for simplicity and portability -- would
   * be more efficient on ARMv6+ if we used inline assembly.
   */
  AtomicWord after;
  int r = 0;
  do {
    AtomicWord before = *ptr;
    after = before + increment;
    r = __kernel_cmpxchg(before, after, ptr);
  } while (r == 0);
  return after;
}

#else /* !defined(NATIVE_CLIENT_USE_KERNEL_ATOMIC_OPS) */

static inline AtomicWord CompareAndSwap(volatile AtomicWord* ptr,
                                 AtomicWord old_value,
                                 AtomicWord new_value) {
  uint32_t old, tmp;

  __asm__ __volatile__("1: @ atomic cmpxchg\n"
                       "mov %0, #0\n"
                       "ldrex %1, [%2]\n"
                       "teq %1, %3\n"
                       "strexeq %0, %4, [%2]\n"
                       "teq %0, #0\n"
                       "bne 1b"
                       : "=&r" (tmp), "=&r" (old)
                       : "r" (ptr), "Ir" (old_value),
                         "r" (new_value)
                       : "cc");
  return old;
}

static inline AtomicWord AtomicExchange(volatile AtomicWord* ptr,
                                 AtomicWord new_value) {
  uint32_t tmp, old;

  __asm__ __volatile__("1: @ atomic xchg\n"
                       "ldrex %1, [%2]\n"
                       "strex %0, %3, [%2]\n"
                       "teq %0, #0\n"
                       "bne 1b"
                       : "=&r" (tmp), "=&r" (old)
                       : "r" (ptr), "r" (new_value)
                       : "cc");

  return old;
}

static inline AtomicWord AtomicIncrement(volatile AtomicWord* ptr,
                                  AtomicWord increment) {
  uint32_t tmp, res;

  __asm__ __volatile__("1: @atomic inc\n"
                       "ldrex %1, [%2]\n"
                       "add %1, %1, %3\n"
                       "strex %0, %1, [%2]\n"
                       "teq %0, #0\n"
                       "bne 1b"
                       : "=&r" (tmp), "=&r"(res)
                       : "r" (ptr), "r"(increment)
                       : "cc");
  return res;
}

#endif /* NATIVE_CLIENT_USE_KERNEL_ATOMIC_OPS */

#endif  /* NATIVE_CLIENT_SRC_INCLUDE_LINUX_ARM_ATOMIC_OPS_LINUX_ARM_H_ */
