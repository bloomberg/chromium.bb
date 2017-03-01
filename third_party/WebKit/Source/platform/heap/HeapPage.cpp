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

#include "base/trace_event/process_memory_dump.h"
#include "platform/MemoryCoordinator.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/heap/BlinkGCMemoryDumpProvider.h"
#include "platform/heap/CallbackStack.h"
#include "platform/heap/HeapCompact.h"
#include "platform/heap/PageMemory.h"
#include "platform/heap/PagePool.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/ThreadState.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "platform/instrumentation/tracing/web_process_memory_dump.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/AutoReset.h"
#include "wtf/ContainerAnnotations.h"
#include "wtf/CurrentTime.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/allocator/Partitions.h"

#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
// FIXME: have ContainerAnnotations.h define an ENABLE_-style name instead.
#define ENABLE_ASAN_CONTAINER_ANNOTATIONS 1

// When finalizing a non-inlined vector backing store/container, remove
// its contiguous container annotation. Required as it will not be destructed
// from its Vector.
#define ASAN_RETIRE_CONTAINER_ANNOTATION(object, objectSize)          \
  do {                                                                \
    BasePage* page = pageFromObject(object);                          \
    ASSERT(page);                                                     \
    bool isContainer =                                                \
        ThreadState::isVectorArenaIndex(page->arena()->arenaIndex()); \
    if (!isContainer && page->isLargeObjectPage())                    \
      isContainer =                                                   \
          static_cast<LargeObjectPage*>(page)->isVectorBackingPage(); \
    if (isContainer)                                                  \
      ANNOTATE_DELETE_BUFFER(object, objectSize, 0);                  \
  } while (0)

// A vector backing store represented by a large object is marked
// so that when it is finalized, its ASan annotation will be
// correctly retired.
#define ASAN_MARK_LARGE_VECTOR_CONTAINER(arena, largeObject)            \
  if (ThreadState::isVectorArenaIndex(arena->arenaIndex())) {           \
    BasePage* largePage = pageFromObject(largeObject);                  \
    ASSERT(largePage->isLargeObjectPage());                             \
    static_cast<LargeObjectPage*>(largePage)->setIsVectorBackingPage(); \
  }
#else
#define ENABLE_ASAN_CONTAINER_ANNOTATIONS 0
#define ASAN_RETIRE_CONTAINER_ANNOTATION(payload, payloadSize)
#define ASAN_MARK_LARGE_VECTOR_CONTAINER(arena, largeObject)
#endif

