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

#include "platform/heap/Heap.h"

#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
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
#include "wtf/CurrentTime.h"
#include "wtf/DataLog.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/PtrUtil.h"
#include "wtf/allocator/Partitions.h"
#include <memory>

namespace blink {

HeapAllocHooks::AllocationHook* HeapAllocHooks::m_allocationHook = nullptr;
HeapAllocHooks::FreeHook* HeapAllocHooks::m_freeHook = nullptr;

void ThreadHeap::flushHeapDoesNotContainCache() {
  m_heapDoesNotContainCache->flush();
}

void ProcessHeap::init() {
  s_totalAllocatedSpace = 0;
  s_totalAllocatedObjectSize = 0;
  s_totalMarkedObjectSize = 0;

  GCInfoTable::init();
  CallbackStackMemoryPool::instance().initialize();
}

void ProcessHeap::resetHeapCounters() {
  s_totalAllocatedObjectSize = 0;
  s_totalMarkedObjectSize = 0;
}

CrossThreadPersistentRegion& ProcessHeap::crossThreadPersistentRegion() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CrossThreadPersistentRegion, persistentRegion,
                                  new CrossThreadPersistentRegion());
  return persistentRegion;
}

size_t ProcessHeap::s_totalAllocatedSpace = 0;
size_t ProcessHeap::s_totalAllocatedObjectSize = 0;
size_t ProcessHeap::s_totalMarkedObjectSize = 0;

ThreadHeapStats::ThreadHeapStats()
    : m_allocatedSpace(0),
      m_allocatedObjectSize(0),
      m_objectSizeAtLastGC(0),
      m_markedObjectSize(0),
      m_markedObjectSizeAtLastCompleteSweep(0),
      m_wrapperCount(0),
      m_wrapperCountAtLastGC(0),
      m_collectedWrapperCount(0),
      m_partitionAllocSizeAtLastGC(
          WTF::Partitions::totalSizeOfCommittedPages()),
      m_estimatedMarkingTimePerByte(0.0) {}

double ThreadHeapStats::estimatedMarkingTime() {
  // Use 8 ms as initial estimated marking time.
  // 8 ms is long enough for low-end mobile devices to mark common
  // real-world object graphs.
  if (m_estimatedMarkingTimePerByte == 0)
    return 0.008;

  // Assuming that the collection rate of this GC will be mostly equal to
  // the collection rate of the last GC, estimate the marking time of this GC.
  return m_estimatedMarkingTimePerByte *
         (allocatedObjectSize() + markedObjectSize());
}

void ThreadHeapStats::reset() {
  m_objectSizeAtLastGC = m_allocatedObjectSize + m_markedObjectSize;
  m_partitionAllocSizeAtLastGC = WTF::Partitions::totalSizeOfCommittedPages();
  m_allocatedObjectSize = 0;
  m_markedObjectSize = 0;
  m_wrapperCountAtLastGC = m_wrapperCount;
  m_collectedWrapperCount = 0;
}

void ThreadHeapStats::increaseAllocatedObjectSize(size_t delta) {
  atomicAdd(&m_allocatedObjectSize, static_cast<long>(delta));
  ProcessHeap::increaseTotalAllocatedObjectSize(delta);
}

void ThreadHeapStats::decreaseAllocatedObjectSize(size_t delta) {
  atomicSubtract(&m_allocatedObjectSize, static_cast<long>(delta));
  ProcessHeap::decreaseTotalAllocatedObjectSize(delta);
}

void ThreadHeapStats::increaseMarkedObjectSize(size_t delta) {
  atomicAdd(&m_markedObjectSize, static_cast<long>(delta));
  ProcessHeap::increaseTotalMarkedObjectSize(delta);
}

void ThreadHeapStats::increaseAllocatedSpace(size_t delta) {
  atomicAdd(&m_allocatedSpace, static_cast<long>(delta));
  ProcessHeap::increaseTotalAllocatedSpace(delta);
}

void ThreadHeapStats::decreaseAllocatedSpace(size_t delta) {
  atomicSubtract(&m_allocatedSpace, static_cast<long>(delta));
  ProcessHeap::decreaseTotalAllocatedSpace(delta);
}

ThreadHeap::ThreadHeap(ThreadState* threadState)
    : m_threadState(threadState),
      m_regionTree(WTF::makeUnique<RegionTree>()),
      m_heapDoesNotContainCache(WTF::wrapUnique(new HeapDoesNotContainCache)),
      m_freePagePool(WTF::wrapUnique(new PagePool)),
      m_markingStack(CallbackStack::create()),
      m_postMarkingCallbackStack(CallbackStack::create()),
      m_weakCallbackStack(CallbackStack::create()),
      m_ephemeronStack(CallbackStack::create()) {
  if (ThreadState::current()->isMainThread())
    s_mainThreadHeap = this;
}

