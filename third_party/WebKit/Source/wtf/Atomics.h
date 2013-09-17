/*
 * Copyright (C) 2007, 2008, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Atomics_h
#define Atomics_h

#include <stdint.h>

#if OS(WIN)
#include <windows.h>
#elif OS(ANDROID)
#include <sys/atomics.h>
#endif

namespace WTF {

#if OS(WIN)

ALWAYS_INLINE int atomicIncrement(int volatile* addend) { return InterlockedIncrement(reinterpret_cast<long volatile*>(addend)); }
ALWAYS_INLINE int atomicDecrement(int volatile* addend) { return InterlockedDecrement(reinterpret_cast<long volatile*>(addend)); }

ALWAYS_INLINE int64_t atomicIncrement(int64_t volatile* addend) { return InterlockedIncrement64(reinterpret_cast<long long volatile*>(addend)); }
ALWAYS_INLINE int64_t atomicDecrement(int64_t volatile* addend) { return InterlockedDecrement64(reinterpret_cast<long long volatile*>(addend)); }

#elif OS(ANDROID)

// Note, __atomic_{inc, dec}() return the previous value of addend's content.
ALWAYS_INLINE int atomicIncrement(int volatile* addend) { return __atomic_inc(addend) + 1; }
ALWAYS_INLINE int atomicDecrement(int volatile* addend) { return __atomic_dec(addend) - 1; }

#else

ALWAYS_INLINE int atomicIncrement(int volatile* addend) { return __sync_add_and_fetch(addend, 1); }
ALWAYS_INLINE int atomicDecrement(int volatile* addend) { return __sync_sub_and_fetch(addend, 1); }

ALWAYS_INLINE int64_t atomicIncrement(int64_t volatile* addend) { return __sync_add_and_fetch(addend, 1); }
ALWAYS_INLINE int64_t atomicDecrement(int64_t volatile* addend) { return __sync_sub_and_fetch(addend, 1); }

#endif

} // namespace WTF

using WTF::atomicDecrement;
using WTF::atomicIncrement;

#endif // Atomics_h
