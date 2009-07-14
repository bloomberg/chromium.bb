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


#ifndef NATIVE_CLIENT_SRC_INCLUDE_OSX_ATOMIC_OPS_OSX_H_
#define NATIVE_CLIENT_SRC_INCLUDE_OSX_ATOMIC_OPS_OSX_H_ 1

#include <libkern/OSAtomic.h>
#include <stdint.h>

typedef int32_t Atomic32;
typedef intptr_t AtomicWord;

#ifdef __LP64__   // Indicates 64-bit pointers under OS
#define OSAtomicCastIntPtr(p) \
               reinterpret_cast<int64_t *>(const_cast<AtomicWord *>(p))
#define OSAtomicCompareAndSwapIntPtr OSAtomicCompareAndSwap64
#define OSAtomicAddIntPtr OSAtomicAdd64
#else
#define OSAtomicCastIntPtr(p) \
               reinterpret_cast<int32_t *>(const_cast<AtomicWord *>(p))
#define OSAtomicCompareAndSwapIntPtr OSAtomicCompareAndSwap32
#define OSAtomicAddIntPtr OSAtomicAdd32
#endif

inline AtomicWord CompareAndSwap(volatile AtomicWord *ptr,
                                 AtomicWord old_value,
                                 AtomicWord new_value) {
  AtomicWord prev_value;
  do {
    if (OSAtomicCompareAndSwapIntPtr(old_value, new_value,
                                     OSAtomicCastIntPtr(ptr))) {
      return old_value;
    }
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline AtomicWord AtomicExchange(volatile AtomicWord *ptr,
                                 AtomicWord new_value) {
  AtomicWord old_value;
  do {
    old_value = *ptr;
  } while (!OSAtomicCompareAndSwapIntPtr(old_value, new_value,
                                         OSAtomicCastIntPtr(ptr)));
  return old_value;
}


inline AtomicWord AtomicIncrement(volatile AtomicWord *ptr,
                                  AtomicWord increment) {
  return OSAtomicAddIntPtr(increment, OSAtomicCastIntPtr(ptr));
}


// MacOS uses long for intptr_t, AtomicWord and Atomic32 are always different
// on the Mac, even when they are the same size.  Thus, we always provide
// Atomic32 versions.

inline Atomic32 CompareAndSwap(volatile Atomic32 *ptr,
                               Atomic32 old_value,
                               Atomic32 new_value) {
  Atomic32 prev_value;
  do {
    if (OSAtomicCompareAndSwap32(old_value, new_value,
                                 const_cast<Atomic32*>(ptr))) {
      return old_value;
    }
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline Atomic32 AtomicExchange(volatile Atomic32 *ptr,
                               Atomic32 new_value) {
  Atomic32 old_value;
  do {
    old_value = *ptr;
  } while (!OSAtomicCompareAndSwap32(old_value, new_value,
                                     const_cast<Atomic32*>(ptr)));
  return old_value;
}

inline Atomic32 AtomicIncrement(volatile Atomic32 *ptr, Atomic32 increment) {
  return OSAtomicAdd32(increment, const_cast<Atomic32*>(ptr));
}

#endif  // NATIVE_CLIENT_SRC_INCLUDE_OSX_ATOMIC_OPS_OSX_H_