namespace blink {

#if DCHECK_IS_ON() && CPU(64BIT)
NO_SANITIZE_ADDRESS
void HeapObjectHeader::zapMagic() {
  ASSERT(checkHeader());
  m_magic = zappedMagic;
}
#endif

void HeapObjectHeader::finalize(Address object, size_t objectSize) {
  HeapAllocHooks::freeHookIfEnabled(object);
  const GCInfo* gcInfo = ThreadHeap::gcInfo(gcInfoIndex());
  if (gcInfo->hasFinalizer())
    gcInfo->m_finalize(object);

  ASAN_RETIRE_CONTAINER_ANNOTATION(object, objectSize);
}

BaseArena::BaseArena(ThreadState* state, int index)
    : m_firstPage(nullptr),
      m_firstUnsweptPage(nullptr),
      m_threadState(state),
      m_index(index) {}

BaseArena::~BaseArena() {
  ASSERT(!m_firstPage);
  ASSERT(!m_firstUnsweptPage);
}

void BaseArena::removeAllPages() {
  clearFreeLists();

  ASSERT(!m_firstUnsweptPage);
  while (m_firstPage) {
    BasePage* page = m_firstPage;
    page->unlink(&m_firstPage);
    page->removeFromHeap();
  }
}

void BaseArena::takeSnapshot(const String& dumpBaseName,
                             ThreadState::GCSnapshotInfo& info) {
  // |dumpBaseName| at this point is "blink_gc/thread_X/heaps/HeapName"
  base::trace_event::MemoryAllocatorDump* allocatorDump =
      BlinkGCMemoryDumpProvider::instance()
          ->createMemoryAllocatorDumpForCurrentGC(dumpBaseName);
  size_t pageCount = 0;
  BasePage::HeapSnapshotInfo heapInfo;
  for (BasePage* page = m_firstUnsweptPage; page; page = page->next()) {
    String dumpName =
        dumpBaseName + String::format("/pages/page_%lu",
                                      static_cast<unsigned long>(pageCount++));
    base::trace_event::MemoryAllocatorDump* pageDump =
        BlinkGCMemoryDumpProvider::instance()
            ->createMemoryAllocatorDumpForCurrentGC(dumpName);

    page->takeSnapshot(pageDump, info, heapInfo);
  }
  allocatorDump->AddScalar("blink_page_count", "objects", pageCount);

  // When taking a full dump (w/ freelist), both the /buckets and /pages
  // report their free size but they are not meant to be added together.
  // Therefore, here we override the free_size of the parent heap to be
  // equal to the free_size of the sum of its heap pages.
  allocatorDump->AddScalar("free_size", "bytes", heapInfo.freeSize);
  allocatorDump->AddScalar("free_count", "objects", heapInfo.freeCount);
}

#if DCHECK_IS_ON()
BasePage* BaseArena::findPageFromAddress(Address address) {
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

void BaseArena::makeConsistentForGC() {
  clearFreeLists();
  ASSERT(isConsistentForGC());
  for (BasePage* page = m_firstPage; page; page = page->next()) {
    page->markAsUnswept();
    page->invalidateObjectStartBitmap();
  }

  // We should not start a new GC until we finish sweeping in the current GC.
  CHECK(!m_firstUnsweptPage);

  HeapCompact* heapCompactor = getThreadState()->heap().compaction();
  if (!heapCompactor->isCompactingArena(arenaIndex()))
    return;

  BasePage* nextPage = m_firstPage;
  while (nextPage) {
    if (!nextPage->isLargeObjectPage())
      heapCompactor->addCompactingPage(nextPage);
    nextPage = nextPage->next();
  }
}

void BaseArena::makeConsistentForMutator() {
  clearFreeLists();
  ASSERT(isConsistentForGC());
  ASSERT(!m_firstPage);

  // Drop marks from marked objects and rebuild free lists in preparation for
  // resuming the executions of mutators.
  BasePage* previousPage = nullptr;
  for (BasePage *page = m_firstUnsweptPage; page;
       previousPage = page, page = page->next()) {
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

size_t BaseArena::objectPayloadSizeForTesting() {
  ASSERT(isConsistentForGC());
  ASSERT(!m_firstUnsweptPage);

  size_t objectPayloadSize = 0;
  for (BasePage* page = m_firstPage; page; page = page->next())
    objectPayloadSize += page->objectPayloadSizeForTesting();
  return objectPayloadSize;
}

void BaseArena::prepareForSweep() {
  ASSERT(getThreadState()->isInGC());
  ASSERT(!m_firstUnsweptPage);

  // Move all pages to a list of unswept pages.
  m_firstUnsweptPage = m_firstPage;
  m_firstPage = nullptr;
}

#if defined(ADDRESS_SANITIZER)
void BaseArena::poisonArena() {
  for (BasePage* page = m_firstUnsweptPage; page; page = page->next())
    page->poisonUnmarkedObjects();
}
#endif

Address BaseArena::lazySweep(size_t allocationSize, size_t gcInfoIndex) {
  // If there are no pages to be swept, return immediately.
  if (!m_firstUnsweptPage)
    return nullptr;

  RELEASE_ASSERT(getThreadState()->isSweepingInProgress());

  // lazySweepPages() can be called recursively if finalizers invoked in
  // page->sweep() allocate memory and the allocation triggers
  // lazySweepPages(). This check prevents the sweeping from being executed
  // recursively.
  if (getThreadState()->sweepForbidden())
    return nullptr;

  TRACE_EVENT0("blink_gc", "BaseArena::lazySweepPages");
  ThreadState::SweepForbiddenScope sweepForbidden(getThreadState());
  ScriptForbiddenIfMainThreadScope scriptForbidden;

  double startTime = WTF::currentTimeMS();
  Address result = lazySweepPages(allocationSize, gcInfoIndex);
  getThreadState()->accumulateSweepingTime(WTF::currentTimeMS() - startTime);
  ThreadHeap::reportMemoryUsageForTracing();

  return result;
}

void BaseArena::sweepUnsweptPage() {
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

bool BaseArena::lazySweepWithDeadline(double deadlineSeconds) {
  // It might be heavy to call
  // Platform::current()->monotonicallyIncreasingTimeSeconds() per page (i.e.,
  // 128 KB sweep or one LargeObject sweep), so we check the deadline per 10
  // pages.
  static const int deadlineCheckInterval = 10;

  RELEASE_ASSERT(getThreadState()->isSweepingInProgress());
  ASSERT(getThreadState()->sweepForbidden());
  ASSERT(!getThreadState()->isMainThread() ||
         ScriptForbiddenScope::isScriptForbidden());

  NormalPageArena* normalArena = nullptr;
  if (m_firstUnsweptPage && !m_firstUnsweptPage->isLargeObjectPage()) {
    // Mark this NormalPageArena as being lazily swept.
    NormalPage* normalPage = reinterpret_cast<NormalPage*>(m_firstUnsweptPage);
    normalArena = normalPage->arenaForNormalPage();
    normalArena->setIsLazySweeping(true);
  }
  int pageCount = 1;
  while (m_firstUnsweptPage) {
    sweepUnsweptPage();
    if (pageCount % deadlineCheckInterval == 0) {
      if (deadlineSeconds <= monotonicallyIncreasingTime()) {
        // Deadline has come.
        ThreadHeap::reportMemoryUsageForTracing();
        if (normalArena)
          normalArena->setIsLazySweeping(false);
        return !m_firstUnsweptPage;
      }
    }
    pageCount++;
  }
  ThreadHeap::reportMemoryUsageForTracing();
  if (normalArena)
    normalArena->setIsLazySweeping(false);
  return true;
}

void BaseArena::completeSweep() {
  RELEASE_ASSERT(getThreadState()->isSweepingInProgress());
  ASSERT(getThreadState()->sweepForbidden());
  ASSERT(!getThreadState()->isMainThread() ||
         ScriptForbiddenScope::isScriptForbidden());

  while (m_firstUnsweptPage) {
    sweepUnsweptPage();
  }
  ThreadHeap::reportMemoryUsageForTracing();
}

Address BaseArena::allocateLargeObject(size_t allocationSize,
                                       size_t gcInfoIndex) {
  // TODO(sof): should need arise, support eagerly finalized large objects.
  CHECK(arenaIndex() != BlinkGC::EagerSweepArenaIndex);
  LargeObjectArena* largeObjectArena = static_cast<LargeObjectArena*>(
      getThreadState()->arena(BlinkGC::LargeObjectArenaIndex));
  Address largeObject =
      largeObjectArena->allocateLargeObjectPage(allocationSize, gcInfoIndex);
  ASAN_MARK_LARGE_VECTOR_CONTAINER(this, largeObject);
  return largeObject;
}

bool BaseArena::willObjectBeLazilySwept(BasePage* page,
                                        void* objectPointer) const {
  // If not on the current page being (potentially) lazily swept,
  // |objectPointer| is an unmarked, sweepable object.
  if (page != m_firstUnsweptPage)
    return true;

  DCHECK(!page->isLargeObjectPage());
  // Check if the arena is currently being lazily swept.
  NormalPage* normalPage = reinterpret_cast<NormalPage*>(page);
  NormalPageArena* normalArena = normalPage->arenaForNormalPage();
  if (!normalArena->isLazySweeping())
    return true;

  // Rare special case: unmarked object is on the page being lazily swept,
  // and a finalizer for an object on that page calls
  // ThreadHeap::willObjectBeLazilySwept().
  //
  // Need to determine if |objectPointer| represents a live (unmarked) object or
  // an unmarked object that will be lazily swept later. As lazy page sweeping
  // doesn't record a frontier pointer representing how far along it is, the
  // page is scanned from the start, skipping past freed & unmarked regions.
  //
  // If no marked objects are encountered before |objectPointer|, we know that
  // the finalizing object calling willObjectBeLazilySwept() comes later, and
  // |objectPointer| has been deemed to be alive already (=> it won't be swept.)
  //
  // If a marked object is encountered before |objectPointer|, it will
  // not have been lazily swept past already. Hence it represents an unmarked,
  // sweepable object.
  //
  // As willObjectBeLazilySwept() is used rarely and it happening to be
  // used while runnning a finalizer on the page being lazily swept is
  // even rarer, the page scan is considered acceptable and something
  // really wanted -- willObjectBeLazilySwept()'s result can be trusted.
  Address pageEnd = normalPage->payloadEnd();
  for (Address headerAddress = normalPage->payload();
       headerAddress < pageEnd;) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(headerAddress);
    size_t size = header->size();
    // Scan made it to |objectPointer| without encountering any marked objects.
    //  => lazy sweep will have processed this unmarked, but live, object.
    //  => |objectPointer| will not be lazily swept.
    //
    // Notice that |objectPointer| might be pointer to a GarbageCollectedMixin,
    // hence using fromPayload() to derive the HeapObjectHeader isn't possible
    // (and use its value to check if |headerAddress| is equal to it.)
    if (headerAddress > objectPointer)
      return false;
    if (!header->isFree() && header->isMarked()) {
      // There must be a marked object on this page and the one located must
      // have room after it for the unmarked |objectPointer| object.
      DCHECK(headerAddress + size < pageEnd);
      return true;
    }
    headerAddress += size;
  }
  NOTREACHED();
  return true;
}

NormalPageArena::NormalPageArena(ThreadState* state, int index)
    : BaseArena(state, index),
      m_currentAllocationPoint(nullptr),
      m_remainingAllocationSize(0),
      m_lastRemainingAllocationSize(0),
      m_promptlyFreedSize(0),
      m_isLazySweeping(false) {
  clearFreeLists();
}

void NormalPageArena::clearFreeLists() {
  setAllocationPoint(nullptr, 0);
  m_freeList.clear();
}

size_t NormalPageArena::arenaSize() {
  size_t size = 0;
  BasePage* page = m_firstPage;
  while (page) {
    size += page->size();
    page = page->next();
  }
  LOG_HEAP_FREELIST_VERBOSE("Heap size: %zu (%d)\n", size, arenaIndex());
  return size;
}

size_t NormalPageArena::freeListSize() {
  size_t freeSize = m_freeList.freeListSize();
  LOG_HEAP_FREELIST_VERBOSE("Free size: %zu (%d)\n", freeSize, arenaIndex());
  return freeSize;
}

void NormalPageArena::sweepAndCompact() {
  ThreadHeap& heap = getThreadState()->heap();
  if (!heap.compaction()->isCompactingArena(arenaIndex()))
    return;

  if (!m_firstUnsweptPage) {
    heap.compaction()->finishedArenaCompaction(this, 0, 0);
    return;
  }

  // Compaction is performed in-place, sliding objects down over unused
  // holes for a smaller heap page footprint and improved locality.
  // A "compaction pointer" is consequently kept, pointing to the next
  // available address to move objects down to. It will belong to one
  // of the already sweep-compacted pages for this arena, but as compaction
  // proceeds, it will not belong to the same page as the one being
  // currently compacted.
  //
  // The compaction pointer is represented by the
  // |(currentPage, allocationPoint)| pair, with |allocationPoint|
  // being the offset into |currentPage|, making up the next
  // available location. When the compaction of an arena page causes the
  // compaction pointer to exhaust the current page it is compacting into,
  // page compaction will advance the current page of the compaction
  // pointer, as well as the allocation point.
  //
  // By construction, the page compaction can be performed without having
  // to allocate any new pages. So to arrange for the page compaction's
  // supply of freed, available pages, we chain them together after each
  // has been "compacted from". The page compaction will then reuse those
  // as needed, and once finished, the chained, available pages can be
  // released back to the OS.
  //
  // To ease the passing of the compaction state when iterating over an
  // arena's pages, package it up into a |CompactionContext|.
  NormalPage::CompactionContext context;
  context.m_compactedPages = &m_firstPage;

  while (m_firstUnsweptPage) {
    BasePage* page = m_firstUnsweptPage;
    if (page->isEmpty()) {
      page->unlink(&m_firstUnsweptPage);
      page->removeFromHeap();
      continue;
    }
    // Large objects do not belong to this arena.
    DCHECK(!page->isLargeObjectPage());
    NormalPage* normalPage = static_cast<NormalPage*>(page);
    normalPage->unlink(&m_firstUnsweptPage);
    normalPage->markAsSwept();
    // If not the first page, add |normalPage| onto the available pages chain.
    if (!context.m_currentPage)
      context.m_currentPage = normalPage;
    else
      normalPage->link(&context.m_availablePages);
    normalPage->sweepAndCompact(context);
  }

  size_t freedSize = 0;
  size_t freedPageCount = 0;

  DCHECK(context.m_currentPage);
  // If the current page hasn't been allocated into, add it to the available
  // list, for subsequent release below.
  size_t allocationPoint = context.m_allocationPoint;
  if (!allocationPoint) {
    context.m_currentPage->link(&context.m_availablePages);
  } else {
    NormalPage* currentPage = context.m_currentPage;
    currentPage->link(&m_firstPage);
    if (allocationPoint != currentPage->payloadSize()) {
      // Put the remainder of the page onto the free list.
      freedSize = currentPage->payloadSize() - allocationPoint;
      Address payload = currentPage->payload();
      SET_MEMORY_INACCESSIBLE(payload + allocationPoint, freedSize);
      currentPage->arenaForNormalPage()->addToFreeList(
          payload + allocationPoint, freedSize);
    }
  }

  // Return available pages to the free page pool, decommitting them from
  // the pagefile.
  BasePage* availablePages = context.m_availablePages;
  while (availablePages) {
    size_t pageSize = availablePages->size();
#if DEBUG_HEAP_COMPACTION
    if (!freedPageCount)
      LOG_HEAP_COMPACTION("Releasing:");
    LOG_HEAP_COMPACTION(" [%p, %p]", availablePages, availablePages + pageSize);
#endif
    freedSize += pageSize;
    freedPageCount++;
    BasePage* nextPage;
    availablePages->unlink(&nextPage);
#if !(DCHECK_IS_ON() || defined(LEAK_SANITIZER) || \
      defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER))
    // Clear out the page before adding it to the free page pool, which
    // decommits it. Recommitting the page must find a zeroed page later.
    // We cannot assume that the OS will hand back a zeroed page across
    // its "decommit" operation.
    //
    // If in a debug setting, the unused page contents will have been
    // zapped already; leave it in that state.
    DCHECK(!availablePages->isLargeObjectPage());
    NormalPage* unusedPage = reinterpret_cast<NormalPage*>(availablePages);
    memset(unusedPage->payload(), 0, unusedPage->payloadSize());
#endif
    availablePages->removeFromHeap();
    availablePages = static_cast<NormalPage*>(nextPage);
  }
  if (freedPageCount)
    LOG_HEAP_COMPACTION("\n");
  heap.compaction()->finishedArenaCompaction(this, freedPageCount, freedSize);
}

#if DCHECK_IS_ON()
bool NormalPageArena::isConsistentForGC() {
  // A thread heap is consistent for sweeping if none of the pages to be swept
  // contain a freelist block or the current allocation point.
  for (size_t i = 0; i < blinkPageSizeLog2; ++i) {
    for (FreeListEntry* freeListEntry = m_freeList.m_freeLists[i];
         freeListEntry; freeListEntry = freeListEntry->next()) {
      if (pagesToBeSweptContains(freeListEntry->getAddress()))
        return false;
    }
  }
  if (hasCurrentAllocationArea()) {
    if (pagesToBeSweptContains(currentAllocationPoint()))
      return false;
  }
  return true;
}

bool NormalPageArena::pagesToBeSweptContains(Address address) {
  for (BasePage* page = m_firstUnsweptPage; page; page = page->next()) {
    if (page->contains(address))
      return true;
  }
  return false;
}
#endif

void NormalPageArena::takeFreelistSnapshot(const String& dumpName) {
  if (m_freeList.takeSnapshot(dumpName)) {
    base::trace_event::MemoryAllocatorDump* bucketsDump =
        BlinkGCMemoryDumpProvider::instance()
            ->createMemoryAllocatorDumpForCurrentGC(dumpName + "/buckets");
    base::trace_event::MemoryAllocatorDump* pagesDump =
        BlinkGCMemoryDumpProvider::instance()
            ->createMemoryAllocatorDumpForCurrentGC(dumpName + "/pages");
    BlinkGCMemoryDumpProvider::instance()
        ->currentProcessMemoryDump()
        ->AddOwnershipEdge(pagesDump->guid(), bucketsDump->guid());
  }
}

void NormalPageArena::allocatePage() {
  getThreadState()->shouldFlushHeapDoesNotContainCache();
  PageMemory* pageMemory =
      getThreadState()->heap().getFreePagePool()->take(arenaIndex());

  if (!pageMemory) {
    // Allocate a memory region for blinkPagesPerRegion pages that
    // will each have the following layout.
    //
    //    [ guard os page | ... payload ... | guard os page ]
    //    ^---{ aligned to blink page size }
    PageMemoryRegion* region = PageMemoryRegion::allocateNormalPages(
        getThreadState()->heap().getRegionTree());

    // Setup the PageMemory object for each of the pages in the region.
    for (size_t i = 0; i < blinkPagesPerRegion; ++i) {
      PageMemory* memory = PageMemory::setupPageMemoryInRegion(
          region, i * blinkPageSize, blinkPagePayloadSize());
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
        getThreadState()->heap().getFreePagePool()->add(arenaIndex(), memory);
      }
    }
  }
  NormalPage* page =
      new (pageMemory->writableStart()) NormalPage(pageMemory, this);
  page->link(&m_firstPage);

  getThreadState()->heap().heapStats().increaseAllocatedSpace(page->size());
#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
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

void NormalPageArena::freePage(NormalPage* page) {
  getThreadState()->heap().heapStats().decreaseAllocatedSpace(page->size());

  PageMemory* memory = page->storage();
  page->~NormalPage();
  getThreadState()->heap().getFreePagePool()->add(arenaIndex(), memory);
}

bool NormalPageArena::coalesce() {
  // Don't coalesce arenas if there are not enough promptly freed entries
  // to be coalesced.
  //
  // FIXME: This threshold is determined just to optimize blink_perf
  // benchmarks. Coalescing is very sensitive to the threashold and
  // we need further investigations on the coalescing scheme.
  if (m_promptlyFreedSize < 1024 * 1024)
    return false;

  if (getThreadState()->sweepForbidden())
    return false;

  ASSERT(!hasCurrentAllocationArea());
  TRACE_EVENT0("blink_gc", "BaseArena::coalesce");

  // Rebuild free lists.
  m_freeList.clear();
  size_t freedSize = 0;
  for (NormalPage* page = static_cast<NormalPage*>(m_firstPage); page;
       page = static_cast<NormalPage*>(page->next())) {
    Address startOfGap = page->payload();
    for (Address headerAddress = startOfGap;
         headerAddress < page->payloadEnd();) {
      HeapObjectHeader* header =
          reinterpret_cast<HeapObjectHeader*>(headerAddress);
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
        SET_MEMORY_INACCESSIBLE(headerAddress, size < sizeof(FreeListEntry)
                                                   ? size
                                                   : sizeof(FreeListEntry));
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
  getThreadState()->decreaseAllocatedObjectSize(freedSize);
  ASSERT(m_promptlyFreedSize == freedSize);
  m_promptlyFreedSize = 0;
  return true;
}

void NormalPageArena::promptlyFreeObject(HeapObjectHeader* header) {
  ASSERT(!getThreadState()->sweepForbidden());
  ASSERT(header->checkHeader());
  Address address = reinterpret_cast<Address>(header);
  Address payload = header->payload();
  size_t size = header->size();
  size_t payloadSize = header->payloadSize();
  ASSERT(size > 0);
  ASSERT(pageFromObject(address) == findPageFromAddress(address));

  {
    ThreadState::SweepForbiddenScope forbiddenScope(getThreadState());
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

bool NormalPageArena::expandObject(HeapObjectHeader* header, size_t newSize) {
  // It's possible that Vector requests a smaller expanded size because
  // Vector::shrinkCapacity can set a capacity smaller than the actual payload
  // size.
  ASSERT(header->checkHeader());
  if (header->payloadSize() >= newSize)
    return true;
  size_t allocationSize = ThreadHeap::allocationSizeFromSize(newSize);
  ASSERT(allocationSize > header->size());
  size_t expandSize = allocationSize - header->size();
  if (isObjectAllocatedAtAllocationPoint(header) &&
      expandSize <= m_remainingAllocationSize) {
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

bool NormalPageArena::shrinkObject(HeapObjectHeader* header, size_t newSize) {
  ASSERT(header->checkHeader());
  ASSERT(header->payloadSize() > newSize);
  size_t allocationSize = ThreadHeap::allocationSizeFromSize(newSize);
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
  HeapObjectHeader* freedHeader = new (NotNull, shrinkAddress)
      HeapObjectHeader(shrinkSize, header->gcInfoIndex());
  freedHeader->markPromptlyFreed();
  ASSERT(pageFromObject(reinterpret_cast<Address>(header)) ==
         findPageFromAddress(reinterpret_cast<Address>(header)));
  m_promptlyFreedSize += shrinkSize;
  header->setSize(allocationSize);
  SET_MEMORY_INACCESSIBLE(shrinkAddress + sizeof(HeapObjectHeader),
                          shrinkSize - sizeof(HeapObjectHeader));
  return false;
}

Address NormalPageArena::lazySweepPages(size_t allocationSize,
                                        size_t gcInfoIndex) {
  ASSERT(!hasCurrentAllocationArea());
  AutoReset<bool> isLazySweeping(&m_isLazySweeping, true);
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

void NormalPageArena::setRemainingAllocationSize(
    size_t newRemainingAllocationSize) {
  m_remainingAllocationSize = newRemainingAllocationSize;

  // Sync recorded allocated-object size:
  //  - if previous alloc checkpoint is larger, allocation size has increased.
  //  - if smaller, a net reduction in size since last call to
  //  updateRemainingAllocationSize().
  if (m_lastRemainingAllocationSize > m_remainingAllocationSize)
    getThreadState()->increaseAllocatedObjectSize(
        m_lastRemainingAllocationSize - m_remainingAllocationSize);
  else if (m_lastRemainingAllocationSize != m_remainingAllocationSize)
    getThreadState()->decreaseAllocatedObjectSize(
        m_remainingAllocationSize - m_lastRemainingAllocationSize);
  m_lastRemainingAllocationSize = m_remainingAllocationSize;
}

void NormalPageArena::updateRemainingAllocationSize() {
  if (m_lastRemainingAllocationSize > remainingAllocationSize()) {
    getThreadState()->increaseAllocatedObjectSize(
        m_lastRemainingAllocationSize - remainingAllocationSize());
    m_lastRemainingAllocationSize = remainingAllocationSize();
  }
  ASSERT(m_lastRemainingAllocationSize == remainingAllocationSize());
}

void NormalPageArena::setAllocationPoint(Address point, size_t size) {
#if DCHECK_IS_ON()
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

Address NormalPageArena::outOfLineAllocate(size_t allocationSize,
                                           size_t gcInfoIndex) {
  ASSERT(allocationSize > remainingAllocationSize());
  ASSERT(allocationSize >= allocationGranularity);

  // 1. If this allocation is big enough, allocate a large object.
  if (allocationSize >= largeObjectSizeThreshold)
    return allocateLargeObject(allocationSize, gcInfoIndex);

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
  getThreadState()->completeSweep();

  // 7. Check if we should trigger a GC.
  getThreadState()->scheduleGCIfNeeded();

  // 8. Add a new page to this heap.
  allocatePage();

  // 9. Try to allocate from a free list. This allocation must succeed.
  result = allocateFromFreeList(allocationSize, gcInfoIndex);
  RELEASE_ASSERT(result);
  return result;
}

Address NormalPageArena::allocateFromFreeList(size_t allocationSize,
                                              size_t gcInfoIndex) {
  // Try reusing a block from the largest bin. The underlying reasoning
  // being that we want to amortize this slow allocation call by carving
  // off as a large a free block as possible in one go; a block that will
  // service this block and let following allocations be serviced quickly
  // by bump allocation.
  size_t bucketSize = static_cast<size_t>(1)
                      << m_freeList.m_biggestFreeListIndex;
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
      setAllocationPoint(entry->getAddress(), entry->size());
      ASSERT(hasCurrentAllocationArea());
      ASSERT(remainingAllocationSize() >= allocationSize);
      m_freeList.m_biggestFreeListIndex = index;
      return allocateObject(allocationSize, gcInfoIndex);
    }
  }
  m_freeList.m_biggestFreeListIndex = index;
  return nullptr;
}

LargeObjectArena::LargeObjectArena(ThreadState* state, int index)
    : BaseArena(state, index) {}

Address LargeObjectArena::allocateLargeObjectPage(size_t allocationSize,
                                                  size_t gcInfoIndex) {
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
  getThreadState()->completeSweep();

  // 3. Check if we should trigger a GC.
  getThreadState()->scheduleGCIfNeeded();

  return doAllocateLargeObjectPage(allocationSize, gcInfoIndex);
}

Address LargeObjectArena::doAllocateLargeObjectPage(size_t allocationSize,
                                                    size_t gcInfoIndex) {
  size_t largeObjectSize = LargeObjectPage::pageHeaderSize() + allocationSize;
// If ASan is supported we add allocationGranularity bytes to the allocated
// space and poison that to detect overflows
#if defined(ADDRESS_SANITIZER)
  largeObjectSize += allocationGranularity;
#endif

  getThreadState()->shouldFlushHeapDoesNotContainCache();
  PageMemory* pageMemory = PageMemory::allocate(
      largeObjectSize, getThreadState()->heap().getRegionTree());
  Address largeObjectAddress = pageMemory->writableStart();
  Address headerAddress =
      largeObjectAddress + LargeObjectPage::pageHeaderSize();
#if DCHECK_IS_ON()
  // Verify that the allocated PageMemory is expectedly zeroed.
  for (size_t i = 0; i < largeObjectSize; ++i)
    ASSERT(!largeObjectAddress[i]);
#endif
  ASSERT(gcInfoIndex > 0);
  HeapObjectHeader* header = new (NotNull, headerAddress)
      HeapObjectHeader(largeObjectSizeInHeader, gcInfoIndex);
  Address result = headerAddress + sizeof(*header);
  ASSERT(!(reinterpret_cast<uintptr_t>(result) & allocationMask));
  LargeObjectPage* largeObject = new (largeObjectAddress)
      LargeObjectPage(pageMemory, this, allocationSize);
  ASSERT(header->checkHeader());

  // Poison the object header and allocationGranularity bytes after the object
  ASAN_POISON_MEMORY_REGION(header, sizeof(*header));
  ASAN_POISON_MEMORY_REGION(largeObject->getAddress() + largeObject->size(),
                            allocationGranularity);

  largeObject->link(&m_firstPage);

  getThreadState()->heap().heapStats().increaseAllocatedSpace(
      largeObject->size());
  getThreadState()->increaseAllocatedObjectSize(largeObject->size());
  return result;
}

void LargeObjectArena::freeLargeObjectPage(LargeObjectPage* object) {
  ASAN_UNPOISON_MEMORY_REGION(object->payload(), object->payloadSize());
  object->heapObjectHeader()->finalize(object->payload(),
                                       object->payloadSize());
  getThreadState()->heap().heapStats().decreaseAllocatedSpace(object->size());

  // Unpoison the object header and allocationGranularity bytes after the
  // object before freeing.
  ASAN_UNPOISON_MEMORY_REGION(object->heapObjectHeader(),
                              sizeof(HeapObjectHeader));
  ASAN_UNPOISON_MEMORY_REGION(object->getAddress() + object->size(),
                              allocationGranularity);

  PageMemory* memory = object->storage();
  object->~LargeObjectPage();
  delete memory;
}

Address LargeObjectArena::lazySweepPages(size_t allocationSize,
                                         size_t gcInfoIndex) {
  Address result = nullptr;
  size_t sweptSize = 0;
  while (m_firstUnsweptPage) {
    BasePage* page = m_firstUnsweptPage;
    if (page->isEmpty()) {
      sweptSize += static_cast<LargeObjectPage*>(page)->payloadSize() +
                   sizeof(HeapObjectHeader);
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

FreeList::FreeList() : m_biggestFreeListIndex(0) {}

void FreeList::addToFreeList(Address address, size_t size) {
  ASSERT(size < blinkPagePayloadSize());
  // The free list entries are only pointer aligned (but when we allocate
  // from them we are 8 byte aligned due to the header size).
  ASSERT(!((reinterpret_cast<uintptr_t>(address) + sizeof(HeapObjectHeader)) &
           allocationMask));
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

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
  // The following logic delays reusing free lists for (at least) one GC
  // cycle or coalescing. This is helpful to detect use-after-free errors
  // that could be caused by lazy sweeping etc.
  size_t allowedCount = 0;
  size_t forbiddenCount = 0;
  getAllowedAndForbiddenCounts(address, size, allowedCount, forbiddenCount);
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

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
NO_SANITIZE_MEMORY
void NEVER_INLINE
FreeList::getAllowedAndForbiddenCounts(Address address,
                                       size_t size,
                                       size_t& allowedCount,
                                       size_t& forbiddenCount) {
  for (size_t i = sizeof(FreeListEntry); i < size; i++) {
    if (address[i] == reuseAllowedZapValue)
      allowedCount++;
    else if (address[i] == reuseForbiddenZapValue)
      forbiddenCount++;
    else
      NOTREACHED();
  }
}

NO_SANITIZE_ADDRESS
NO_SANITIZE_MEMORY
void NEVER_INLINE FreeList::zapFreedMemory(Address address, size_t size) {
  for (size_t i = 0; i < size; i++) {
    // See the comment in addToFreeList().
    if (address[i] != reuseAllowedZapValue)
      address[i] = reuseForbiddenZapValue;
  }
}

void NEVER_INLINE FreeList::checkFreedMemoryIsZapped(Address address,
                                                     size_t size) {
  for (size_t i = 0; i < size; i++) {
    ASSERT(address[i] == reuseAllowedZapValue ||
           address[i] == reuseForbiddenZapValue);
  }
}
#endif

size_t FreeList::freeListSize() const {
  size_t freeSize = 0;
  for (unsigned i = 0; i < blinkPageSizeLog2; ++i) {
    FreeListEntry* entry = m_freeLists[i];
    while (entry) {
      freeSize += entry->size();
      entry = entry->next();
    }
  }
#if DEBUG_HEAP_FREELIST
  if (freeSize) {
    LOG_HEAP_FREELIST_VERBOSE("FreeList(%p): %zu\n", this, freeSize);
    for (unsigned i = 0; i < blinkPageSizeLog2; ++i) {
      FreeListEntry* entry = m_freeLists[i];
      size_t bucket = 0;
      size_t count = 0;
      while (entry) {
        bucket += entry->size();
        count++;
        entry = entry->next();
      }
      if (bucket) {
        LOG_HEAP_FREELIST_VERBOSE("[%d, %d]: %zu (%zu)\n", 0x1 << i,
                                  0x1 << (i + 1), bucket, count);
      }
    }
  }
#endif
  return freeSize;
}

void FreeList::clear() {
  m_biggestFreeListIndex = 0;
  for (size_t i = 0; i < blinkPageSizeLog2; ++i)
    m_freeLists[i] = nullptr;
}

int FreeList::bucketIndexForSize(size_t size) {
  ASSERT(size > 0);
  int index = -1;
  while (size) {
    size >>= 1;
    index++;
  }
  return index;
}

bool FreeList::takeSnapshot(const String& dumpBaseName) {
  bool didDumpBucketStats = false;
  for (size_t i = 0; i < blinkPageSizeLog2; ++i) {
    size_t entryCount = 0;
    size_t freeSize = 0;
    for (FreeListEntry* entry = m_freeLists[i]; entry; entry = entry->next()) {
      ++entryCount;
      freeSize += entry->size();
    }

    String dumpName =
        dumpBaseName + String::format("/buckets/bucket_%lu",
                                      static_cast<unsigned long>(1 << i));
    base::trace_event::MemoryAllocatorDump* bucketDump =
        BlinkGCMemoryDumpProvider::instance()
            ->createMemoryAllocatorDumpForCurrentGC(dumpName);
    bucketDump->AddScalar("free_count", "objects", entryCount);
    bucketDump->AddScalar("free_size", "bytes", freeSize);
    didDumpBucketStats = true;
  }
  return didDumpBucketStats;
}

BasePage::BasePage(PageMemory* storage, BaseArena* arena)
    : m_storage(storage),
      m_arena(arena),
      m_next(nullptr),
      m_swept(true) {
  ASSERT(isPageHeaderAddress(reinterpret_cast<Address>(this)));
}

NormalPage::NormalPage(PageMemory* storage, BaseArena* arena)
    : BasePage(storage, arena), m_objectStartBitMapComputed(false) {
  ASSERT(isPageHeaderAddress(reinterpret_cast<Address>(this)));
}

size_t NormalPage::objectPayloadSizeForTesting() {
  size_t objectPayloadSize = 0;
  Address headerAddress = payload();
  markAsSwept();
  ASSERT(headerAddress != payloadEnd());
  do {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(headerAddress);
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

bool NormalPage::isEmpty() {
  HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(payload());
  return header->isFree() && header->size() == payloadSize();
}

void NormalPage::removeFromHeap() {
  arenaForNormalPage()->freePage(this);
}

#if !DCHECK_IS_ON() && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
static void discardPages(Address begin, Address end) {
  uintptr_t beginAddress =
      WTF::RoundUpToSystemPage(reinterpret_cast<uintptr_t>(begin));
  uintptr_t endAddress =
      WTF::RoundDownToSystemPage(reinterpret_cast<uintptr_t>(end));
  if (beginAddress < endAddress)
    WTF::DiscardSystemPages(reinterpret_cast<void*>(beginAddress),
                            endAddress - beginAddress);
}
#endif

void NormalPage::sweep() {
  size_t markedObjectSize = 0;
  Address startOfGap = payload();
  NormalPageArena* pageArena = arenaForNormalPage();
  for (Address headerAddress = startOfGap; headerAddress < payloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(headerAddress);
    size_t size = header->size();
    ASSERT(size > 0);
    ASSERT(size < blinkPagePayloadSize());

    if (header->isPromptlyFreed())
      pageArena->decreasePromptlyFreedSize(size);
    if (header->isFree()) {
      // Zero the memory in the free list header to maintain the
      // invariant that memory on the free list is zero filled.
      // The rest of the memory is already on the free list and is
      // therefore already zero filled.
      SET_MEMORY_INACCESSIBLE(headerAddress, size < sizeof(FreeListEntry)
                                                 ? size
                                                 : sizeof(FreeListEntry));
      CHECK_MEMORY_INACCESSIBLE(headerAddress, size);
      headerAddress += size;
      continue;
    }
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
      pageArena->addToFreeList(startOfGap, headerAddress - startOfGap);
#if !DCHECK_IS_ON() && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
      // Discarding pages increases page faults and may regress performance.
      // So we enable this only on low-RAM devices.
      if (MemoryCoordinator::isLowEndDevice())
        discardPages(startOfGap + sizeof(FreeListEntry), headerAddress);
#endif
    }
    header->unmark();
    headerAddress += size;
    markedObjectSize += size;
    startOfGap = headerAddress;
  }
  if (startOfGap != payloadEnd()) {
    pageArena->addToFreeList(startOfGap, payloadEnd() - startOfGap);
#if !DCHECK_IS_ON() && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
    if (MemoryCoordinator::isLowEndDevice())
      discardPages(startOfGap + sizeof(FreeListEntry), payloadEnd());
#endif
  }

  if (markedObjectSize)
    pageArena->getThreadState()->increaseMarkedObjectSize(markedObjectSize);
}

void NormalPage::sweepAndCompact(CompactionContext& context) {
  NormalPage*& currentPage = context.m_currentPage;
  size_t& allocationPoint = context.m_allocationPoint;

  size_t markedObjectSize = 0;
  NormalPageArena* pageArena = arenaForNormalPage();
#if defined(ADDRESS_SANITIZER)
  bool isVectorArena = ThreadState::isVectorArenaIndex(pageArena->arenaIndex());
#endif
  HeapCompact* compact = pageArena->getThreadState()->heap().compaction();
  for (Address headerAddress = payload(); headerAddress < payloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(headerAddress);
    size_t size = header->size();
    DCHECK(size > 0 && size < blinkPagePayloadSize());

    if (header->isPromptlyFreed())
      pageArena->decreasePromptlyFreedSize(size);
    if (header->isFree()) {
      // Unpoison the freelist entry so that we
      // can compact into it as wanted.
      ASAN_UNPOISON_MEMORY_REGION(headerAddress, size);
      headerAddress += size;
      continue;
    }
    // This is a fast version of header->payloadSize().
    size_t payloadSize = size - sizeof(HeapObjectHeader);
    Address payload = header->payload();
    if (!header->isMarked()) {
      // For ASan, unpoison the object before calling the finalizer. The
      // finalized object will be zero-filled and poison'ed afterwards.
      // Given all other unmarked objects are poisoned, ASan will detect
      // an error if the finalizer touches any other on-heap object that
      // die at the same GC cycle.
      ASAN_UNPOISON_MEMORY_REGION(headerAddress, size);
      header->finalize(payload, payloadSize);

// As compaction is under way, leave the freed memory accessible
// while compacting the rest of the page. We just zap the payload
// to catch out other finalizers trying to access it.
#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
      FreeList::zapFreedMemory(payload, payloadSize);
#endif
      headerAddress += size;
      continue;
    }
    header->unmark();
    // Allocate and copy over the live object.
    Address compactFrontier = currentPage->payload() + allocationPoint;
    if (compactFrontier + size > currentPage->payloadEnd()) {
      // Can't fit on current allocation page; add remaining onto the
      // freelist and advance to next available page.
      //
      // TODO(sof): be more clever & compact later objects into
      // |currentPage|'s unused slop.
      currentPage->link(context.m_compactedPages);
      size_t freeSize = currentPage->payloadSize() - allocationPoint;
      if (freeSize) {
        SET_MEMORY_INACCESSIBLE(compactFrontier, freeSize);
        currentPage->arenaForNormalPage()->addToFreeList(compactFrontier,
                                                         freeSize);
      }

      BasePage* nextAvailablePage;
      context.m_availablePages->unlink(&nextAvailablePage);
      currentPage = reinterpret_cast<NormalPage*>(context.m_availablePages);
      context.m_availablePages = nextAvailablePage;
      allocationPoint = 0;
      compactFrontier = currentPage->payload();
    }
    if (LIKELY(compactFrontier != headerAddress)) {
#if defined(ADDRESS_SANITIZER)
      // Unpoison the header + if it is a vector backing
      // store object, let go of the container annotations.
      // Do that by unpoisoning the payload entirely.
      ASAN_UNPOISON_MEMORY_REGION(header, sizeof(HeapObjectHeader));
      if (isVectorArena)
        ASAN_UNPOISON_MEMORY_REGION(payload, payloadSize);
#endif
      // Use a non-overlapping copy, if possible.
      if (currentPage == this)
        memmove(compactFrontier, headerAddress, size);
      else
        memcpy(compactFrontier, headerAddress, size);
      compact->relocate(payload, compactFrontier + sizeof(HeapObjectHeader));
    }
    headerAddress += size;
    markedObjectSize += size;
    allocationPoint += size;
    DCHECK(allocationPoint <= currentPage->payloadSize());
  }
  if (markedObjectSize)
    pageArena->getThreadState()->increaseMarkedObjectSize(markedObjectSize);

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
  // Zap the unused portion, until it is either compacted into or freed.
  if (currentPage != this) {
    FreeList::zapFreedMemory(payload(), payloadSize());
  } else {
    FreeList::zapFreedMemory(payload() + allocationPoint,
                             payloadSize() - allocationPoint);
  }
#endif
}

void NormalPage::makeConsistentForMutator() {
  Address startOfGap = payload();
  NormalPageArena* normalArena = arenaForNormalPage();
  for (Address headerAddress = payload(); headerAddress < payloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(headerAddress);
    size_t size = header->size();
    ASSERT(size < blinkPagePayloadSize());
    if (header->isPromptlyFreed())
      arenaForNormalPage()->decreasePromptlyFreedSize(size);
    if (header->isFree()) {
      // Zero the memory in the free list header to maintain the
      // invariant that memory on the free list is zero filled.
      // The rest of the memory is already on the free list and is
      // therefore already zero filled.
      SET_MEMORY_INACCESSIBLE(headerAddress, size < sizeof(FreeListEntry)
                                                 ? size
                                                 : sizeof(FreeListEntry));
      CHECK_MEMORY_INACCESSIBLE(headerAddress, size);
      headerAddress += size;
      continue;
    }
    if (startOfGap != headerAddress)
      normalArena->addToFreeList(startOfGap, headerAddress - startOfGap);
    if (header->isMarked())
      header->unmark();
    headerAddress += size;
    startOfGap = headerAddress;
    ASSERT(headerAddress <= payloadEnd());
  }
  if (startOfGap != payloadEnd())
    normalArena->addToFreeList(startOfGap, payloadEnd() - startOfGap);
}

#if defined(ADDRESS_SANITIZER)
void NormalPage::poisonUnmarkedObjects() {
  for (Address headerAddress = payload(); headerAddress < payloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(headerAddress);
    ASSERT(header->size() < blinkPagePayloadSize());
    // Check if a free list entry first since we cannot call
    // isMarked on a free list entry.
    if (header->isFree()) {
      headerAddress += header->size();
      continue;
    }
    if (!header->isMarked())
      ASAN_POISON_MEMORY_REGION(header->payload(), header->payloadSize());
    headerAddress += header->size();
  }
}
#endif

void NormalPage::populateObjectStartBitMap() {
  memset(&m_objectStartBitMap, 0, objectStartBitMapSize);
  Address start = payload();
  for (Address headerAddress = start; headerAddress < payloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(headerAddress);
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

static int numberOfLeadingZeroes(uint8_t byte) {
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

HeapObjectHeader* NormalPage::findHeaderFromAddress(Address address) {
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

#if DCHECK_IS_ON()
static bool isUninitializedMemory(void* objectPointer, size_t objectSize) {
  // Scan through the object's fields and check that they are all zero.
  Address* objectFields = reinterpret_cast<Address*>(objectPointer);
  for (size_t i = 0; i < objectSize / sizeof(Address); ++i) {
    if (objectFields[i] != 0)
      return false;
  }
  return true;
}
#endif

static void markPointer(Visitor* visitor, HeapObjectHeader* header) {
  ASSERT(header->checkHeader());
  const GCInfo* gcInfo = ThreadHeap::gcInfo(header->gcInfoIndex());
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

void NormalPage::checkAndMarkPointer(Visitor* visitor, Address address) {
#if DCHECK_IS_ON()
  DCHECK(contains(address));
#endif
  HeapObjectHeader* header = findHeaderFromAddress(address);
  if (!header)
    return;
  markPointer(visitor, header);
}

#if DCHECK_IS_ON()
void NormalPage::checkAndMarkPointer(Visitor* visitor,
                                     Address address,
                                     MarkedPointerCallbackForTesting callback) {
  DCHECK(contains(address));
  HeapObjectHeader* header = findHeaderFromAddress(address);
  if (!header)
    return;
  if (!callback(header))
    markPointer(visitor, header);
}
#endif

void NormalPage::takeSnapshot(base::trace_event::MemoryAllocatorDump* pageDump,
                              ThreadState::GCSnapshotInfo& info,
                              HeapSnapshotInfo& heapInfo) {
  HeapObjectHeader* header = nullptr;
  size_t liveCount = 0;
  size_t deadCount = 0;
  size_t freeCount = 0;
  size_t liveSize = 0;
  size_t deadSize = 0;
  size_t freeSize = 0;
  for (Address headerAddress = payload(); headerAddress < payloadEnd();
       headerAddress += header->size()) {
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

  pageDump->AddScalar("live_count", "objects", liveCount);
  pageDump->AddScalar("dead_count", "objects", deadCount);
  pageDump->AddScalar("free_count", "objects", freeCount);
  pageDump->AddScalar("live_size", "bytes", liveSize);
  pageDump->AddScalar("dead_size", "bytes", deadSize);
  pageDump->AddScalar("free_size", "bytes", freeSize);
  heapInfo.freeSize += freeSize;
  heapInfo.freeCount += freeCount;
}

#if DCHECK_IS_ON()
bool NormalPage::contains(Address addr) {
  Address blinkPageStart = roundToBlinkPageStart(getAddress());
  // Page is at aligned address plus guard page size.
  ASSERT(blinkPageStart == getAddress() - blinkGuardPageSize);
  return blinkPageStart <= addr && addr < blinkPageStart + blinkPageSize;
}
#endif

LargeObjectPage::LargeObjectPage(PageMemory* storage,
                                 BaseArena* arena,
                                 size_t payloadSize)
    : BasePage(storage, arena),
      m_payloadSize(payloadSize)
#if ENABLE(ASAN_CONTAINER_ANNOTATIONS)
      ,
      m_isVectorBackingPage(false)
#endif
{
}

size_t LargeObjectPage::objectPayloadSizeForTesting() {
  markAsSwept();
  return payloadSize();
}

bool LargeObjectPage::isEmpty() {
  return !heapObjectHeader()->isMarked();
}

void LargeObjectPage::removeFromHeap() {
  static_cast<LargeObjectArena*>(arena())->freeLargeObjectPage(this);
}

void LargeObjectPage::sweep() {
  heapObjectHeader()->unmark();
  arena()->getThreadState()->increaseMarkedObjectSize(size());
}

void LargeObjectPage::makeConsistentForMutator() {
  HeapObjectHeader* header = heapObjectHeader();
  if (header->isMarked())
    header->unmark();
}

#if defined(ADDRESS_SANITIZER)
void LargeObjectPage::poisonUnmarkedObjects() {
  HeapObjectHeader* header = heapObjectHeader();
  if (!header->isMarked())
    ASAN_POISON_MEMORY_REGION(header->payload(), header->payloadSize());
}
#endif

void LargeObjectPage::checkAndMarkPointer(Visitor* visitor, Address address) {
#if DCHECK_IS_ON()
  DCHECK(contains(address));
#endif
  if (!containedInObjectPayload(address))
    return;
  markPointer(visitor, heapObjectHeader());
}

#if DCHECK_IS_ON()
void LargeObjectPage::checkAndMarkPointer(
    Visitor* visitor,
    Address address,
    MarkedPointerCallbackForTesting callback) {
  DCHECK(contains(address));
  if (!containedInObjectPayload(address))
    return;
  if (!callback(heapObjectHeader()))
    markPointer(visitor, heapObjectHeader());
}
#endif

void LargeObjectPage::takeSnapshot(
    base::trace_event::MemoryAllocatorDump* pageDump,
    ThreadState::GCSnapshotInfo& info,
    HeapSnapshotInfo&) {
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

  pageDump->AddScalar("live_count", "objects", liveCount);
  pageDump->AddScalar("dead_count", "objects", deadCount);
  pageDump->AddScalar("live_size", "bytes", liveSize);
  pageDump->AddScalar("dead_size", "bytes", deadSize);
}

#if DCHECK_IS_ON()
bool LargeObjectPage::contains(Address object) {
  return roundToBlinkPageStart(getAddress()) <= object &&
         object < roundToBlinkPageEnd(getAddress() + size());
}
#endif

void HeapDoesNotContainCache::flush() {
  if (m_hasEntries) {
    for (int i = 0; i < numberOfEntries; ++i)
      m_entries[i] = nullptr;
    m_hasEntries = false;
  }
}

size_t HeapDoesNotContainCache::hash(Address address) {
  size_t value = (reinterpret_cast<size_t>(address) >> blinkPageSizeLog2);
  value ^= value >> numberOfEntriesLog2;
  value ^= value >> (numberOfEntriesLog2 * 2);
  value &= numberOfEntries - 1;
  return value & ~1;  // Returns only even number.
}

bool HeapDoesNotContainCache::lookup(Address address) {
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

void HeapDoesNotContainCache::addEntry(Address address) {
  ASSERT(ThreadState::current()->isInGC());

  m_hasEntries = true;
  size_t index = hash(address);
  ASSERT(!(index & 1));
  Address cachePage = roundToBlinkPageStart(address);
  m_entries[index + 1] = m_entries[index];
  m_entries[index] = cachePage;
}

}  // namespace blink
