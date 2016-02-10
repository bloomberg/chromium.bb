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

#include "platform/heap/HeapPage.h"

#include "platform/ScriptForbiddenScope.h"
#include "platform/Task.h"
#include "platform/TraceEvent.h"
#include "platform/heap/BlinkGCMemoryDumpProvider.h"
#include "platform/heap/CallbackStack.h"
#include "platform/heap/Heap.h"
#include "platform/heap/MarkingVisitor.h"
#include "platform/heap/PageMemory.h"
#include "platform/heap/PagePool.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/ThreadState.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMemoryAllocatorDump.h"
#include "public/platform/WebProcessMemoryDump.h"
#include "wtf/Assertions.h"
#include "wtf/ContainerAnnotations.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/MainThread.h"
#include "wtf/PageAllocator.h"
#include "wtf/Partitions.h"
#include "wtf/PassOwnPtr.h"

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
        bool isContainer = ThreadState::isVectorHeapIndex(page->heap()->heapIndex()); \
        if (!isContainer && page->isLargeObjectPage())                                \
            isContainer = static_cast<LargeObjectPage*>(page)->isVectorBackingPage(); \
        if (isContainer)                                                              \
            ANNOTATE_DELETE_BUFFER(object, objectSize, 0);                            \
    } while (0)

// A vector backing store represented by a large object is marked
// so that when it is finalized, its ASan annotation will be
// correctly retired.
#define ASAN_MARK_LARGE_VECTOR_CONTAINER(heap, largeObject)                 \
    if (ThreadState::isVectorHeapIndex(heap->heapIndex())) {                \
        BasePage* largePage = pageFromObject(largeObject);                  \
        ASSERT(largePage->isLargeObjectPage());                             \
        static_cast<LargeObjectPage*>(largePage)->setIsVectorBackingPage(); \
    }
#else
#define ENABLE_ASAN_CONTAINER_ANNOTATIONS 0
#define ASAN_RETIRE_CONTAINER_ANNOTATION(payload, payloadSize)
#define ASAN_MARK_LARGE_VECTOR_CONTAINER(heap, largeObject)
#endif

