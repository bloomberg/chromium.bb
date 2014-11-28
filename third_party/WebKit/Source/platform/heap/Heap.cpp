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
#include "platform/heap/Heap.h"

#include "platform/ScriptForbiddenScope.h"
#include "platform/Task.h"
#include "platform/TraceEvent.h"
#include "platform/heap/CallbackStack.h"
#include "platform/heap/ThreadState.h"
#include "public/platform/Platform.h"
#include "wtf/AddressSpaceRandomization.h"
#include "wtf/Assertions.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/PassOwnPtr.h"
#if ENABLE(GC_PROFILE_MARKING)
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"
#include <stdio.h>
#include <utility>
#endif
#if ENABLE(GC_PROFILE_HEAP)
#include "platform/TracedValue.h"
#endif

#if OS(POSIX)
#include <sys/mman.h>
#include <unistd.h>
#elif OS(WIN)
#include <windows.h>
#endif

namespace blink {

#if ENABLE(GC_PROFILE_MARKING)
static String classOf(const void* object)
{
    const GCInfo* gcInfo = Heap::findGCInfo(reinterpret_cast<Address>(const_cast<void*>(object)));
    if (gcInfo)
        return gcInfo->m_className;

    return "unknown";
}
#endif

static bool vTableInitialized(void* objectPointer)
{
    return !!(*reinterpret_cast<Address*>(objectPointer));
}

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
    return (size + WTF::kSystemPageSize - 1) & ~(WTF::kSystemPageSize - 1);
}

class MemoryRegion {
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
        return !mprotect(m_base, m_size, PROT_READ | PROT_WRITE);
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
    ~PageMemoryRegion()
    {
        release();
    }

    void pageDeleted(Address page)
    {
        markPageUnused(page);
        if (!--m_numPages) {
            Heap::removePageMemoryRegion(this);
            delete this;
        }
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

    BaseHeapPage* pageFromAddress(Address address)
    {
        ASSERT(contains(address));
        if (!m_inUse[index(address)])
            return 0;
        if (m_isLargePage)
            return pageFromObject(base());
        return pageFromObject(address);
    }

private:
    PageMemoryRegion(Address base, size_t size, unsigned numPages)
        : MemoryRegion(base, size)
        , m_isLargePage(numPages == 1)
        , m_numPages(numPages)
    {
        for (size_t i = 0; i < blinkPagesPerRegion; ++i)
            m_inUse[i] = false;
    }

    unsigned index(Address address)
    {
        ASSERT(contains(address));
        if (m_isLargePage)
            return 0;
        size_t offset = blinkPageAddress(address) - base();
        ASSERT(offset % blinkPageSize == 0);
        return offset / blinkPageSize;
    }

    static PageMemoryRegion* allocate(size_t size, unsigned numPages)
    {
        // Compute a random blink page aligned address for the page memory
        // region and attempt to get the memory there.
        Address randomAddress = reinterpret_cast<Address>(WTF::getRandomPageBase());
        Address alignedRandomAddress = roundToBlinkPageBoundary(randomAddress);

#if OS(POSIX)
        Address base = static_cast<Address>(mmap(alignedRandomAddress, size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0));
        if (base == roundToBlinkPageBoundary(base))
            return new PageMemoryRegion(base, size, numPages);

        // We failed to get a blink page aligned chunk of memory.
        // Unmap the chunk that we got and fall back to overallocating
        // and selecting an aligned sub part of what we allocate.
        if (base != MAP_FAILED) {
            int error = munmap(base, size);
            RELEASE_ASSERT(!error);
        }
        size_t allocationSize = size + blinkPageSize;
        for (int attempt = 0; attempt < 10; attempt++) {
            base = static_cast<Address>(mmap(alignedRandomAddress, allocationSize, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0));
            if (base != MAP_FAILED)
                break;
            randomAddress = reinterpret_cast<Address>(WTF::getRandomPageBase());
            alignedRandomAddress = roundToBlinkPageBoundary(randomAddress);
        }
        RELEASE_ASSERT(base != MAP_FAILED);

        Address end = base + allocationSize;
        Address alignedBase = roundToBlinkPageBoundary(base);
        Address regionEnd = alignedBase + size;

        // If the allocated memory was not blink page aligned release
        // the memory before the aligned address.
        if (alignedBase != base)
            MemoryRegion(base, alignedBase - base).release();

        // Free the additional memory at the end of the page if any.
        if (regionEnd < end)
            MemoryRegion(regionEnd, end - regionEnd).release();

        return new PageMemoryRegion(alignedBase, size, numPages);
#else
        Address base = static_cast<Address>(VirtualAlloc(alignedRandomAddress, size, MEM_RESERVE, PAGE_NOACCESS));
        if (base) {
            ASSERT(base == alignedRandomAddress);
            return new PageMemoryRegion(base, size, numPages);
        }

        // We failed to get the random aligned address that we asked
        // for. Fall back to overallocating. On Windows it is
        // impossible to partially release a region of memory
        // allocated by VirtualAlloc. To avoid wasting virtual address
        // space we attempt to release a large region of memory
        // returned as a whole and then allocate an aligned region
        // inside this larger region.
        size_t allocationSize = size + blinkPageSize;
        for (int attempt = 0; attempt < 3; attempt++) {
            base = static_cast<Address>(VirtualAlloc(0, allocationSize, MEM_RESERVE, PAGE_NOACCESS));
            RELEASE_ASSERT(base);
            VirtualFree(base, 0, MEM_RELEASE);

            Address alignedBase = roundToBlinkPageBoundary(base);
            base = static_cast<Address>(VirtualAlloc(alignedBase, size, MEM_RESERVE, PAGE_NOACCESS));
            if (base) {
                ASSERT(base == alignedBase);
                return new PageMemoryRegion(alignedBase, size, numPages);
            }
        }

        // We failed to avoid wasting virtual address space after
        // several attempts.
        base = static_cast<Address>(VirtualAlloc(0, allocationSize, MEM_RESERVE, PAGE_NOACCESS));
        RELEASE_ASSERT(base);

        // FIXME: If base is by accident blink page size aligned
        // here then we can create two pages out of reserved
        // space. Do this.
        Address alignedBase = roundToBlinkPageBoundary(base);

        return new PageMemoryRegion(alignedBase, size, numPages);
#endif
    }

    bool m_isLargePage;
    bool m_inUse[blinkPagesPerRegion];
    unsigned m_numPages;
};

// Representation of the memory used for a Blink heap page.
//
// The representation keeps track of two memory regions:
//
// 1. The virtual memory reserved from the system in order to be able
//    to free all the virtual memory reserved. Multiple PageMemory
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

    static PageMemory* setupPageMemoryInRegion(PageMemoryRegion* region, size_t pageOffset, size_t payloadSize)
    {
        // Setup the payload one OS page into the page memory. The
        // first os page is the guard page.
        Address payloadAddress = region->base() + pageOffset + WTF::kSystemPageSize;
        return new PageMemory(region, MemoryRegion(payloadAddress, payloadSize));
    }

    // Allocate a virtual address space for one blink page with the
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

        // Overallocate by 2 times OS page size to have space for a
        // guard page at the beginning and end of blink heap page.
        size_t allocationSize = payloadSize + 2 * WTF::kSystemPageSize;
        PageMemoryRegion* pageMemoryRegion = PageMemoryRegion::allocateLargePage(allocationSize);
        PageMemory* storage = setupPageMemoryInRegion(pageMemoryRegion, 0, payloadSize);
        RELEASE_ASSERT(storage->commit());
        return storage;
    }

private:
    PageMemory(PageMemoryRegion* reserved, const MemoryRegion& writable)
        : m_reserved(reserved)
        , m_writable(writable)
    {
        ASSERT(reserved->contains(writable));

        // Register the writable area of the memory as part of the LSan root set.
        // Only the writable area is mapped and can contain C++ objects. Those
        // C++ objects can contain pointers to objects outside of the heap and
        // should therefore be part of the LSan root set.
        __lsan_register_root_region(m_writable.base(), m_writable.size());
    }


    PageMemoryRegion* m_reserved;
    MemoryRegion m_writable;
};

class GCScope {
public:
    explicit GCScope(ThreadState::StackState stackState)
        : m_state(ThreadState::current())
        , m_safePointScope(stackState)
        , m_parkedAllThreads(false)
    {
        TRACE_EVENT0("blink_gc", "Heap::GCScope");
        const char* samplingState = TRACE_EVENT_GET_SAMPLING_STATE();
        if (m_state->isMainThread())
            TRACE_EVENT_SET_SAMPLING_STATE("blink_gc", "BlinkGCWaiting");

        m_state->checkThread();

        // FIXME: in an unlikely coincidence that two threads decide
        // to collect garbage at the same time, avoid doing two GCs in
        // a row.
        if (LIKELY(ThreadState::stopThreads())) {
            m_parkedAllThreads = true;
        }
        if (m_state->isMainThread())
            TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(samplingState);
    }

    bool allThreadsParked() { return m_parkedAllThreads; }

    ~GCScope()
    {
        // Only cleanup if we parked all threads in which case the GC happened
        // and we need to resume the other threads.
        if (LIKELY(m_parkedAllThreads)) {
            ThreadState::resumeThreads();
        }
    }

private:
    ThreadState* m_state;
    ThreadState::SafePointScope m_safePointScope;
    bool m_parkedAllThreads; // False if we fail to park all threads
};