ThreadHeap::~ThreadHeap() {
}

#if DCHECK_IS_ON()
BasePage* ThreadHeap::findPageFromAddress(Address address) {
  return m_threadState->findPageFromAddress(address);
}
#endif

Address ThreadHeap::checkAndMarkPointer(Visitor* visitor, Address address) {
  ASSERT(ThreadState::current()->isInGC());

#if !DCHECK_IS_ON()
  if (m_heapDoesNotContainCache->lookup(address))
    return nullptr;
#endif

  if (BasePage* page = lookupPageForAddress(address)) {
    ASSERT(page->contains(address));
    ASSERT(!m_heapDoesNotContainCache->lookup(address));
    DCHECK(&visitor->heap() == &page->arena()->getThreadState()->heap());
    page->checkAndMarkPointer(visitor, address);
    return address;
  }

#if !DCHECK_IS_ON()
  m_heapDoesNotContainCache->addEntry(address);
#else
  if (!m_heapDoesNotContainCache->lookup(address))
    m_heapDoesNotContainCache->addEntry(address);
#endif
  return nullptr;
}

#if DCHECK_IS_ON()
// To support unit testing of the marking of off-heap root references
// into the heap, provide a checkAndMarkPointer() version with an
// extra notification argument.
Address ThreadHeap::checkAndMarkPointer(
    Visitor* visitor,
    Address address,
    MarkedPointerCallbackForTesting callback) {
  DCHECK(ThreadState::current()->isInGC());

  if (BasePage* page = lookupPageForAddress(address)) {
    DCHECK(page->contains(address));
    DCHECK(!m_heapDoesNotContainCache->lookup(address));
    DCHECK(&visitor->heap() == &page->arena()->getThreadState()->heap());
    page->checkAndMarkPointer(visitor, address, callback);
    return address;
  }
  if (!m_heapDoesNotContainCache->lookup(address))
    m_heapDoesNotContainCache->addEntry(address);
  return nullptr;
}
#endif

void ThreadHeap::pushTraceCallback(void* object, TraceCallback callback) {
  ASSERT(ThreadState::current()->isInGC());

  CallbackStack::Item* slot = m_markingStack->allocateEntry();
  *slot = CallbackStack::Item(object, callback);
}

bool ThreadHeap::popAndInvokeTraceCallback(Visitor* visitor) {
  CallbackStack::Item* item = m_markingStack->pop();
  if (!item)
    return false;
  item->call(visitor);
  return true;
}

void ThreadHeap::pushPostMarkingCallback(void* object, TraceCallback callback) {
  ASSERT(ThreadState::current()->isInGC());

  CallbackStack::Item* slot = m_postMarkingCallbackStack->allocateEntry();
  *slot = CallbackStack::Item(object, callback);
}

bool ThreadHeap::popAndInvokePostMarkingCallback(Visitor* visitor) {
  if (CallbackStack::Item* item = m_postMarkingCallbackStack->pop()) {
    item->call(visitor);
    return true;
  }
  return false;
}

void ThreadHeap::pushWeakCallback(void* closure, WeakCallback callback) {
  ASSERT(ThreadState::current()->isInGC());

  CallbackStack::Item* slot = m_weakCallbackStack->allocateEntry();
  *slot = CallbackStack::Item(closure, callback);
}

bool ThreadHeap::popAndInvokeWeakCallback(Visitor* visitor) {
  if (CallbackStack::Item* item = m_weakCallbackStack->pop()) {
    item->call(visitor);
    return true;
  }
  return false;
}

void ThreadHeap::registerWeakTable(void* table,
                                   EphemeronCallback iterationCallback,
                                   EphemeronCallback iterationDoneCallback) {
  ASSERT(ThreadState::current()->isInGC());

  CallbackStack::Item* slot = m_ephemeronStack->allocateEntry();
  *slot = CallbackStack::Item(table, iterationCallback);

  // Register a post-marking callback to tell the tables that
  // ephemeron iteration is complete.
  pushPostMarkingCallback(table, iterationDoneCallback);
}

#if DCHECK_IS_ON()
bool ThreadHeap::weakTableRegistered(const void* table) {
  ASSERT(m_ephemeronStack);
  return m_ephemeronStack->hasCallbackForObject(table);
}
#endif

