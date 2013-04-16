/*
 * Copyright (C) 2008, 2009 Paul Pedriana <ppedriana@ea.com>. All rights reserved.
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

#ifndef FastAllocBase_h
#define FastAllocBase_h

// Provides customizable overrides of fastMalloc/fastFree and operator new/delete
//
// Provided functionality:
//    Macro: WTF_MAKE_FAST_ALLOCATED
//
// Example usage:
//    class Widget {
//        WTF_MAKE_FAST_ALLOCATED
//    ...
//    };
//
//    struct Data {
//        WTF_MAKE_FAST_ALLOCATED
//    public:
//    ...
//    };
//

#include <new>
#include <stdint.h>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>

// This needs to exist because some classes (e.g. StringImpl) want to use
// fastMalloc() to return an object pointer and then have it deleted (as
// opposed to free'd) by the RefPtr machinery.
#define NEW_DELETE_SAME_AS_MALLOC_FREE \
public: \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        void* p = ::WTF::fastMalloc(size); \
         ::WTF::fastMallocMatchValidateMalloc(p, ::WTF::Internal::AllocTypeClassNew); \
        return p; \
    } \
    \
    void operator delete(void* p) \
    { \
        ::WTF::fastMallocMatchValidateFree(p, ::WTF::Internal::AllocTypeClassNew); \
        ::WTF::fastFree(p); \
    } \
    \
    void* operator new[](size_t size) \
    { \
        void* p = ::WTF::fastMalloc(size); \
        ::WTF::fastMallocMatchValidateMalloc(p, ::WTF::Internal::AllocTypeClassNewArray); \
        return p; \
    } \
    \
    void operator delete[](void* p) \
    { \
         ::WTF::fastMallocMatchValidateFree(p, ::WTF::Internal::AllocTypeClassNewArray); \
         ::WTF::fastFree(p); \
    } \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
private: \
typedef int __thisIsHereToForceASemicolonAfterThisMacro

#if !USE(SYSTEM_MALLOC)
#define WTF_MAKE_FAST_ALLOCATED NEW_DELETE_SAME_AS_MALLOC_FREE
#else
#define WTF_MAKE_FAST_ALLOCATED
#endif

#endif // FastAllocBase_h