NO_SANITIZE_ADDRESS inline
bool HeapObjectHeader::isMarked() const
{
    checkHeader();
    return m_size & markBitMask;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::unmark()
{
    checkHeader();
    m_size &= ~markBitMask;
}

NO_SANITIZE_ADDRESS inline
bool HeapObjectHeader::isDead() const
{
    checkHeader();
    return m_size & deadBitMask;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::markDead()
{
    ASSERT(!isMarked());
    checkHeader();
    m_size |= deadBitMask;
}

#if ENABLE(ASSERT)
NO_SANITIZE_ADDRESS
void HeapObjectHeader::zapMagic()
{
    checkHeader();
    m_magic = zappedMagic;
}
#endif

HeapObjectHeader* HeapObjectHeader::fromPayload(const void* payload)
{
    Address addr = reinterpret_cast<Address>(const_cast<void*>(payload));
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(addr - objectHeaderSize);
    return header;
}

void HeapObjectHeader::finalize(const GCInfo* gcInfo, Address object, size_t objectSize)
{
    ASSERT(gcInfo);
    if (gcInfo->hasFinalizer()) {
        gcInfo->m_finalize(object);
    }

#if ENABLE(ASSERT) || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
    // In Debug builds, memory is zapped when it's freed, and the zapped memory is
    // zeroed out when the memory is reused. Memory is also zapped when using Leak
    // Sanitizer because the heap is used as a root region for LSan and therefore
    // pointers in unreachable memory could hide leaks.
    for (size_t i = 0; i < objectSize; i++)
        object[i] = finalizedZapValue;

    // Zap the primary vTable entry (secondary vTable entries are not zapped).
    if (gcInfo->hasVTable()) {
        *(reinterpret_cast<uintptr_t*>(object)) = zappedVTable;
    }
#endif
    // In Release builds, the entire object is zeroed out when it is added to the free list.
    // This happens right after sweeping the page and before the thread commences execution.
}

NO_SANITIZE_ADDRESS
void FinalizedHeapObjectHeader::finalize()
{
    HeapObjectHeader::finalize(m_gcInfo, payload(), payloadSize());
}

template<typename Header>
void LargeObject<Header>::unmark()
{
    return heapObjectHeader()->unmark();
}

template<typename Header>
bool LargeObject<Header>::isMarked()
{
    return heapObjectHeader()->isMarked();
}

template<typename Header>
void LargeObject<Header>::markDead()
{
    heapObjectHeader()->markDead();
}

template<typename Header>
void LargeObject<Header>::checkAndMarkPointer(Visitor* visitor, Address address)
{
    ASSERT(contains(address));
    if (!objectContains(address) || heapObjectHeader()->isDead())
        return;
#if ENABLE(GC_PROFILE_MARKING)
    visitor->setHostInfo(&address, "stack");
#endif
    mark(visitor);
}

template<typename Header>
void LargeObject<Header>::markUnmarkedObjectsDead()
{
    if (isMarked())
        unmark();
    else
        markDead();
}

#if ENABLE(ASSERT)
static bool isUninitializedMemory(void* objectPointer, size_t objectSize)
{
    // Scan through the object's fields and check that they are all zero.
    Address* objectFields = reinterpret_cast<Address*>(objectPointer);
    for (size_t i = 0; i < objectSize / sizeof(Address); ++i) {
        if (objectFields[i] != 0)
            return false;
    }
    return true;
}
#endif

template<>
void LargeObject<FinalizedHeapObjectHeader>::mark(Visitor* visitor)
{
    if (heapObjectHeader()->hasVTable() && !vTableInitialized(payload())) {
        FinalizedHeapObjectHeader* header = heapObjectHeader();
        visitor->markNoTracing(header);
        ASSERT(isUninitializedMemory(header->payload(), header->payloadSize()));
    } else {
        visitor->mark(heapObjectHeader(), heapObjectHeader()->traceCallback());
    }
}

template<>
void LargeObject<HeapObjectHeader>::mark(Visitor* visitor)
{
    ASSERT(gcInfo());
    if (gcInfo()->hasVTable() && !vTableInitialized(payload())) {
        HeapObjectHeader* header = heapObjectHeader();
        visitor->markNoTracing(header);
        ASSERT(isUninitializedMemory(header->payload(), header->payloadSize()));
    } else {
        visitor->mark(heapObjectHeader(), gcInfo()->m_trace);
    }
}

template<>
void LargeObject<FinalizedHeapObjectHeader>::finalize()
{
    heapObjectHeader()->finalize();
}

template<>
void LargeObject<HeapObjectHeader>::finalize()
{
    ASSERT(gcInfo());
    HeapObjectHeader::finalize(gcInfo(), payload(), payloadSize());
}

FinalizedHeapObjectHeader* FinalizedHeapObjectHeader::fromPayload(const void* payload)
{
    Address addr = reinterpret_cast<Address>(const_cast<void*>(payload));
    FinalizedHeapObjectHeader* header =
        reinterpret_cast<FinalizedHeapObjectHeader*>(addr - finalizedHeaderSize);
    return header;
}

template<typename Header>
ThreadHeap<Header>::ThreadHeap(ThreadState* state, int index)
    : m_currentAllocationPoint(0)
    , m_remainingAllocationSize(0)
    , m_lastRemainingAllocationSize(0)
    , m_firstPage(0)
    , m_firstLargeObject(0)
    , m_firstPageAllocatedDuringSweeping(0)
    , m_lastPageAllocatedDuringSweeping(0)
    , m_firstLargeObjectAllocatedDuringSweeping(0)
    , m_lastLargeObjectAllocatedDuringSweeping(0)
    , m_threadState(state)
    , m_index(index)
    , m_numberOfNormalPages(0)
    , m_promptlyFreedCount(0)
{
    clearFreeLists();
}

template<typename Header>
FreeList<Header>::FreeList()
    : m_biggestFreeListIndex(0)
{
}

template<typename Header>
ThreadHeap<Header>::~ThreadHeap()
{
    ASSERT(!m_firstPage);
    ASSERT(!m_firstLargeObject);
}

template<typename Header>
void ThreadHeap<Header>::cleanupPages()
{
    clearFreeLists();

    // Add the ThreadHeap's pages to the orphanedPagePool.
    for (HeapPage<Header>* page = m_firstPage; page; page = page->m_next) {
        Heap::decreaseAllocatedSpace(blinkPageSize);
        Heap::orphanedPagePool()->addOrphanedPage(m_index, page);
    }
    m_firstPage = 0;

    for (LargeObject<Header>* largeObject = m_firstLargeObject; largeObject; largeObject = largeObject->m_next) {
        Heap::decreaseAllocatedSpace(largeObject->size());
        Heap::orphanedPagePool()->addOrphanedPage(m_index, largeObject);
    }
    m_firstLargeObject = 0;
}

template<typename Header>
void ThreadHeap<Header>::updateRemainingAllocationSize()
{
    if (m_lastRemainingAllocationSize > remainingAllocationSize()) {
        Heap::increaseAllocatedObjectSize(m_lastRemainingAllocationSize - remainingAllocationSize());
        m_lastRemainingAllocationSize = remainingAllocationSize();
    }
    ASSERT(m_lastRemainingAllocationSize == remainingAllocationSize());
}

template<typename Header>
Address ThreadHeap<Header>::outOfLineAllocate(size_t payloadSize, size_t allocationSize, const GCInfo* gcInfo)
{
    ASSERT(allocationSize > remainingAllocationSize());
    if (allocationSize > blinkPageSize / 2)
        return allocateLargeObject(allocationSize, gcInfo);

    updateRemainingAllocationSize();
    threadState()->scheduleGCOrForceConservativeGCIfNeeded();

    setAllocationPoint(0, 0);
    ASSERT(allocationSize >= allocationGranularity);
    if (allocateFromFreeList(allocationSize))
        return allocate(payloadSize, gcInfo);
    if (coalesce(allocationSize) && allocateFromFreeList(allocationSize))
        return allocate(payloadSize, gcInfo);

    addPageToHeap(gcInfo);
    bool success = allocateFromFreeList(allocationSize);
    RELEASE_ASSERT(success);
    return allocate(payloadSize, gcInfo);
}

template<typename Header>
bool ThreadHeap<Header>::allocateFromFreeList(size_t allocationSize)
{
    ASSERT(!hasCurrentAllocationArea());
    size_t bucketSize = 1 << m_freeList.m_biggestFreeListIndex;
    int i = m_freeList.m_biggestFreeListIndex;
    for (; i > 0; i--, bucketSize >>= 1) {
        if (bucketSize < allocationSize) {
            // A FreeListEntry for bucketSize might be larger than allocationSize.
            // FIXME: We check only the first FreeListEntry because searching
            // the entire list is costly.
            if (!m_freeList.m_freeLists[i] || m_freeList.m_freeLists[i]->size() < allocationSize)
                break;
        }
        FreeListEntry* entry = m_freeList.m_freeLists[i];
        if (entry) {
            m_freeList.m_biggestFreeListIndex = i;
            entry->unlink(&m_freeList.m_freeLists[i]);
            setAllocationPoint(entry->address(), entry->size());
            ASSERT(hasCurrentAllocationArea());
            ASSERT(remainingAllocationSize() >= allocationSize);
            return true;
        }
    }
    m_freeList.m_biggestFreeListIndex = i;
    return false;
}

#if ENABLE(ASSERT)
template<typename Header>
static bool isLargeObjectAligned(LargeObject<Header>* largeObject, Address address)
{
    // Check that a large object is blinkPageSize aligned (modulo the osPageSize
    // for the guard page).
    return reinterpret_cast<Address>(largeObject) - WTF::kSystemPageSize == roundToBlinkPageStart(reinterpret_cast<Address>(largeObject));
}
#endif

template<typename Header>
BaseHeapPage* ThreadHeap<Header>::pageFromAddress(Address address)
{
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next()) {
        if (page->contains(address))
            return page;
    }
    for (HeapPage<Header>* page = m_firstPageAllocatedDuringSweeping; page; page = page->next()) {
        if (page->contains(address))
            return page;
    }
    for (LargeObject<Header>* largeObject = m_firstLargeObject; largeObject; largeObject = largeObject->next()) {
        ASSERT(isLargeObjectAligned(largeObject, address));
        if (largeObject->contains(address))
            return largeObject;
    }
    for (LargeObject<Header>* largeObject = m_firstLargeObjectAllocatedDuringSweeping; largeObject; largeObject = largeObject->next()) {
        ASSERT(isLargeObjectAligned(largeObject, address));
        if (largeObject->contains(address))
            return largeObject;
    }
    return 0;
}

#if ENABLE(GC_PROFILE_MARKING)
template<typename Header>
const GCInfo* ThreadHeap<Header>::findGCInfoOfLargeObject(Address address)
{
    for (LargeObject<Header>* largeObject = m_firstLargeObject; largeObject; largeObject = largeObject->next()) {
        if (largeObject->contains(address))
            return largeObject->gcInfo();
    }
    return 0;
}
#endif

#if ENABLE(GC_PROFILE_HEAP)
#define GC_PROFILE_HEAP_PAGE_SNAPSHOT_THRESHOLD 0
template<typename Header>
void ThreadHeap<Header>::snapshot(TracedValue* json, ThreadState::SnapshotInfo* info)
{
    ASSERT(isConsistentForSweeping());
    size_t previousPageCount = info->pageCount;

    json->beginArray("pages");
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next(), ++info->pageCount) {
        // FIXME: To limit the size of the snapshot we only output "threshold" many page snapshots.
        if (info->pageCount < GC_PROFILE_HEAP_PAGE_SNAPSHOT_THRESHOLD) {
            json->beginArray();
            json->pushInteger(reinterpret_cast<intptr_t>(page));
            page->snapshot(json, info);
            json->endArray();
        } else {
            page->snapshot(0, info);
        }
    }
    json->endArray();

    json->beginArray("largeObjects");
    for (LargeObject<Header>* largeObject = m_firstLargeObject; largeObject; largeObject = largeObject->next()) {
        json->beginDictionary();
        largeObject->snapshot(json, info);
        json->endDictionary();
    }
    json->endArray();

    json->setInteger("pageCount", info->pageCount - previousPageCount);
}
#endif

template<typename Header>
void FreeList<Header>::addToFreeList(Address address, size_t size)
{
    ASSERT(size < blinkPagePayloadSize());
    // The free list entries are only pointer aligned (but when we allocate
    // from them we are 8 byte aligned due to the header size).
    ASSERT(!((reinterpret_cast<uintptr_t>(address) + sizeof(Header)) & allocationMask));
    ASSERT(!(size & allocationMask));
    ASAN_POISON_MEMORY_REGION(address, size);
    FreeListEntry* entry;
    if (size < sizeof(*entry)) {
        // Create a dummy header with only a size and freelist bit set.
        ASSERT(size >= sizeof(BasicObjectHeader));
        // Free list encode the size to mark the lost memory as freelist memory.
        new (NotNull, address) BasicObjectHeader(BasicObjectHeader::freeListEncodedSize(size));
        // This memory gets lost. Sweeping can reclaim it.
        return;
    }
    entry = new (NotNull, address) FreeListEntry(size);
#if defined(ADDRESS_SANITIZER)
    // For ASan we don't add the entry to the free lists until the asanDeferMemoryReuseCount
    // reaches zero. However we always add entire pages to ensure that adding a new page will
    // increase the allocation space.
    if (HeapPage<Header>::payloadSize() != size && !entry->shouldAddToFreeList())
        return;
#endif
    int index = bucketIndexForSize(size);
    entry->link(&m_freeLists[index]);
    if (!m_lastFreeListEntries[index])
        m_lastFreeListEntries[index] = entry;
    if (index > m_biggestFreeListIndex)
        m_biggestFreeListIndex = index;
}