void ThreadHeap::commitCallbackStacks() {
  m_markingStack->commit();
  m_postMarkingCallbackStack->commit();
  m_weakCallbackStack->commit();
  m_ephemeronStack->commit();
}

HeapCompact* ThreadHeap::compaction() {
  if (!m_compaction)
    m_compaction = HeapCompact::create();
  return m_compaction.get();
}

void ThreadHeap::registerMovingObjectReference(MovableReference* slot) {
  DCHECK(slot);
  DCHECK(*slot);
  compaction()->registerMovingObjectReference(slot);
}

void ThreadHeap::registerMovingObjectCallback(MovableReference reference,
                                              MovingObjectCallback callback,
                                              void* callbackData) {
  DCHECK(reference);
  compaction()->registerMovingObjectCallback(reference, callback, callbackData);
}

void ThreadHeap::decommitCallbackStacks() {
  m_markingStack->decommit();
  m_postMarkingCallbackStack->decommit();
  m_weakCallbackStack->decommit();
  m_ephemeronStack->decommit();
}

void ThreadHeap::preGC() {
  ASSERT(!ThreadState::current()->isInGC());
  m_threadState->preGC();
}

void ThreadHeap::postGC(BlinkGC::GCType gcType) {
  ASSERT(ThreadState::current()->isInGC());
  m_threadState->postGC(gcType);
}

void ThreadHeap::preSweep(BlinkGC::GCType gcType) {
  m_threadState->preSweep(gcType);
}

void ThreadHeap::processMarkingStack(Visitor* visitor) {
  // Ephemeron fixed point loop.
  do {
    {
      // Iteratively mark all objects that are reachable from the objects
      // currently pushed onto the marking stack.
      TRACE_EVENT0("blink_gc", "ThreadHeap::processMarkingStackSingleThreaded");
      while (popAndInvokeTraceCallback(visitor)) {
      }
    }

    {
      // Mark any strong pointers that have now become reachable in
      // ephemeron maps.
      TRACE_EVENT0("blink_gc", "ThreadHeap::processEphemeronStack");
      m_ephemeronStack->invokeEphemeronCallbacks(visitor);
    }

    // Rerun loop if ephemeron processing queued more objects for tracing.
  } while (!m_markingStack->isEmpty());
}

void ThreadHeap::postMarkingProcessing(Visitor* visitor) {
  TRACE_EVENT0("blink_gc", "ThreadHeap::postMarkingProcessing");
  // Call post-marking callbacks including:
  // 1. the ephemeronIterationDone callbacks on weak tables to do cleanup
  //    (specifically to clear the queued bits for weak hash tables), and
  // 2. the markNoTracing callbacks on collection backings to mark them
  //    if they are only reachable from their front objects.
  while (popAndInvokePostMarkingCallback(visitor)) {
  }

  // Post-marking callbacks should not trace any objects and
  // therefore the marking stack should be empty after the
  // post-marking callbacks.
  ASSERT(m_markingStack->isEmpty());
}

void ThreadHeap::weakProcessing(Visitor* visitor) {
  TRACE_EVENT0("blink_gc", "ThreadHeap::weakProcessing");
  double startTime = WTF::currentTimeMS();

  // Call weak callbacks on objects that may now be pointing to dead objects.
  while (popAndInvokeWeakCallback(visitor)) {
  }

  // It is not permitted to trace pointers of live objects in the weak
  // callback phase, so the marking stack should still be empty here.
  ASSERT(m_markingStack->isEmpty());

  double timeForWeakProcessing = WTF::currentTimeMS() - startTime;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, weakProcessingTimeHistogram,
      new CustomCountHistogram("BlinkGC.TimeForGlobalWeakProcessing", 1,
                               10 * 1000, 50));
  weakProcessingTimeHistogram.count(timeForWeakProcessing);
}

void ThreadHeap::reportMemoryUsageHistogram() {
  static size_t supportedMaxSizeInMB = 4 * 1024;
  static size_t observedMaxSizeInMB = 0;

  // We only report the memory in the main thread.
  if (!isMainThread())
    return;
  // +1 is for rounding up the sizeInMB.
  size_t sizeInMB =
      ThreadState::current()->heap().heapStats().allocatedSpace() / 1024 /
          1024 +
      1;
  if (sizeInMB >= supportedMaxSizeInMB)
    sizeInMB = supportedMaxSizeInMB - 1;
  if (sizeInMB > observedMaxSizeInMB) {
    // Send a UseCounter only when we see the highest memory usage
    // we've ever seen.
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        EnumerationHistogram, commitedSizeHistogram,
        new EnumerationHistogram("BlinkGC.CommittedSize",
                                 supportedMaxSizeInMB));
    commitedSizeHistogram.count(sizeInMB);
    observedMaxSizeInMB = sizeInMB;
  }
}

