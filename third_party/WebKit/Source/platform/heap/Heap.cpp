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
#include "platform/heap/MarkingVisitorImpl.h"
#include "platform/heap/ThreadState.h"
#include "public/platform/Platform.h"
#include "wtf/AddressSpaceRandomization.h"
#include "wtf/Assertions.h"
#include "wtf/ContainerAnnotations.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/PassOwnPtr.h"
#if ENABLE(GC_PROFILING)
#include "platform/TracedValue.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"
#include <stdio.h>
#include <utility>
#endif

#if OS(POSIX)
#include <sys/mman.h>
#include <unistd.h>
#elif OS(WIN)
#include <windows.h>
#endif

#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
// FIXME: have ContainerAnnotations.h define an ENABLE_-style name instead.
#define ENABLE_ASAN_CONTAINER_ANNOTATIONS 1

// When finalizing a non-inlined vector backing store/container, remove
// its contiguous container annotation. Required as it will not be destructed
// from its Vector.
#define ASAN_RETIRE_CONTAINER_ANNOTATION(object, objectSize)                          \
    do {                                                                              \
        BasePage* page = pageFromObject(object);                                      \
        ASSERT(page);                                                                 \
        bool isContainer = page->heap()->heapIndex() == VectorHeapIndex;              \
        if (!isContainer && page->isLargeObjectPage())                                \
            isContainer = static_cast<LargeObjectPage*>(page)->isVectorBackingPage(); \
        if (isContainer)                                                              \
            ANNOTATE_DELETE_BUFFER(object, objectSize, 0);                            \
    } while (0)

// A vector backing store represented by a large object is marked
// so that when it is finalized, its ASan annotation will be
// correctly retired.
#define ASAN_MARK_LARGE_VECTOR_CONTAINER(heap, largeObject)                  \
    if (heap->heapIndex() == VectorHeapIndex) {                              \
        BasePage* largePage = pageFromObject(largeObject);                   \
        ASSERT(largePage->isLargeObjectPage());                              \
        static_cast<LargeObjectPage*>(largePage)->setIsVectorBackingPage();  \
    }
#else
#define ENABLE_ASAN_CONTAINER_ANNOTATIONS 0
#define ASAN_RETIRE_CONTAINER_ANNOTATION(payload, payloadSize)
#define ASAN_MARK_LARGE_VECTOR_CONTAINER(heap, largeObject)
#endif