template<typename Header>
bool ThreadHeap<Header>::expandObject(Header* header, size_t newSize)
{
    ASSERT(header->payloadSize() < newSize);
    size_t allocationSize = allocationSizeFromSize(newSize);
    ASSERT(allocationSize > header->size());
    size_t expandSize = allocationSize - header->size();
    if (header->payloadEnd() == m_currentAllocationPoint && expandSize <= m_remainingAllocationSize) {
        m_currentAllocationPoint += expandSize;
        m_remainingAllocationSize -= expandSize;

        // Unpoison the memory used for the object (payload).
        ASAN_UNPOISON_MEMORY_REGION(header->payloadEnd(), expandSize);
#if ENABLE(ASSERT) || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
        memset(header->payloadEnd(), 0, expandSize);
#endif
        header->setSize(allocationSize);
        ASSERT(pageFromAddress(header->payloadEnd() - 1));
        return true;
    }
    return false;
}

template<typename Header>
void ThreadHeap<Header>::promptlyFreeObject(Header* header)
{
    ASSERT(!m_threadState->sweepForbidden());
    header->checkHeader();
    Address address = reinterpret_cast<Address>(header);
    Address payload = header->payload();
    size_t size = header->size();
    size_t payloadSize = header->payloadSize();
    BaseHeapPage* page = pageFromObject(address);
    ASSERT(size > 0);
    ASSERT(page == pageFromAddress(address));

    {
        ThreadState::SweepForbiddenScope forbiddenScope(m_threadState);
        HeapObjectHeader::finalize(header->gcInfo(), payload, payloadSize);
        if (address + size == m_currentAllocationPoint) {
            m_currentAllocationPoint = address;
            if (m_lastRemainingAllocationSize == m_remainingAllocationSize) {
                Heap::decreaseAllocatedObjectSize(size);
                m_lastRemainingAllocationSize += size;
            }
            m_remainingAllocationSize += size;
#if !ENABLE(ASSERT) && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
            memset(address, 0, size);
#endif
            ASAN_POISON_MEMORY_REGION(address, size);
            return;
        }
#if !ENABLE(ASSERT) && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
        memset(payload, 0, payloadSize);
#endif
        header->markPromptlyFreed();
    }

    page->addToPromptlyFreedSize(size);
    m_promptlyFreedCount++;
}

template<typename Header>
bool ThreadHeap<Header>::coalesce(size_t allocationSize)
{
    if (m_threadState->sweepForbidden())
        return false;

    if (m_promptlyFreedCount < 256)
        return false;

    // The smallest bucket able to satisfy an allocation request for
    // allocationSize is the bucket where all free-list entries are guaranteed
    // to be larger than allocationSize. That bucket is one larger than
    // the bucket allocationSize would go into.
    size_t neededBucketIndex = FreeList<Header>::bucketIndexForSize(allocationSize) + 1;
    size_t neededFreeEntrySize = 1 << neededBucketIndex;
    size_t neededPromptlyFreedSize = neededFreeEntrySize * 3;
    size_t foundFreeEntrySize = 0;

    // Bailout early on large requests because it is unlikely we will find a free-list entry.
    if (neededPromptlyFreedSize >= blinkPageSize)
        return false;

    TRACE_EVENT_BEGIN2("blink_gc", "ThreadHeap::coalesce" , "requestedSize", (unsigned)allocationSize , "neededSize", (unsigned)neededFreeEntrySize);

    // Search for a coalescing candidate.
    ASSERT(!hasCurrentAllocationArea());
    size_t pageCount = 0;
    HeapPage<Header>* page = m_firstPage;
    while (page) {
        // Only consider one of the first 'n' pages. A "younger" page is more likely to have freed backings.
        if (++pageCount > numberOfPagesToConsiderForCoalescing) {
            page = 0;
            break;
        }
        // Only coalesce pages with "sufficient" promptly freed space.
        if (page->promptlyFreedSize() >= neededPromptlyFreedSize) {
            break;
        }
        page = page->next();
    }

    // If we found a likely candidate, fully coalesce all its promptly-freed entries.
    if (page) {
        page->clearObjectStartBitMap();
        page->resetPromptlyFreedSize();
        size_t freedCount = 0;
        Address startOfGap = page->payload();
        for (Address headerAddress = startOfGap; headerAddress < page->end(); ) {
            BasicObjectHeader* basicHeader = reinterpret_cast<BasicObjectHeader*>(headerAddress);
            ASSERT(basicHeader->size() > 0);
            ASSERT(basicHeader->size() < blinkPagePayloadSize());

            if (basicHeader->isPromptlyFreed()) {
                Heap::decreaseAllocatedObjectSize(reinterpret_cast<Header*>(basicHeader)->size());
                size_t size = basicHeader->size();
                ASSERT(size >= sizeof(Header));
#if !ENABLE(ASSERT) && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
                memset(headerAddress, 0, sizeof(Header));
#endif
                ++freedCount;
                headerAddress += size;
                continue;
            }

            if (startOfGap != headerAddress) {
                size_t size = headerAddress - startOfGap;
                addToFreeList(startOfGap, size);
                if (size > foundFreeEntrySize)
                    foundFreeEntrySize = size;
            }

            headerAddress += basicHeader->size();
            startOfGap = headerAddress;
        }

        if (startOfGap != page->end()) {
            size_t size = page->end() - startOfGap;
            addToFreeList(startOfGap, size);
            if (size > foundFreeEntrySize)
                foundFreeEntrySize = size;
        }

        // Check before subtracting because freedCount might not be balanced with freed entries.
        if (freedCount < m_promptlyFreedCount)
            m_promptlyFreedCount -= freedCount;
        else
            m_promptlyFreedCount = 0;
    }

    TRACE_EVENT_END1("blink_gc", "ThreadHeap::coalesce", "foundFreeEntrySize", (unsigned)foundFreeEntrySize);

    if (foundFreeEntrySize < neededFreeEntrySize) {
        // If coalescing failed, reset the freed count to delay coalescing again.
        m_promptlyFreedCount = 0;
        return false;
    }

    return true;
}

template<typename Header>
Address ThreadHeap<Header>::allocateLargeObject(size_t size, const GCInfo* gcInfo)
{
    // Caller already added space for object header and rounded up to allocation alignment
    ASSERT(!(size & allocationMask));

    size_t allocationSize = sizeof(LargeObject<Header>) + size;

    // Ensure that there is enough space for alignment. If the header
    // is not a multiple of 8 bytes we will allocate an extra
    // headerPadding<Header> bytes to ensure it 8 byte aligned.
    allocationSize += headerPadding<Header>();

    // If ASan is supported we add allocationGranularity bytes to the allocated space and
    // poison that to detect overflows
#if defined(ADDRESS_SANITIZER)
    allocationSize += allocationGranularity;
#endif

    updateRemainingAllocationSize();
    m_threadState->scheduleGCOrForceConservativeGCIfNeeded();

    m_threadState->shouldFlushHeapDoesNotContainCache();
    PageMemory* pageMemory = PageMemory::allocate(allocationSize);
    m_threadState->allocatedRegionsSinceLastGC().append(pageMemory->region());
    Address largeObjectAddress = pageMemory->writableStart();
    Address headerAddress = largeObjectAddress + sizeof(LargeObject<Header>) + headerPadding<Header>();
    memset(headerAddress, 0, size);
    Header* header = new (NotNull, headerAddress) Header(size, gcInfo);
    header->checkHeader();
    Address result = headerAddress + sizeof(*header);
    ASSERT(!(reinterpret_cast<uintptr_t>(result) & allocationMask));
    LargeObject<Header>* largeObject = new (largeObjectAddress) LargeObject<Header>(pageMemory, gcInfo, threadState());

    // Poison the object header and allocationGranularity bytes after the object
    ASAN_POISON_MEMORY_REGION(header, sizeof(*header));
    ASAN_POISON_MEMORY_REGION(largeObject->address() + largeObject->size(), allocationGranularity);

    // Use a separate list for large objects allocated during sweeping to make
    // sure that we do not accidentally sweep objects that have been
    // allocated during sweeping.
    if (m_threadState->sweepForbidden()) {
        if (!m_lastLargeObjectAllocatedDuringSweeping)
            m_lastLargeObjectAllocatedDuringSweeping = largeObject;
        largeObject->link(&m_firstLargeObjectAllocatedDuringSweeping);
    } else {
        largeObject->link(&m_firstLargeObject);
    }

    Heap::increaseAllocatedSpace(largeObject->size());
    Heap::increaseAllocatedObjectSize(largeObject->size());
    return result;
}

template<typename Header>
void ThreadHeap<Header>::freeLargeObject(LargeObject<Header>* object, LargeObject<Header>** previousNext)
{
    object->unlink(previousNext);
    object->finalize();
    Heap::decreaseAllocatedSpace(object->size());

    // Unpoison the object header and allocationGranularity bytes after the
    // object before freeing.
    ASAN_UNPOISON_MEMORY_REGION(object->heapObjectHeader(), sizeof(Header));
    ASAN_UNPOISON_MEMORY_REGION(object->address() + object->size(), allocationGranularity);

    if (object->terminating()) {
        ASSERT(ThreadState::current()->isTerminating());
        // The thread is shutting down and this page is being removed as a part
        // of the thread local GC. In that case the object could be traced in
        // the next global GC if there is a dangling pointer from a live thread
        // heap to this dead thread heap. To guard against this, we put the page
        // into the orphaned page pool and zap the page memory. This ensures
        // that tracing the dangling pointer in the next global GC just
        // crashes instead of causing use-after-frees. After the next global GC,
        // the orphaned pages are removed.
        Heap::orphanedPagePool()->addOrphanedPage(m_index, object);
    } else {
        ASSERT(!ThreadState::current()->isTerminating());
        PageMemory* memory = object->storage();
        object->~LargeObject<Header>();
        delete memory;
    }
}

template<typename DataType>
PagePool<DataType>::PagePool()
{
    for (int i = 0; i < NumberOfHeaps; ++i) {
        m_pool[i] = 0;
    }
}

FreePagePool::~FreePagePool()
{
    for (int index = 0; index < NumberOfHeaps; ++index) {
        while (PoolEntry* entry = m_pool[index]) {
            m_pool[index] = entry->next;
            PageMemory* memory = entry->data;
            ASSERT(memory);
            delete memory;
            delete entry;
        }
    }
}

void FreePagePool::addFreePage(int index, PageMemory* memory)
{
    // When adding a page to the pool we decommit it to ensure it is unused
    // while in the pool. This also allows the physical memory, backing the
    // page, to be given back to the OS.
    memory->decommit();
    MutexLocker locker(m_mutex[index]);
    PoolEntry* entry = new PoolEntry(memory, m_pool[index]);
    m_pool[index] = entry;
}