void ThreadHeap::reportMemoryUsageForTracing() {
#if PRINT_HEAP_STATS
// dataLogF("allocatedSpace=%ldMB, allocatedObjectSize=%ldMB, "
//          "markedObjectSize=%ldMB, partitionAllocSize=%ldMB, "
//          "wrapperCount=%ld, collectedWrapperCount=%ld\n",
//          ThreadHeap::allocatedSpace() / 1024 / 1024,
//          ThreadHeap::allocatedObjectSize() / 1024 / 1024,
//          ThreadHeap::markedObjectSize() / 1024 / 1024,
//          WTF::Partitions::totalSizeOfCommittedPages() / 1024 / 1024,
//          ThreadHeap::wrapperCount(), ThreadHeap::collectedWrapperCount());
#endif

  bool gcTracingEnabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                                     &gcTracingEnabled);
  if (!gcTracingEnabled)
    return;

  ThreadHeap& heap = ThreadState::current()->heap();
  // These values are divided by 1024 to avoid overflow in practical cases
  // (TRACE_COUNTER values are 32-bit ints).
  // They are capped to INT_MAX just in case.
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::allocatedObjectSizeKB",
                 std::min(heap.heapStats().allocatedObjectSize() / 1024,
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::markedObjectSizeKB",
                 std::min(heap.heapStats().markedObjectSize() / 1024,
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("blink_gc"),
      "ThreadHeap::markedObjectSizeAtLastCompleteSweepKB",
      std::min(heap.heapStats().markedObjectSizeAtLastCompleteSweep() / 1024,
               static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::allocatedSpaceKB",
                 std::min(heap.heapStats().allocatedSpace() / 1024,
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::objectSizeAtLastGCKB",
                 std::min(heap.heapStats().objectSizeAtLastGC() / 1024,
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::wrapperCount",
      std::min(heap.heapStats().wrapperCount(), static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::wrapperCountAtLastGC",
                 std::min(heap.heapStats().wrapperCountAtLastGC(),
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::collectedWrapperCount",
                 std::min(heap.heapStats().collectedWrapperCount(),
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadHeap::partitionAllocSizeAtLastGCKB",
                 std::min(heap.heapStats().partitionAllocSizeAtLastGC() / 1024,
                          static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "Partitions::totalSizeOfCommittedPagesKB",
                 std::min(WTF::Partitions::totalSizeOfCommittedPages() / 1024,
                          static_cast<size_t>(INT_MAX)));
}

size_t ThreadHeap::objectPayloadSizeForTesting() {
  size_t objectPayloadSize = 0;
  m_threadState->setGCState(ThreadState::GCRunning);
  m_threadState->makeConsistentForGC();
  objectPayloadSize += m_threadState->objectPayloadSizeForTesting();
  m_threadState->setGCState(ThreadState::Sweeping);
  m_threadState->setGCState(ThreadState::NoGCScheduled);
  return objectPayloadSize;
}

void ThreadHeap::visitPersistentRoots(Visitor* visitor) {
  ASSERT(ThreadState::current()->isInGC());
  TRACE_EVENT0("blink_gc", "ThreadHeap::visitPersistentRoots");
  ProcessHeap::crossThreadPersistentRegion().tracePersistentNodes(visitor);

  m_threadState->visitPersistents(visitor);
}

void ThreadHeap::visitStackRoots(Visitor* visitor) {
  ASSERT(ThreadState::current()->isInGC());
  TRACE_EVENT0("blink_gc", "ThreadHeap::visitStackRoots");
  m_threadState->visitStack(visitor);
}

BasePage* ThreadHeap::lookupPageForAddress(Address address) {
  ASSERT(ThreadState::current()->isInGC());
  if (PageMemoryRegion* region = m_regionTree->lookup(address)) {
    return region->pageFromAddress(address);
  }
  return nullptr;
}

void ThreadHeap::resetHeapCounters() {
  ASSERT(ThreadState::current()->isInGC());

  ThreadHeap::reportMemoryUsageForTracing();

  ProcessHeap::decreaseTotalAllocatedObjectSize(m_stats.allocatedObjectSize());
  ProcessHeap::decreaseTotalMarkedObjectSize(m_stats.markedObjectSize());

  m_stats.reset();
  m_threadState->resetHeapCounters();
}

ThreadHeap* ThreadHeap::s_mainThreadHeap = nullptr;

}  // namespace blink
