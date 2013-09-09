/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "wtf/PageAllocator.h"

#include "wtf/Assertions.h"
#include "wtf/CPU.h"
#include "wtf/CryptographicallyRandomNumber.h"

#if OS(POSIX)

#include <sys/mman.h>

#ifndef MADV_FREE
#define MADV_FREE MADV_DONTNEED
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#elif OS(WIN)

#include <windows.h>

#else
#error Unknown OS
#endif // OS(POSIX)

namespace WTF {

void* allocSuperPages(void* addr, size_t len)
{
    ASSERT(!(len & kSuperPageOffsetMask));
    ASSERT(!(reinterpret_cast<uintptr_t>(addr) & kSuperPageOffsetMask));
#if OS(POSIX)
    char* ptr = reinterpret_cast<char*>(mmap(addr, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    RELEASE_ASSERT(ptr != MAP_FAILED);
    // If our requested address collided with another mapping, there's a
    // chance we'll get back an unaligned address. We fix this by attempting
    // the allocation again, but with enough slack pages that we can find
    // correct alignment within the allocation.
    if (UNLIKELY(reinterpret_cast<uintptr_t>(ptr) & kSuperPageOffsetMask)) {
        int ret = munmap(ptr, len);
        ASSERT(!ret);
        ptr = reinterpret_cast<char*>(mmap(0, len + kSuperPageSize - kSystemPageSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
        RELEASE_ASSERT(ptr != MAP_FAILED);
        int numSystemPagesToUnmap = kNumSystemPagesPerSuperPage - 1;
        int numSystemPagesBefore = (kNumSystemPagesPerSuperPage - ((reinterpret_cast<uintptr_t>(ptr) & kSuperPageOffsetMask) / kSystemPageSize)) % kNumSystemPagesPerSuperPage;
        ASSERT(numSystemPagesBefore <= numSystemPagesToUnmap);
        int numSystemPagesAfter = numSystemPagesToUnmap - numSystemPagesBefore;
        if (numSystemPagesBefore) {
            size_t beforeSize = kSystemPageSize * numSystemPagesBefore;
            ret = munmap(ptr, beforeSize);
            ASSERT(!ret);
            ptr += beforeSize;
        }
        if (numSystemPagesAfter) {
            ret = munmap(ptr + len, kSystemPageSize * numSystemPagesAfter);
            ASSERT(!ret);
        }
    }
    void* ret = ptr;
#else
    // Windows is a lot simpler because we've designed around its
    // coarser-grained alignement.
    void* ret = VirtualAlloc(addr, len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!ret)
        ret = VirtualAlloc(0, len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    RELEASE_ASSERT(ret);
#endif // OS(POSIX)
    return ret;
}

void freeSuperPages(void* addr, size_t len)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(addr) & kSuperPageOffsetMask));
    ASSERT(!(len & kSuperPageOffsetMask));
#if OS(POSIX)
    int ret = munmap(addr, len);
    ASSERT(!ret);
#else
    BOOL ret = VirtualFree(addr, 0, MEM_RELEASE);
    ASSERT(ret);
#endif
}

void setSystemPagesInaccessible(void* addr, size_t len)
{
    ASSERT(!(len & kSystemPageOffsetMask));
#if OS(POSIX)
    int ret = mprotect(addr, len, PROT_NONE);
    ASSERT(!ret);
#else
    BOOL ret = VirtualFree(addr, len, MEM_DECOMMIT);
    ASSERT(ret);
#endif
}

void decommitSystemPages(void* addr, size_t len)
{
    ASSERT(!(len & kSystemPageOffsetMask));
#if OS(POSIX)
    int ret = madvise(addr, len, MADV_FREE);
    ASSERT(!ret);
#else
    void* ret = VirtualAlloc(addr, len, MEM_RESET, PAGE_READWRITE);
    ASSERT(ret);
#endif
}

char* getRandomSuperPageBase()
{
    uintptr_t random;
    random = static_cast<uintptr_t>(cryptographicallyRandomNumber());
#if CPU(X86_64)
    random <<= 32UL;
    random |= static_cast<uintptr_t>(cryptographicallyRandomNumber());
    // This address mask gives a low liklihood of address space collisions.
    // We handle the situation gracefully if there is a collision.
#if OS(WIN)
    // 64-bit Windows has a bizarrely small 8TB user address space.
    // Allocates in the 1-5TB region.
    random &= (0x3ffffffffffUL & kSuperPageBaseMask);
    random += 0x10000000000UL;
#else
    random &= (0x3fffffffffffUL & kSuperPageBaseMask);
#endif
#else // !CPU(X86_64)
    // This is a good range on Windows, Linux and Mac.
    // Allocates in the 0.5-1.5GB region.
    random &= (0x3fffffff & kSuperPageBaseMask);
    random += 0x20000000;
#endif // CPU(X86_64)
    return reinterpret_cast<char*>(random);
}

} // namespace WTF