PageMemory* FreePagePool::takeFreePage(int index)
{
    MutexLocker locker(m_mutex[index]);
    while (PoolEntry* entry = m_pool[index]) {
        m_pool[index] = entry->next;
        PageMemory* memory = entry->data;
        ASSERT(memory);
        delete entry;
        if (memory->commit())
            return memory;

        // We got some memory, but failed to commit it, try again.
        delete memory;
    }
    return 0;
}

BaseHeapPage::BaseHeapPage(PageMemory* storage, const GCInfo* gcInfo, ThreadState* state)
    : m_storage(storage)
    , m_gcInfo(gcInfo)
    , m_threadState(state)
    , m_terminating(false)
{
    ASSERT(isPageHeaderAddress(reinterpret_cast<Address>(this)));
}

void BaseHeapPage::markOrphaned()
{
    m_threadState = 0;
    m_gcInfo = 0;
    m_terminating = false;
    // Since we zap the page payload for orphaned pages we need to mark it as
    // unused so a conservative pointer won't interpret the object headers.
    storage()->markUnused();
}

OrphanedPagePool::~OrphanedPagePool()
{
    for (int index = 0; index < NumberOfHeaps; ++index) {
        while (PoolEntry* entry = m_pool[index]) {
            m_pool[index] = entry->next;
            BaseHeapPage* page = entry->data;
            delete entry;
            PageMemory* memory = page->storage();
            ASSERT(memory);
            page->~BaseHeapPage();
            delete memory;
        }
    }
}

void OrphanedPagePool::addOrphanedPage(int index, BaseHeapPage* page)
{
    page->markOrphaned();
    PoolEntry* entry = new PoolEntry(page, m_pool[index]);
    m_pool[index] = entry;
}

NO_SANITIZE_ADDRESS
void OrphanedPagePool::decommitOrphanedPages()
{
    ASSERT(Heap::isInGC());

#if ENABLE(ASSERT)
    // No locking needed as all threads are at safepoints at this point in time.
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it)
        ASSERT((*it)->isAtSafePoint());
#endif

    for (int index = 0; index < NumberOfHeaps; ++index) {
        PoolEntry* entry = m_pool[index];
        PoolEntry** prevNext = &m_pool[index];
        while (entry) {
            BaseHeapPage* page = entry->data;
            // Check if we should reuse the memory or just free it.
            // Large object memory is not reused but freed, normal blink heap
            // pages are reused.
            // NOTE: We call the destructor before freeing or adding to the
            // free page pool.
            PageMemory* memory = page->storage();
            if (page->isLargeObject()) {
                page->~BaseHeapPage();
                delete memory;
            } else {
                page->~BaseHeapPage();
                clearMemory(memory);
                Heap::freePagePool()->addFreePage(index, memory);
            }

            PoolEntry* deadEntry = entry;
            entry = entry->next;
            *prevNext = entry;
            delete deadEntry;
        }
    }
}

NO_SANITIZE_ADDRESS
void OrphanedPagePool::clearMemory(PageMemory* memory)
{
#if defined(ADDRESS_SANITIZER)
    // Don't use memset when running with ASan since this needs to zap
    // poisoned memory as well and the NO_SANITIZE_ADDRESS annotation
    // only works for code in this method and not for calls to memset.
    Address base = memory->writableStart();
    for (Address current = base; current < base + blinkPagePayloadSize(); ++current)
        *current = 0;
#else
    memset(memory->writableStart(), 0, blinkPagePayloadSize());
#endif
}

#if ENABLE(ASSERT)
bool OrphanedPagePool::contains(void* object)
{
    for (int index = 0; index < NumberOfHeaps; ++index) {
        for (PoolEntry* entry = m_pool[index]; entry; entry = entry->next) {
            BaseHeapPage* page = entry->data;
            if (page->contains(reinterpret_cast<Address>(object)))
                return true;
        }
    }
    return false;
}
#endif

template<>
void ThreadHeap<FinalizedHeapObjectHeader>::addPageToHeap(const GCInfo* gcInfo)
{
    // When adding a page to the ThreadHeap using FinalizedHeapObjectHeaders the GCInfo on
    // the heap should be unused (ie. 0).
    allocatePage(0);
}

template<>
void ThreadHeap<HeapObjectHeader>::addPageToHeap(const GCInfo* gcInfo)
{
    // When adding a page to the ThreadHeap using HeapObjectHeaders store the GCInfo on the heap
    // since it is the same for all objects
    ASSERT(gcInfo);
    allocatePage(gcInfo);
}

template <typename Header>
void ThreadHeap<Header>::removePageFromHeap(HeapPage<Header>* page)
{
    Heap::decreaseAllocatedSpace(blinkPageSize);

    if (page->terminating()) {
        // The thread is shutting down and this page is being removed as a part
        // of the thread local GC. In that case the object could be traced in
        // the next global GC if there is a dangling pointer from a live thread
        // heap to this dead thread heap. To guard against this, we put the page
        // into the orphaned page pool and zap the page memory. This ensures
        // that tracing the dangling pointer in the next global GC just
        // crashes instead of causing use-after-frees. After the next global GC,
        // the orphaned pages are removed.
        Heap::orphanedPagePool()->addOrphanedPage(m_index, page);
    } else {
        PageMemory* memory = page->storage();
        page->~HeapPage<Header>();
        Heap::freePagePool()->addFreePage(m_index, memory);
    }
}

template<typename Header>
void ThreadHeap<Header>::allocatePage(const GCInfo* gcInfo)
{
    m_threadState->shouldFlushHeapDoesNotContainCache();
    PageMemory* pageMemory = Heap::freePagePool()->takeFreePage(m_index);
    // We continue allocating page memory until we succeed in committing one.
    while (!pageMemory) {
        // Allocate a memory region for blinkPagesPerRegion pages that
        // will each have the following layout.
        //
        //    [ guard os page | ... payload ... | guard os page ]
        //    ^---{ aligned to blink page size }
        PageMemoryRegion* region = PageMemoryRegion::allocateNormalPages();
        m_threadState->allocatedRegionsSinceLastGC().append(region);

        // Setup the PageMemory object for each of the pages in the
        // region.
        size_t offset = 0;
        for (size_t i = 0; i < blinkPagesPerRegion; i++) {
            PageMemory* memory = PageMemory::setupPageMemoryInRegion(region, offset, blinkPagePayloadSize());
            // Take the first possible page ensuring that this thread actually
            // gets a page and add the rest to the page pool.
            if (!pageMemory) {
                if (memory->commit())
                    pageMemory = memory;
                else
                    delete memory;
            } else {
                Heap::freePagePool()->addFreePage(m_index, memory);
            }
            offset += blinkPageSize;
        }
    }
    HeapPage<Header>* page = new (pageMemory->writableStart()) HeapPage<Header>(pageMemory, this, gcInfo);

    // Use a separate list for pages allocated during sweeping to make
    // sure that we do not accidentally sweep objects that have been
    // allocated during sweeping.
    if (m_threadState->sweepForbidden()) {
        if (!m_lastPageAllocatedDuringSweeping)
            m_lastPageAllocatedDuringSweeping = page;
        page->link(&m_firstPageAllocatedDuringSweeping);
    } else {
        page->link(&m_firstPage);
    }

    Heap::increaseAllocatedSpace(blinkPageSize);
    ++m_numberOfNormalPages;
    addToFreeList(page->payload(), HeapPage<Header>::payloadSize());
}

#if ENABLE(ASSERT)
template<typename Header>
bool ThreadHeap<Header>::pagesToBeSweptContains(Address address)
{
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next()) {
        if (page->contains(address))
            return true;
    }
    return false;
}

template<typename Header>
bool ThreadHeap<Header>::pagesAllocatedDuringSweepingContains(Address address)
{
    for (HeapPage<Header>* page = m_firstPageAllocatedDuringSweeping; page; page = page->next()) {
        if (page->contains(address))
            return true;
    }
    return false;
}
#endif

template<typename Header>
size_t ThreadHeap<Header>::objectPayloadSizeForTesting()
{
    ASSERT(isConsistentForSweeping());
    ASSERT(!m_firstPageAllocatedDuringSweeping);
    ASSERT(!m_firstLargeObjectAllocatedDuringSweeping);
    size_t objectPayloadSize = 0;
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next())
        objectPayloadSize += page->objectPayloadSizeForTesting();
    for (LargeObject<Header>* largeObject = m_firstLargeObject; largeObject; largeObject = largeObject->next())
        objectPayloadSize += largeObject->objectPayloadSizeForTesting();
    return objectPayloadSize;
}

template<typename Header>
void ThreadHeap<Header>::sweepNormalPages()
{
    TRACE_EVENT0("blink_gc", "ThreadHeap::sweepNormalPages");
    HeapPage<Header>* page = m_firstPage;
    HeapPage<Header>** previousNext = &m_firstPage;
    while (page) {
        if (page->isEmpty()) {
            HeapPage<Header>* next = page->next();
            page->unlink(previousNext);
            removePageFromHeap(page);
            page = next;
            --m_numberOfNormalPages;
        } else {
            page->sweep(this);
            previousNext = &page->m_next;
            page = page->next();
        }
    }
}

template<typename Header>
void ThreadHeap<Header>::sweepLargePages()
{
    TRACE_EVENT0("blink_gc", "ThreadHeap::sweepLargePages");
    LargeObject<Header>** previousNext = &m_firstLargeObject;
    for (LargeObject<Header>* current = m_firstLargeObject; current;) {
        if (current->isMarked()) {
            Heap::increaseMarkedObjectSize(current->size());
            current->unmark();
            previousNext = &current->m_next;
            current = current->next();
        } else {
            LargeObject<Header>* next = current->next();
            freeLargeObject(current, previousNext);
            current = next;
        }
    }
}


// STRICT_ASAN_FINALIZATION_CHECKING turns on poisoning of all objects during
// sweeping to catch cases where dead objects touch each other. This is not
// turned on by default because it also triggers for cases that are safe.
// Examples of such safe cases are context life cycle observers and timers
// embedded in garbage collected objects.
#define STRICT_ASAN_FINALIZATION_CHECKING 0

template<typename Header>
void ThreadHeap<Header>::sweep()
{
    ASSERT(isConsistentForSweeping());
#if defined(ADDRESS_SANITIZER) && STRICT_ASAN_FINALIZATION_CHECKING
    // When using ASan do a pre-sweep where all unmarked objects are
    // poisoned before calling their finalizer methods. This can catch
    // the case where the finalizer of an object tries to modify
    // another object as part of finalization.
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next())
        page->poisonUnmarkedObjects();
#endif
    sweepNormalPages();
    sweepLargePages();
}

template<typename Header>
void ThreadHeap<Header>::postSweepProcessing()
{
    // If pages have been allocated during sweeping, link them into
    // the list of pages.
    if (m_firstPageAllocatedDuringSweeping) {
        m_lastPageAllocatedDuringSweeping->m_next = m_firstPage;
        m_firstPage = m_firstPageAllocatedDuringSweeping;
        m_lastPageAllocatedDuringSweeping = 0;
        m_firstPageAllocatedDuringSweeping = 0;
    }
    if (m_firstLargeObjectAllocatedDuringSweeping) {
        m_lastLargeObjectAllocatedDuringSweeping->m_next = m_firstLargeObject;
        m_firstLargeObject = m_firstLargeObjectAllocatedDuringSweeping;
        m_lastLargeObjectAllocatedDuringSweeping = 0;
        m_firstLargeObjectAllocatedDuringSweeping = 0;
    }
}