namespace blink {

#if ENABLE(GC_PROFILING)
static String classOf(const void* object)
{
    if (const GCInfo* gcInfo = Heap::findGCInfo(reinterpret_cast<Address>(const_cast<void*>(object))))
        return gcInfo->m_className;
    return "unknown";
}
#endif

static bool vTableInitialized(void* objectPointer)
{
    return !!(*reinterpret_cast<Address*>(objectPointer));
}

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
        for (int attempt = 0; attempt < 10; ++attempt) {
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
        for (int attempt = 0; attempt < 3; ++attempt) {
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
    // The returned page memory region will be zeroed.
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
        // Only the writable area is mapped and can contain C++ objects.  Those
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

#if ENABLE(ASSERT)
NO_SANITIZE_ADDRESS
void HeapObjectHeader::zapMagic()
{
    checkHeader();
    m_magic = zappedMagic;
}
#endif

void HeapObjectHeader::finalize(Address object, size_t objectSize)
{
    const GCInfo* gcInfo = Heap::gcInfo(gcInfoIndex());
    if (gcInfo->hasFinalizer()) {
        gcInfo->m_finalize(object);
    }

    ASAN_RETIRE_CONTAINER_ANNOTATION(object, objectSize);

#if ENABLE(ASSERT) || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
    // In Debug builds, memory is zapped when it's freed, and the zapped memory
    // is zeroed out when the memory is reused.  Memory is also zapped when
    // using Leak Sanitizer because the heap is used as a root region for LSan
    // and therefore pointers in unreachable memory could hide leaks.
    for (size_t i = 0; i < objectSize; ++i)
        object[i] = finalizedZapValue;

    // Zap the primary vTable entry (secondary vTable entries are not zapped).
    if (gcInfo->hasVTable()) {
        *(reinterpret_cast<uintptr_t*>(object)) = zappedVTable;
    }
#endif
    // In Release builds, the entire object is zeroed out when it is added to
    // the free list.  This happens right after sweeping the page and before the
    // thread commences execution.
}

BaseHeap::BaseHeap(ThreadState* state, int index)
    : m_firstPage(nullptr)
    , m_firstUnsweptPage(nullptr)
    , m_threadState(state)
    , m_index(index)
{
}

BaseHeap::~BaseHeap()
{
    ASSERT(!m_firstPage);
    ASSERT(!m_firstUnsweptPage);
}

void BaseHeap::cleanupPages()
{
    clearFreeLists();

    ASSERT(!m_firstUnsweptPage);
    // Add the BaseHeap's pages to the orphanedPagePool.
    for (BasePage* page = m_firstPage; page; page = page->next()) {
        Heap::decreaseAllocatedSpace(page->size());
        Heap::orphanedPagePool()->addOrphanedPage(heapIndex(), page);
    }
    m_firstPage = nullptr;
}

#if ENABLE(ASSERT) || ENABLE(GC_PROFILING)
BasePage* BaseHeap::findPageFromAddress(Address address)
{
    for (BasePage* page = m_firstPage; page; page = page->next()) {
        if (page->contains(address))
            return page;
    }
    for (BasePage* page = m_firstUnsweptPage; page; page = page->next()) {
        if (page->contains(address))
            return page;
    }
    return nullptr;
}
#endif

#if ENABLE(GC_PROFILING)
#define GC_PROFILE_HEAP_PAGE_SNAPSHOT_THRESHOLD 0
void BaseHeap::snapshot(TracedValue* json, ThreadState::SnapshotInfo* info)
{
    ASSERT(isConsistentForSweeping());
    size_t previousPageCount = info->pageCount;

    json->beginArray("pages");
    for (BasePage* page = m_firstPage; page; page = page->next(), ++info->pageCount) {
        // FIXME: To limit the size of the snapshot we only output "threshold" many page snapshots.
        if (info->pageCount < GC_PROFILE_HEAP_PAGE_SNAPSHOT_THRESHOLD) {
            json->beginArray();
            json->pushInteger(reinterpret_cast<intptr_t>(page));
            page->snapshot(json, info);
            json->endArray();
        } else {
            page->snapshot(nullptr, info);
        }
    }
    json->endArray();

    json->setInteger("pageCount", info->pageCount - previousPageCount);
}

void BaseHeap::countMarkedObjects(ClassAgeCountsMap& classAgeCounts) const
{
    for (BasePage* page = m_firstPage; page; page = page->next())
        page->countMarkedObjects(classAgeCounts);
}

void BaseHeap::countObjectsToSweep(ClassAgeCountsMap& classAgeCounts) const
{
    for (BasePage* page = m_firstPage; page; page = page->next())
        page->countObjectsToSweep(classAgeCounts);
}

void BaseHeap::incrementMarkedObjectsAge()
{
    for (BasePage* page = m_firstPage; page; page = page->next())
        page->incrementMarkedObjectsAge();
}
#endif

void BaseHeap::makeConsistentForSweeping()
{
    clearFreeLists();
    ASSERT(isConsistentForSweeping());
    for (BasePage* page = m_firstPage; page; page = page->next())
        page->markAsUnswept();

    // If a new GC is requested before this thread got around to sweep,
    // ie. due to the thread doing a long running operation, we clear
    // the mark bits and mark any of the dead objects as dead. The latter
    // is used to ensure the next GC marking does not trace already dead
    // objects. If we trace a dead object we could end up tracing into
    // garbage or the middle of another object via the newly conservatively
    // found object.
    BasePage* previousPage = nullptr;
    for (BasePage* page = m_firstUnsweptPage; page; previousPage = page, page = page->next()) {
        page->markUnmarkedObjectsDead();
        ASSERT(!page->hasBeenSwept());
    }
    if (previousPage) {
        ASSERT(m_firstUnsweptPage);
        previousPage->m_next = m_firstPage;
        m_firstPage = m_firstUnsweptPage;
        m_firstUnsweptPage = nullptr;
    }
    ASSERT(!m_firstUnsweptPage);
}

size_t BaseHeap::objectPayloadSizeForTesting()
{
    ASSERT(isConsistentForSweeping());
    ASSERT(!m_firstUnsweptPage);

    size_t objectPayloadSize = 0;
    for (BasePage* page = m_firstPage; page; page = page->next())
        objectPayloadSize += page->objectPayloadSizeForTesting();
    return objectPayloadSize;
}

void BaseHeap::prepareHeapForTermination()
{
    ASSERT(!m_firstUnsweptPage);
    for (BasePage* page = m_firstPage; page; page = page->next()) {
        page->setTerminating();
    }
}

void BaseHeap::prepareForSweep()
{
    ASSERT(!threadState()->isInGC());
    ASSERT(!m_firstUnsweptPage);

    // Move all pages to a list of unswept pages.
    m_firstUnsweptPage = m_firstPage;
    m_firstPage = nullptr;
}

Address BaseHeap::lazySweep(size_t allocationSize, size_t gcInfoIndex)
{
    // If there are no pages to be swept, return immediately.
    if (!m_firstUnsweptPage)
        return nullptr;

    RELEASE_ASSERT(threadState()->isSweepingInProgress());

    // lazySweepPages() can be called recursively if finalizers invoked in
    // page->sweep() allocate memory and the allocation triggers
    // lazySweepPages(). This check prevents the sweeping from being executed
    // recursively.
    if (threadState()->sweepForbidden())
        return nullptr;

    TRACE_EVENT0("blink_gc", "BaseHeap::lazySweepPages");
    ThreadState::SweepForbiddenScope scope(threadState());

    if (threadState()->isMainThread())
        ScriptForbiddenScope::enter();

    Address result = lazySweepPages(allocationSize, gcInfoIndex);

    if (threadState()->isMainThread())
        ScriptForbiddenScope::exit();
    return result;
}

void BaseHeap::completeSweep()
{
    RELEASE_ASSERT(threadState()->isSweepingInProgress());
    ASSERT(threadState()->sweepForbidden());

    if (threadState()->isMainThread())
        ScriptForbiddenScope::enter();

    while (m_firstUnsweptPage) {
        BasePage* page = m_firstUnsweptPage;
        if (page->isEmpty()) {
            page->unlink(&m_firstUnsweptPage);
            page->removeFromHeap();
        } else {
            // Sweep a page and move the page from m_firstUnsweptPages to
            // m_firstPages.
            page->sweep();
            page->unlink(&m_firstUnsweptPage);
            page->link(&m_firstPage);
            page->markAsSwept();
        }
    }

    if (threadState()->isMainThread())
        ScriptForbiddenScope::exit();
}

NormalPageHeap::NormalPageHeap(ThreadState* state, int index)
    : BaseHeap(state, index)
    , m_currentAllocationPoint(nullptr)
    , m_remainingAllocationSize(0)
    , m_lastRemainingAllocationSize(0)
    , m_promptlyFreedSize(0)
#if ENABLE(GC_PROFILING)
    , m_cumulativeAllocationSize(0)
    , m_allocationCount(0)
    , m_inlineAllocationCount(0)
#endif
{
    clearFreeLists();
}

void NormalPageHeap::clearFreeLists()
{
    setAllocationPoint(nullptr, 0);
    m_freeList.clear();
}

#if ENABLE(ASSERT)
bool NormalPageHeap::isConsistentForSweeping()
{
    // A thread heap is consistent for sweeping if none of the pages to be swept
    // contain a freelist block or the current allocation point.
    for (size_t i = 0; i < blinkPageSizeLog2; ++i) {
        for (FreeListEntry* freeListEntry = m_freeList.m_freeLists[i]; freeListEntry; freeListEntry = freeListEntry->next()) {
            if (pagesToBeSweptContains(freeListEntry->address()))
                return false;
        }
    }
    if (hasCurrentAllocationArea()) {
        if (pagesToBeSweptContains(currentAllocationPoint()))
            return false;
    }
    return true;
}

bool NormalPageHeap::pagesToBeSweptContains(Address address)
{
    for (BasePage* page = m_firstUnsweptPage; page; page = page->next()) {
        if (page->contains(address))
            return true;
    }
    return false;
}
#endif

#if ENABLE(GC_PROFILING)
void NormalPageHeap::snapshotFreeList(TracedValue& json)
{
    json.setInteger("cumulativeAllocationSize", m_cumulativeAllocationSize);
    json.setDouble("inlineAllocationRate", static_cast<double>(m_inlineAllocationCount) / m_allocationCount);
    json.setInteger("inlineAllocationCount", m_inlineAllocationCount);
    json.setInteger("allocationCount", m_allocationCount);
    size_t pageCount = 0;
    size_t totalPageSize = 0;
    for (NormalPage* page = static_cast<NormalPage*>(m_firstPage); page; page = static_cast<NormalPage*>(page->next())) {
        ++pageCount;
        totalPageSize += page->payloadSize();
    }
    json.setInteger("pageCount", pageCount);
    json.setInteger("totalPageSize", totalPageSize);

    FreeList::PerBucketFreeListStats bucketStats[blinkPageSizeLog2];
    size_t totalFreeSize;
    m_freeList.getFreeSizeStats(bucketStats, totalFreeSize);
    json.setInteger("totalFreeSize", totalFreeSize);

    json.beginArray("perBucketEntryCount");
    for (size_t i = 0; i < blinkPageSizeLog2; ++i)
        json.pushInteger(bucketStats[i].entryCount);
    json.endArray();

    json.beginArray("perBucketFreeSize");
    for (size_t i = 0; i < blinkPageSizeLog2; ++i)
        json.pushInteger(bucketStats[i].freeSize);
    json.endArray();
}
#endif

void NormalPageHeap::allocatePage()
{
    threadState()->shouldFlushHeapDoesNotContainCache();
    PageMemory* pageMemory = Heap::freePagePool()->takeFreePage(heapIndex());
    // We continue allocating page memory until we succeed in committing one.
    while (!pageMemory) {
        // Allocate a memory region for blinkPagesPerRegion pages that
        // will each have the following layout.
        //
        //    [ guard os page | ... payload ... | guard os page ]
        //    ^---{ aligned to blink page size }
        PageMemoryRegion* region = PageMemoryRegion::allocateNormalPages();
        threadState()->allocatedRegionsSinceLastGC().append(region);

        // Setup the PageMemory object for each of the pages in the region.
        size_t offset = 0;
        for (size_t i = 0; i < blinkPagesPerRegion; ++i) {
            PageMemory* memory = PageMemory::setupPageMemoryInRegion(region, offset, blinkPagePayloadSize());
            // Take the first possible page ensuring that this thread actually
            // gets a page and add the rest to the page pool.
            if (!pageMemory) {
                if (memory->commit())
                    pageMemory = memory;
                else
                    delete memory;
            } else {
                Heap::freePagePool()->addFreePage(heapIndex(), memory);
            }
            offset += blinkPageSize;
        }
    }
    NormalPage* page = new (pageMemory->writableStart()) NormalPage(pageMemory, this);
    page->link(&m_firstPage);

    Heap::increaseAllocatedSpace(page->size());
    addToFreeList(page->payload(), page->payloadSize());
}

void NormalPageHeap::freePage(NormalPage* page)
{
    Heap::decreaseAllocatedSpace(page->size());

    if (page->terminating()) {
        // The thread is shutting down and this page is being removed as a part
        // of the thread local GC.  In that case the object could be traced in
        // the next global GC if there is a dangling pointer from a live thread
        // heap to this dead thread heap.  To guard against this, we put the
        // page into the orphaned page pool and zap the page memory.  This
        // ensures that tracing the dangling pointer in the next global GC just
        // crashes instead of causing use-after-frees.  After the next global
        // GC, the orphaned pages are removed.
        Heap::orphanedPagePool()->addOrphanedPage(heapIndex(), page);
    } else {
        PageMemory* memory = page->storage();
        page->~NormalPage();
        Heap::freePagePool()->addFreePage(heapIndex(), memory);
    }
}

bool NormalPageHeap::coalesce()
{
    // Don't coalesce heaps if there are not enough promptly freed entries
    // to be coalesced.
    //
    // FIXME: This threshold is determined just to optimize blink_perf
    // benchmarks. Coalescing is very sensitive to the threashold and
    // we need further investigations on the coalescing scheme.
    if (m_promptlyFreedSize < 1024 * 1024)
        return false;

    if (threadState()->sweepForbidden())
        return false;

    ASSERT(!hasCurrentAllocationArea());
    TRACE_EVENT0("blink_gc", "BaseHeap::coalesce");

    // Rebuild free lists.
    m_freeList.clear();
    size_t freedSize = 0;
    for (NormalPage* page = static_cast<NormalPage*>(m_firstPage); page; page = static_cast<NormalPage*>(page->next())) {
        page->clearObjectStartBitMap();
        Address startOfGap = page->payload();
        for (Address headerAddress = startOfGap; headerAddress < page->payloadEnd(); ) {
            HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(headerAddress);
            size_t size = header->size();
            ASSERT(size > 0);
            ASSERT(size < blinkPagePayloadSize());

            if (header->isPromptlyFreed()) {
                ASSERT(size >= sizeof(HeapObjectHeader));
                FILL_ZERO_IF_PRODUCTION(headerAddress, sizeof(HeapObjectHeader));
                freedSize += size;
                headerAddress += size;
                continue;
            }
            if (header->isFree()) {
                // Zero the memory in the free list header to maintain the
                // invariant that memory on the free list is zero filled.
                // The rest of the memory is already on the free list and is
                // therefore already zero filled.
                FILL_ZERO_IF_PRODUCTION(headerAddress, size < sizeof(FreeListEntry) ? size : sizeof(FreeListEntry));
                headerAddress += size;
                continue;
            }
            if (startOfGap != headerAddress)
                addToFreeList(startOfGap, headerAddress - startOfGap);

            headerAddress += size;
            startOfGap = headerAddress;
        }

        if (startOfGap != page->payloadEnd())
            addToFreeList(startOfGap, page->payloadEnd() - startOfGap);
    }
    Heap::decreaseAllocatedObjectSize(freedSize);
    ASSERT(m_promptlyFreedSize == freedSize);
    m_promptlyFreedSize = 0;
    return true;
}

void NormalPageHeap::promptlyFreeObject(HeapObjectHeader* header)
{
    ASSERT(!threadState()->sweepForbidden());
    header->checkHeader();
    Address address = reinterpret_cast<Address>(header);
    Address payload = header->payload();
    size_t size = header->size();
    size_t payloadSize = header->payloadSize();
    ASSERT(size > 0);
    ASSERT(pageFromObject(address) == findPageFromAddress(address));

    {
        ThreadState::SweepForbiddenScope forbiddenScope(threadState());
        header->finalize(payload, payloadSize);
        if (address + size == m_currentAllocationPoint) {
            m_currentAllocationPoint = address;
            if (m_lastRemainingAllocationSize == m_remainingAllocationSize) {
                Heap::decreaseAllocatedObjectSize(size);
                m_lastRemainingAllocationSize += size;
            }
            m_remainingAllocationSize += size;
            FILL_ZERO_IF_PRODUCTION(address, size);
            ASAN_POISON_MEMORY_REGION(address, size);
            return;
        }
        FILL_ZERO_IF_PRODUCTION(payload, payloadSize);
        header->markPromptlyFreed();
    }

    m_promptlyFreedSize += size;
}

bool NormalPageHeap::expandObject(HeapObjectHeader* header, size_t newSize)
{
    // It's possible that Vector requests a smaller expanded size because
    // Vector::shrinkCapacity can set a capacity smaller than the actual payload
    // size.
    if (header->payloadSize() >= newSize)
        return true;
    size_t allocationSize = allocationSizeFromSize(newSize);
    ASSERT(allocationSize > header->size());
    size_t expandSize = allocationSize - header->size();
    if (header->payloadEnd() == m_currentAllocationPoint && expandSize <= m_remainingAllocationSize) {
        m_currentAllocationPoint += expandSize;
        m_remainingAllocationSize -= expandSize;

        // Unpoison the memory used for the object (payload).
        ASAN_UNPOISON_MEMORY_REGION(header->payloadEnd(), expandSize);
        FILL_ZERO_IF_NOT_PRODUCTION(header->payloadEnd(), expandSize);
        header->setSize(allocationSize);
        ASSERT(findPageFromAddress(header->payloadEnd() - 1));
        return true;
    }
    return false;
}

void NormalPageHeap::shrinkObject(HeapObjectHeader* header, size_t newSize)
{
    ASSERT(header->payloadSize() > newSize);
    size_t allocationSize = allocationSizeFromSize(newSize);
    ASSERT(header->size() > allocationSize);
    size_t shrinkSize = header->size() - allocationSize;
    if (header->payloadEnd() == m_currentAllocationPoint) {
        m_currentAllocationPoint -= shrinkSize;
        m_remainingAllocationSize += shrinkSize;
        FILL_ZERO_IF_PRODUCTION(m_currentAllocationPoint, shrinkSize);
        ASAN_POISON_MEMORY_REGION(m_currentAllocationPoint, shrinkSize);
        header->setSize(allocationSize);
    } else {
        ASSERT(shrinkSize >= sizeof(HeapObjectHeader));
        ASSERT(header->gcInfoIndex() > 0);
        HeapObjectHeader* freedHeader = new (NotNull, header->payloadEnd() - shrinkSize) HeapObjectHeader(shrinkSize, header->gcInfoIndex());
        freedHeader->markPromptlyFreed();
        ASSERT(pageFromObject(reinterpret_cast<Address>(header)) == findPageFromAddress(reinterpret_cast<Address>(header)));
        m_promptlyFreedSize += shrinkSize;
        header->setSize(allocationSize);
    }
}

Address NormalPageHeap::lazySweepPages(size_t allocationSize, size_t gcInfoIndex)
{
    ASSERT(!hasCurrentAllocationArea());
    Address result = nullptr;
    while (m_firstUnsweptPage) {
        BasePage* page = m_firstUnsweptPage;
        if (page->isEmpty()) {
            page->unlink(&m_firstUnsweptPage);
            page->removeFromHeap();
        } else {
            // Sweep a page and move the page from m_firstUnsweptPages to
            // m_firstPages.
            page->sweep();
            page->unlink(&m_firstUnsweptPage);
            page->link(&m_firstPage);
            page->markAsSwept();

            // For NormalPage, stop lazy sweeping once we find a slot to
            // allocate a new object.
            result = allocateFromFreeList(allocationSize, gcInfoIndex);
            if (result)
                break;
        }
    }
    return result;
}

void NormalPageHeap::updateRemainingAllocationSize()
{
    if (m_lastRemainingAllocationSize > remainingAllocationSize()) {
        Heap::increaseAllocatedObjectSize(m_lastRemainingAllocationSize - remainingAllocationSize());
        m_lastRemainingAllocationSize = remainingAllocationSize();
    }
    ASSERT(m_lastRemainingAllocationSize == remainingAllocationSize());
}

void NormalPageHeap::setAllocationPoint(Address point, size_t size)
{
#if ENABLE(ASSERT)
    if (point) {
        ASSERT(size);
        BasePage* page = pageFromObject(point);
        ASSERT(!page->isLargeObjectPage());
        ASSERT(size <= static_cast<NormalPage*>(page)->payloadSize());
    }
#endif
    if (hasCurrentAllocationArea()) {
        addToFreeList(currentAllocationPoint(), remainingAllocationSize());
    }
    updateRemainingAllocationSize();
    m_currentAllocationPoint = point;
    m_lastRemainingAllocationSize = m_remainingAllocationSize = size;
}

Address NormalPageHeap::outOfLineAllocate(size_t allocationSize, size_t gcInfoIndex)
{
    ASSERT(allocationSize > remainingAllocationSize());
    ASSERT(allocationSize >= allocationGranularity);

#if ENABLE(GC_PROFILING)
    threadState()->snapshotFreeListIfNecessary();
#endif

    // 1. If this allocation is big enough, allocate a large object.
    if (allocationSize >= largeObjectSizeThreshold) {
        LargeObjectHeap* largeObjectHeap = static_cast<LargeObjectHeap*>(threadState()->heap(LargeObjectHeapIndex));
        Address largeObject = largeObjectHeap->allocateLargeObjectPage(allocationSize, gcInfoIndex);
        ASAN_MARK_LARGE_VECTOR_CONTAINER(this, largeObject);
        return largeObject;
    }

    // 2. Check if we should trigger a GC.
    updateRemainingAllocationSize();
    threadState()->scheduleGCIfNeeded();

    // 3. Try to allocate from a free list.
    Address result = allocateFromFreeList(allocationSize, gcInfoIndex);
    if (result)
        return result;

    // 4. Reset the allocation point.
    setAllocationPoint(nullptr, 0);

    // 5. Lazily sweep pages of this heap until we find a freed area for
    // this allocation or we finish sweeping all pages of this heap.
    result = lazySweep(allocationSize, gcInfoIndex);
    if (result)
        return result;

    // 6. Coalesce promptly freed areas and then try to allocate from a free
    // list.
    if (coalesce()) {
        result = allocateFromFreeList(allocationSize, gcInfoIndex);
        if (result)
            return result;
    }

    // 7. Complete sweeping.
    threadState()->completeSweep();

    // 8. Add a new page to this heap.
    allocatePage();

    // 9. Try to allocate from a free list. This allocation must succeed.
    result = allocateFromFreeList(allocationSize, gcInfoIndex);
    RELEASE_ASSERT(result);
    return result;
}

Address NormalPageHeap::allocateFromFreeList(size_t allocationSize, size_t gcInfoIndex)
{
    // Try reusing a block from the largest bin. The underlying reasoning
    // being that we want to amortize this slow allocation call by carving
    // off as a large a free block as possible in one go; a block that will
    // service this block and let following allocations be serviced quickly
    // by bump allocation.
    size_t bucketSize = 1 << m_freeList.m_biggestFreeListIndex;
    int index = m_freeList.m_biggestFreeListIndex;
    for (; index > 0; --index, bucketSize >>= 1) {
        FreeListEntry* entry = m_freeList.m_freeLists[index];
        if (allocationSize > bucketSize) {
            // Final bucket candidate; check initial entry if it is able
            // to service this allocation. Do not perform a linear scan,
            // as it is considered too costly.
            if (!entry || entry->size() < allocationSize)
                break;
        }
        if (entry) {
            entry->unlink(&m_freeList.m_freeLists[index]);
            setAllocationPoint(entry->address(), entry->size());
            ASSERT(hasCurrentAllocationArea());
            ASSERT(remainingAllocationSize() >= allocationSize);
            m_freeList.m_biggestFreeListIndex = index;
            return allocateObject(allocationSize, gcInfoIndex);
        }
    }
    m_freeList.m_biggestFreeListIndex = index;
    return nullptr;
}

LargeObjectHeap::LargeObjectHeap(ThreadState* state, int index)
    : BaseHeap(state, index)
{
}

Address LargeObjectHeap::allocateLargeObjectPage(size_t allocationSize, size_t gcInfoIndex)
{
    // Caller already added space for object header and rounded up to allocation
    // alignment
    ASSERT(!(allocationSize & allocationMask));

    // 1. Check if we should trigger a GC.
    threadState()->scheduleGCIfNeeded();

    // 2. Try to sweep large objects more than allocationSize bytes
    // before allocating a new large object.
    Address result = lazySweep(allocationSize, gcInfoIndex);
    if (result)
        return result;

    // 3. If we have failed in sweeping allocationSize bytes,
    // we complete sweeping before allocating this large object.
    threadState()->completeSweep();
    return doAllocateLargeObjectPage(allocationSize, gcInfoIndex);
}

Address LargeObjectHeap::doAllocateLargeObjectPage(size_t allocationSize, size_t gcInfoIndex)
{
    size_t largeObjectSize = LargeObjectPage::pageHeaderSize() + allocationSize;
    // If ASan is supported we add allocationGranularity bytes to the allocated
    // space and poison that to detect overflows
#if defined(ADDRESS_SANITIZER)
    largeObjectSize += allocationGranularity;
#endif

    threadState()->shouldFlushHeapDoesNotContainCache();
    PageMemory* pageMemory = PageMemory::allocate(largeObjectSize);
    threadState()->allocatedRegionsSinceLastGC().append(pageMemory->region());
    Address largeObjectAddress = pageMemory->writableStart();
    Address headerAddress = largeObjectAddress + LargeObjectPage::pageHeaderSize();
#if ENABLE(ASSERT)
    // Verify that the allocated PageMemory is expectedly zeroed.
    for (size_t i = 0; i < largeObjectSize; ++i)
        ASSERT(!largeObjectAddress[i]);
#endif
    ASSERT(gcInfoIndex > 0);
    HeapObjectHeader* header = new (NotNull, headerAddress) HeapObjectHeader(largeObjectSizeInHeader, gcInfoIndex);
    Address result = headerAddress + sizeof(*header);
    ASSERT(!(reinterpret_cast<uintptr_t>(result) & allocationMask));
    LargeObjectPage* largeObject = new (largeObjectAddress) LargeObjectPage(pageMemory, this, allocationSize);
    header->checkHeader();

    // Poison the object header and allocationGranularity bytes after the object
    ASAN_POISON_MEMORY_REGION(header, sizeof(*header));
    ASAN_POISON_MEMORY_REGION(largeObject->address() + largeObject->size(), allocationGranularity);

    largeObject->link(&m_firstPage);

    Heap::increaseAllocatedSpace(largeObject->size());
    Heap::increaseAllocatedObjectSize(largeObject->size());
    return result;
}

void LargeObjectHeap::freeLargeObjectPage(LargeObjectPage* object)
{
    object->heapObjectHeader()->finalize(object->payload(), object->payloadSize());
    Heap::decreaseAllocatedSpace(object->size());

    // Unpoison the object header and allocationGranularity bytes after the
    // object before freeing.
    ASAN_UNPOISON_MEMORY_REGION(object->heapObjectHeader(), sizeof(HeapObjectHeader));
    ASAN_UNPOISON_MEMORY_REGION(object->address() + object->size(), allocationGranularity);

    if (object->terminating()) {
        ASSERT(ThreadState::current()->isTerminating());
        // The thread is shutting down and this page is being removed as a part
        // of the thread local GC.  In that case the object could be traced in
        // the next global GC if there is a dangling pointer from a live thread
        // heap to this dead thread heap.  To guard against this, we put the
        // page into the orphaned page pool and zap the page memory.  This
        // ensures that tracing the dangling pointer in the next global GC just
        // crashes instead of causing use-after-frees.  After the next global
        // GC, the orphaned pages are removed.
        Heap::orphanedPagePool()->addOrphanedPage(heapIndex(), object);
    } else {
        ASSERT(!ThreadState::current()->isTerminating());
        PageMemory* memory = object->storage();
        object->~LargeObjectPage();
        delete memory;
    }
}

Address LargeObjectHeap::lazySweepPages(size_t allocationSize, size_t gcInfoIndex)
{
    Address result = nullptr;
    size_t sweptSize = 0;
    while (m_firstUnsweptPage) {
        BasePage* page = m_firstUnsweptPage;
        if (page->isEmpty()) {
            sweptSize += static_cast<LargeObjectPage*>(page)->payloadSize() + sizeof(HeapObjectHeader);
            page->unlink(&m_firstUnsweptPage);
            page->removeFromHeap();
            // For LargeObjectPage, stop lazy sweeping once we have swept
            // more than allocationSize bytes.
            if (sweptSize >= allocationSize) {
                result = doAllocateLargeObjectPage(allocationSize, gcInfoIndex);
                ASSERT(result);
                break;
            }
        } else {
            // Sweep a page and move the page from m_firstUnsweptPages to
            // m_firstPages.
            page->sweep();
            page->unlink(&m_firstUnsweptPage);
            page->link(&m_firstPage);
            page->markAsSwept();
        }
    }
    return result;
}

template<typename DataType>
PagePool<DataType>::PagePool()
{
    for (int i = 0; i < NumberOfHeaps; ++i) {
        m_pool[i] = nullptr;
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
    // while in the pool.  This also allows the physical memory, backing the
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
    return nullptr;
}

OrphanedPagePool::~OrphanedPagePool()
{
    for (int index = 0; index < NumberOfHeaps; ++index) {
        while (PoolEntry* entry = m_pool[index]) {
            m_pool[index] = entry->next;
            BasePage* page = entry->data;
            delete entry;
            PageMemory* memory = page->storage();
            ASSERT(memory);
            page->~BasePage();
            delete memory;
        }
    }
}

void OrphanedPagePool::addOrphanedPage(int index, BasePage* page)
{
    page->markOrphaned();
    PoolEntry* entry = new PoolEntry(page, m_pool[index]);
    m_pool[index] = entry;
}

NO_SANITIZE_ADDRESS
void OrphanedPagePool::decommitOrphanedPages()
{
    ASSERT(ThreadState::current()->isInGC());

#if ENABLE(ASSERT)
    // No locking needed as all threads are at safepoints at this point in time.
    for (ThreadState* state : ThreadState::attachedThreads())
        ASSERT(state->isAtSafePoint());
#endif

    for (int index = 0; index < NumberOfHeaps; ++index) {
        PoolEntry* entry = m_pool[index];
        PoolEntry** prevNext = &m_pool[index];
        while (entry) {
            BasePage* page = entry->data;
            // Check if we should reuse the memory or just free it.
            // Large object memory is not reused but freed, normal blink heap
            // pages are reused.
            // NOTE: We call the destructor before freeing or adding to the
            // free page pool.
            PageMemory* memory = page->storage();
            if (page->isLargeObjectPage()) {
                page->~BasePage();
                delete memory;
            } else {
                page->~BasePage();
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
            BasePage* page = entry->data;
            if (page->contains(reinterpret_cast<Address>(object)))
                return true;
        }
    }
    return false;
}
#endif

FreeList::FreeList()
    : m_biggestFreeListIndex(0)
{
}

void FreeList::addToFreeList(Address address, size_t size)
{
    ASSERT(size < blinkPagePayloadSize());
    // The free list entries are only pointer aligned (but when we allocate
    // from them we are 8 byte aligned due to the header size).
    ASSERT(!((reinterpret_cast<uintptr_t>(address) + sizeof(HeapObjectHeader)) & allocationMask));
    ASSERT(!(size & allocationMask));
    ASAN_POISON_MEMORY_REGION(address, size);
    FreeListEntry* entry;
    if (size < sizeof(*entry)) {
        // Create a dummy header with only a size and freelist bit set.
        ASSERT(size >= sizeof(HeapObjectHeader));
        // Free list encode the size to mark the lost memory as freelist memory.
        new (NotNull, address) HeapObjectHeader(size, gcInfoIndexForFreeListHeader);
        // This memory gets lost. Sweeping can reclaim it.
        return;
    }
    entry = new (NotNull, address) FreeListEntry(size);
#if defined(ADDRESS_SANITIZER)
    BasePage* page = pageFromObject(address);
    ASSERT(!page->isLargeObjectPage());
    // For ASan we don't add the entry to the free lists until the
    // asanDeferMemoryReuseCount reaches zero.  However we always add entire
    // pages to ensure that adding a new page will increase the allocation
    // space.
    if (static_cast<NormalPage*>(page)->payloadSize() != size && !entry->shouldAddToFreeList())
        return;
#endif
    int index = bucketIndexForSize(size);
    entry->link(&m_freeLists[index]);
    if (index > m_biggestFreeListIndex)
        m_biggestFreeListIndex = index;
}

void FreeList::clear()
{
    m_biggestFreeListIndex = 0;
    for (size_t i = 0; i < blinkPageSizeLog2; ++i)
        m_freeLists[i] = nullptr;
}

int FreeList::bucketIndexForSize(size_t size)
{
    ASSERT(size > 0);
    int index = -1;
    while (size) {
        size >>= 1;
        index++;
    }
    return index;
}

#if ENABLE(GC_PROFILING)
void FreeList::getFreeSizeStats(PerBucketFreeListStats bucketStats[], size_t& totalFreeSize) const
{
    totalFreeSize = 0;
    for (size_t i = 0; i < blinkPageSizeLog2; i++) {
        size_t& entryCount = bucketStats[i].entryCount;
        size_t& freeSize = bucketStats[i].freeSize;
        for (FreeListEntry* entry = m_freeLists[i]; entry; entry = entry->next()) {
            ++entryCount;
            freeSize += entry->size();
        }
        totalFreeSize += freeSize;
    }
}
#endif

BasePage::BasePage(PageMemory* storage, BaseHeap* heap)
    : m_storage(storage)
    , m_heap(heap)
    , m_next(nullptr)
    , m_terminating(false)
    , m_swept(true)
{
    ASSERT(isPageHeaderAddress(reinterpret_cast<Address>(this)));
}

void BasePage::markOrphaned()
{
    m_heap = nullptr;
    m_terminating = false;
    // Since we zap the page payload for orphaned pages we need to mark it as
    // unused so a conservative pointer won't interpret the object headers.
    storage()->markUnused();
}

NormalPage::NormalPage(PageMemory* storage, BaseHeap* heap)
    : BasePage(storage, heap)
{
    m_objectStartBitMapComputed = false;
    ASSERT(isPageHeaderAddress(reinterpret_cast<Address>(this)));
}

size_t NormalPage::objectPayloadSizeForTesting()
{
    size_t objectPayloadSize = 0;
    Address headerAddress = payload();
    markAsSwept();
    ASSERT(headerAddress != payloadEnd());
    do {
        HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(headerAddress);
        if (!header->isFree()) {
            header->checkHeader();
            objectPayloadSize += header->payloadSize();
        }
        ASSERT(header->size() < blinkPagePayloadSize());
        headerAddress += header->size();
        ASSERT(headerAddress <= payloadEnd());
    } while (headerAddress < payloadEnd());
    return objectPayloadSize;
}

bool NormalPage::isEmpty()
{
    HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(payload());
    return header->isFree() && header->size() == payloadSize();
}

void NormalPage::removeFromHeap()
{
    heapForNormalPage()->freePage(this);
}

void NormalPage::sweep()
{
    clearObjectStartBitMap();

    size_t markedObjectSize = 0;
    Address startOfGap = payload();
    for (Address headerAddress = startOfGap; headerAddress < payloadEnd(); ) {
        HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(headerAddress);
        ASSERT(header->size() > 0);
        ASSERT(header->size() < blinkPagePayloadSize());

        if (header->isPromptlyFreed())
            heapForNormalPage()->decreasePromptlyFreedSize(header->size());
        if (header->isFree()) {
            size_t size = header->size();
            // Zero the memory in the free list header to maintain the
            // invariant that memory on the free list is zero filled.
            // The rest of the memory is already on the free list and is
            // therefore already zero filled.
            FILL_ZERO_IF_PRODUCTION(headerAddress, size < sizeof(FreeListEntry) ? size : sizeof(FreeListEntry));
            headerAddress += size;
            continue;
        }
        header->checkHeader();

        if (!header->isMarked()) {
            size_t size = header->size();
            // This is a fast version of header->payloadSize().
            size_t payloadSize = size - sizeof(HeapObjectHeader);
            Address payload = header->payload();
            // For ASan we unpoison the specific object when calling the
            // finalizer and poison it again when done to allow the object's own
            // finalizer to operate on the object, but not have other finalizers
            // be allowed to access it.
            ASAN_UNPOISON_MEMORY_REGION(payload, payloadSize);
            header->finalize(payload, payloadSize);
            // This memory will be added to the freelist. Maintain the invariant
            // that memory on the freelist is zero filled.
            FILL_ZERO_IF_PRODUCTION(headerAddress, size);
            ASAN_POISON_MEMORY_REGION(payload, payloadSize);
            headerAddress += size;
            continue;
        }

        if (startOfGap != headerAddress)
            heapForNormalPage()->addToFreeList(startOfGap, headerAddress - startOfGap);
        header->unmark();
        headerAddress += header->size();
        markedObjectSize += header->size();
        startOfGap = headerAddress;
    }
    if (startOfGap != payloadEnd())
        heapForNormalPage()->addToFreeList(startOfGap, payloadEnd() - startOfGap);

    if (markedObjectSize)
        Heap::increaseMarkedObjectSize(markedObjectSize);
}

void NormalPage::markUnmarkedObjectsDead()
{
    for (Address headerAddress = payload(); headerAddress < payloadEnd();) {
        HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(headerAddress);
        ASSERT(header->size() < blinkPagePayloadSize());
        // Check if a free list entry first since we cannot call
        // isMarked on a free list entry.
        if (header->isFree()) {
            headerAddress += header->size();
            continue;
        }
        header->checkHeader();
        if (header->isMarked())
            header->unmark();
        else
            header->markDead();
        headerAddress += header->size();
    }
}

void NormalPage::populateObjectStartBitMap()
{
    memset(&m_objectStartBitMap, 0, objectStartBitMapSize);
    Address start = payload();
    for (Address headerAddress = start; headerAddress < payloadEnd();) {
        HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(headerAddress);
        size_t objectOffset = headerAddress - start;
        ASSERT(!(objectOffset & allocationMask));
        size_t objectStartNumber = objectOffset / allocationGranularity;
        size_t mapIndex = objectStartNumber / 8;
        ASSERT(mapIndex < objectStartBitMapSize);
        m_objectStartBitMap[mapIndex] |= (1 << (objectStartNumber & 7));
        headerAddress += header->size();
        ASSERT(headerAddress <= payloadEnd());
    }
    m_objectStartBitMapComputed = true;
}

void NormalPage::clearObjectStartBitMap()
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

HeapObjectHeader* NormalPage::findHeaderFromAddress(Address address)
{
    if (address < payload())
        return nullptr;
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
    HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(objectAddress);
    if (header->isFree())
        return nullptr;
    header->checkHeader();
    return header;
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

static void markPointer(Visitor* visitor, HeapObjectHeader* header)
{
    const GCInfo* gcInfo = Heap::gcInfo(header->gcInfoIndex());
    if (gcInfo->hasVTable() && !vTableInitialized(header->payload())) {
        visitor->markHeaderNoTracing(header);
        ASSERT(isUninitializedMemory(header->payload(), header->payloadSize()));
    } else {
        visitor->markHeader(header, gcInfo->m_trace);
    }
}

void NormalPage::checkAndMarkPointer(Visitor* visitor, Address address)
{
    ASSERT(contains(address));
    HeapObjectHeader* header = findHeaderFromAddress(address);
    if (!header || header->isDead())
        return;
#if ENABLE(GC_PROFILING)
    visitor->setHostInfo(&address, "stack");
#endif
    markPointer(visitor, header);
}

void NormalPage::markOrphaned()
{
    // Zap the payload with a recognizable value to detect any incorrect
    // cross thread pointer usage.
#if defined(ADDRESS_SANITIZER)
    // This needs to zap poisoned memory as well.
    // Force unpoison memory before memset.
    ASAN_UNPOISON_MEMORY_REGION(payload(), payloadSize());
#endif
    memset(payload(), orphanedZapValue, payloadSize());
    BasePage::markOrphaned();
}

#if ENABLE(GC_PROFILING)
const GCInfo* NormalPage::findGCInfo(Address address)
{
    if (address < payload())
        return nullptr;

    HeapObjectHeader* header = findHeaderFromAddress(address);
    if (!header)
        return nullptr;

    return Heap::gcInfo(header->gcInfoIndex());
}
#endif

#if ENABLE(GC_PROFILING)
void NormalPage::snapshot(TracedValue* json, ThreadState::SnapshotInfo* info)
{
    HeapObjectHeader* header = nullptr;
    for (Address addr = payload(); addr < payloadEnd(); addr += header->size()) {
        header = reinterpret_cast<HeapObjectHeader*>(addr);
        if (json)
            json->pushInteger(header->encodedSize());
        if (header->isFree()) {
            info->freeSize += header->size();
            continue;
        }

        size_t tag = info->getClassTag(Heap::gcInfo(header->gcInfoIndex()));
        size_t age = header->age();
        if (json)
            json->pushInteger(tag);
        if (header->isMarked()) {
            info->liveCount[tag] += 1;
            info->liveSize[tag] += header->size();
            // Count objects that are live when promoted to the final generation.
            if (age == maxHeapObjectAge - 1)
                info->generations[tag][maxHeapObjectAge] += 1;
        } else {
            info->deadCount[tag] += 1;
            info->deadSize[tag] += header->size();
            // Count objects that are dead before the final generation.
            if (age < maxHeapObjectAge)
                info->generations[tag][age] += 1;
        }
    }
}

void NormalPage::incrementMarkedObjectsAge()
{
    HeapObjectHeader* header = nullptr;
    for (Address address = payload(); address < payloadEnd(); address += header->size()) {
        header = reinterpret_cast<HeapObjectHeader*>(address);
        if (header->isMarked())
            header->incrementAge();
    }
}

void NormalPage::countMarkedObjects(ClassAgeCountsMap& classAgeCounts)
{
    HeapObjectHeader* header = nullptr;
    for (Address address = payload(); address < payloadEnd(); address += header->size()) {
        header = reinterpret_cast<HeapObjectHeader*>(address);
        if (header->isMarked()) {
            String className(classOf(header->payload()));
            ++(classAgeCounts.add(className, AgeCounts()).storedValue->value.ages[header->age()]);
        }
    }
}

void NormalPage::countObjectsToSweep(ClassAgeCountsMap& classAgeCounts)
{
    HeapObjectHeader* header = nullptr;
    for (Address address = payload(); address < payloadEnd(); address += header->size()) {
        header = reinterpret_cast<HeapObjectHeader*>(address);
        if (!header->isFree() && !header->isMarked()) {
            String className(classOf(header->payload()));
            ++(classAgeCounts.add(className, AgeCounts()).storedValue->value.ages[header->age()]);
        }
    }
}
#endif

#if ENABLE(ASSERT) || ENABLE(GC_PROFILING)
bool NormalPage::contains(Address addr)
{
    Address blinkPageStart = roundToBlinkPageStart(address());
    ASSERT(blinkPageStart == address() - WTF::kSystemPageSize); // Page is at aligned address plus guard page size.
    return blinkPageStart <= addr && addr < blinkPageStart + blinkPageSize;
}
#endif

NormalPageHeap* NormalPage::heapForNormalPage()
{
    return static_cast<NormalPageHeap*>(heap());
}

LargeObjectPage::LargeObjectPage(PageMemory* storage, BaseHeap* heap, size_t payloadSize)
    : BasePage(storage, heap)
    , m_payloadSize(payloadSize)
#if ENABLE(ASAN_CONTAINER_ANNOTATIONS)
    , m_isVectorBackingPage(false)
#endif
{
}

size_t LargeObjectPage::objectPayloadSizeForTesting()
{
    markAsSwept();
    return payloadSize();
}

bool LargeObjectPage::isEmpty()
{
    return !heapObjectHeader()->isMarked();
}

void LargeObjectPage::removeFromHeap()
{
    static_cast<LargeObjectHeap*>(heap())->freeLargeObjectPage(this);
}

void LargeObjectPage::sweep()
{
    Heap::increaseMarkedObjectSize(size());
    heapObjectHeader()->unmark();
}

void LargeObjectPage::markUnmarkedObjectsDead()
{
    HeapObjectHeader* header = heapObjectHeader();
    if (header->isMarked())
        header->unmark();
    else
        header->markDead();
}

void LargeObjectPage::checkAndMarkPointer(Visitor* visitor, Address address)
{
    ASSERT(contains(address));
    if (!containedInObjectPayload(address) || heapObjectHeader()->isDead())
        return;
#if ENABLE(GC_PROFILING)
    visitor->setHostInfo(&address, "stack");
#endif
    markPointer(visitor, heapObjectHeader());
}

void LargeObjectPage::markOrphaned()
{
    // Zap the payload with a recognizable value to detect any incorrect
    // cross thread pointer usage.
    memset(payload(), orphanedZapValue, payloadSize());
    BasePage::markOrphaned();
}

#if ENABLE(GC_PROFILING)
const GCInfo* LargeObjectPage::findGCInfo(Address address)
{
    if (!containedInObjectPayload(address))
        return nullptr;
    HeapObjectHeader* header = heapObjectHeader();
    return Heap::gcInfo(header->gcInfoIndex());
}

void LargeObjectPage::snapshot(TracedValue* json, ThreadState::SnapshotInfo* info)
{
    HeapObjectHeader* header = heapObjectHeader();
    size_t tag = info->getClassTag(Heap::gcInfo(header->gcInfoIndex()));
    size_t age = header->age();
    if (header->isMarked()) {
        info->liveCount[tag] += 1;
        info->liveSize[tag] += header->size();
        // Count objects that are live when promoted to the final generation.
        if (age == maxHeapObjectAge - 1)
            info->generations[tag][maxHeapObjectAge] += 1;
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
        json->setInteger("isMarked", header->isMarked());
    }
}

void LargeObjectPage::incrementMarkedObjectsAge()
{
    HeapObjectHeader* header = heapObjectHeader();
    if (header->isMarked())
        header->incrementAge();
}

void LargeObjectPage::countMarkedObjects(ClassAgeCountsMap& classAgeCounts)
{
    HeapObjectHeader* header = heapObjectHeader();
    if (header->isMarked()) {
        String className(classOf(header->payload()));
        ++(classAgeCounts.add(className, AgeCounts()).storedValue->value.ages[header->age()]);
    }
}

void LargeObjectPage::countObjectsToSweep(ClassAgeCountsMap& classAgeCounts)
{
    HeapObjectHeader* header = heapObjectHeader();
    if (!header->isFree() && !header->isMarked()) {
        String className(classOf(header->payload()));
        ++(classAgeCounts.add(className, AgeCounts()).storedValue->value.ages[header->age()]);
    }
}
#endif

#if ENABLE(ASSERT) || ENABLE(GC_PROFILING)
bool LargeObjectPage::contains(Address object)
{
    return roundToBlinkPageStart(address()) <= object && object < roundToBlinkPageEnd(address() + size());
}
#endif

void HeapDoesNotContainCache::flush()
{
    if (m_hasEntries) {
        for (int i = 0; i < numberOfEntries; ++i)
            m_entries[i] = nullptr;
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
    ASSERT(ThreadState::current()->isInGC());

    size_t index = hash(address);
    ASSERT(!(index & 1));
    Address cachePage = roundToBlinkPageStart(address);
    if (m_entries[index] == cachePage)
        return m_entries[index];
    if (m_entries[index + 1] == cachePage)
        return m_entries[index + 1];
    return false;
}

void HeapDoesNotContainCache::addEntry(Address address)
{
    ASSERT(ThreadState::current()->isInGC());

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

enum MarkingMode {
    GlobalMarking,
    ThreadLocalMarking,
};

template <MarkingMode Mode>
class MarkingVisitor final : public Visitor, public MarkingVisitorImpl<MarkingVisitor<Mode>> {
public:
    using Impl = MarkingVisitorImpl<MarkingVisitor<Mode>>;
    friend class MarkingVisitorImpl<MarkingVisitor<Mode>>;

#if ENABLE(GC_PROFILING)
    using LiveObjectSet = HashSet<uintptr_t>;
    using LiveObjectMap = HashMap<String, LiveObjectSet>;
    using ObjectGraph = HashMap<uintptr_t, std::pair<uintptr_t, String>>;
#endif

    MarkingVisitor()
        : Visitor(Mode == GlobalMarking ? Visitor::GlobalMarkingVisitorType : Visitor::GenericVisitorType)
    {
    }

    virtual void markHeader(HeapObjectHeader* header, TraceCallback callback) override
    {
        Impl::visitHeader(header, header->payload(), callback);
    }

    virtual void mark(const void* objectPointer, TraceCallback callback) override
    {
        Impl::mark(objectPointer, callback);
    }

    virtual void registerDelayedMarkNoTracing(const void* object) override
    {
        Impl::registerDelayedMarkNoTracing(object);
    }

    virtual void registerWeakMembers(const void* closure, const void* objectPointer, WeakPointerCallback callback) override
    {
        Impl::registerWeakMembers(closure, objectPointer, callback);
    }

    virtual void registerWeakTable(const void* closure, EphemeronCallback iterationCallback, EphemeronCallback iterationDoneCallback)
    {
        Impl::registerWeakTable(closure, iterationCallback, iterationDoneCallback);
    }

#if ENABLE(ASSERT)
    virtual bool weakTableRegistered(const void* closure)
    {
        return Impl::weakTableRegistered(closure);
    }
#endif

    virtual bool isMarked(const void* objectPointer) override
    {
        return Impl::isMarked(objectPointer);
    }

    virtual bool ensureMarked(const void* objectPointer) override
    {
        return Impl::ensureMarked(objectPointer);
    }

#if ENABLE(GC_PROFILING)
    virtual void recordObjectGraphEdge(const void* objectPointer) override
    {
        MutexLocker locker(objectGraphMutex());
        String className(classOf(objectPointer));
        {
            LiveObjectMap::AddResult result = currentlyLive().add(className, LiveObjectSet());
            result.storedValue->value.add(reinterpret_cast<uintptr_t>(objectPointer));
        }
        ObjectGraph::AddResult result = objectGraph().add(reinterpret_cast<uintptr_t>(objectPointer), std::make_pair(reinterpret_cast<uintptr_t>(m_hostObject), m_hostName));
        ASSERT(result.isNewEntry);
        // fprintf(stderr, "%s[%p] -> %s[%p]\n", m_hostName.ascii().data(), m_hostObject, className.ascii().data(), objectPointer);
    }

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

        for (uintptr_t object : objectsToFindPath()) {
            dumpPathToObjectFromObjectGraph(objectGraph(), object);
        }
    }

    static void reportStillAlive(LiveObjectSet current, LiveObjectSet previous)
    {
        int count = 0;

        fprintf(stderr, " [previously %u]", previous.size());
        for (uintptr_t object : current) {
            if (previous.find(object) == previous.end())
                continue;
            count++;
        }

        if (!count)
            return;

        fprintf(stderr, " {survived 2GCs %d: ", count);
        for (uintptr_t object : current) {
            if (previous.find(object) == previous.end())
                continue;
            fprintf(stderr, "%ld", object);
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
        AtomicallyInitializedStaticReference(Mutex, mutex, new Mutex);
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
    virtual void registerWeakCellWithCallback(void** cell, WeakPointerCallback callback) override
    {
        Impl::registerWeakCellWithCallback(cell, callback);
    }

    inline bool shouldMarkObject(const void* objectPointer)
    {
        if (Mode != ThreadLocalMarking)
            return true;

        BasePage* page = pageFromObject(objectPointer);
        ASSERT(!page->orphaned());
        // When doing a thread local GC, the marker checks if
        // the object resides in another thread's heap. If it
        // does, the object should not be marked & traced.
        return page->terminating();
    }

#if ENABLE(ASSERT)
    virtual void checkMarkingAllowed() override
    {
        ASSERT(ThreadState::current()->isInGC());
    }
#endif
};

void Heap::init()
{
    ThreadState::init();
    s_markingStack = new CallbackStack();
    s_postMarkingCallbackStack = new CallbackStack();
    s_weakCallbackStack = new CallbackStack();
    s_ephemeronStack = new CallbackStack();
    s_heapDoesNotContainCache = new HeapDoesNotContainCache();
    s_markingVisitor = new MarkingVisitor<GlobalMarking>();
    s_freePagePool = new FreePagePool();
    s_orphanedPagePool = new OrphanedPagePool();
    s_allocatedObjectSize = 0;
    s_allocatedSpace = 0;
    s_markedObjectSize = 0;

    GCInfoTable::init();
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

    ASSERT(!ThreadState::attachedThreads().size());
    delete s_markingVisitor;
    s_markingVisitor = nullptr;
    delete s_heapDoesNotContainCache;
    s_heapDoesNotContainCache = nullptr;
    delete s_freePagePool;
    s_freePagePool = nullptr;
    delete s_orphanedPagePool;
    s_orphanedPagePool = nullptr;
    delete s_weakCallbackStack;
    s_weakCallbackStack = nullptr;
    delete s_postMarkingCallbackStack;
    s_postMarkingCallbackStack = nullptr;
    delete s_markingStack;
    s_markingStack = nullptr;
    delete s_ephemeronStack;
    s_ephemeronStack = nullptr;
    delete s_regionTree;
    s_regionTree = nullptr;
    GCInfoTable::shutdown();
    ThreadState::shutdown();
    ASSERT(Heap::allocatedSpace() == 0);
}

#if ENABLE(ASSERT)
BasePage* Heap::findPageFromAddress(Address address)
{
    ASSERT(ThreadState::current()->isInGC());
    for (ThreadState* state : ThreadState::attachedThreads()) {
        if (BasePage* page = state->findPageFromAddress(address))
            return page;
    }
    return nullptr;
}

bool Heap::containedInHeapOrOrphanedPage(void* object)
{
    return findPageFromAddress(object) || orphanedPagePool()->contains(object);
}
#endif

Address Heap::checkAndMarkPointer(Visitor* visitor, Address address)
{
    ASSERT(ThreadState::current()->isInGC());

#if !ENABLE(ASSERT)
    if (s_heapDoesNotContainCache->lookup(address))
        return nullptr;
#endif

    if (BasePage* page = lookup(address)) {
        ASSERT(page->contains(address));
        ASSERT(!page->orphaned());
        ASSERT(!s_heapDoesNotContainCache->lookup(address));
        page->checkAndMarkPointer(visitor, address);
        // FIXME: We only need to set the conservative flag if
        // checkAndMarkPointer actually marked the pointer.
        s_lastGCWasConservative = true;
        return address;
    }

#if !ENABLE(ASSERT)
    s_heapDoesNotContainCache->addEntry(address);
#else
    if (!s_heapDoesNotContainCache->lookup(address))
        s_heapDoesNotContainCache->addEntry(address);
#endif
    return nullptr;
}

#if ENABLE(GC_PROFILING)
const GCInfo* Heap::findGCInfo(Address address)
{
    return ThreadState::findGCInfoFromAllThreads(address);
}
#endif

#if ENABLE(GC_PROFILING)
void Heap::dumpPathToObjectOnNextGC(void* p)
{
    static_cast<MarkingVisitor<GlobalMarking>*>(s_markingVisitor)->dumpPathToObjectOnNextGC(p);
}

String Heap::createBacktraceString()
{
    int framesToShow = 3;
    int stackFrameSize = 16;
    ASSERT(stackFrameSize >= framesToShow);
    using FramePointer = void*;
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

void Heap::pushTraceCallback(void* object, TraceCallback callback)
{
    ASSERT(Heap::containedInHeapOrOrphanedPage(object));
    CallbackStack::Item* slot = s_markingStack->allocateEntry();
    *slot = CallbackStack::Item(object, callback);
}

bool Heap::popAndInvokeTraceCallback(Visitor* visitor)
{
    CallbackStack::Item* item = s_markingStack->pop();
    if (!item)
        return false;

#if ENABLE(GC_PROFILING)
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
    BasePage* page = pageFromObject(object);
    ASSERT(!page->orphaned());
    ThreadState* state = page->heap()->threadState();
    state->pushWeakPointerCallback(closure, callback);
}

bool Heap::popAndInvokeWeakPointerCallback(Visitor* visitor)
{
    // For weak processing we should never reach orphaned pages since orphaned
    // pages are not traced and thus objects on those pages are never be
    // registered as objects on orphaned pages.  We cannot assert this here
    // since we might have an off-heap collection.  We assert it in
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
        // Check that the ephemeron table being pushed onto the stack is not on
        // an orphaned page.
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
    ASSERT(!ThreadState::current()->isInGC());
    for (ThreadState* state : ThreadState::attachedThreads())
        state->preGC();
}

void Heap::postGC(ThreadState::GCType gcType)
{
    ASSERT(ThreadState::current()->isInGC());
    for (ThreadState* state : ThreadState::attachedThreads())
        state->postGC(gcType);
}

void Heap::collectGarbage(ThreadState::StackState stackState, ThreadState::GCType gcType)
{
    ThreadState* state = ThreadState::current();
    ThreadState::GCState originalGCState = state->gcState();
    state->setGCState(ThreadState::StoppingOtherThreads);

    GCScope gcScope(stackState);
    // Check if we successfully parked the other threads.  If not we bail out of
    // the GC.
    if (!gcScope.allThreadsParked()) {
        // Restore the original GCState.
        state->setGCState(originalGCState);
        return;
    }

    if (state->isMainThread())
        ScriptForbiddenScope::enter();

    s_lastGCWasConservative = false;

    TRACE_EVENT2("blink_gc", "Heap::collectGarbage",
        "precise", stackState == ThreadState::NoHeapPointersOnStack,
        "forced", gcType == ThreadState::GCWithSweep);
    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink_gc", "BlinkGC");
    double timeStamp = WTF::currentTimeMS();
#if ENABLE(GC_PROFILING)
    static_cast<MarkingVisitor<GlobalMarking>*>(s_markingVisitor)->objectGraph().clear();
#endif

    // Disallow allocation during garbage collection (but not during the
    // finalization that happens when the gcScope is torn down).
    ThreadState::NoAllocationScope noAllocationScope(state);

    preGC();
    s_markingVisitor->configureEagerTraceLimit();
    ASSERT(s_markingVisitor->canTraceEagerly());

    Heap::resetHeapCounters();

    // 1. Trace persistent roots.
    ThreadState::visitPersistentRoots(s_markingVisitor);

    // 2. Trace objects reachable from the persistent roots including
    // ephemerons.
    processMarkingStack(s_markingVisitor);

    // 3. Trace objects reachable from the stack.  We do this independent of the
    // given stackState since other threads might have a different stack state.
    ThreadState::visitStackRoots(s_markingVisitor);

    // 4. Trace objects reachable from the stack "roots" including ephemerons.
    // Only do the processing if we found a pointer to an object on one of the
    // thread stacks.
    if (lastGCWasConservative())
        processMarkingStack(s_markingVisitor);

    postMarkingProcessing(s_markingVisitor);
    globalWeakProcessing(s_markingVisitor);

    // Now we can delete all orphaned pages because there are no dangling
    // pointers to the orphaned pages.  (If we have such dangling pointers,
    // we should have crashed during marking before getting here.)
    orphanedPagePool()->decommitOrphanedPages();

    postGC(gcType);

#if ENABLE(GC_PROFILING)
    static_cast<MarkingVisitor<GlobalMarking>*>(s_markingVisitor)->reportStats();
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
        MarkingVisitor<ThreadLocalMarking> markingVisitor;
        ThreadState::NoAllocationScope noAllocationScope(state);

        state->preGC();
        s_markingVisitor->configureEagerTraceLimit();
        ASSERT(s_markingVisitor->canTraceEagerly());

        // 1. Trace the thread local persistent roots. For thread local GCs we
        // don't trace the stack (ie. no conservative scanning) since this is
        // only called during thread shutdown where there should be no objects
        // on the stack.
        // We also assume that orphaned pages have no objects reachable from
        // persistent handles on other threads or CrossThreadPersistents.  The
        // only cases where this could happen is if a subsequent conservative
        // global GC finds a "pointer" on the stack or due to a programming
        // error where an object has a dangling cross-thread pointer to an
        // object on this heap.
        state->visitPersistents(&markingVisitor);

        // 2. Trace objects reachable from the thread's persistent roots
        // including ephemerons.
        processMarkingStack(&markingVisitor);

        postMarkingProcessing(&markingVisitor);
        globalWeakProcessing(&markingVisitor);

        state->postGC(ThreadState::GCWithSweep);
    }
    state->postGCProcessing();
}

void Heap::processMarkingStack(Visitor* markingVisitor)
{
    // Ephemeron fixed point loop.
    do {
        {
            // Iteratively mark all objects that are reachable from the objects
            // currently pushed onto the marking stack.
            TRACE_EVENT0("blink_gc", "Heap::processMarkingStackSingleThreaded");
            while (popAndInvokeTraceCallback(markingVisitor)) { }
        }

        {
            // Mark any strong pointers that have now become reachable in
            // ephemeron maps.
            TRACE_EVENT0("blink_gc", "Heap::processEphemeronStack");
            s_ephemeronStack->invokeEphemeronCallbacks(markingVisitor);
        }

        // Rerun loop if ephemeron processing queued more objects for tracing.
    } while (!s_markingStack->isEmpty());
}

void Heap::postMarkingProcessing(Visitor* markingVisitor)
{
    TRACE_EVENT0("blink_gc", "Heap::postMarkingProcessing");
    // Call post-marking callbacks including:
    // 1. the ephemeronIterationDone callbacks on weak tables to do cleanup
    //    (specifically to clear the queued bits for weak hash tables), and
    // 2. the markNoTracing callbacks on collection backings to mark them
    //    if they are only reachable from their front objects.
    while (popAndInvokePostMarkingCallback(markingVisitor)) { }

    s_ephemeronStack->clear();

    // Post-marking callbacks should not trace any objects and
    // therefore the marking stack should be empty after the
    // post-marking callbacks.
    ASSERT(s_markingStack->isEmpty());
}

void Heap::globalWeakProcessing(Visitor* markingVisitor)
{
    TRACE_EVENT0("blink_gc", "Heap::globalWeakProcessing");
    // Call weak callbacks on objects that may now be pointing to dead objects.
    while (popAndInvokeWeakPointerCallback(markingVisitor)) { }

    // It is not permitted to trace pointers of live objects in the weak
    // callback phase, so the marking stack should still be empty here.
    ASSERT(s_markingStack->isEmpty());
}

void Heap::collectAllGarbage()
{
    // FIXME: Oilpan: we should perform a single GC and everything
    // should die. Unfortunately it is not the case for all objects
    // because the hierarchy was not completely moved to the heap and
    // some heap allocated objects own objects that contain persistents
    // pointing to other heap allocated objects.
    for (int i = 0; i < 5; ++i)
        collectGarbage(ThreadState::NoHeapPointersOnStack);
}

double Heap::estimatedMarkingTime()
{
    // FIXME: Implement heuristics
    return 0.0;
}

size_t Heap::objectPayloadSizeForTesting()
{
    size_t objectPayloadSize = 0;
    for (ThreadState* state : ThreadState::attachedThreads()) {
        state->setGCState(ThreadState::GCRunning);
        state->makeConsistentForSweeping();
        objectPayloadSize += state->objectPayloadSizeForTesting();
        state->setGCState(ThreadState::EagerSweepScheduled);
        state->setGCState(ThreadState::Sweeping);
        state->setGCState(ThreadState::NoGCScheduled);
    }
    return objectPayloadSize;
}

void HeapAllocator::backingFree(void* address)
{
    if (!address)
        return;

    ThreadState* state = ThreadState::current();
    if (state->sweepForbidden())
        return;
    ASSERT(!state->isInGC());

    // Don't promptly free large objects because their page is never reused.
    // Don't free backings allocated on other threads.
    BasePage* page = pageFromObject(address);
    if (page->isLargeObjectPage() || page->heap()->threadState() != state)
        return;

    HeapObjectHeader* header = HeapObjectHeader::fromPayload(address);
    header->checkHeader();
    static_cast<NormalPage*>(page)->heapForNormalPage()->promptlyFreeObject(header);
}

void HeapAllocator::freeVectorBacking(void* address)
{
    backingFree(address);
}

void HeapAllocator::freeInlineVectorBacking(void* address)
{
    backingFree(address);
}

void HeapAllocator::freeHashTableBacking(void* address)
{
    backingFree(address);
}

bool HeapAllocator::backingExpand(void* address, size_t newSize)
{
    if (!address)
        return false;

    ThreadState* state = ThreadState::current();
    if (state->sweepForbidden())
        return false;
    ASSERT(!state->isInGC());
    ASSERT(state->isAllocationAllowed());

    // FIXME: Support expand for large objects.
    // Don't expand backings allocated on other threads.
    BasePage* page = pageFromObject(address);
    if (page->isLargeObjectPage() || page->heap()->threadState() != state)
        return false;

    HeapObjectHeader* header = HeapObjectHeader::fromPayload(address);
    header->checkHeader();
    return static_cast<NormalPage*>(page)->heapForNormalPage()->expandObject(header, newSize);
}

bool HeapAllocator::expandVectorBacking(void* address, size_t newSize)
{
    return backingExpand(address, newSize);
}

bool HeapAllocator::expandInlineVectorBacking(void* address, size_t newSize)
{
    return backingExpand(address, newSize);
}

bool HeapAllocator::expandHashTableBacking(void* address, size_t newSize)
{
    return backingExpand(address, newSize);
}

void HeapAllocator::backingShrink(void* address, size_t quantizedCurrentSize, size_t quantizedShrunkSize)
{
    // We shrink the object only if the shrinking will make a non-small
    // prompt-free block.
    // FIXME: Optimize the threshold size.
    if (quantizedCurrentSize <= quantizedShrunkSize + sizeof(HeapObjectHeader) + sizeof(void*) * 32)
        return;

    if (!address)
        return;

    ThreadState* state = ThreadState::current();
    if (state->sweepForbidden())
        return;
    ASSERT(!state->isInGC());
    ASSERT(state->isAllocationAllowed());

    // FIXME: Support shrink for large objects.
    // Don't shrink backings allocated on other threads.
    BasePage* page = pageFromObject(address);
    if (page->isLargeObjectPage() || page->heap()->threadState() != state)
        return;

    HeapObjectHeader* header = HeapObjectHeader::fromPayload(address);
    header->checkHeader();
    static_cast<NormalPage*>(page)->heapForNormalPage()->shrinkObject(header, quantizedShrunkSize);
}

BasePage* Heap::lookup(Address address)
{
    ASSERT(ThreadState::current()->isInGC());
    if (!s_regionTree)
        return nullptr;
    if (PageMemoryRegion* region = s_regionTree->lookup(address)) {
        BasePage* page = region->pageFromAddress(address);
        return page && !page->orphaned() ? page : nullptr;
    }
    return nullptr;
}

static Mutex& regionTreeMutex()
{
    AtomicallyInitializedStaticReference(Mutex, mutex, new Mutex);
    return mutex;
}

void Heap::removePageMemoryRegion(PageMemoryRegion* region)
{
    // Deletion of large objects (and thus their regions) can happen
    // concurrently on sweeper threads.  Removal can also happen during thread
    // shutdown, but that case is safe.  Regardless, we make all removals
    // mutually exclusive.
    MutexLocker locker(regionTreeMutex());
    RegionTree::remove(region, &s_regionTree);
}

void Heap::addPageMemoryRegion(PageMemoryRegion* region)
{
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
    return nullptr;
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

void Heap::resetHeapCounters()
{
    ASSERT(ThreadState::current()->isInGC());

    s_allocatedObjectSize = 0;
    s_markedObjectSize = 0;

    // Similarly, reset the amount of externally allocated memory.
    s_externallyAllocatedBytes = 0;
    s_externallyAllocatedBytesAlive = 0;

    s_requestedUrgentGC = false;
}

void Heap::requestUrgentGC()
{
    // The urgent-gc flag will be considered the next time an out-of-line
    // allocation is made. Bump allocations from the current block will
    // go ahead until it can no longer service an allocation request.
    //
    // FIXME: if that delays urgently needed GCs for too long, consider
    // flushing out per-heap "allocation points" to trigger the GC
    // right away.
    releaseStore(&s_requestedUrgentGC, 1);
}

Visitor* Heap::s_markingVisitor;
CallbackStack* Heap::s_markingStack;
CallbackStack* Heap::s_postMarkingCallbackStack;
CallbackStack* Heap::s_weakCallbackStack;
CallbackStack* Heap::s_ephemeronStack;
HeapDoesNotContainCache* Heap::s_heapDoesNotContainCache;
bool Heap::s_shutdownCalled = false;
bool Heap::s_lastGCWasConservative = false;
FreePagePool* Heap::s_freePagePool;
OrphanedPagePool* Heap::s_orphanedPagePool;
Heap::RegionTree* Heap::s_regionTree = nullptr;
size_t Heap::s_allocatedObjectSize = 0;
size_t Heap::s_allocatedSpace = 0;
size_t Heap::s_markedObjectSize = 0;

size_t Heap::s_externallyAllocatedBytes = 0;
size_t Heap::s_externallyAllocatedBytesAlive = 0;
unsigned Heap::s_requestedUrgentGC = false;

} // namespace blink