namespace blink {

#if ENABLE(ASSERT)
NO_SANITIZE_ADDRESS
void HeapObjectHeader::zapMagic()
{
    ASSERT(checkHeader());
    m_magic = zappedMagic;
}
#endif

void HeapObjectHeader::finalize(Address object, size_t objectSize)
{
    const GCInfo* gcInfo = Heap::gcInfo(gcInfoIndex());
    if (gcInfo->hasFinalizer())
        gcInfo->m_finalize(object);

    ASAN_RETIRE_CONTAINER_ANNOTATION(object, objectSize);
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

void BaseHeap::takeSnapshot(const String& dumpBaseName, ThreadState::GCSnapshotInfo& info)
{
    // |dumpBaseName| at this point is "blink_gc/thread_X/heaps/HeapName"
    WebMemoryAllocatorDump* allocatorDump = BlinkGCMemoryDumpProvider::instance()->createMemoryAllocatorDumpForCurrentGC(dumpBaseName);
    size_t pageIndex = 0;
    size_t heapTotalFreeSize = 0;
    size_t heapTotalFreeCount = 0;
    for (BasePage* page = m_firstUnsweptPage; page; page = page->next()) {
        size_t heapPageFreeSize = 0;
        size_t heapPageFreeCount = 0;
        page->takeSnapshot(dumpBaseName, pageIndex, info, &heapPageFreeSize, &heapPageFreeCount);
        heapTotalFreeSize += heapPageFreeSize;
        heapTotalFreeCount += heapPageFreeCount;
        pageIndex++;
    }
    allocatorDump->addScalar("blink_page_count", "objects", pageIndex);

    // When taking a full dump (w/ freelist), both the /buckets and /pages
    // report their free size but they are not meant to be added together.
    // Therefore, here we override the free_size of the parent heap to be
    // equal to the free_size of the sum of its heap pages.
    allocatorDump->addScalar("free_size", "bytes", heapTotalFreeSize);
    allocatorDump->addScalar("free_count", "objects", heapTotalFreeCount);
}

#if ENABLE(ASSERT)
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

void BaseHeap::makeConsistentForGC()
{
    clearFreeLists();
    ASSERT(isConsistentForGC());
    for (BasePage* page = m_firstPage; page; page = page->next()) {
        page->markAsUnswept();
        page->invalidateObjectStartBitmap();
    }

    // If a new GC is requested before this thread got around to sweep,
    // ie. due to the thread doing a long running operation, we clear
    // the mark bits and mark any of the dead objects as dead. The latter
    // is used to ensure the next GC marking does not trace already dead
    // objects. If we trace a dead object we could end up tracing into
    // garbage or the middle of another object via the newly conservatively
    // found object.
    BasePage* previousPage = nullptr;
    for (BasePage* page = m_firstUnsweptPage; page; previousPage = page, page = page->next()) {
        page->makeConsistentForGC();
        ASSERT(!page->hasBeenSwept());
        page->invalidateObjectStartBitmap();
    }
    if (previousPage) {
        ASSERT(m_firstUnsweptPage);
        previousPage->m_next = m_firstPage;
        m_firstPage = m_firstUnsweptPage;
        m_firstUnsweptPage = nullptr;
    }
    ASSERT(!m_firstUnsweptPage);
}

void BaseHeap::makeConsistentForMutator()
{
    clearFreeLists();
    ASSERT(isConsistentForGC());
    ASSERT(!m_firstPage);

    // Drop marks from marked objects and rebuild free lists in preparation for
    // resuming the executions of mutators.
    BasePage* previousPage = nullptr;
    for (BasePage* page = m_firstUnsweptPage; page; previousPage = page, page = page->next()) {
        page->makeConsistentForMutator();
        page->markAsSwept();
        page->invalidateObjectStartBitmap();
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
    ASSERT(isConsistentForGC());
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
    ASSERT(threadState()->isInGC());
    ASSERT(!m_firstUnsweptPage);

    // Move all pages to a list of unswept pages.
    m_firstUnsweptPage = m_firstPage;
    m_firstPage = nullptr;
}

#if defined(ADDRESS_SANITIZER)
void BaseHeap::poisonHeap(BlinkGC::ObjectsToPoison objectsToPoison, BlinkGC::Poisoning poisoning)
{
    // TODO(sof): support complete poisoning of all heaps.
    ASSERT(objectsToPoison != BlinkGC::MarkedAndUnmarked || heapIndex() == BlinkGC::EagerSweepHeapIndex);

    // This method may either be called to poison (SetPoison) heap
    // object payloads prior to sweeping, or it may be called at
    // the completion of a sweep to unpoison (ClearPoison) the
    // objects remaining in the heap. Those will all be live and unmarked.
    //
    // Poisoning may be limited to unmarked objects only, or apply to all.
    if (poisoning == BlinkGC::SetPoison) {
        for (BasePage* page = m_firstUnsweptPage; page; page = page->next())
            page->poisonObjects(objectsToPoison, poisoning);
        return;
    }
    // Support clearing of poisoning after sweeping has completed,
    // in which case the pages of the live objects are reachable
    // via m_firstPage.
    ASSERT(!m_firstUnsweptPage);
    for (BasePage* page = m_firstPage; page; page = page->next())
        page->poisonObjects(objectsToPoison, poisoning);
}
#endif

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
    ThreadState::SweepForbiddenScope sweepForbidden(threadState());
    ScriptForbiddenIfMainThreadScope scriptForbidden;

    double startTime = WTF::currentTimeMS();
    Address result = lazySweepPages(allocationSize, gcInfoIndex);
    threadState()->accumulateSweepingTime(WTF::currentTimeMS() - startTime);
    Heap::reportMemoryUsageForTracing();

    return result;
}

void BaseHeap::sweepUnsweptPage()
{
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

bool BaseHeap::lazySweepWithDeadline(double deadlineSeconds)
{
    // It might be heavy to call Platform::current()->monotonicallyIncreasingTimeSeconds()
    // per page (i.e., 128 KB sweep or one LargeObject sweep), so we check
    // the deadline per 10 pages.
    static const int deadlineCheckInterval = 10;

    RELEASE_ASSERT(threadState()->isSweepingInProgress());
    ASSERT(threadState()->sweepForbidden());
    ASSERT(!threadState()->isMainThread() || ScriptForbiddenScope::isScriptForbidden());

    int pageCount = 1;
    while (m_firstUnsweptPage) {
        sweepUnsweptPage();
        if (pageCount % deadlineCheckInterval == 0) {
            if (deadlineSeconds <= Platform::current()->monotonicallyIncreasingTimeSeconds()) {
                // Deadline has come.
                Heap::reportMemoryUsageForTracing();
                return !m_firstUnsweptPage;
            }
        }
        pageCount++;
    }
    Heap::reportMemoryUsageForTracing();
    return true;
}

void BaseHeap::completeSweep()
{
    RELEASE_ASSERT(threadState()->isSweepingInProgress());
    ASSERT(threadState()->sweepForbidden());
    ASSERT(!threadState()->isMainThread() || ScriptForbiddenScope::isScriptForbidden());

    while (m_firstUnsweptPage) {
        sweepUnsweptPage();
    }
    Heap::reportMemoryUsageForTracing();
}

NormalPageHeap::NormalPageHeap(ThreadState* state, int index)
    : BaseHeap(state, index)
    , m_currentAllocationPoint(nullptr)
    , m_remainingAllocationSize(0)
    , m_lastRemainingAllocationSize(0)
    , m_promptlyFreedSize(0)
{
    clearFreeLists();
}

void NormalPageHeap::clearFreeLists()
{
    setAllocationPoint(nullptr, 0);
    m_freeList.clear();
}

#if ENABLE(ASSERT)
bool NormalPageHeap::isConsistentForGC()
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

void NormalPageHeap::takeFreelistSnapshot(const String& dumpName)
{
    if (m_freeList.takeSnapshot(dumpName)) {
        WebMemoryAllocatorDump* bucketsDump = BlinkGCMemoryDumpProvider::instance()->createMemoryAllocatorDumpForCurrentGC(dumpName + "/buckets");
        WebMemoryAllocatorDump* pagesDump = BlinkGCMemoryDumpProvider::instance()->createMemoryAllocatorDumpForCurrentGC(dumpName + "/pages");
        BlinkGCMemoryDumpProvider::instance()->currentProcessMemoryDump()->addOwnershipEdge(pagesDump->guid(), bucketsDump->guid());
    }
}

void NormalPageHeap::allocatePage()
{
    threadState()->shouldFlushHeapDoesNotContainCache();
    PageMemory* pageMemory = Heap::freePagePool()->takeFreePage(heapIndex());

    if (!pageMemory) {
        // Allocate a memory region for blinkPagesPerRegion pages that
        // will each have the following layout.
        //
        //    [ guard os page | ... payload ... | guard os page ]
        //    ^---{ aligned to blink page size }
        PageMemoryRegion* region = PageMemoryRegion::allocateNormalPages();

        // Setup the PageMemory object for each of the pages in the region.
        for (size_t i = 0; i < blinkPagesPerRegion; ++i) {
            PageMemory* memory = PageMemory::setupPageMemoryInRegion(region, i * blinkPageSize, blinkPagePayloadSize());
            // Take the first possible page ensuring that this thread actually
            // gets a page and add the rest to the page pool.
            if (!pageMemory) {
                bool result = memory->commit();
                // If you hit the ASSERT, it will mean that you're hitting
                // the limit of the number of mmapped regions OS can support
                // (e.g., /proc/sys/vm/max_map_count in Linux).
                RELEASE_ASSERT(result);
                pageMemory = memory;
            } else {
                Heap::freePagePool()->addFreePage(heapIndex(), memory);
            }
        }
    }

    NormalPage* page = new (pageMemory->writableStart()) NormalPage(pageMemory, this);
    page->link(&m_firstPage);

    Heap::increaseAllocatedSpace(page->size());
#if ENABLE(ASSERT) || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
    // Allow the following addToFreeList() to add the newly allocated memory
    // to the free list.
    ASAN_UNPOISON_MEMORY_REGION(page->payload(), page->payloadSize());
    Address address = page->payload();
    for (size_t i = 0; i < page->payloadSize(); i++)
        address[i] = reuseAllowedZapValue;
    ASAN_POISON_MEMORY_REGION(page->payload(), page->payloadSize());
#endif
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
        Address startOfGap = page->payload();
        for (Address headerAddress = startOfGap; headerAddress < page->payloadEnd(); ) {
            HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(headerAddress);
            size_t size = header->size();
            ASSERT(size > 0);
            ASSERT(size < blinkPagePayloadSize());

            if (header->isPromptlyFreed()) {
                ASSERT(size >= sizeof(HeapObjectHeader));
                // Zero the memory in the free list header to maintain the
                // invariant that memory on the free list is zero filled.
                // The rest of the memory is already on the free list and is
                // therefore already zero filled.
                SET_MEMORY_INACCESSIBLE(headerAddress, sizeof(HeapObjectHeader));
                CHECK_MEMORY_INACCESSIBLE(headerAddress, size);
                freedSize += size;
                headerAddress += size;
                continue;
            }
            if (header->isFree()) {
                // Zero the memory in the free list header to maintain the
                // invariant that memory on the free list is zero filled.
                // The rest of the memory is already on the free list and is
                // therefore already zero filled.
                SET_MEMORY_INACCESSIBLE(headerAddress, size < sizeof(FreeListEntry) ? size : sizeof(FreeListEntry));
                CHECK_MEMORY_INACCESSIBLE(headerAddress, size);
                headerAddress += size;
                continue;
            }
            ASSERT(header->checkHeader());
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
    ASSERT(header->checkHeader());
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
            setRemainingAllocationSize(m_remainingAllocationSize + size);
            SET_MEMORY_INACCESSIBLE(address, size);
            return;
        }
        SET_MEMORY_INACCESSIBLE(payload, payloadSize);
        header->markPromptlyFreed();
    }

    m_promptlyFreedSize += size;
}

bool NormalPageHeap::expandObject(HeapObjectHeader* header, size_t newSize)
{
    // It's possible that Vector requests a smaller expanded size because
    // Vector::shrinkCapacity can set a capacity smaller than the actual payload
    // size.
    ASSERT(header->checkHeader());
    if (header->payloadSize() >= newSize)
        return true;
    size_t allocationSize = Heap::allocationSizeFromSize(newSize);
    ASSERT(allocationSize > header->size());
    size_t expandSize = allocationSize - header->size();
    if (isObjectAllocatedAtAllocationPoint(header) && expandSize <= m_remainingAllocationSize) {
        m_currentAllocationPoint += expandSize;
        ASSERT(m_remainingAllocationSize >= expandSize);
        setRemainingAllocationSize(m_remainingAllocationSize - expandSize);
        // Unpoison the memory used for the object (payload).
        SET_MEMORY_ACCESSIBLE(header->payloadEnd(), expandSize);
        header->setSize(allocationSize);
        ASSERT(findPageFromAddress(header->payloadEnd() - 1));
        return true;
    }
    return false;
}

bool NormalPageHeap::shrinkObject(HeapObjectHeader* header, size_t newSize)
{
    ASSERT(header->checkHeader());
    ASSERT(header->payloadSize() > newSize);
    size_t allocationSize = Heap::allocationSizeFromSize(newSize);
    ASSERT(header->size() > allocationSize);
    size_t shrinkSize = header->size() - allocationSize;
    if (isObjectAllocatedAtAllocationPoint(header)) {
        m_currentAllocationPoint -= shrinkSize;
        setRemainingAllocationSize(m_remainingAllocationSize + shrinkSize);
        SET_MEMORY_INACCESSIBLE(m_currentAllocationPoint, shrinkSize);
        header->setSize(allocationSize);
        return true;
    }
    ASSERT(shrinkSize >= sizeof(HeapObjectHeader));
    ASSERT(header->gcInfoIndex() > 0);
    Address shrinkAddress = header->payloadEnd() - shrinkSize;
    HeapObjectHeader* freedHeader = new (NotNull, shrinkAddress) HeapObjectHeader(shrinkSize, header->gcInfoIndex());
    freedHeader->markPromptlyFreed();
    ASSERT(pageFromObject(reinterpret_cast<Address>(header)) == findPageFromAddress(reinterpret_cast<Address>(header)));
    m_promptlyFreedSize += shrinkSize;
    header->setSize(allocationSize);
    SET_MEMORY_INACCESSIBLE(shrinkAddress + sizeof(HeapObjectHeader), shrinkSize - sizeof(HeapObjectHeader));
    return false;
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

void NormalPageHeap::setRemainingAllocationSize(size_t newRemainingAllocationSize)
{
    m_remainingAllocationSize = newRemainingAllocationSize;

    // Sync recorded allocated-object size:
    //  - if previous alloc checkpoint is larger, allocation size has increased.
    //  - if smaller, a net reduction in size since last call to updateRemainingAllocationSize().
    if (m_lastRemainingAllocationSize > m_remainingAllocationSize)
        Heap::increaseAllocatedObjectSize(m_lastRemainingAllocationSize - m_remainingAllocationSize);
    else if (m_lastRemainingAllocationSize != m_remainingAllocationSize)
        Heap::decreaseAllocatedObjectSize(m_remainingAllocationSize - m_lastRemainingAllocationSize);
    m_lastRemainingAllocationSize = m_remainingAllocationSize;
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

    // 1. If this allocation is big enough, allocate a large object.
    if (allocationSize >= largeObjectSizeThreshold) {
        // TODO(sof): support eagerly finalized large objects, if ever needed.
        RELEASE_ASSERT(heapIndex() != BlinkGC::EagerSweepHeapIndex);
        LargeObjectHeap* largeObjectHeap = static_cast<LargeObjectHeap*>(threadState()->heap(BlinkGC::LargeObjectHeapIndex));
        Address largeObject = largeObjectHeap->allocateLargeObjectPage(allocationSize, gcInfoIndex);
        ASAN_MARK_LARGE_VECTOR_CONTAINER(this, largeObject);
        return largeObject;
    }

    // 2. Try to allocate from a free list.
    updateRemainingAllocationSize();
    Address result = allocateFromFreeList(allocationSize, gcInfoIndex);
    if (result)
        return result;

    // 3. Reset the allocation point.
    setAllocationPoint(nullptr, 0);

    // 4. Lazily sweep pages of this heap until we find a freed area for
    // this allocation or we finish sweeping all pages of this heap.
    result = lazySweep(allocationSize, gcInfoIndex);
    if (result)
        return result;

    // 5. Coalesce promptly freed areas and then try to allocate from a free
    // list.
    if (coalesce()) {
        result = allocateFromFreeList(allocationSize, gcInfoIndex);
        if (result)
            return result;
    }

    // 6. Complete sweeping.
    threadState()->completeSweep();

    // 7. Check if we should trigger a GC.
    threadState()->scheduleGCIfNeeded();

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

    // 1. Try to sweep large objects more than allocationSize bytes
    // before allocating a new large object.
    Address result = lazySweep(allocationSize, gcInfoIndex);
    if (result)
        return result;

    // 2. If we have failed in sweeping allocationSize bytes,
    // we complete sweeping before allocating this large object.
    threadState()->completeSweep();

    // 3. Check if we should trigger a GC.
    threadState()->scheduleGCIfNeeded();

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
    ASSERT(header->checkHeader());

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
    ASAN_UNPOISON_MEMORY_REGION(object->payload(), object->payloadSize());
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
    ASAN_UNPOISON_MEMORY_REGION(address, size);
    FreeListEntry* entry;
    if (size < sizeof(*entry)) {
        // Create a dummy header with only a size and freelist bit set.
        ASSERT(size >= sizeof(HeapObjectHeader));
        // Free list encode the size to mark the lost memory as freelist memory.
        new (NotNull, address) HeapObjectHeader(size, gcInfoIndexForFreeListHeader);

        ASAN_POISON_MEMORY_REGION(address, size);
        // This memory gets lost. Sweeping can reclaim it.
        return;
    }
    entry = new (NotNull, address) FreeListEntry(size);

#if ENABLE(ASSERT) || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
    // The following logic delays reusing free lists for (at least) one GC
    // cycle or coalescing. This is helpful to detect use-after-free errors
    // that could be caused by lazy sweeping etc.
    size_t allowedCount = 0;
    size_t forbiddenCount = 0;
    for (size_t i = sizeof(FreeListEntry); i < size; i++) {
        if (address[i] == reuseAllowedZapValue) {
            allowedCount++;
        } else if (address[i] == reuseForbiddenZapValue) {
            forbiddenCount++;
        } else {
            // TODO(haraken): Remove these assertions once bug 537922 is fixed.
            HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(&address[i]);
            ASSERT(header->checkHeader());
            if (header->isPromptlyFreed())
                ASSERT_NOT_REACHED();
            else if (header->isFree())
                ASSERT_NOT_REACHED();
            else if (header->isDead())
                ASSERT_NOT_REACHED();
            else if (header->isMarked())
                ASSERT_NOT_REACHED();
            ASSERT_NOT_REACHED();
        }
    }
    size_t entryCount = size - sizeof(FreeListEntry);
    if (forbiddenCount == entryCount) {
        // If all values in the memory region are reuseForbiddenZapValue,
        // we flip them to reuseAllowedZapValue. This allows the next
        // addToFreeList() to add the memory region to the free list
        // (unless someone concatenates the memory region with another memory
        // region that contains reuseForbiddenZapValue.)
        for (size_t i = sizeof(FreeListEntry); i < size; i++)
            address[i] = reuseAllowedZapValue;
        ASAN_POISON_MEMORY_REGION(address, size);
        // Don't add the memory region to the free list in this addToFreeList().
        return;
    }
    if (allowedCount != entryCount) {
        // If the memory region mixes reuseForbiddenZapValue and
        // reuseAllowedZapValue, we (conservatively) flip all the values
        // to reuseForbiddenZapValue. These values will be changed to
        // reuseAllowedZapValue in the next addToFreeList().
        for (size_t i = sizeof(FreeListEntry); i < size; i++)
            address[i] = reuseForbiddenZapValue;
        ASAN_POISON_MEMORY_REGION(address, size);
        // Don't add the memory region to the free list in this addToFreeList().
        return;
    }
    // We reach here only when all the values in the memory region are
    // reuseAllowedZapValue. In this case, we are allowed to add the memory
    // region to the free list and reuse it for another object.
#endif
    ASAN_POISON_MEMORY_REGION(address, size);

    int index = bucketIndexForSize(size);
    entry->link(&m_freeLists[index]);
    if (index > m_biggestFreeListIndex)
        m_biggestFreeListIndex = index;
}

#if ENABLE(ASSERT) || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
NO_SANITIZE_ADDRESS
NO_SANITIZE_MEMORY
void NEVER_INLINE FreeList::zapFreedMemory(Address address, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        // See the comment in addToFreeList().
        if (address[i] != reuseAllowedZapValue)
            address[i] = reuseForbiddenZapValue;
    }
}

void NEVER_INLINE FreeList::checkFreedMemoryIsZapped(Address address, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        ASSERT(address[i] == reuseAllowedZapValue || address[i] == reuseForbiddenZapValue);
    }
}
#endif

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

bool FreeList::takeSnapshot(const String& dumpBaseName)
{
    bool didDumpBucketStats = false;
    for (size_t i = 0; i < blinkPageSizeLog2; ++i) {
        size_t entryCount = 0;
        size_t freeSize = 0;
        for (FreeListEntry* entry = m_freeLists[i]; entry; entry = entry->next()) {
            ++entryCount;
            freeSize += entry->size();
        }

        String dumpName = dumpBaseName + String::format("/buckets/bucket_%lu", static_cast<unsigned long>(1 << i));
        WebMemoryAllocatorDump* bucketDump = BlinkGCMemoryDumpProvider::instance()->createMemoryAllocatorDumpForCurrentGC(dumpName);
        bucketDump->addScalar("free_count", "objects", entryCount);
        bucketDump->addScalar("free_size", "bytes", freeSize);
        didDumpBucketStats = true;
    }
    return didDumpBucketStats;
}

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
    , m_objectStartBitMapComputed(false)
{
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
            ASSERT(header->checkHeader());
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

#if !ENABLE(ASSERT) && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
static void discardPages(Address begin, Address end)
{
    uintptr_t beginAddress = WTF::roundUpToSystemPage(reinterpret_cast<uintptr_t>(begin));
    uintptr_t endAddress = WTF::roundDownToSystemPage(reinterpret_cast<uintptr_t>(end));
    if (beginAddress < endAddress)
        WTF::discardSystemPages(reinterpret_cast<void*>(beginAddress), endAddress - beginAddress);
}
#endif

void NormalPage::sweep()
{
    size_t markedObjectSize = 0;
    Address startOfGap = payload();
    for (Address headerAddress = startOfGap; headerAddress < payloadEnd(); ) {
        HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(headerAddress);
        size_t size = header->size();
        ASSERT(size > 0);
        ASSERT(size < blinkPagePayloadSize());

        if (header->isPromptlyFreed())
            heapForNormalPage()->decreasePromptlyFreedSize(size);
        if (header->isFree()) {
            // Zero the memory in the free list header to maintain the
            // invariant that memory on the free list is zero filled.
            // The rest of the memory is already on the free list and is
            // therefore already zero filled.
            SET_MEMORY_INACCESSIBLE(headerAddress, size < sizeof(FreeListEntry) ? size : sizeof(FreeListEntry));
            CHECK_MEMORY_INACCESSIBLE(headerAddress, size);
            headerAddress += size;
            continue;
        }
        ASSERT(header->checkHeader());

        if (!header->isMarked()) {
            // This is a fast version of header->payloadSize().
            size_t payloadSize = size - sizeof(HeapObjectHeader);
            Address payload = header->payload();
            // For ASan, unpoison the object before calling the finalizer. The
            // finalized object will be zero-filled and poison'ed afterwards.
            // Given all other unmarked objects are poisoned, ASan will detect
            // an error if the finalizer touches any other on-heap object that
            // die at the same GC cycle.
            ASAN_UNPOISON_MEMORY_REGION(payload, payloadSize);
            header->finalize(payload, payloadSize);
            // This memory will be added to the freelist. Maintain the invariant
            // that memory on the freelist is zero filled.
            SET_MEMORY_INACCESSIBLE(headerAddress, size);
            headerAddress += size;
            continue;
        }
        if (startOfGap != headerAddress) {
            heapForNormalPage()->addToFreeList(startOfGap, headerAddress - startOfGap);
#if !ENABLE(ASSERT) && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
            // Discarding pages increases page faults and may regress performance.
            // So we enable this only on low-RAM devices.
            if (Heap::isLowEndDevice())
                discardPages(startOfGap + sizeof(FreeListEntry), headerAddress);
#endif
        }
        header->unmark();
        headerAddress += size;
        markedObjectSize += size;
        startOfGap = headerAddress;
    }
    if (startOfGap != payloadEnd()) {
        heapForNormalPage()->addToFreeList(startOfGap, payloadEnd() - startOfGap);
#if !ENABLE(ASSERT) && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
        if (Heap::isLowEndDevice())
            discardPages(startOfGap + sizeof(FreeListEntry), payloadEnd());
#endif
    }

    if (markedObjectSize)
        Heap::increaseMarkedObjectSize(markedObjectSize);
}

void NormalPage::makeConsistentForGC()
{
    size_t markedObjectSize = 0;
    for (Address headerAddress = payload(); headerAddress < payloadEnd();) {
        HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(headerAddress);
        ASSERT(header->size() < blinkPagePayloadSize());
        // Check if a free list entry first since we cannot call
        // isMarked on a free list entry.
        if (header->isFree()) {
            headerAddress += header->size();
            continue;
        }
        ASSERT(header->checkHeader());
        if (header->isMarked()) {
            header->unmark();
            markedObjectSize += header->size();
        } else {
            header->markDead();
        }
        headerAddress += header->size();
    }
    if (markedObjectSize)
        Heap::increaseMarkedObjectSize(markedObjectSize);
}

void NormalPage::makeConsistentForMutator()
{
    Address startOfGap = payload();
    for (Address headerAddress = payload(); headerAddress < payloadEnd();) {
        HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(headerAddress);
        size_t size = header->size();
        ASSERT(size < blinkPagePayloadSize());
        if (header->isPromptlyFreed())
            heapForNormalPage()->decreasePromptlyFreedSize(size);
        if (header->isFree()) {
            // Zero the memory in the free list header to maintain the
            // invariant that memory on the free list is zero filled.
            // The rest of the memory is already on the free list and is
            // therefore already zero filled.
            SET_MEMORY_INACCESSIBLE(headerAddress, size < sizeof(FreeListEntry) ? size : sizeof(FreeListEntry));
            CHECK_MEMORY_INACCESSIBLE(headerAddress, size);
            headerAddress += size;
            continue;
        }
        ASSERT(header->checkHeader());

        if (startOfGap != headerAddress)
            heapForNormalPage()->addToFreeList(startOfGap, headerAddress - startOfGap);
        if (header->isMarked())
            header->unmark();
        headerAddress += size;
        startOfGap = headerAddress;
        ASSERT(headerAddress <= payloadEnd());
    }
    if (startOfGap != payloadEnd())
        heapForNormalPage()->addToFreeList(startOfGap, payloadEnd() - startOfGap);
}

#if defined(ADDRESS_SANITIZER)
void NormalPage::poisonObjects(BlinkGC::ObjectsToPoison objectsToPoison, BlinkGC::Poisoning poisoning)
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
        ASSERT(header->checkHeader());
        if (objectsToPoison == BlinkGC::MarkedAndUnmarked || !header->isMarked()) {
            if (poisoning == BlinkGC::SetPoison)
                ASAN_POISON_MEMORY_REGION(header->payload(), header->payloadSize());
            else
                ASAN_UNPOISON_MEMORY_REGION(header->payload(), header->payloadSize());
        }
        headerAddress += header->size();
    }
}
#endif

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
    if (!m_objectStartBitMapComputed)
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
    ASSERT(header->checkHeader());
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
    ASSERT(header->checkHeader());
    const GCInfo* gcInfo = Heap::gcInfo(header->gcInfoIndex());
    if (gcInfo->hasVTable() && !vTableInitialized(header->payload())) {
        // We hit this branch when a GC strikes before GarbageCollected<>'s
        // constructor runs.
        //
        // class A : public GarbageCollected<A> { virtual void f() = 0; };
        // class B : public A {
        //   B() : A(foo()) { };
        // };
        //
        // If foo() allocates something and triggers a GC, the vtable of A
        // has not yet been initialized. In this case, we should mark the A
        // object without tracing any member of the A object.
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
    OrphanedPagePool::asanDisabledMemset(payload(), OrphanedPagePool::orphanedZapValue, payloadSize());
    BasePage::markOrphaned();
}