#if ENABLE(ASSERT)
template<typename Header>
bool ThreadHeap<Header>::isConsistentForSweeping()
{
    // A thread heap is consistent for sweeping if none of the pages to
    // be swept contain a freelist block or the current allocation
    // point.
    for (size_t i = 0; i < blinkPageSizeLog2; i++) {
        for (FreeListEntry* freeListEntry = m_freeList.m_freeLists[i]; freeListEntry; freeListEntry = freeListEntry->next()) {
            if (pagesToBeSweptContains(freeListEntry->address()))
                return false;
            ASSERT(pagesAllocatedDuringSweepingContains(freeListEntry->address()));
        }
    }
    if (hasCurrentAllocationArea()) {
        if (pagesToBeSweptContains(currentAllocationPoint()))
            return false;
        ASSERT(pagesAllocatedDuringSweepingContains(currentAllocationPoint()));
    }
    return true;
}
#endif

template<typename Header>
void ThreadHeap<Header>::makeConsistentForSweeping()
{
    setAllocationPoint(0, 0);
    clearFreeLists();
}

template<typename Header>
void ThreadHeap<Header>::markUnmarkedObjectsDead()
{
    ASSERT(Heap::isInGC());
    ASSERT(isConsistentForSweeping());
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next()) {
        page->markUnmarkedObjectsDead();
    }
    for (LargeObject<Header>* largeObject = m_firstLargeObject; largeObject; largeObject = largeObject->next()) {
        largeObject->markUnmarkedObjectsDead();
    }
}

template<typename Header>
void ThreadHeap<Header>::clearFreeLists()
{
    m_promptlyFreedCount = 0;
    m_freeList.clear();
}

template<typename Header>
void FreeList<Header>::clear()
{
    m_biggestFreeListIndex = 0;
    for (size_t i = 0; i < blinkPageSizeLog2; i++) {
        m_freeLists[i] = 0;
        m_lastFreeListEntries[i] = 0;
    }
}

template<typename Header>
int FreeList<Header>::bucketIndexForSize(size_t size)
{
    ASSERT(size > 0);
    int index = -1;
    while (size) {
        size >>= 1;
        index++;
    }
    return index;
}

template<typename Header>
HeapPage<Header>::HeapPage(PageMemory* storage, ThreadHeap<Header>* heap, const GCInfo* gcInfo)
    : BaseHeapPage(storage, gcInfo, heap->threadState())
    , m_next(0)
{
    COMPILE_ASSERT(!(sizeof(HeapPage<Header>) & allocationMask), page_header_incorrectly_aligned);
    m_objectStartBitMapComputed = false;
    ASSERT(isPageHeaderAddress(reinterpret_cast<Address>(this)));
}

template<typename Header>
size_t HeapPage<Header>::objectPayloadSizeForTesting()
{
    size_t objectPayloadSize = 0;
    Address headerAddress = payload();
    ASSERT(headerAddress != end());
    do {
        Header* header = reinterpret_cast<Header*>(headerAddress);
        if (!header->isFree()) {
            header->checkHeader();
            objectPayloadSize += header->payloadSize();
        }
        ASSERT(header->size() < blinkPagePayloadSize());
        headerAddress += header->size();
        ASSERT(headerAddress <= end());
    } while (headerAddress < end());
    return objectPayloadSize;
}

template<typename Header>
bool HeapPage<Header>::isEmpty()
{
    BasicObjectHeader* header = reinterpret_cast<BasicObjectHeader*>(payload());
    return header->isFree() && (header->size() == payloadSize());
}

template<typename Header>
void HeapPage<Header>::sweep(ThreadHeap<Header>* heap)
{
    clearObjectStartBitMap();
    resetPromptlyFreedSize();

    Address startOfGap = payload();
    for (Address headerAddress = startOfGap; headerAddress < end(); ) {
        BasicObjectHeader* basicHeader = reinterpret_cast<BasicObjectHeader*>(headerAddress);
        ASSERT(basicHeader->size() > 0);
        ASSERT(basicHeader->size() < blinkPagePayloadSize());

        if (basicHeader->isFree()) {
            size_t size = basicHeader->size();
#if !ENABLE(ASSERT) && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
            // Zero the memory in the free list header to maintain the
            // invariant that memory on the free list is zero filled.
            // The rest of the memory is already on the free list and is
            // therefore already zero filled.
            if (size < sizeof(FreeListEntry))
                memset(headerAddress, 0, size);
            else
                memset(headerAddress, 0, sizeof(FreeListEntry));
#endif
            headerAddress += size;
            continue;
        }
        // At this point we know this is a valid object of type Header
        Header* header = static_cast<Header*>(basicHeader);
        header->checkHeader();

        if (!header->isMarked()) {
            // For ASan we unpoison the specific object when calling the finalizer and
            // poison it again when done to allow the object's own finalizer to operate
            // on the object, but not have other finalizers be allowed to access it.
            ASAN_UNPOISON_MEMORY_REGION(header->payload(), header->payloadSize());
            finalize(header);
            size_t size = header->size();
#if !ENABLE(ASSERT) && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
            // This memory will be added to the freelist. Maintain the invariant
            // that memory on the freelist is zero filled.
            memset(headerAddress, 0, size);
#endif
            ASAN_POISON_MEMORY_REGION(header->payload(), header->payloadSize());
            headerAddress += size;
            continue;
        }

        if (startOfGap != headerAddress)
            heap->addToFreeList(startOfGap, headerAddress - startOfGap);
        header->unmark();
        headerAddress += header->size();
        Heap::increaseMarkedObjectSize(header->size());
        startOfGap = headerAddress;
    }
    if (startOfGap != end())
        heap->addToFreeList(startOfGap, end() - startOfGap);
}

template<typename Header>
void HeapPage<Header>::markUnmarkedObjectsDead()
{
    ASSERT(Heap::isInGC());
    for (Address headerAddress = payload(); headerAddress < end();) {
        Header* header = reinterpret_cast<Header*>(headerAddress);
        ASSERT(header->size() < blinkPagePayloadSize());
        // Check if a free list entry first since we cannot call
        // isMarked on a free list entry.
        if (header->isFree()) {
            headerAddress += header->size();
            continue;
        }
        if (header->isMarked())
            header->unmark();
        else
            header->markDead();
        headerAddress += header->size();
    }
}

template<typename Header>
void HeapPage<Header>::populateObjectStartBitMap()
{
    memset(&m_objectStartBitMap, 0, objectStartBitMapSize);
    Address start = payload();
    for (Address headerAddress = start; headerAddress < end();) {
        Header* header = reinterpret_cast<Header*>(headerAddress);
        size_t objectOffset = headerAddress - start;
        ASSERT(!(objectOffset & allocationMask));
        size_t objectStartNumber = objectOffset / allocationGranularity;
        size_t mapIndex = objectStartNumber / 8;
        ASSERT(mapIndex < objectStartBitMapSize);
        m_objectStartBitMap[mapIndex] |= (1 << (objectStartNumber & 7));
        headerAddress += header->size();
        ASSERT(headerAddress <= end());
    }
    m_objectStartBitMapComputed = true;
}

template<typename Header>
void HeapPage<Header>::clearObjectStartBitMap()
{
    m_objectStartBitMapComputed = false;
}

static int numberOfLeadingZeroes(uint8_t byte)
{
    if (!byte)
        return 8;
    int result = 0;
    if (byte <= 0x0F) {
        result += 4;
        byte = byte << 4;
    }
    if (byte <= 0x3F) {
        result += 2;
        byte = byte << 2;
    }
    if (byte <= 0x7F)
        result++;
    return result;
}

template<typename Header>
Header* HeapPage<Header>::findHeaderFromAddress(Address address)
{
    if (address < payload())
        return 0;
    if (!isObjectStartBitMapComputed())
        populateObjectStartBitMap();
    size_t objectOffset = address - payload();
    size_t objectStartNumber = objectOffset / allocationGranularity;
    size_t mapIndex = objectStartNumber / 8;
    ASSERT(mapIndex < objectStartBitMapSize);
    size_t bit = objectStartNumber & 7;
    uint8_t byte = m_objectStartBitMap[mapIndex] & ((1 << (bit + 1)) - 1);
    while (!byte) {
        ASSERT(mapIndex > 0);
        byte = m_objectStartBitMap[--mapIndex];
    }
    int leadingZeroes = numberOfLeadingZeroes(byte);
    objectStartNumber = (mapIndex * 8) + 7 - leadingZeroes;
    objectOffset = objectStartNumber * allocationGranularity;
    Address objectAddress = objectOffset + payload();
    Header* header = reinterpret_cast<Header*>(objectAddress);
    if (header->isFree())
        return 0;
    header->checkHeader();
    return header;
}

template<typename Header>
void HeapPage<Header>::checkAndMarkPointer(Visitor* visitor, Address address)
{
    ASSERT(contains(address));
    Header* header = findHeaderFromAddress(address);
    if (!header || header->isDead())
        return;

#if ENABLE(GC_PROFILE_MARKING)
    visitor->setHostInfo(&address, "stack");
#endif
    if (hasVTable(header) && !vTableInitialized(header->payload())) {
        visitor->markNoTracing(header);
        ASSERT(isUninitializedMemory(header->payload(), header->payloadSize()));
    } else {
        visitor->mark(header, traceCallback(header));
    }
}

#if ENABLE(GC_PROFILE_MARKING)
template<typename Header>
const GCInfo* HeapPage<Header>::findGCInfo(Address address)
{
    if (address < payload())
        return 0;

    if (gcInfo()) // for non FinalizedObjectHeader
        return gcInfo();

    Header* header = findHeaderFromAddress(address);
    if (!header)
        return 0;

    return header->gcInfo();
}
#endif

#if ENABLE(GC_PROFILE_HEAP)
template<typename Header>
void HeapPage<Header>::snapshot(TracedValue* json, ThreadState::SnapshotInfo* info)
{
    Header* header = 0;
    for (Address addr = payload(); addr < end(); addr += header->size()) {
        header = reinterpret_cast<Header*>(addr);
        if (json)
            json->pushInteger(header->encodedSize());
        if (header->isFree()) {
            info->freeSize += header->size();
            continue;
        }

        const GCInfo* gcinfo = header->gcInfo() ? header->gcInfo() : gcInfo();
        size_t tag = info->getClassTag(gcinfo);
        size_t age = header->age();
        if (json)
            json->pushInteger(tag);
        if (header->isMarked()) {
            info->liveCount[tag] += 1;
            info->liveSize[tag] += header->size();
            // Count objects that are live when promoted to the final generation.
            if (age == maxHeapObjectAge - 1)
                info->generations[tag][maxHeapObjectAge] += 1;
            header->incAge();
        } else {
            info->deadCount[tag] += 1;
            info->deadSize[tag] += header->size();
            // Count objects that are dead before the final generation.
            if (age < maxHeapObjectAge)
                info->generations[tag][age] += 1;
        }
    }
}
#endif

