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

#ifdef __native_client__
#define SFI_ATOMIC_OPS
#endif

static inline AtomicWord CompareAndSwap(volatile AtomicWord* ptr,
                                 AtomicWord old_value,
                                 AtomicWord new_value) {
  uint32_t old, tmp;

  __asm__ __volatile__(
#ifdef SFI_ATOMIC_OPS
      ".align 4\n"
#endif
      "1: @ atomic cmpxchg\n"
      "ldrex %1, [%2]\n"
      "teq %1, %3\n"
#ifdef SFI_ATOMIC_OPS
      "sfi_data_mask %2, eq\n"
#endif
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

  __asm__ __volatile__(
#ifdef SFI_ATOMIC_OPS
      ".align 4\n"
#endif
      "1: @ atomic xchg\n"
      "ldrex %1, [%2]\n"
#ifdef SFI_ATOMIC_OPS
      "sfi_data_mask %2, al\n"
#endif
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

  __asm__ __volatile__(
#ifdef SFI_ATOMIC_OPS
      ".align 4\n"
#endif
      "1: @atomic inc\n"
      "ldrex %1, [%2]\n"
      "add %1, %1, %3\n"
#ifdef SFI_ATOMIC_OPS
      "sfi_data_mask %2, al\n"
#endif
      "strex %0, %1, [%2]\n"
      "teq %0, #0\n"
      "bne 1b"
      : "=&r" (tmp), "=&r"(res)
      : "r" (ptr), "r"(increment)
      : "cc");
  return res;
}

#undef SFI_ATOMIC_OPS

#endif  /* NATIVE_CLIENT_SRC_INCLUDE_LINUX_ARM_ATOMIC_OPS_LINUX_ARM_H_ */
