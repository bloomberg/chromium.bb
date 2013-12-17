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
#include "heap/Heap.h"

#include "heap/ThreadState.h"

#if OS(POSIX)
#include <sys/mman.h>
#include <unistd.h>
#elif OS(WIN)
#include <windows.h>
#endif

namespace WebCore {

#if OS(WIN)
static bool IsPowerOf2(size_t power)
{
    return !((power - 1) & power);
}
#endif

static Address roundToBlinkPageBoundary(void* base)
{
    return reinterpret_cast<Address>((reinterpret_cast<uintptr_t>(base) + blinkPageOffsetMask) & blinkPageBaseMask);
}

static size_t roundToOsPageSize(size_t size)
{
    return (size + osPageSize() - 1) & ~(osPageSize() - 1);
}

size_t osPageSize()
{
#if OS(POSIX)
    static const size_t pageSize = getpagesize();
#else
    static size_t pageSize = 0;
    if (!pageSize) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        ASSERT(IsPowerOf2(pageSize));
    }
#endif
    return pageSize;
}

class MemoryRegion {
public:
    MemoryRegion(Address base, size_t size) : m_base(base), m_size(size) { ASSERT(size > 0); }

    bool contains(Address addr) const
    {
        return m_base <= addr && addr < (m_base + m_size);
    }


    bool contains(const MemoryRegion& other) const
    {
        return contains(other.m_base) && contains(other.m_base + other.m_size - 1);
    }

    void release()
    {
#if OS(POSIX)
        int err = munmap(m_base, m_size);
        RELEASE_ASSERT(!err);
#else
        bool success = VirtualFree(m_base, 0, MEM_RELEASE);
        RELEASE_ASSERT(success);
#endif
    }

    WARN_UNUSED_RETURN bool commit()
    {
#if OS(POSIX)
        int err = mprotect(m_base, m_size, PROT_READ | PROT_WRITE);
        if (!err) {
            madvise(m_base, m_size, MADV_NORMAL);
            return true;
        }
        return false;
#else
        void* result = VirtualAlloc(m_base, m_size, MEM_COMMIT, PAGE_READWRITE);
        return !!result;
#endif
    }

    void decommit()
    {
#if OS(POSIX)
        int err = mprotect(m_base, m_size, PROT_NONE);
        RELEASE_ASSERT(!err);
        // FIXME: Consider using MADV_FREE on MacOS.
        madvise(m_base, m_size, MADV_DONTNEED);
#else
        bool success = VirtualFree(m_base, m_size, MEM_DECOMMIT);
        RELEASE_ASSERT(success);
#endif
    }

    Address base() const { return m_base; }

private:
    Address m_base;
    size_t m_size;
};

// Representation of the memory used for a Blink heap page.
//
// The representation keeps track of two memory regions:
//
// 1. The virtual memory reserved from the sytem in order to be able
//    to free all the virtual memory reserved on destruction.
//
// 2. The writable memory (a sub-region of the reserved virtual
//    memory region) that is used for the actual heap page payload.
//
// Guard pages are create before and after the writable memory.
class PageMemory {
public:
    ~PageMemory() { m_reserved.release(); }

    bool commit() WARN_UNUSED_RETURN { return m_writable.commit(); }
    void decommit() { m_writable.decommit(); }

    Address writableStart() { return m_writable.base(); }

    // Allocate a virtual address space for the blink page with the
    // following layout:
    //
    //    [ guard os page | ... payload ... | guard os page ]
    //    ^---{ aligned to blink page size }
    //
    static PageMemory* allocate(size_t payloadSize)
    {
        ASSERT(payloadSize > 0);

        // Virtual memory allocation routines operate in OS page sizes.
        // Round up the requested size to nearest os page size.
        payloadSize = roundToOsPageSize(payloadSize);

        // Overallocate by blinkPageSize and 2 times OS page size to
        // ensure a chunk of memory which is blinkPageSize aligned and
        // has a system page before and after to use for guarding. We
        // unmap the excess memory before returning.
        size_t allocationSize = payloadSize + 2 * osPageSize() + blinkPageSize;

#if OS(POSIX)
        Address base = static_cast<Address>(mmap(0, allocationSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0));
        RELEASE_ASSERT(base != MAP_FAILED);

        Address end = base + allocationSize;
        Address alignedBase = roundToBlinkPageBoundary(base);
        Address payloadBase = alignedBase + osPageSize();
        Address payloadEnd = payloadBase + payloadSize;
        Address blinkPageEnd = payloadEnd + osPageSize();

        // If the allocate memory was not blink page aligned release
        // the memory before the aligned address.
        if (alignedBase != base)
            MemoryRegion(base, alignedBase - base).release();

        // Create guard pages by decommiting an OS page before and
        // after the payload.
        MemoryRegion(alignedBase, osPageSize()).decommit();
        MemoryRegion(payloadEnd, osPageSize()).decommit();

        // Free the additional memory at the end of the page if any.
        if (blinkPageEnd < end)
            MemoryRegion(blinkPageEnd, end - blinkPageEnd).release();

        return new PageMemory(MemoryRegion(alignedBase, blinkPageEnd - alignedBase), MemoryRegion(payloadBase, payloadSize));
#else
        Address base = 0;
        Address alignedBase = 0;

        // On Windows it is impossible to partially release a region
        // of memory allocated by VirtualAlloc. To avoid wasting
        // virtual address space we attempt to release a large region
        // of memory returned as a whole and then allocate an aligned
        // region inside this larger region.
        for (int attempt = 0; attempt < 3; attempt++) {
            base = static_cast<Address>(VirtualAlloc(0, allocationSize, MEM_RESERVE, PAGE_NOACCESS));
            RELEASE_ASSERT(base);
            VirtualFree(base, 0, MEM_RELEASE);

            alignedBase = roundToBlinkPageBoundary(base);
            base = static_cast<Address>(VirtualAlloc(alignedBase, payloadSize + 2 * osPageSize(), MEM_RESERVE, PAGE_NOACCESS));
            if (base) {
                RELEASE_ASSERT(base == alignedBase);
                allocationSize = payloadSize + 2 * osPageSize();
                break;
            }
        }

        if (!base) {
            // We failed to avoid wasting virtual address space after
            // several attempts.
            base = static_cast<Address>(VirtualAlloc(0, allocationSize, MEM_RESERVE, PAGE_NOACCESS));
            RELEASE_ASSERT(base);

            // FIXME: If base is by accident blink page size aligned
            // here then we can create two pages out of reserved
            // space. Do this.
            alignedBase = roundToBlinkPageBoundary(base);
        }

        Address payloadBase = alignedBase + osPageSize();
        PageMemory* storage = new PageMemory(MemoryRegion(base, allocationSize), MemoryRegion(payloadBase, payloadSize));
        bool res = storage->commit();
        RELEASE_ASSERT(res);
        return storage;
#endif
    }

private:
    PageMemory(const MemoryRegion& reserved, const MemoryRegion& writable)
        : m_reserved(reserved)
        , m_writable(writable)
    {
        ASSERT(reserved.contains(writable));
    }

    MemoryRegion m_reserved;
    MemoryRegion m_writable;
};

void Heap::init(intptr_t* startOfStack)
{
    ThreadState::init(startOfStack);
}

void Heap::shutdown()
{
    ThreadState::shutdown();
}

}