#if defined(ADDRESS_SANITIZER)
template<typename Header>
void HeapPage<Header>::poisonUnmarkedObjects()
{
    for (Address headerAddress = payload(); headerAddress < end(); ) {
        Header* header = reinterpret_cast<Header*>(headerAddress);
        ASSERT(header->size() < blinkPagePayloadSize());

        if (!header->isFree() && !header->isMarked())
            ASAN_POISON_MEMORY_REGION(header->payload(), header->payloadSize());
        headerAddress += header->size();
    }
}
#endif

template<>
inline void HeapPage<FinalizedHeapObjectHeader>::finalize(FinalizedHeapObjectHeader* header)
{
    header->finalize();
}

template<>
inline void HeapPage<HeapObjectHeader>::finalize(HeapObjectHeader* header)
{
    ASSERT(gcInfo());
    HeapObjectHeader::finalize(gcInfo(), header->payload(), header->payloadSize());
}

template<>
inline TraceCallback HeapPage<HeapObjectHeader>::traceCallback(HeapObjectHeader* header)
{
    ASSERT(gcInfo());
    return gcInfo()->m_trace;
}

template<>
inline TraceCallback HeapPage<FinalizedHeapObjectHeader>::traceCallback(FinalizedHeapObjectHeader* header)
{
    return header->traceCallback();
}

template<>
inline bool HeapPage<HeapObjectHeader>::hasVTable(HeapObjectHeader* header)
{
    ASSERT(gcInfo());
    return gcInfo()->hasVTable();
}

template<>
inline bool HeapPage<FinalizedHeapObjectHeader>::hasVTable(FinalizedHeapObjectHeader* header)
{
    return header->hasVTable();
}

template<typename Header>
size_t LargeObject<Header>::objectPayloadSizeForTesting()
{
    return payloadSize();
}

#if ENABLE(GC_PROFILE_HEAP)
template<typename Header>
void LargeObject<Header>::snapshot(TracedValue* json, ThreadState::SnapshotInfo* info)
{
    Header* header = heapObjectHeader();
    size_t tag = info->getClassTag(header->gcInfo());
    size_t age = header->age();
    if (isMarked()) {
        info->liveCount[tag] += 1;
        info->liveSize[tag] += header->size();
        // Count objects that are live when promoted to the final generation.
        if (age == maxHeapObjectAge - 1)
            info->generations[tag][maxHeapObjectAge] += 1;
        header->incAge();
    } else {
        info->deadCount[tag] += 1;
        info->deadSize[tag] += header->size();
        // Count objects that are dead before the final generation.
        if (age < maxHeapObjectAge)
            info->generations[tag][age] += 1;
    }

    if (json) {
        json->setInteger("class", tag);
        json->setInteger("size", header->size());
        json->setInteger("isMarked", isMarked());
    }
}
#endif

void HeapDoesNotContainCache::flush()
{
    ASSERT(Heap::isInGC());

    if (m_hasEntries) {
        for (int i = 0; i < numberOfEntries; i++)
            m_entries[i] = 0;
        m_hasEntries = false;
    }
}

size_t HeapDoesNotContainCache::hash(Address address)
{
    size_t value = (reinterpret_cast<size_t>(address) >> blinkPageSizeLog2);
    value ^= value >> numberOfEntriesLog2;
    value ^= value >> (numberOfEntriesLog2 * 2);
    value &= numberOfEntries - 1;
    return value & ~1; // Returns only even number.
}

bool HeapDoesNotContainCache::lookup(Address address)
{
    ASSERT(Heap::isInGC());

    size_t index = hash(address);
    ASSERT(!(index & 1));
    Address cachePage = roundToBlinkPageStart(address);
    if (m_entries[index] == cachePage)
        return m_entries[index];
    if (m_entries[index + 1] == cachePage)
        return m_entries[index + 1];
    return 0;
}

void HeapDoesNotContainCache::addEntry(Address address)
{
    ASSERT(Heap::isInGC());

    m_hasEntries = true;
    size_t index = hash(address);
    ASSERT(!(index & 1));
    Address cachePage = roundToBlinkPageStart(address);
    m_entries[index + 1] = m_entries[index];
    m_entries[index] = cachePage;
}

void Heap::flushHeapDoesNotContainCache()
{
    s_heapDoesNotContainCache->flush();
}

static void markNoTracingCallback(Visitor* visitor, void* object)
{
    visitor->markNoTracing(object);
}

class MarkingVisitor final : public Visitor {
public:
#if ENABLE(GC_PROFILE_MARKING)
    typedef HashSet<uintptr_t> LiveObjectSet;
    typedef HashMap<String, LiveObjectSet> LiveObjectMap;
    typedef HashMap<uintptr_t, std::pair<uintptr_t, String> > ObjectGraph;
#endif

    MarkingVisitor(CallbackStack* markingStack) : m_markingStack(markingStack)
    {
    }

    inline void visitHeader(HeapObjectHeader* header, const void* objectPointer, TraceCallback callback)
    {
        ASSERT(header);
#if ENABLE(ASSERT)
        {
            // Check that we are not marking objects that are outside
            // the heap by calling Heap::contains. However we cannot
            // call Heap::contains when outside a GC and we call mark
            // when doing weakness for ephemerons. Hence we only check
            // when called within.
            ASSERT(!Heap::isInGC() || Heap::containedInHeapOrOrphanedPage(header));
        }
#endif
        ASSERT(objectPointer);
        if (header->isMarked())
            return;
        header->mark();
#if ENABLE(GC_PROFILE_MARKING)
        MutexLocker locker(objectGraphMutex());
        String className(classOf(objectPointer));
        {
            LiveObjectMap::AddResult result = currentlyLive().add(className, LiveObjectSet());
            result.storedValue->value.add(reinterpret_cast<uintptr_t>(objectPointer));
        }
        ObjectGraph::AddResult result = objectGraph().add(reinterpret_cast<uintptr_t>(objectPointer), std::make_pair(reinterpret_cast<uintptr_t>(m_hostObject), m_hostName));
        ASSERT(result.isNewEntry);
        // fprintf(stderr, "%s[%p] -> %s[%p]\n", m_hostName.ascii().data(), m_hostObject, className.ascii().data(), objectPointer);
#endif
        if (callback)
            Heap::pushTraceCallback(m_markingStack, const_cast<void*>(objectPointer), callback);
    }

    virtual void mark(HeapObjectHeader* header, TraceCallback callback) override
    {
        // We need both the HeapObjectHeader and FinalizedHeapObjectHeader
        // version to correctly find the payload.
        visitHeader(header, header->payload(), callback);
    }

    virtual void mark(FinalizedHeapObjectHeader* header, TraceCallback callback) override
    {
        // We need both the HeapObjectHeader and FinalizedHeapObjectHeader
        // version to correctly find the payload.
        visitHeader(header, header->payload(), callback);
    }

    virtual void mark(const void* objectPointer, TraceCallback callback) override
    {
        if (!objectPointer)
            return;
        FinalizedHeapObjectHeader* header = FinalizedHeapObjectHeader::fromPayload(objectPointer);
        visitHeader(header, header->payload(), callback);
    }

    virtual void registerDelayedMarkNoTracing(const void* object) override
    {
        Heap::pushPostMarkingCallback(const_cast<void*>(object), markNoTracingCallback);
    }

    virtual void registerWeakMembers(const void* closure, const void* containingObject, WeakPointerCallback callback) override
    {
        Heap::pushWeakPointerCallback(const_cast<void*>(closure), const_cast<void*>(containingObject), callback);
    }

    virtual void registerWeakTable(const void* closure, EphemeronCallback iterationCallback, EphemeronCallback iterationDoneCallback)
    {
        Heap::registerWeakTable(const_cast<void*>(closure), iterationCallback, iterationDoneCallback);
    }

#if ENABLE(ASSERT)
    virtual bool weakTableRegistered(const void* closure)
    {
        return Heap::weakTableRegistered(closure);
    }
#endif

    virtual bool isMarked(const void* objectPointer) override
    {
        return FinalizedHeapObjectHeader::fromPayload(objectPointer)->isMarked();
    }

    // This macro defines the necessary visitor methods for typed heaps
#define DEFINE_VISITOR_METHODS(Type)                                              \
    virtual void mark(const Type* objectPointer, TraceCallback callback) override \
    {                                                                             \
        if (!objectPointer)                                                       \
            return;                                                               \
        HeapObjectHeader* header =                                                \
            HeapObjectHeader::fromPayload(objectPointer);                         \
        visitHeader(header, header->payload(), callback);                         \
    }                                                                             \
    virtual bool isMarked(const Type* objectPointer) override                     \
    {                                                                             \
        return HeapObjectHeader::fromPayload(objectPointer)->isMarked();          \
    }

    FOR_EACH_TYPED_HEAP(DEFINE_VISITOR_METHODS)
#undef DEFINE_VISITOR_METHODS

#if ENABLE(GC_PROFILE_MARKING)
    void reportStats()
    {
        fprintf(stderr, "\n---------- AFTER MARKING -------------------\n");
        for (LiveObjectMap::iterator it = currentlyLive().begin(), end = currentlyLive().end(); it != end; ++it) {
            fprintf(stderr, "%s %u", it->key.ascii().data(), it->value.size());

            if (it->key == "blink::Document")
                reportStillAlive(it->value, previouslyLive().get(it->key));

            fprintf(stderr, "\n");
        }

        previouslyLive().swap(currentlyLive());
        currentlyLive().clear();

        for (HashSet<uintptr_t>::iterator it = objectsToFindPath().begin(), end = objectsToFindPath().end(); it != end; ++it) {
            dumpPathToObjectFromObjectGraph(objectGraph(), *it);
        }
    }

    static void reportStillAlive(LiveObjectSet current, LiveObjectSet previous)
    {
        int count = 0;

        fprintf(stderr, " [previously %u]", previous.size());
        for (LiveObjectSet::iterator it = current.begin(), end = current.end(); it != end; ++it) {
            if (previous.find(*it) == previous.end())
                continue;
            count++;
        }

        if (!count)
            return;

        fprintf(stderr, " {survived 2GCs %d: ", count);
        for (LiveObjectSet::iterator it = current.begin(), end = current.end(); it != end; ++it) {
            if (previous.find(*it) == previous.end())
                continue;
            fprintf(stderr, "%ld", *it);
            if (--count)
                fprintf(stderr, ", ");
        }
        ASSERT(!count);
        fprintf(stderr, "}");
    }

    static void dumpPathToObjectFromObjectGraph(const ObjectGraph& graph, uintptr_t target)
    {
        ObjectGraph::const_iterator it = graph.find(target);
        if (it == graph.end())
            return;
        fprintf(stderr, "Path to %lx of %s\n", target, classOf(reinterpret_cast<const void*>(target)).ascii().data());
        while (it != graph.end()) {
            fprintf(stderr, "<- %lx of %s\n", it->value.first, it->value.second.utf8().data());
            it = graph.find(it->value.first);
        }
        fprintf(stderr, "\n");
    }

    static void dumpPathToObjectOnNextGC(void* p)
    {
        objectsToFindPath().add(reinterpret_cast<uintptr_t>(p));
    }

    static Mutex& objectGraphMutex()
    {
        AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
        return mutex;
    }

    static LiveObjectMap& previouslyLive()
    {
        DEFINE_STATIC_LOCAL(LiveObjectMap, map, ());
        return map;
    }

    static LiveObjectMap& currentlyLive()
    {
        DEFINE_STATIC_LOCAL(LiveObjectMap, map, ());
        return map;
    }

