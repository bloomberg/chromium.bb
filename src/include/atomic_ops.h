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


#ifndef NATIVE_CLIENT_SRC_INCLUDE_ATOMIC_OPS_H_
#define NATIVE_CLIENT_SRC_INCLUDE_ATOMIC_OPS_H_ 1

// ------------------------------------------------------------------------
// Include the platform specific implementations of the types
// and operations listed below.
// ------------------------------------------------------------------------

#include "native_client/src/include/nacl_base.h"

#if NACL_OSX
#include "native_client/src/include/osx/atomic_ops_osx.h"
#elif NACL_LINUX

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
#include "native_client/src/include/linux/x86/atomic_ops.h"
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
#include "native_client/src/include/linux/arm/atomic_ops.h"
#else
#error Unknown platform!
#endif

#elif NACL_WINDOWS
#include "native_client/src/include/win/atomic_ops_win32.h"
#else
#error You need to implement atomic operations for this architecture
#endif

// ------------------------------------------------------------------------
// Commented out type definitions and method declarations for documentation
// of the interface provided by this module.
// ------------------------------------------------------------------------

#if 0

// Signed type that can hold a pointer and supports the atomic ops below, as
// well as atomic loads and stores.  Instances must be naturally-aligned.
typedef intptr_t AtomicWord;

// Signed 32-bit type that supports the atomic ops below, as well as atomic
// loads and stores.  Instances must be naturally aligned.  This type differs
// from AtomicWord in 64-bit binaries where AtomicWord is 64-bits.
typedef int32_t Atomic32;

// Atomically execute:
//      result = *ptr;
//      if (*ptr == old_value)
//        *ptr = new_value;
//      return result;
//
// I.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value".
// Always return the old value of "*ptr"
//
// This routine implies no memory barriers.
AtomicWord CompareAndSwap(volatile AtomicWord* ptr,
                          AtomicWord old_value,
                          AtomicWord new_value);

// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
AtomicWord AtomicExchange(volatile AtomicWord* ptr, AtomicWord new_value);

// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory
// barriers.
AtomicWord AtomicIncrement(volatile AtomicWord* ptr, AtomicWord increment);

#endif

#endif  // NATIVE_CLIENT_SRC_INCLUDE_ATOMIC_OPS_H_