void NormalPage::takeSnapshot(String dumpName, size_t pageIndex, ThreadState::GCSnapshotInfo& info, size_t* outFreeSize, size_t* outFreeCount)
{
    dumpName.append(String::format("/pages/page_%lu", static_cast<unsigned long>(pageIndex)));
    WebMemoryAllocatorDump* pageDump = BlinkGCMemoryDumpProvider::instance()->createMemoryAllocatorDumpForCurrentGC(dumpName);

    HeapObjectHeader* header = nullptr;
    size_t liveCount = 0;
    size_t deadCount = 0;
    size_t freeCount = 0;
    size_t liveSize = 0;
    size_t deadSize = 0;
    size_t freeSize = 0;
    for (Address headerAddress = payload(); headerAddress < payloadEnd(); headerAddress += header->size()) {
        header = reinterpret_cast<HeapObjectHeader*>(headerAddress);
        if (header->isFree()) {
            freeCount++;
            freeSize += header->size();
        } else if (header->isMarked()) {
            liveCount++;
            liveSize += header->size();

            size_t gcInfoIndex = header->gcInfoIndex();
            info.liveCount[gcInfoIndex]++;
            info.liveSize[gcInfoIndex] += header->size();
        } else {
            deadCount++;
            deadSize += header->size();

            size_t gcInfoIndex = header->gcInfoIndex();
            info.deadCount[gcInfoIndex]++;
            info.deadSize[gcInfoIndex] += header->size();
        }
    }

    pageDump->addScalar("live_count", "objects", liveCount);
    pageDump->addScalar("dead_count", "objects", deadCount);
    pageDump->addScalar("free_count", "objects", freeCount);
    pageDump->addScalar("live_size", "bytes", liveSize);
    pageDump->addScalar("dead_size", "bytes", deadSize);
    pageDump->addScalar("free_size", "bytes", freeSize);
    *outFreeSize = freeSize;
    *outFreeCount = freeCount;
}