    static ObjectGraph& objectGraph()
    {
        DEFINE_STATIC_LOCAL(ObjectGraph, graph, ());
        return graph;
    }

    static HashSet<uintptr_t>& objectsToFindPath()
    {
        DEFINE_STATIC_LOCAL(HashSet<uintptr_t>, set, ());
        return set;
    }
#endif

protected:
    virtual void registerWeakCell(void** cell, WeakPointerCallback callback) override
    {
        Heap::pushWeakCellPointerCallback(cell, callback);
    }

private:
    CallbackStack* m_markingStack;
};

void Heap::init()
{
    ThreadState::init();
    s_markingStack = new CallbackStack();
    s_postMarkingCallbackStack = new CallbackStack();
    s_weakCallbackStack = new CallbackStack();
    s_ephemeronStack = new CallbackStack();
    s_heapDoesNotContainCache = new HeapDoesNotContainCache();
    s_markingVisitor = new MarkingVisitor(s_markingStack);
    s_freePagePool = new FreePagePool();
    s_orphanedPagePool = new OrphanedPagePool();
    s_allocatedObjectSize = 0;
    s_allocatedSpace = 0;
    s_markedObjectSize = 0;
}

void Heap::shutdown()
{
    s_shutdownCalled = true;
    ThreadState::shutdownHeapIfNecessary();
}

void Heap::doShutdown()
{
    // We don't want to call doShutdown() twice.
    if (!s_markingVisitor)
        return;

    ASSERT(!Heap::isInGC());
    ASSERT(!ThreadState::attachedThreads().size());
    delete s_markingVisitor;
    s_markingVisitor = 0;
    delete s_heapDoesNotContainCache;
    s_heapDoesNotContainCache = 0;
    delete s_freePagePool;
    s_freePagePool = 0;
    delete s_orphanedPagePool;
    s_orphanedPagePool = 0;
    delete s_weakCallbackStack;
    s_weakCallbackStack = 0;
    delete s_postMarkingCallbackStack;
    s_postMarkingCallbackStack = 0;
    delete s_markingStack;
    s_markingStack = 0;
    delete s_ephemeronStack;
    s_ephemeronStack = 0;
    delete s_regionTree;
    s_regionTree = 0;
    ThreadState::shutdown();
    ASSERT(Heap::allocatedSpace() == 0);
}

BaseHeapPage* Heap::contains(Address address)
{
    ASSERT(Heap::isInGC());
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it) {
        BaseHeapPage* page = (*it)->contains(address);
        if (page)
            return page;
    }
    return 0;
}

#if ENABLE(ASSERT)
bool Heap::containedInHeapOrOrphanedPage(void* object)
{
    return contains(object) || orphanedPagePool()->contains(object);
}
#endif

Address Heap::checkAndMarkPointer(Visitor* visitor, Address address)
{
    ASSERT(Heap::isInGC());

#if !ENABLE(ASSERT)
    if (s_heapDoesNotContainCache->lookup(address))
        return 0;
#endif

    if (BaseHeapPage* page = lookup(address)) {
        ASSERT(page->contains(address));
        ASSERT(!page->orphaned());
        ASSERT(!s_heapDoesNotContainCache->lookup(address));
        page->checkAndMarkPointer(visitor, address);
        // FIXME: We only need to set the conservative flag if checkAndMarkPointer actually marked the pointer.
        s_lastGCWasConservative = true;
        return address;
    }

#if !ENABLE(ASSERT)
    s_heapDoesNotContainCache->addEntry(address);
#else
    if (!s_heapDoesNotContainCache->lookup(address))
        s_heapDoesNotContainCache->addEntry(address);
#endif
    return 0;
}

#if ENABLE(GC_PROFILE_MARKING)
const GCInfo* Heap::findGCInfo(Address address)
{
    return ThreadState::findGCInfoFromAllThreads(address);
}
#endif

#if ENABLE(GC_PROFILE_MARKING)
void Heap::dumpPathToObjectOnNextGC(void* p)
{
    static_cast<MarkingVisitor*>(s_markingVisitor)->dumpPathToObjectOnNextGC(p);
}

String Heap::createBacktraceString()
{
    int framesToShow = 3;
    int stackFrameSize = 16;
    ASSERT(stackFrameSize >= framesToShow);
    typedef void* FramePointer;
    FramePointer* stackFrame = static_cast<FramePointer*>(alloca(sizeof(FramePointer) * stackFrameSize));
    WTFGetBacktrace(stackFrame, &stackFrameSize);

    StringBuilder builder;
    builder.append("Persistent");
    bool didAppendFirstName = false;
    // Skip frames before/including "blink::Persistent".
    bool didSeePersistent = false;
    for (int i = 0; i < stackFrameSize && framesToShow > 0; ++i) {
        FrameToNameScope frameToName(stackFrame[i]);
        if (!frameToName.nullableName())
            continue;
        if (strstr(frameToName.nullableName(), "blink::Persistent")) {
            didSeePersistent = true;
            continue;
        }
        if (!didSeePersistent)
            continue;
        if (!didAppendFirstName) {
            didAppendFirstName = true;
            builder.append(" ... Backtrace:");
        }
        builder.append("\n\t");
        builder.append(frameToName.nullableName());
        --framesToShow;
    }
    return builder.toString().replace("blink::", "");
}
#endif

void Heap::pushTraceCallback(CallbackStack* stack, void* object, TraceCallback callback)
{
#if ENABLE(ASSERT)
    {
        ASSERT(Heap::containedInHeapOrOrphanedPage(object));
    }
#endif
    CallbackStack::Item* slot = stack->allocateEntry();
    *slot = CallbackStack::Item(object, callback);
}

template<CallbackInvocationMode Mode>
bool Heap::popAndInvokeTraceCallback(CallbackStack* stack, Visitor* visitor)
{
    CallbackStack::Item* item = stack->pop();
    if (!item)
        return false;
#if ENABLE(ASSERT)
    if (Mode == GlobalMarking) {
        BaseHeapPage* page = pageFromObject(item->object());
        // If you hit this ASSERT, it means that there is a dangling pointer
        // from a live thread heap to a dead thread heap. We must eliminate
        // the dangling pointer.
        // Release builds don't have the ASSERT, but it is OK because
        // release builds will crash at the following item->call
        // because all the entries of the orphaned heaps are zeroed out and
        // thus the item does not have a valid vtable.
        ASSERT(!page->orphaned());
    }
#endif
    if (Mode == ThreadLocalMarking) {
        BaseHeapPage* page = pageFromObject(item->object());
        ASSERT(!page->orphaned());
        // When doing a thread local GC, don't trace an object located in
        // a heap of another thread.
        if (!page->terminating())
            return true;
    }

#if ENABLE(GC_PROFILE_MARKING)
    visitor->setHostInfo(item->object(), classOf(item->object()));
#endif
    item->call(visitor);
    return true;
}

void Heap::pushPostMarkingCallback(void* object, TraceCallback callback)
{
    ASSERT(!Heap::orphanedPagePool()->contains(object));
    CallbackStack::Item* slot = s_postMarkingCallbackStack->allocateEntry();
    *slot = CallbackStack::Item(object, callback);
}

bool Heap::popAndInvokePostMarkingCallback(Visitor* visitor)
{
    if (CallbackStack::Item* item = s_postMarkingCallbackStack->pop()) {
        item->call(visitor);
        return true;
    }
    return false;
}

void Heap::pushWeakCellPointerCallback(void** cell, WeakPointerCallback callback)
{
    ASSERT(!Heap::orphanedPagePool()->contains(cell));
    CallbackStack::Item* slot = s_weakCallbackStack->allocateEntry();
    *slot = CallbackStack::Item(cell, callback);
}

void Heap::pushWeakPointerCallback(void* closure, void* object, WeakPointerCallback callback)
{
    ASSERT(Heap::contains(object));
    BaseHeapPage* page = pageFromObject(object);
    ASSERT(!page->orphaned());
    ASSERT(Heap::contains(object) == page);
    ThreadState* state = page->threadState();
    state->pushWeakPointerCallback(closure, callback);
}

bool Heap::popAndInvokeWeakPointerCallback(Visitor* visitor)
{
    // For weak processing we should never reach orphaned pages since orphaned
    // pages are not traced and thus objects on those pages are never be
    // registered as objects on orphaned pages. We cannot assert this here since
    // we might have an off-heap collection. We assert it in
    // Heap::pushWeakPointerCallback.
    if (CallbackStack::Item* item = s_weakCallbackStack->pop()) {
        item->call(visitor);
        return true;
    }
    return false;
}

void Heap::registerWeakTable(void* table, EphemeronCallback iterationCallback, EphemeronCallback iterationDoneCallback)
{
    {
        // Check that the ephemeron table being pushed onto the stack is not on an
        // orphaned page.
        ASSERT(!Heap::orphanedPagePool()->contains(table));
        CallbackStack::Item* slot = s_ephemeronStack->allocateEntry();
        *slot = CallbackStack::Item(table, iterationCallback);
    }

    // Register a post-marking callback to tell the tables that
    // ephemeron iteration is complete.
    pushPostMarkingCallback(table, iterationDoneCallback);
}

#if ENABLE(ASSERT)
bool Heap::weakTableRegistered(const void* table)
{
    ASSERT(s_ephemeronStack);
    return s_ephemeronStack->hasCallbackForObject(table);
}
#endif

void Heap::preGC()
{
    ASSERT(Heap::isInGC());
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it)
        (*it)->preGC();
}

void Heap::postGC()
{
    ASSERT(Heap::isInGC());
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it)
        (*it)->postGC();
}

void Heap::collectGarbage(ThreadState::StackState stackState, ThreadState::GCType gcType)
{
    ThreadState* state = ThreadState::current();
    state->setGCState(ThreadState::StoppingOtherThreads);

    GCScope gcScope(stackState);
    // Check if we successfully parked the other threads. If not we bail out of the GC.
    if (!gcScope.allThreadsParked()) {
        state->scheduleGC();
        return;
    }

    if (state->isMainThread())
        ScriptForbiddenScope::enter();

    s_lastGCWasConservative = false;

    TRACE_EVENT2("blink_gc", "Heap::collectGarbage",
        "precise", stackState == ThreadState::NoHeapPointersOnStack,
        "forced", gcType == ThreadState::ForcedGC);
    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink_gc", "BlinkGC");
    double timeStamp = WTF::currentTimeMS();
#if ENABLE(GC_PROFILE_MARKING)
    static_cast<MarkingVisitor*>(s_markingVisitor)->objectGraph().clear();
#endif

    // Disallow allocation during garbage collection (but not
    // during the finalization that happens when the gcScope is
    // torn down).
    ThreadState::NoAllocationScope noAllocationScope(state);

    enterGC();
    preGC();

    Heap::resetMarkedObjectSize();
    Heap::resetAllocatedObjectSize();

    // 1. trace persistent roots.
    ThreadState::visitPersistentRoots(s_markingVisitor);

    // 2. trace objects reachable from the persistent roots including ephemerons.
    processMarkingStack<GlobalMarking>();

    // 3. trace objects reachable from the stack. We do this independent of the
    // given stackState since other threads might have a different stack state.
    ThreadState::visitStackRoots(s_markingVisitor);

    // 4. trace objects reachable from the stack "roots" including ephemerons.
    // Only do the processing if we found a pointer to an object on one of the
    // thread stacks.
    if (lastGCWasConservative()) {
        processMarkingStack<GlobalMarking>();
    }

    postMarkingProcessing();
    globalWeakProcessing();

    // Now we can delete all orphaned pages because there are no dangling
    // pointers to the orphaned pages. (If we have such dangling pointers,
    // we should have crashed during marking before getting here.)
    orphanedPagePool()->decommitOrphanedPages();

    postGC();
    leaveGC();

#if ENABLE(GC_PROFILE_MARKING)
    static_cast<MarkingVisitor*>(s_markingVisitor)->reportStats();
#endif

    if (Platform::current()) {
        Platform::current()->histogramCustomCounts("BlinkGC.CollectGarbage", WTF::currentTimeMS() - timeStamp, 0, 10 * 1000, 50);
        Platform::current()->histogramCustomCounts("BlinkGC.TotalObjectSpace", Heap::allocatedObjectSize() / 1024, 0, 4 * 1024 * 1024, 50);
        Platform::current()->histogramCustomCounts("BlinkGC.TotalAllocatedSpace", Heap::allocatedSpace() / 1024, 0, 4 * 1024 * 1024, 50);
    }

    if (state->isMainThread())
        ScriptForbiddenScope::exit();
}

