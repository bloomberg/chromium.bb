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

// Used only by trusted code.  Untrusted code uses gcc intrinsics.

#include <stdint.h>

typedef intptr_t AtomicWord;
typedef int32_t Atomic32;

static inline AtomicWord CompareAndSwap(volatile AtomicWord* ptr,
                                 AtomicWord old_value,
                                 AtomicWord new_value) {
  uint32_t old, tmp;

  // TODO(adonovan): bne may loop forever if old_value is bogus.  We
  // should instead break out of the loop if the first condition fails.

  __asm__ __volatile__(
      "1:\n"
      "ldrex %1, [%2]\n"
      "teq %1, %3\n"
      "strexeq %0, %4, [%2]\n"
      "teqeq %0, #0\n"
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
      "1:\n"
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

  __asm__ __volatile__(
      "1:\n"
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

#endif  /* NATIVE_CLIENT_SRC_INCLUDE_LINUX_ARM_ATOMIC_OPS_LINUX_ARM_H_ */