#if ENABLE(ASSERT)
bool NormalPage::contains(Address addr)
{
    Address blinkPageStart = roundToBlinkPageStart(address());
    ASSERT(blinkPageStart == address() - blinkGuardPageSize); // Page is at aligned address plus guard page size.
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
    heapObjectHeader()->unmark();
    Heap::increaseMarkedObjectSize(size());
}

void LargeObjectPage::makeConsistentForGC()
{
    HeapObjectHeader* header = heapObjectHeader();
    if (header->isMarked()) {
        header->unmark();
        Heap::increaseMarkedObjectSize(size());
    } else {
        header->markDead();
    }
}

void LargeObjectPage::makeConsistentForMutator()
{
    HeapObjectHeader* header = heapObjectHeader();
    if (header->isMarked())
        header->unmark();
}

#if defined(ADDRESS_SANITIZER)
void LargeObjectPage::poisonObjects(BlinkGC::ObjectsToPoison objectsToPoison, BlinkGC::Poisoning poisoning)
{
    HeapObjectHeader* header = heapObjectHeader();
    if (objectsToPoison == BlinkGC::MarkedAndUnmarked || !header->isMarked()) {
        if (poisoning == BlinkGC::SetPoison)
            ASAN_POISON_MEMORY_REGION(header->payload(), header->payloadSize());
        else
            ASAN_UNPOISON_MEMORY_REGION(header->payload(), header->payloadSize());
    }
}
#endif