void Heap::collectGarbageForTerminatingThread(ThreadState* state)
{
    // We explicitly do not enter a safepoint while doing thread specific
    // garbage collection since we don't want to allow a global GC at the
    // same time as a thread local GC.

    {
        ThreadState::NoAllocationScope noAllocationScope(state);

        enterGC();
        state->preGC();

        // 1. trace the thread local persistent roots. For thread local GCs we
        // don't trace the stack (ie. no conservative scanning) since this is
        // only called during thread shutdown where there should be no objects
        // on the stack.
        // We also assume that orphaned pages have no objects reachable from
        // persistent handles on other threads or CrossThreadPersistents. The
        // only cases where this could happen is if a subsequent conservative
        // global GC finds a "pointer" on the stack or due to a programming
        // error where an object has a dangling cross-thread pointer to an
        // object on this heap.
        state->visitPersistents(s_markingVisitor);

        // 2. trace objects reachable from the thread's persistent roots
        // including ephemerons.
        processMarkingStack<ThreadLocalMarking>();

        postMarkingProcessing();
        globalWeakProcessing();

        state->postGC();
        leaveGC();
    }
    state->performPendingSweep();
}

template<CallbackInvocationMode Mode>
void Heap::processMarkingStack()
{
    // Ephemeron fixed point loop.
    do {
        {
            // Iteratively mark all objects that are reachable from the objects
            // currently pushed onto the marking stack. If Mode is ThreadLocalMarking
            // don't continue tracing if the trace hits an object on another thread's
            // heap.
            TRACE_EVENT0("blink_gc", "Heap::processMarkingStackSingleThreaded");
            while (popAndInvokeTraceCallback<Mode>(s_markingStack, s_markingVisitor)) { }
        }

        {
            // Mark any strong pointers that have now become reachable in ephemeron
            // maps.
            TRACE_EVENT0("blink_gc", "Heap::processEphemeronStack");
            s_ephemeronStack->invokeEphemeronCallbacks(s_markingVisitor);
        }

        // Rerun loop if ephemeron processing queued more objects for tracing.
    } while (!s_markingStack->isEmpty());
}

void Heap::postMarkingProcessing()
{
    TRACE_EVENT0("blink_gc", "Heap::postMarkingProcessing");
    // Call post-marking callbacks including:
    // 1. the ephemeronIterationDone callbacks on weak tables to do cleanup
    //    (specifically to clear the queued bits for weak hash tables), and
    // 2. the markNoTracing callbacks on collection backings to mark them
    //    if they are only reachable from their front objects.
    while (popAndInvokePostMarkingCallback(s_markingVisitor)) { }

    s_ephemeronStack->clear();

    // Post-marking callbacks should not trace any objects and
    // therefore the marking stack should be empty after the
    // post-marking callbacks.
    ASSERT(s_markingStack->isEmpty());
}

void Heap::globalWeakProcessing()
{
    TRACE_EVENT0("blink_gc", "Heap::globalWeakProcessing");
    // Call weak callbacks on objects that may now be pointing to dead
    // objects.
    while (popAndInvokeWeakPointerCallback(s_markingVisitor)) { }

    // It is not permitted to trace pointers of live objects in the weak
    // callback phase, so the marking stack should still be empty here.
    ASSERT(s_markingStack->isEmpty());
}

void Heap::collectAllGarbage()
{
    // FIXME: oilpan: we should perform a single GC and everything
    // should die. Unfortunately it is not the case for all objects
    // because the hierarchy was not completely moved to the heap and
    // some heap allocated objects own objects that contain persistents
    // pointing to other heap allocated objects.
    for (int i = 0; i < 5; i++)
        collectGarbage(ThreadState::NoHeapPointersOnStack);
}

template<typename Header>
void ThreadHeap<Header>::prepareHeapForTermination()
{
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next()) {
        page->setTerminating();
    }
    for (LargeObject<Header>* largeObject = m_firstLargeObject; largeObject; largeObject = largeObject->next()) {
        largeObject->setTerminating();
    }
}

size_t Heap::objectPayloadSizeForTesting()
{
    size_t objectPayloadSize = 0;
    ASSERT(Heap::isInGC());
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    typedef ThreadState::AttachedThreadStateSet::iterator ThreadStateIterator;
    for (ThreadStateIterator it = threads.begin(), end = threads.end(); it != end; ++it) {
        (*it)->makeConsistentForSweeping();
        objectPayloadSize += (*it)->objectPayloadSizeForTesting();
    }
    return objectPayloadSize;
}

template<typename HeapTraits, typename HeapType, typename HeaderType>
void HeapAllocator::backingFree(void* address)
{
    if (!address || Heap::isInGC())
        return;

    ThreadState* state = ThreadState::current();
    if (state->sweepForbidden())
        return;

    // Don't promptly free large objects because their page is never reused
    // and don't free backings allocated on other threads.
    BaseHeapPage* page = pageFromObject(address);
    if (page->isLargeObject() || page->threadState() != state)
        return;

    HeaderType* header = HeaderType::fromPayload(address);
    header->checkHeader();

    const GCInfo* gcInfo = header->gcInfo();
    int heapIndex = HeapTraits::index(gcInfo->hasFinalizer(), header->payloadSize());
    HeapType* heap = static_cast<HeapType*>(state->heap(heapIndex));
    heap->promptlyFreeObject(header);
}

void HeapAllocator::vectorBackingFree(void* address)
{
    typedef HeapIndexTrait<VectorBackingHeap> HeapTraits;
    typedef HeapTraits::HeapType HeapType;
    typedef HeapTraits::HeaderType HeaderType;
    backingFree<HeapTraits, HeapType, HeaderType>(address);
}

void HeapAllocator::hashTableBackingFree(void* address)
{
    typedef HeapIndexTrait<HashTableBackingHeap> HeapTraits;
    typedef HeapTraits::HeapType HeapType;
    typedef HeapTraits::HeaderType HeaderType;
    backingFree<HeapTraits, HeapType, HeaderType>(address);
}

template<typename HeapTraits, typename HeapType, typename HeaderType>
bool HeapAllocator::backingExpand(void* address, size_t newSize)
{
    if (!address || Heap::isInGC())
        return false;

    ThreadState* state = ThreadState::current();
    if (state->sweepForbidden())
        return false;
    ASSERT(state->isAllocationAllowed());

    BaseHeapPage* page = pageFromObject(address);
    if (page->isLargeObject() || page->threadState() != state)
        return false;

    HeaderType* header = HeaderType::fromPayload(address);
    header->checkHeader();

    const GCInfo* gcInfo = header->gcInfo();
    int heapIndex = HeapTraits::index(gcInfo->hasFinalizer(), header->payloadSize());
    HeapType* heap = static_cast<HeapType*>(state->heap(heapIndex));
    return heap->expandObject(header, newSize);
}

bool HeapAllocator::vectorBackingExpand(void* address, size_t newSize)
{
    typedef HeapIndexTrait<VectorBackingHeap> HeapTraits;
    typedef HeapTraits::HeapType HeapType;
    typedef HeapTraits::HeaderType HeaderType;
    return backingExpand<HeapTraits, HeapType, HeaderType>(address, newSize);
}

BaseHeapPage* Heap::lookup(Address address)
{
    ASSERT(Heap::isInGC());
    if (!s_regionTree)
        return 0;
    if (PageMemoryRegion* region = s_regionTree->lookup(address)) {
        BaseHeapPage* page = region->pageFromAddress(address);
        return page && !page->orphaned() ? page : 0;
    }
    return 0;
}

static Mutex& regionTreeMutex()
{
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
    return mutex;
}

void Heap::removePageMemoryRegion(PageMemoryRegion* region)
{
    // Deletion of large objects (and thus their regions) can happen concurrently
    // on sweeper threads. Removal can also happen during thread shutdown, but
    // that case is safe. Regardless, we make all removals mutually exclusive.
    MutexLocker locker(regionTreeMutex());
    RegionTree::remove(region, &s_regionTree);
}

void Heap::addPageMemoryRegion(PageMemoryRegion* region)
{
    ASSERT(Heap::isInGC());
    RegionTree::add(new RegionTree(region), &s_regionTree);
}

PageMemoryRegion* Heap::RegionTree::lookup(Address address)
{
    RegionTree* current = s_regionTree;
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
    return 0;
}

void Heap::RegionTree::add(RegionTree* newTree, RegionTree** context)
{
    ASSERT(newTree);
    Address base = newTree->m_region->base();
    for (RegionTree* current = *context; current; current = *context) {
        ASSERT(!current->m_region->contains(base));
        context = (base < current->m_region->base()) ? &current->m_left : &current->m_right;
    }
    *context = newTree;
}

void Heap::RegionTree::remove(PageMemoryRegion* region, RegionTree** context)
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

    *context = 0;
    if (current->m_left) {
        add(current->m_left, context);
        current->m_left = 0;
    }
    if (current->m_right) {
        add(current->m_right, context);
        current->m_right = 0;
    }
    delete current;
}

// Force template instantiations for the types that we need.
template class HeapPage<FinalizedHeapObjectHeader>;
template class HeapPage<HeapObjectHeader>;
template class ThreadHeap<FinalizedHeapObjectHeader>;
template class ThreadHeap<HeapObjectHeader>;

Visitor* Heap::s_markingVisitor;
CallbackStack* Heap::s_markingStack;
CallbackStack* Heap::s_postMarkingCallbackStack;
CallbackStack* Heap::s_weakCallbackStack;
CallbackStack* Heap::s_ephemeronStack;
HeapDoesNotContainCache* Heap::s_heapDoesNotContainCache;
bool Heap::s_shutdownCalled = false;
bool Heap::s_lastGCWasConservative = false;
bool Heap::s_inGC = false;
FreePagePool* Heap::s_freePagePool;
OrphanedPagePool* Heap::s_orphanedPagePool;
Heap::RegionTree* Heap::s_regionTree = 0;
size_t Heap::s_allocatedObjectSize = 0;
size_t Heap::s_allocatedSpace = 0;
size_t Heap::s_markedObjectSize = 0;

} // namespace blink
