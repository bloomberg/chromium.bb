// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/PageMemory.h"

#include "platform/heap/Heap.h"
#include "wtf/Assertions.h"
#include "wtf/PageAllocator.h"

namespace blink {

void MemoryRegion::release()
{
    WTF::freePages(m_base, m_size);
}

bool MemoryRegion::commit()
{
    WTF::recommitSystemPages(m_base, m_size);
    return WTF::setSystemPagesAccessible(m_base, m_size);
}

void MemoryRegion::decommit()
{
    WTF::decommitSystemPages(m_base, m_size);
    WTF::setSystemPagesInaccessible(m_base, m_size);
}


PageMemoryRegion::PageMemoryRegion(Address base, size_t size, unsigned numPages)
    : MemoryRegion(base, size)
    , m_isLargePage(numPages == 1)
    , m_numPages(numPages)
{
    Heap::addPageMemoryRegion(this);
    for (size_t i = 0; i < blinkPagesPerRegion; ++i)
        m_inUse[i] = false;
}

PageMemoryRegion::~PageMemoryRegion()
{
    Heap::removePageMemoryRegion(this);
    release();
}

// TODO(haraken): Like partitionOutOfMemoryWithLotsOfUncommitedPages(),
// we should probably have a way to distinguish physical memory OOM from
// virtual address space OOM.
static NEVER_INLINE void blinkGCOutOfMemory()
{
    IMMEDIATE_CRASH();
}

PageMemoryRegion* PageMemoryRegion::allocate(size_t size, unsigned numPages)
{
    // Round size up to the allocation granularity.
    size = (size + WTF::kPageAllocationGranularityOffsetMask) & WTF::kPageAllocationGranularityBaseMask;
    Address base = static_cast<Address>(WTF::allocPages(nullptr, size, blinkPageSize, WTF::PageInaccessible));
    if (!base)
        blinkGCOutOfMemory();
    return new PageMemoryRegion(base, size, numPages);
}

PageMemoryRegion* RegionTree::lookup(Address address)
{
    RegionTree* current = this;
    while (current) {
        Address base = current->m_region->base();
        if (address < base) {
            current = current->m_left;
            continue;
        }
        if (address >= base + current->m_region->size()) {
            current = current->m_right;
            continue;
        }
        ASSERT(current->m_region->contains(address));
        return current->m_region;
    }
    return nullptr;
}

void RegionTree::add(RegionTree* newTree, RegionTree** context)
{
    ASSERT(newTree);
    Address base = newTree->m_region->base();
    for (RegionTree* current = *context; current; current = *context) {
        ASSERT(!current->m_region->contains(base));
        context = (base < current->m_region->base()) ? &current->m_left : &current->m_right;
    }
    *context = newTree;
}

void RegionTree::remove(PageMemoryRegion* region, RegionTree** context)
{
    ASSERT(region);
    ASSERT(context);
    Address base = region->base();
    RegionTree* current = *context;
    for (; current; current = *context) {
        if (region == current->m_region)
            break;
        context = (base < current->m_region->base()) ? &current->m_left : &current->m_right;
    }

    // Shutdown via detachMainThread might not have populated the region tree.
    if (!current)
        return;

    *context = nullptr;
    if (current->m_left) {
        add(current->m_left, context);
        current->m_left = nullptr;
    }
    if (current->m_right) {
        add(current->m_right, context);
        current->m_right = nullptr;
    }
    delete current;
}

PageMemory::PageMemory(PageMemoryRegion* reserved, const MemoryRegion& writable)
    : m_reserved(reserved)
    , m_writable(writable)
{
    ASSERT(reserved->contains(writable));

    // Register the writable area of the memory as part of the LSan root set.
    // Only the writable area is mapped and can contain C++ objects.  Those
    // C++ objects can contain pointers to objects outside of the heap and
    // should therefore be part of the LSan root set.
    __lsan_register_root_region(m_writable.base(), m_writable.size());
}

PageMemory* PageMemory::setupPageMemoryInRegion(PageMemoryRegion* region, size_t pageOffset, size_t payloadSize)
{
    // Setup the payload one guard page into the page memory.
    Address payloadAddress = region->base() + pageOffset + blinkGuardPageSize;
    return new PageMemory(region, MemoryRegion(payloadAddress, payloadSize));
}

static size_t roundToOsPageSize(size_t size)
{
    return (size + WTF::kSystemPageSize - 1) & ~(WTF::kSystemPageSize - 1);
}

PageMemory* PageMemory::allocate(size_t payloadSize)
{
    ASSERT(payloadSize > 0);

    // Virtual memory allocation routines operate in OS page sizes.
    // Round up the requested size to nearest os page size.
    payloadSize = roundToOsPageSize(payloadSize);

    // Overallocate by 2 times OS page size to have space for a
    // guard page at the beginning and end of blink heap page.
    size_t allocationSize = payloadSize + 2 * blinkGuardPageSize;
    PageMemoryRegion* pageMemoryRegion = PageMemoryRegion::allocateLargePage(allocationSize);
    PageMemory* storage = setupPageMemoryInRegion(pageMemoryRegion, 0, payloadSize);
    RELEASE_ASSERT(storage->commit());
    return storage;
}

} // namespace blink