void LargeObjectPage::checkAndMarkPointer(Visitor* visitor, Address address)
{
    ASSERT(contains(address));
    if (!containedInObjectPayload(address) || heapObjectHeader()->isDead())
        return;
    markPointer(visitor, heapObjectHeader());
}

void LargeObjectPage::markOrphaned()
{
    // Zap the payload with a recognizable value to detect any incorrect
    // cross thread pointer usage.
    OrphanedPagePool::asanDisabledMemset(payload(), OrphanedPagePool::orphanedZapValue, payloadSize());
    BasePage::markOrphaned();
}

void LargeObjectPage::takeSnapshot(String dumpName, size_t pageIndex, ThreadState::GCSnapshotInfo& info, size_t* outFreeSize, size_t* outFreeCount)
{
    dumpName.append(String::format("/pages/page_%lu", static_cast<unsigned long>(pageIndex)));
    WebMemoryAllocatorDump* pageDump = BlinkGCMemoryDumpProvider::instance()->createMemoryAllocatorDumpForCurrentGC(dumpName);

    size_t liveSize = 0;
    size_t deadSize = 0;
    size_t liveCount = 0;
    size_t deadCount = 0;
    HeapObjectHeader* header = heapObjectHeader();
    size_t gcInfoIndex = header->gcInfoIndex();
    size_t payloadSize = header->payloadSize();
    if (header->isMarked()) {
        liveCount = 1;
        liveSize += payloadSize;
        info.liveCount[gcInfoIndex]++;
        info.liveSize[gcInfoIndex] += payloadSize;
    } else {
        deadCount = 1;
        deadSize += payloadSize;
        info.deadCount[gcInfoIndex]++;
        info.deadSize[gcInfoIndex] += payloadSize;
    }

    pageDump->addScalar("live_count", "objects", liveCount);
    pageDump->addScalar("dead_count", "objects", deadCount);
    pageDump->addScalar("live_size", "bytes", liveSize);
    pageDump->addScalar("dead_size", "bytes", deadSize);
}

#if ENABLE(ASSERT)
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

} // namespace blink
