// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PageMemory_h
#define PageMemory_h

#include "platform/heap/HeapPage.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/PageAllocator.h"

#if OS(POSIX)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace blink {

class MemoryRegion {
    USING_FAST_MALLOC(MemoryRegion);
public:
    MemoryRegion(Address base, size_t size)
        : m_base(base)
        , m_size(size)
    {
        ASSERT(size > 0);
    }

    bool contains(Address addr) const
    {
        return m_base <= addr && addr < (m_base + m_size);
    }

    bool contains(const MemoryRegion& other) const
    {
        return contains(other.m_base) && contains(other.m_base + other.m_size - 1);
    }

    void release();
    WARN_UNUSED_RETURN bool commit();
    void decommit();

    Address base() const { return m_base; }
    size_t size() const { return m_size; }

private:
    Address m_base;
    size_t m_size;
};

// A PageMemoryRegion represents a chunk of reserved virtual address
// space containing a number of blink heap pages. On Windows, reserved
// virtual address space can only be given back to the system as a
// whole. The PageMemoryRegion allows us to do that by keeping track
// of the number of pages using it in order to be able to release all
// of the virtual address space when there are no more pages using it.
class PageMemoryRegion : public MemoryRegion {
public:
    ~PageMemoryRegion();

    void pageDeleted(Address page)
    {
        markPageUnused(page);
        if (!--m_numPages)
            delete this;
    }

    void markPageUsed(Address page)
    {
        ASSERT(!m_inUse[index(page)]);
        m_inUse[index(page)] = true;
    }

    void markPageUnused(Address page)
    {
        m_inUse[index(page)] = false;
    }

    static PageMemoryRegion* allocateLargePage(size_t size)
    {
        return allocate(size, 1);
    }

    static PageMemoryRegion* allocateNormalPages()
    {
        return allocate(blinkPageSize * blinkPagesPerRegion, blinkPagesPerRegion);
    }

    BasePage* pageFromAddress(Address address)
    {
        ASSERT(contains(address));
        if (!m_inUse[index(address)])
            return nullptr;
        if (m_isLargePage)
            return pageFromObject(base());
        return pageFromObject(address);
    }

private:
    PageMemoryRegion(Address base, size_t, unsigned numPages);

    unsigned index(Address address)
    {
        ASSERT(contains(address));
        if (m_isLargePage)
            return 0;
        size_t offset = blinkPageAddress(address) - base();
        ASSERT(offset % blinkPageSize == 0);
        return offset / blinkPageSize;
    }

    static PageMemoryRegion* allocate(size_t, unsigned numPages);

    bool m_isLargePage;
    bool m_inUse[blinkPagesPerRegion];
    unsigned m_numPages;
};

// A RegionTree is a simple binary search tree of PageMemoryRegions sorted
// by base addresses.
class RegionTree {
    USING_FAST_MALLOC(RegionTree);
public:
    explicit RegionTree(PageMemoryRegion* region)
        : m_region(region)
        , m_left(nullptr)
        , m_right(nullptr)
    {
    }

    ~RegionTree()
    {
        delete m_left;
        delete m_right;
    }

    PageMemoryRegion* lookup(Address);
    static void add(RegionTree*, RegionTree**);
    static void remove(PageMemoryRegion*, RegionTree**);

private:
    PageMemoryRegion* m_region;
    RegionTree* m_left;
    RegionTree* m_right;

    static RegionTree* s_regionTree;
};

// Representation of the memory used for a Blink heap page.
//
// The representation keeps track of two memory regions:
//
// 1. The virtual memory reserved from the system in order to be able
//    to free all the virtual memory reserved.  Multiple PageMemory
//    instances can share the same reserved memory region and
//    therefore notify the reserved memory region on destruction so
//    that the system memory can be given back when all PageMemory
//    instances for that memory are gone.
//
// 2. The writable memory (a sub-region of the reserved virtual
//    memory region) that is used for the actual heap page payload.
//
// Guard pages are created before and after the writable memory.
class PageMemory {
    USING_FAST_MALLOC(PageMemory);
public:
    ~PageMemory()
    {
        __lsan_unregister_root_region(m_writable.base(), m_writable.size());
        m_reserved->pageDeleted(writableStart());
    }

    WARN_UNUSED_RETURN bool commit()
    {
        m_reserved->markPageUsed(writableStart());
        return m_writable.commit();
    }

    void decommit()
    {
        m_reserved->markPageUnused(writableStart());
        m_writable.decommit();
    }

    void markUnused() { m_reserved->markPageUnused(writableStart()); }

    PageMemoryRegion* region() { return m_reserved; }

    Address writableStart() { return m_writable.base(); }

    static PageMemory* setupPageMemoryInRegion(PageMemoryRegion*, size_t pageOffset, size_t payloadSize);

    // Allocate a virtual address space for one blink page with the
    // following layout:
    //
    //    [ guard os page | ... payload ... | guard os page ]
    //    ^---{ aligned to blink page size }
    //
    // The returned page memory region will be zeroed.
    //
    static PageMemory* allocate(size_t payloadSize);

private:
    PageMemory(PageMemoryRegion* reserved, const MemoryRegion& writable);

    PageMemoryRegion* m_reserved;
    MemoryRegion m_writable;
};

} // namespace blink

#endif
