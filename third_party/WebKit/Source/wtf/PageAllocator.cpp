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

#include "wtf/AddressSpaceRandomization.h"
#include "wtf/Assertions.h"

#include <limits.h>

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

// This internal function wraps the OS-specific page allocation call so that
// it behaves consistently: the address is a hint and if it cannot be used,
// the allocation will be placed elsewhere.
static void* systemAllocPages(void* addr, size_t len, PageAccessibilityConfiguration pageAccessibility)
{
    ASSERT(!(len & kPageAllocationGranularityOffsetMask));
    ASSERT(!(reinterpret_cast<uintptr_t>(addr) & kPageAllocationGranularityOffsetMask));
    void* ret;
#if OS(WIN)
    DWORD accessFlag = pageAccessibility == PageAccessible ? PAGE_READWRITE : PAGE_NOACCESS;
    ret = VirtualAlloc(addr, len, MEM_RESERVE | MEM_COMMIT, accessFlag);
    if (!ret)
        ret = VirtualAlloc(0, len, MEM_RESERVE | MEM_COMMIT, accessFlag);
#else
    int accessFlag = pageAccessibility == PageAccessible ? (PROT_READ | PROT_WRITE) : PROT_NONE;
    ret = mmap(addr, len, accessFlag, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ret == MAP_FAILED)
        ret = 0;
#endif
    return ret;
}

void* allocPages(void* addr, size_t len, size_t align, PageAccessibilityConfiguration pageAccessibility)
{
    ASSERT(len >= kPageAllocationGranularity);
    ASSERT(!(len & kPageAllocationGranularityOffsetMask));
    ASSERT(align >= kPageAllocationGranularity);
    ASSERT(!(align & kPageAllocationGranularityOffsetMask));
    ASSERT(!(reinterpret_cast<uintptr_t>(addr) & kPageAllocationGranularityOffsetMask));
    uintptr_t alignOffsetMask = align - 1;
    uintptr_t alignBaseMask = ~alignOffsetMask;
    ASSERT(!(reinterpret_cast<uintptr_t>(addr) & alignOffsetMask));
    // If the client passed null as the address, choose a good one.
    if (!addr) {
        addr = getRandomPageBase();
        addr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) & alignBaseMask);
    }

    // First try to force an exact-size, aligned allocation from our random base.
    for (int count = 0; count < 3; ++count) {
        void* ret = systemAllocPages(addr, len, pageAccessibility);
        // If the alignment is to our liking, we're done.
        if (!(reinterpret_cast<uintptr_t>(ret) & alignOffsetMask))
            return ret;
        // We failed, so we retry another range depending on the size of our address space.
        freePages(ret, len);
#if CPU(32BIT)
        // Use a linear probe on 32-bit systems, where the address space tends to be cramped.
        // This may wrap, but we'll just fall back to the guaranteed method in that case.
        addr = reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(ret) + align) & alignBaseMask);
#else
        // Keep trying random addresses on systems that have a large address space.
        addr = getRandomPageBase();
        addr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) & alignBaseMask);
#endif
    }

    // Map a larger allocation so we can force alignment, but continuing randomizing only on
    // 64-bit POSIX.
    size_t tryLen = len + (align - kPageAllocationGranularity);
    RELEASE_ASSERT(tryLen > len);
    while (true) {
        addr = nullptr;
#if OS(POSIX) && CPU(32BIT)
        addr = getRandomPageBase();
#endif
        void* ret = systemAllocPages(addr, tryLen, pageAccessibility);
        if (!ret)
            return nullptr;
        size_t preSlack = reinterpret_cast<uintptr_t>(ret) & alignOffsetMask;
        preSlack = preSlack ? align - preSlack : 0;
        size_t postSlack = tryLen - preSlack - len;
        ASSERT(preSlack || postSlack);
        ASSERT(preSlack < tryLen);
        ASSERT(postSlack < tryLen);
#if OS(POSIX) // On POSIX we can resize the allocation run.
        if (preSlack) {
            int res = munmap(ret, preSlack);
            RELEASE_ASSERT(!res);
            ret = addr = reinterpret_cast<char*>(ret) + preSlack;
        }
        if (postSlack) {
            int res = munmap(reinterpret_cast<char*>(ret) + len, postSlack);
            RELEASE_ASSERT(!res);
        }
#else // On Windows we can't resize the allocation run.
        if (preSlack || postSlack) {
            addr = reinterpret_cast<char*>(ret) + preSlack;
            freePages(ret, len);
            ret = systemAllocPages(addr, len, pageAccessibility);
            if (!ret)
                return nullptr;
        }
#endif
        if (ret == addr) {
            ASSERT(!(reinterpret_cast<uintptr_t>(ret) & alignOffsetMask));
            return ret;
        }
        freePages(ret, len);
    }

    return nullptr;
}

void freePages(void* addr, size_t len)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(addr) & kPageAllocationGranularityOffsetMask));
    ASSERT(!(len & kPageAllocationGranularityOffsetMask));
#if OS(POSIX)
    int ret = munmap(addr, len);
    RELEASE_ASSERT(!ret);
#else
    BOOL ret = VirtualFree(addr, 0, MEM_RELEASE);
    RELEASE_ASSERT(ret);
#endif
}

void setSystemPagesInaccessible(void* addr, size_t len)
{
    ASSERT(!(len & kSystemPageOffsetMask));
#if OS(POSIX)
    int ret = mprotect(addr, len, PROT_NONE);
    RELEASE_ASSERT(!ret);
#else
    BOOL ret = VirtualFree(addr, len, MEM_DECOMMIT);
    RELEASE_ASSERT(ret);
#endif
}

bool setSystemPagesAccessible(void* addr, size_t len)
{
    ASSERT(!(len & kSystemPageOffsetMask));
#if OS(POSIX)
    return !mprotect(addr, len, PROT_READ | PROT_WRITE);
#else
    return !!VirtualAlloc(addr, len, MEM_COMMIT, PAGE_READWRITE);
#endif
}

void decommitSystemPages(void* addr, size_t len)
{
    ASSERT(!(len & kSystemPageOffsetMask));
#if OS(POSIX)
    int ret = madvise(addr, len, MADV_FREE);
    RELEASE_ASSERT(!ret);
#else
    setSystemPagesInaccessible(addr, len);
#endif
}

void recommitSystemPages(void* addr, size_t len)
{
    ASSERT(!(len & kSystemPageOffsetMask));
#if OS(POSIX)
    (void) addr;
#else
    RELEASE_ASSERT(setSystemPagesAccessible(addr, len));
#endif
}

void discardSystemPages(void* addr, size_t len)
{
    ASSERT(!(len & kSystemPageOffsetMask));
#if OS(POSIX)
    // On POSIX, the implementation detail is that discard and decommit are the
    // same, and lead to pages that are returned to the system immediately and
    // get replaced with zeroed pages when touched. So we just call
    // decommitSystemPages() here to avoid code duplication.
    decommitSystemPages(addr, len);
#else
    (void) addr;
    (void) len;
    // TODO(cevans): implement this using MEM_RESET for Windows, once we've
    // decided that the semantics are a match.
#endif
}

} // namespace WTF

