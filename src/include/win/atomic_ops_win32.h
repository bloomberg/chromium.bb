/*
 * Copyright 2008, Google Inc.
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


#ifndef NATIVE_CLIENT_SRC_INCLUDE_WIN_ATOMIC_OPS_WIN32_H_
#define NATIVE_CLIENT_SRC_INCLUDE_WIN_ATOMIC_OPS_WIN32_H_ 1

#include <windows.h>
#include <crtdefs.h>  // for intptr_t on WINCE

typedef intptr_t AtomicWord;
#ifdef _WIN64
typedef LONG Atomic32;
#else
typedef AtomicWord Atomic32;
#endif

// TODO(aa): we don't have COMPILE_ASSERT in Gears.
// COMPILE_ASSERT(sizeof(AtomicWord) == sizeof(PVOID), atomic_word_is_atomic);

inline AtomicWord CompareAndSwap(volatile AtomicWord* ptr,
                                 AtomicWord old_value,
                                 AtomicWord new_value) {
  PVOID result = InterlockedCompareExchangePointer(
    reinterpret_cast<volatile PVOID*>(ptr),
    reinterpret_cast<PVOID>(new_value), reinterpret_cast<PVOID>(old_value));
  return reinterpret_cast<AtomicWord>(result);
}

inline AtomicWord AtomicExchange(volatile AtomicWord* ptr,
                                 AtomicWord new_value) {
  PVOID result = InterlockedExchangePointer(
    const_cast<PVOID*>(reinterpret_cast<volatile PVOID*>(ptr)),
    reinterpret_cast<PVOID>(new_value));
  return reinterpret_cast<AtomicWord>(result);
}

#ifdef _WIN64
inline Atomic32 AtomicIncrement(volatile Atomic32* ptr, Atomic32 increment) {
  // InterlockedExchangeAdd returns *ptr before being incremented
  // and we must return nonzero iff *ptr is nonzero after being
  // incremented.
  return InterlockedExchangeAdd(ptr, increment) + increment;
}

inline AtomicWord AtomicIncrement(volatile AtomicWord* ptr,
                                  AtomicWord increment) {
  return InterlockedExchangeAdd64(
    reinterpret_cast<volatile LONGLONG*>(ptr),
    static_cast<LONGLONG>(increment)) + increment;
}
#else
inline AtomicWord AtomicIncrement(volatile AtomicWord* ptr,
                                  AtomicWord increment) {
  return InterlockedExchangeAdd(
#ifdef WINCE
      // It seems that for WinCE InterlockedExchangeAdd takes LONG* as its first
      // parameter. The const_cast is required to remove the volatile modifier.
      const_cast<LONG*>(reinterpret_cast<volatile LONG*>(ptr)),
#else
      reinterpret_cast<volatile LONG*>(ptr),
#endif
      static_cast<LONG>(increment)) + increment;
}
#endif

#endif  // NATIVE_CLIENT_SRC_INCLUDE_WIN_ATOMIC_OPS_WIN32_H_
