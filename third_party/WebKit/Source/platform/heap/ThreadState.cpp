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

#include "platform/heap/ThreadState.h"

#include "base/trace_event/process_memory_dump.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/heap/BlinkGCMemoryDumpProvider.h"
#include "platform/heap/CallbackStack.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/heap/HeapCompact.h"
#include "platform/heap/PagePool.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/Visitor.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "platform/instrumentation/tracing/web_process_memory_dump.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/CurrentTime.h"
#include "wtf/DataLog.h"
#include "wtf/PtrUtil.h"
#include "wtf/StackUtil.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/allocator/Partitions.h"
#include <memory>
#include <v8.h>

#if OS(WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#endif

#if defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#endif

#if OS(FREEBSD)
#include <pthread_np.h>
#endif

namespace blink {

WTF::ThreadSpecific<ThreadState*>* ThreadState::s_threadSpecific = nullptr;
uint8_t ThreadState::s_mainThreadStateStorage[sizeof(ThreadState)];

const size_t defaultAllocatedObjectSizeThreshold = 100 * 1024;

const char* ThreadState::gcReasonString(BlinkGC::GCReason reason) {
  switch (reason) {
    case BlinkGC::IdleGC:
      return "IdleGC";
    case BlinkGC::PreciseGC:
      return "PreciseGC";
    case BlinkGC::ConservativeGC:
      return "ConservativeGC";
    case BlinkGC::ForcedGC:
      return "ForcedGC";
    case BlinkGC::MemoryPressureGC:
      return "MemoryPressureGC";
    case BlinkGC::PageNavigationGC:
      return "PageNavigationGC";
    case BlinkGC::ThreadTerminationGC:
      return "ThreadTerminationGC";
  }
  return "<Unknown>";
}

ThreadState::ThreadState()
    : m_thread(currentThread()),
      m_persistentRegion(WTF::makeUnique<PersistentRegion>()),
      m_startOfStack(reinterpret_cast<intptr_t*>(WTF::getStackStart())),
      m_endOfStack(reinterpret_cast<intptr_t*>(WTF::getStackStart())),
      m_safePointScopeMarker(nullptr),
      m_sweepForbidden(false),
      m_noAllocationCount(0),
      m_gcForbiddenCount(0),
      m_mixinsBeingConstructedCount(0),
      m_accumulatedSweepingTime(0),
      m_vectorBackingArenaIndex(BlinkGC::Vector1ArenaIndex),
      m_currentArenaAges(0),
      m_gcMixinMarker(nullptr),
      m_shouldFlushHeapDoesNotContainCache(false),
      m_gcState(NoGCScheduled),
      m_isolate(nullptr),
      m_traceDOMWrappers(nullptr),
      m_invalidateDeadObjectsInWrappersMarkingDeque(nullptr),
#if defined(ADDRESS_SANITIZER)
      m_asanFakeStack(__asan_get_current_fake_stack()),
#endif
#if defined(LEAK_SANITIZER)
      m_disabledStaticPersistentsRegistration(0),
#endif
      m_allocatedObjectSize(0),
      m_markedObjectSize(0),
      m_reportedMemoryToV8(0) {
  ASSERT(checkThread());
  ASSERT(!**s_threadSpecific);
  **s_threadSpecific = this;

  m_heap = WTF::wrapUnique(new ThreadHeap(this));

  for (int arenaIndex = 0; arenaIndex < BlinkGC::LargeObjectArenaIndex;
       arenaIndex++)
    m_arenas[arenaIndex] = new NormalPageArena(this, arenaIndex);
  m_arenas[BlinkGC::LargeObjectArenaIndex] =
      new LargeObjectArena(this, BlinkGC::LargeObjectArenaIndex);

  m_likelyToBePromptlyFreed =
      wrapArrayUnique(new int[likelyToBePromptlyFreedArraySize]);
  clearArenaAges();
}

ThreadState::~ThreadState() {
  ASSERT(checkThread());
  if (isMainThread())
    DCHECK_EQ(heap().heapStats().allocatedSpace(), 0u);
  CHECK(gcState() == ThreadState::NoGCScheduled);

  for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
    delete m_arenas[i];

  **s_threadSpecific = nullptr;
}

void ThreadState::attachMainThread() {
  s_threadSpecific = new WTF::ThreadSpecific<ThreadState*>();
  new (s_mainThreadStateStorage) ThreadState();
}

void ThreadState::attachCurrentThread() {
  new ThreadState();
}

void ThreadState::detachCurrentThread() {
  ThreadState* state = current();
  state->runTerminationGC();
  delete state;
}

void ThreadState::removeAllPages() {
  ASSERT(checkThread());
  for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
    m_arenas[i]->removeAllPages();
}

void ThreadState::runTerminationGC() {
  if (isMainThread()) {
    removeAllPages();
    return;
  }
  ASSERT(checkThread());

  // Finish sweeping.
  completeSweep();

  releaseStaticPersistentNodes();

  ProcessHeap::crossThreadPersistentRegion().prepareForThreadStateTermination(
      this);

  // Do thread local GC's as long as the count of thread local Persistents
  // changes and is above zero.
  int oldCount = -1;
  int currentCount = getPersistentRegion()->numberOfPersistents();
  ASSERT(currentCount >= 0);
  while (currentCount != oldCount) {
    collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep,
                   BlinkGC::ThreadTerminationGC);
    // Release the thread-local static persistents that were
    // instantiated while running the termination GC.
    releaseStaticPersistentNodes();
    oldCount = currentCount;
    currentCount = getPersistentRegion()->numberOfPersistents();
  }
  // We should not have any persistents left when getting to this point,
  // if we have it is probably a bug so adding a debug ASSERT to catch this.
  ASSERT(!currentCount);
  // All of pre-finalizers should be consumed.
  ASSERT(m_orderedPreFinalizers.isEmpty());
  RELEASE_ASSERT(gcState() == NoGCScheduled);

  removeAllPages();
}

NO_SANITIZE_ADDRESS
void ThreadState::visitAsanFakeStackForPointer(Visitor* visitor, Address ptr) {
#if defined(ADDRESS_SANITIZER)
  Address* start = reinterpret_cast<Address*>(m_startOfStack);
  Address* end = reinterpret_cast<Address*>(m_endOfStack);
  Address* fakeFrameStart = nullptr;
  Address* fakeFrameEnd = nullptr;
  Address* maybeFakeFrame = reinterpret_cast<Address*>(ptr);
  Address* realFrameForFakeFrame = reinterpret_cast<Address*>(
      __asan_addr_is_in_fake_stack(m_asanFakeStack, maybeFakeFrame,
                                   reinterpret_cast<void**>(&fakeFrameStart),
                                   reinterpret_cast<void**>(&fakeFrameEnd)));
  if (realFrameForFakeFrame) {
    // This is a fake frame from the asan fake stack.
    if (realFrameForFakeFrame > end && start > realFrameForFakeFrame) {
      // The real stack address for the asan fake frame is
      // within the stack range that we need to scan so we need
      // to visit the values in the fake frame.
      for (Address* p = fakeFrameStart; p < fakeFrameEnd; ++p)
        m_heap->checkAndMarkPointer(visitor, *p);
    }
  }
#endif
}

// Stack scanning may overrun the bounds of local objects and/or race with
// other threads that use this stack.
NO_SANITIZE_ADDRESS
NO_SANITIZE_THREAD
void ThreadState::visitStack(Visitor* visitor) {
  if (m_stackState == BlinkGC::NoHeapPointersOnStack)
    return;

  Address* start = reinterpret_cast<Address*>(m_startOfStack);
  // If there is a safepoint scope marker we should stop the stack
  // scanning there to not touch active parts of the stack. Anything
  // interesting beyond that point is in the safepoint stack copy.
  // If there is no scope marker the thread is blocked and we should
  // scan all the way to the recorded end stack pointer.
  Address* end = reinterpret_cast<Address*>(m_endOfStack);
  Address* safePointScopeMarker =
      reinterpret_cast<Address*>(m_safePointScopeMarker);
  Address* current = safePointScopeMarker ? safePointScopeMarker : end;

  // Ensure that current is aligned by address size otherwise the loop below
  // will read past start address.
  current = reinterpret_cast<Address*>(reinterpret_cast<intptr_t>(current) &
                                       ~(sizeof(Address) - 1));

  for (; current < start; ++current) {
    Address ptr = *current;
#if defined(MEMORY_SANITIZER)
    // |ptr| may be uninitialized by design. Mark it as initialized to keep
    // MSan from complaining.
    // Note: it may be tempting to get rid of |ptr| and simply use |current|
    // here, but that would be incorrect. We intentionally use a local
    // variable because we don't want to unpoison the original stack.
    __msan_unpoison(&ptr, sizeof(ptr));
#endif
    m_heap->checkAndMarkPointer(visitor, ptr);
    visitAsanFakeStackForPointer(visitor, ptr);
  }

  for (Address ptr : m_safePointStackCopy) {
#if defined(MEMORY_SANITIZER)
    // See the comment above.
    __msan_unpoison(&ptr, sizeof(ptr));
#endif
    m_heap->checkAndMarkPointer(visitor, ptr);
    visitAsanFakeStackForPointer(visitor, ptr);
  }
}

void ThreadState::visitPersistents(Visitor* visitor) {
  m_persistentRegion->tracePersistentNodes(visitor);
  if (m_traceDOMWrappers) {
    TRACE_EVENT0("blink_gc", "V8GCController::traceDOMWrappers");
    m_traceDOMWrappers(m_isolate, visitor);
  }
}

ThreadState::GCSnapshotInfo::GCSnapshotInfo(size_t numObjectTypes)
    : liveCount(Vector<int>(numObjectTypes)),
      deadCount(Vector<int>(numObjectTypes)),
      liveSize(Vector<size_t>(numObjectTypes)),
      deadSize(Vector<size_t>(numObjectTypes)) {}

size_t ThreadState::totalMemorySize() {
  return m_heap->heapStats().allocatedObjectSize() +
         m_heap->heapStats().markedObjectSize() +
         WTF::Partitions::totalSizeOfCommittedPages();
}

size_t ThreadState::estimatedLiveSize(size_t estimationBaseSize,
                                      size_t sizeAtLastGC) {
  if (m_heap->heapStats().wrapperCountAtLastGC() == 0) {
    // We'll reach here only before hitting the first GC.
    return 0;
  }

  // (estimated size) = (estimation base size) - (heap size at the last GC) /
  //   (# of persistent handles at the last GC) *
  //     (# of persistent handles collected since the last GC);
  size_t sizeRetainedByCollectedPersistents = static_cast<size_t>(
      1.0 * sizeAtLastGC / m_heap->heapStats().wrapperCountAtLastGC() *
      m_heap->heapStats().collectedWrapperCount());
  if (estimationBaseSize < sizeRetainedByCollectedPersistents)
    return 0;
  return estimationBaseSize - sizeRetainedByCollectedPersistents;
}

double ThreadState::heapGrowingRate() {
  size_t currentSize = m_heap->heapStats().allocatedObjectSize() +
                       m_heap->heapStats().markedObjectSize();
  size_t estimatedSize = estimatedLiveSize(
      m_heap->heapStats().markedObjectSizeAtLastCompleteSweep(),
      m_heap->heapStats().markedObjectSizeAtLastCompleteSweep());

  // If the estimatedSize is 0, we set a high growing rate to trigger a GC.
  double growingRate =
      estimatedSize > 0 ? 1.0 * currentSize / estimatedSize : 100;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::heapEstimatedSizeKB",
                 std::min(estimatedSize / 1024, static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::heapGrowingRate",
                 static_cast<int>(100 * growingRate));
  return growingRate;
}

double ThreadState::partitionAllocGrowingRate() {
  size_t currentSize = WTF::Partitions::totalSizeOfCommittedPages();
  size_t estimatedSize = estimatedLiveSize(
      currentSize, m_heap->heapStats().partitionAllocSizeAtLastGC());

  // If the estimatedSize is 0, we set a high growing rate to trigger a GC.
  double growingRate =
      estimatedSize > 0 ? 1.0 * currentSize / estimatedSize : 100;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::partitionAllocEstimatedSizeKB",
                 std::min(estimatedSize / 1024, static_cast<size_t>(INT_MAX)));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::partitionAllocGrowingRate",
                 static_cast<int>(100 * growingRate));
  return growingRate;
}

// TODO(haraken): We should improve the GC heuristics. The heuristics affect
// performance significantly.
bool ThreadState::judgeGCThreshold(size_t allocatedObjectSizeThreshold,
                                   size_t totalMemorySizeThreshold,
                                   double heapGrowingRateThreshold) {
  // If the allocated object size or the total memory size is small, don't
  // trigger a GC.
  if (m_heap->heapStats().allocatedObjectSize() <
          allocatedObjectSizeThreshold ||
      totalMemorySize() < totalMemorySizeThreshold)
    return false;
// If the growing rate of Oilpan's heap or PartitionAlloc is high enough,
// trigger a GC.
#if PRINT_HEAP_STATS
  dataLogF("heapGrowingRate=%.1lf, partitionAllocGrowingRate=%.1lf\n",
           heapGrowingRate(), partitionAllocGrowingRate());
#endif
  return heapGrowingRate() >= heapGrowingRateThreshold ||
         partitionAllocGrowingRate() >= heapGrowingRateThreshold;
}

bool ThreadState::shouldScheduleIdleGC() {
  if (gcState() != NoGCScheduled)
    return false;
  return judgeGCThreshold(defaultAllocatedObjectSizeThreshold, 1024 * 1024,
                          1.5);
}

bool ThreadState::shouldScheduleV8FollowupGC() {
  return judgeGCThreshold(defaultAllocatedObjectSizeThreshold, 32 * 1024 * 1024,
                          1.5);
}

bool ThreadState::shouldSchedulePageNavigationGC(float estimatedRemovalRatio) {
  // If estimatedRemovalRatio is low we should let IdleGC handle this.
  if (estimatedRemovalRatio < 0.01)
    return false;
  return judgeGCThreshold(defaultAllocatedObjectSizeThreshold, 32 * 1024 * 1024,
                          1.5 * (1 - estimatedRemovalRatio));
}

bool ThreadState::shouldForceConservativeGC() {
  // TODO(haraken): 400% is too large. Lower the heap growing factor.
  return judgeGCThreshold(defaultAllocatedObjectSizeThreshold, 32 * 1024 * 1024,
                          5.0);
}

// If we're consuming too much memory, trigger a conservative GC
// aggressively. This is a safe guard to avoid OOM.
bool ThreadState::shouldForceMemoryPressureGC() {
  if (totalMemorySize() < 300 * 1024 * 1024)
    return false;
  return judgeGCThreshold(0, 0, 1.5);
}

void ThreadState::scheduleV8FollowupGCIfNeeded(BlinkGC::V8GCType gcType) {
  ASSERT(checkThread());
  ThreadHeap::reportMemoryUsageForTracing();

#if PRINT_HEAP_STATS
  dataLogF("ThreadState::scheduleV8FollowupGCIfNeeded (gcType=%s)\n",
           gcType == BlinkGC::V8MajorGC ? "MajorGC" : "MinorGC");
#endif

  if (isGCForbidden())
    return;

  // This completeSweep() will do nothing in common cases since we've
  // called completeSweep() before V8 starts minor/major GCs.
  completeSweep();
  ASSERT(!isSweepingInProgress());
  ASSERT(!sweepForbidden());

  if ((gcType == BlinkGC::V8MajorGC && shouldForceMemoryPressureGC()) ||
      shouldScheduleV8FollowupGC()) {
#if PRINT_HEAP_STATS
    dataLogF("Scheduled PreciseGC\n");
#endif
    schedulePreciseGC();
    return;
  }
  if (gcType == BlinkGC::V8MajorGC && shouldScheduleIdleGC()) {
#if PRINT_HEAP_STATS
    dataLogF("Scheduled IdleGC\n");
#endif
    scheduleIdleGC();
    return;
  }
}

void ThreadState::willStartV8GC(BlinkGC::V8GCType gcType) {
  // Finish Oilpan's complete sweeping before running a V8 major GC.
  // This will let the GC collect more V8 objects.
  //
  // TODO(haraken): It's a bit too late for a major GC to schedule
  // completeSweep() here, because gcPrologue for a major GC is called
  // not at the point where the major GC started but at the point where
  // the major GC requests object grouping.
  if (gcType == BlinkGC::V8MajorGC)
    completeSweep();
}

void ThreadState::schedulePageNavigationGCIfNeeded(
    float estimatedRemovalRatio) {
  ASSERT(checkThread());
  ThreadHeap::reportMemoryUsageForTracing();

#if PRINT_HEAP_STATS
  dataLogF(
      "ThreadState::schedulePageNavigationGCIfNeeded "
      "(estimatedRemovalRatio=%.2lf)\n",
      estimatedRemovalRatio);
#endif

  if (isGCForbidden())
    return;

  // Finish on-going lazy sweeping.
  // TODO(haraken): It might not make sense to force completeSweep() for all
  // page navigations.
  completeSweep();
  ASSERT(!isSweepingInProgress());
  ASSERT(!sweepForbidden());

  if (shouldForceMemoryPressureGC()) {
#if PRINT_HEAP_STATS
    dataLogF("Scheduled MemoryPressureGC\n");
#endif
    collectGarbage(BlinkGC::HeapPointersOnStack, BlinkGC::GCWithoutSweep,
                   BlinkGC::MemoryPressureGC);
    return;
  }
  if (shouldSchedulePageNavigationGC(estimatedRemovalRatio)) {
#if PRINT_HEAP_STATS
    dataLogF("Scheduled PageNavigationGC\n");
#endif
    schedulePageNavigationGC();
  }
}

void ThreadState::schedulePageNavigationGC() {
  ASSERT(checkThread());
  ASSERT(!isSweepingInProgress());
  setGCState(PageNavigationGCScheduled);
}

void ThreadState::scheduleGCIfNeeded() {
  ASSERT(checkThread());
  ThreadHeap::reportMemoryUsageForTracing();

#if PRINT_HEAP_STATS
  dataLogF("ThreadState::scheduleGCIfNeeded\n");
#endif

  // Allocation is allowed during sweeping, but those allocations should not
  // trigger nested GCs.
  if (isGCForbidden())
    return;

  if (isSweepingInProgress())
    return;
  ASSERT(!sweepForbidden());

  reportMemoryToV8();

  if (shouldForceMemoryPressureGC()) {
    completeSweep();
    if (shouldForceMemoryPressureGC()) {
#if PRINT_HEAP_STATS
      dataLogF("Scheduled MemoryPressureGC\n");
#endif
      collectGarbage(BlinkGC::HeapPointersOnStack, BlinkGC::GCWithoutSweep,
                     BlinkGC::MemoryPressureGC);
      return;
    }
  }

  if (shouldForceConservativeGC()) {
    completeSweep();
    if (shouldForceConservativeGC()) {
#if PRINT_HEAP_STATS
      dataLogF("Scheduled ConservativeGC\n");
#endif
      collectGarbage(BlinkGC::HeapPointersOnStack, BlinkGC::GCWithoutSweep,
                     BlinkGC::ConservativeGC);
      return;
    }
  }
  if (shouldScheduleIdleGC()) {
#if PRINT_HEAP_STATS
    dataLogF("Scheduled IdleGC\n");
#endif
    scheduleIdleGC();
    return;
  }
}

ThreadState* ThreadState::fromObject(const void* object) {
  ASSERT(object);
  BasePage* page = pageFromObject(object);
  ASSERT(page);
  ASSERT(page->arena());
  return page->arena()->getThreadState();
}

void ThreadState::performIdleGC(double deadlineSeconds) {
  ASSERT(checkThread());
  ASSERT(isMainThread());
  ASSERT(Platform::current()->currentThread()->scheduler());

  if (gcState() != IdleGCScheduled)
    return;

  if (isGCForbidden()) {
    // If GC is forbidden at this point, try again.
    scheduleIdleGC();
    return;
  }

  double idleDeltaInSeconds = deadlineSeconds - monotonicallyIncreasingTime();
  if (idleDeltaInSeconds <= m_heap->heapStats().estimatedMarkingTime() &&
      !Platform::current()
           ->currentThread()
           ->scheduler()
           ->canExceedIdleDeadlineIfRequired()) {
    // If marking is estimated to take longer than the deadline and we can't
    // exceed the deadline, then reschedule for the next idle period.
    scheduleIdleGC();
    return;
  }

  TRACE_EVENT2("blink_gc", "ThreadState::performIdleGC", "idleDeltaInSeconds",
               idleDeltaInSeconds, "estimatedMarkingTime",
               m_heap->heapStats().estimatedMarkingTime());
  collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithoutSweep,
                 BlinkGC::IdleGC);
}

void ThreadState::performIdleLazySweep(double deadlineSeconds) {
  ASSERT(checkThread());
  ASSERT(isMainThread());

  // If we are not in a sweeping phase, there is nothing to do here.
  if (!isSweepingInProgress())
    return;

  // This check is here to prevent performIdleLazySweep() from being called
  // recursively. I'm not sure if it can happen but it would be safer to have
  // the check just in case.
  if (sweepForbidden())
    return;

  TRACE_EVENT1("blink_gc,devtools.timeline",
               "ThreadState::performIdleLazySweep", "idleDeltaInSeconds",
               deadlineSeconds - monotonicallyIncreasingTime());

  SweepForbiddenScope scope(this);
  ScriptForbiddenIfMainThreadScope scriptForbiddenScope;

  double startTime = WTF::currentTimeMS();
  bool sweepCompleted = true;
  for (int i = 0; i < BlinkGC::NumberOfArenas; i++) {
    // lazySweepWithDeadline() won't check the deadline until it sweeps
    // 10 pages. So we give a small slack for safety.
    double slack = 0.001;
    double remainingBudget =
        deadlineSeconds - slack - monotonicallyIncreasingTime();
    if (remainingBudget <= 0 ||
        !m_arenas[i]->lazySweepWithDeadline(deadlineSeconds)) {
      // We couldn't finish the sweeping within the deadline.
      // We request another idle task for the remaining sweeping.
      scheduleIdleLazySweep();
      sweepCompleted = false;
      break;
    }
  }
  accumulateSweepingTime(WTF::currentTimeMS() - startTime);

  if (sweepCompleted)
    postSweep();
}

void ThreadState::scheduleIdleGC() {
  // TODO(haraken): Idle GC should be supported in worker threads as well.
  if (!isMainThread())
    return;

  if (isSweepingInProgress()) {
    setGCState(SweepingAndIdleGCScheduled);
    return;
  }

  // Some threads (e.g. PPAPI thread) don't have a scheduler.
  if (!Platform::current()->currentThread()->scheduler())
    return;

  Platform::current()->currentThread()->scheduler()->postNonNestableIdleTask(
      BLINK_FROM_HERE,
      WTF::bind(&ThreadState::performIdleGC, WTF::unretained(this)));
  setGCState(IdleGCScheduled);
}

void ThreadState::scheduleIdleLazySweep() {
  // TODO(haraken): Idle complete sweep should be supported in worker threads.
  if (!isMainThread())
    return;

  // Some threads (e.g. PPAPI thread) don't have a scheduler.
  if (!Platform::current()->currentThread()->scheduler())
    return;

  Platform::current()->currentThread()->scheduler()->postIdleTask(
      BLINK_FROM_HERE,
      WTF::bind(&ThreadState::performIdleLazySweep, WTF::unretained(this)));
}

void ThreadState::schedulePreciseGC() {
  ASSERT(checkThread());
  if (isSweepingInProgress()) {
    setGCState(SweepingAndPreciseGCScheduled);
    return;
  }

  setGCState(PreciseGCScheduled);
}

namespace {

#define UNEXPECTED_GCSTATE(s)                                   \
  case ThreadState::s:                                          \
    LOG(FATAL) << "Unexpected transition while in GCState " #s; \
    return

void unexpectedGCState(ThreadState::GCState gcState) {
  switch (gcState) {
    UNEXPECTED_GCSTATE(NoGCScheduled);
    UNEXPECTED_GCSTATE(IdleGCScheduled);
    UNEXPECTED_GCSTATE(PreciseGCScheduled);
    UNEXPECTED_GCSTATE(FullGCScheduled);
    UNEXPECTED_GCSTATE(GCRunning);
    UNEXPECTED_GCSTATE(Sweeping);
    UNEXPECTED_GCSTATE(SweepingAndIdleGCScheduled);
    UNEXPECTED_GCSTATE(SweepingAndPreciseGCScheduled);
    default:
      ASSERT_NOT_REACHED();
      return;
  }
}

#undef UNEXPECTED_GCSTATE

}  // namespace

#define VERIFY_STATE_TRANSITION(condition) \
  if (UNLIKELY(!(condition)))              \
  unexpectedGCState(m_gcState)

void ThreadState::setGCState(GCState gcState) {
  switch (gcState) {
    case NoGCScheduled:
      ASSERT(checkThread());
      VERIFY_STATE_TRANSITION(m_gcState == Sweeping ||
                              m_gcState == SweepingAndIdleGCScheduled);
      break;
    case IdleGCScheduled:
    case PreciseGCScheduled:
    case FullGCScheduled:
    case PageNavigationGCScheduled:
      ASSERT(checkThread());
      VERIFY_STATE_TRANSITION(
          m_gcState == NoGCScheduled || m_gcState == IdleGCScheduled ||
          m_gcState == PreciseGCScheduled || m_gcState == FullGCScheduled ||
          m_gcState == PageNavigationGCScheduled || m_gcState == Sweeping ||
          m_gcState == SweepingAndIdleGCScheduled ||
          m_gcState == SweepingAndPreciseGCScheduled);
      completeSweep();
      break;
    case GCRunning:
      ASSERT(!isInGC());
      VERIFY_STATE_TRANSITION(m_gcState != GCRunning);
      break;
    case Sweeping:
      DCHECK(isInGC());
      ASSERT(checkThread());
      VERIFY_STATE_TRANSITION(m_gcState == GCRunning);
      break;
    case SweepingAndIdleGCScheduled:
    case SweepingAndPreciseGCScheduled:
      ASSERT(checkThread());
      VERIFY_STATE_TRANSITION(m_gcState == Sweeping ||
                              m_gcState == SweepingAndIdleGCScheduled ||
                              m_gcState == SweepingAndPreciseGCScheduled);
      break;
    default:
      ASSERT_NOT_REACHED();
  }
  m_gcState = gcState;
}

#undef VERIFY_STATE_TRANSITION

void ThreadState::runScheduledGC(BlinkGC::StackState stackState) {
  ASSERT(checkThread());
  if (stackState != BlinkGC::NoHeapPointersOnStack)
    return;

  // If a safe point is entered while initiating a GC, we clearly do
  // not want to do another as part of that -- the safe point is only
  // entered after checking if a scheduled GC ought to run first.
  // Prevent that from happening by marking GCs as forbidden while
  // one is initiated and later running.
  if (isGCForbidden())
    return;

  switch (gcState()) {
    case FullGCScheduled:
      collectAllGarbage();
      break;
    case PreciseGCScheduled:
      collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithoutSweep,
                     BlinkGC::PreciseGC);
      break;
    case PageNavigationGCScheduled:
      collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep,
                     BlinkGC::PageNavigationGC);
      break;
    case IdleGCScheduled:
      // Idle time GC will be scheduled by Blink Scheduler.
      break;
    default:
      break;
  }
}

void ThreadState::flushHeapDoesNotContainCacheIfNeeded() {
  if (m_shouldFlushHeapDoesNotContainCache) {
    m_heap->flushHeapDoesNotContainCache();
    m_shouldFlushHeapDoesNotContainCache = false;
  }
}

void ThreadState::makeConsistentForGC() {
  ASSERT(isInGC());
  TRACE_EVENT0("blink_gc", "ThreadState::makeConsistentForGC");
  for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
    m_arenas[i]->makeConsistentForGC();
}

void ThreadState::compact() {
  if (!heap().compaction()->isCompacting())
    return;

  SweepForbiddenScope scope(this);
  ScriptForbiddenIfMainThreadScope scriptForbiddenScope;
  NoAllocationScope noAllocationScope(this);

  // Compaction is done eagerly and before the mutator threads get
  // to run again. Doing it lazily is problematic, as the mutator's
  // references to live objects could suddenly be invalidated by
  // compaction of a page/heap. We do know all the references to
  // the relocating objects just after marking, but won't later.
  // (e.g., stack references could have been created, new objects
  // created which refer to old collection objects, and so on.)

  // Compact the hash table backing store arena first, it usually has
  // higher fragmentation and is larger.
  //
  // TODO: implement bail out wrt any overall deadline, not compacting
  // the remaining arenas if the time budget has been exceeded.
  heap().compaction()->startThreadCompaction();
  for (int i = BlinkGC::HashTableArenaIndex; i >= BlinkGC::Vector1ArenaIndex;
       --i)
    static_cast<NormalPageArena*>(m_arenas[i])->sweepAndCompact();
  heap().compaction()->finishThreadCompaction();
}

void ThreadState::makeConsistentForMutator() {
  ASSERT(isInGC());
  for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
    m_arenas[i]->makeConsistentForMutator();
}

void ThreadState::preGC() {
  if (m_isolate && m_performCleanup)
    m_performCleanup(m_isolate);

  ASSERT(!isInGC());
  setGCState(GCRunning);
  makeConsistentForGC();
  flushHeapDoesNotContainCacheIfNeeded();
  clearArenaAges();
}

void ThreadState::postGC(BlinkGC::GCType gcType) {
  if (m_invalidateDeadObjectsInWrappersMarkingDeque)
    m_invalidateDeadObjectsInWrappersMarkingDeque(m_isolate);

  DCHECK(isInGC());
  DCHECK(checkThread());
  for (int i = 0; i < BlinkGC::NumberOfArenas; i++)
    m_arenas[i]->prepareForSweep();

  if (gcType == BlinkGC::TakeSnapshot) {
    takeSnapshot(SnapshotType::HeapSnapshot);

    // This unmarks all marked objects and marks all unmarked objects dead.
    makeConsistentForMutator();

    takeSnapshot(SnapshotType::FreelistSnapshot);

    // Force setting NoGCScheduled to circumvent checkThread()
    // in setGCState().
    m_gcState = NoGCScheduled;
    return;
  }
}

void ThreadState::preSweep(BlinkGC::GCType gcType) {
  if (gcState() == NoGCScheduled)
    return;
  // We have to set the GCState to Sweeping before calling pre-finalizers
  // to disallow a GC during the pre-finalizers.
  setGCState(Sweeping);

  // Allocation is allowed during the pre-finalizers and destructors.
  // However, they must not mutate an object graph in a way in which
  // a dead object gets resurrected.
  invokePreFinalizers();

  m_accumulatedSweepingTime = 0;

  eagerSweep();

  // Any sweep compaction must happen after pre-finalizers and eager
  // sweeping, as it will finalize dead objects in compactable arenas
  // (e.g., backing stores for container objects.)
  //
  // As per-contract for prefinalizers, those finalizable objects must
  // still be accessible when the prefinalizer runs, hence we cannot
  // schedule compaction until those have run. Similarly for eager sweeping.
  compact();

#if defined(ADDRESS_SANITIZER)
  poisonAllHeaps();
#endif

  if (gcType == BlinkGC::GCWithSweep) {
    // Eager sweeping should happen only in testing.
    completeSweep();
  } else {
    DCHECK(gcType == BlinkGC::GCWithoutSweep);
    // The default behavior is lazy sweeping.
    scheduleIdleLazySweep();
  }
}

#if defined(ADDRESS_SANITIZER)
void ThreadState::poisonAllHeaps() {
  CrossThreadPersistentRegion::LockScope persistentLock(
      ProcessHeap::crossThreadPersistentRegion());
  // Poisoning all unmarked objects in the other arenas.
  for (int i = 1; i < BlinkGC::NumberOfArenas; i++)
    m_arenas[i]->poisonArena();
  // CrossThreadPersistents in unmarked objects may be accessed from other
  // threads (e.g. in CrossThreadPersistentRegion::shouldTracePersistent) and
  // that would be fine.
  ProcessHeap::crossThreadPersistentRegion().unpoisonCrossThreadPersistents();
}

void ThreadState::poisonEagerArena() {
  CrossThreadPersistentRegion::LockScope persistentLock(
      ProcessHeap::crossThreadPersistentRegion());
  m_arenas[BlinkGC::EagerSweepArenaIndex]->poisonArena();
  // CrossThreadPersistents in unmarked objects may be accessed from other
  // threads (e.g. in CrossThreadPersistentRegion::shouldTracePersistent) and
  // that would be fine.
  ProcessHeap::crossThreadPersistentRegion().unpoisonCrossThreadPersistents();
}
#endif

void ThreadState::eagerSweep() {
#if defined(ADDRESS_SANITIZER)
  poisonEagerArena();
#endif
  ASSERT(checkThread());
  // Some objects need to be finalized promptly and cannot be handled
  // by lazy sweeping. Keep those in a designated heap and sweep it
  // eagerly.
  ASSERT(isSweepingInProgress());

  // Mirroring the completeSweep() condition; see its comment.
  if (sweepForbidden())
    return;

  SweepForbiddenScope scope(this);
  ScriptForbiddenIfMainThreadScope scriptForbiddenScope;

  double startTime = WTF::currentTimeMS();
  m_arenas[BlinkGC::EagerSweepArenaIndex]->completeSweep();
  accumulateSweepingTime(WTF::currentTimeMS() - startTime);
}

void ThreadState::completeSweep() {
  ASSERT(checkThread());
  // If we are not in a sweeping phase, there is nothing to do here.
  if (!isSweepingInProgress())
    return;

  // completeSweep() can be called recursively if finalizers can allocate
  // memory and the allocation triggers completeSweep(). This check prevents
  // the sweeping from being executed recursively.
  if (sweepForbidden())
    return;

  SweepForbiddenScope scope(this);
  ScriptForbiddenIfMainThreadScope scriptForbiddenScope;

  TRACE_EVENT0("blink_gc,devtools.timeline", "ThreadState::completeSweep");
  double startTime = WTF::currentTimeMS();

  static_assert(BlinkGC::EagerSweepArenaIndex == 0,
                "Eagerly swept arenas must be processed first.");
  for (int i = 0; i < BlinkGC::NumberOfArenas; i++)
    m_arenas[i]->completeSweep();

  double timeForCompleteSweep = WTF::currentTimeMS() - startTime;
  accumulateSweepingTime(timeForCompleteSweep);

  if (isMainThread()) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, completeSweepHistogram,
                        ("BlinkGC.CompleteSweep", 1, 10 * 1000, 50));
    completeSweepHistogram.count(timeForCompleteSweep);
  }

  postSweep();
}

void ThreadState::postSweep() {
  ASSERT(checkThread());
  ThreadHeap::reportMemoryUsageForTracing();

  if (isMainThread()) {
    double collectionRate = 0;
    if (m_heap->heapStats().objectSizeAtLastGC() > 0)
      collectionRate = 1 -
                       1.0 * m_heap->heapStats().markedObjectSize() /
                           m_heap->heapStats().objectSizeAtLastGC();
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                   "ThreadState::collectionRate",
                   static_cast<int>(100 * collectionRate));

#if PRINT_HEAP_STATS
    dataLogF("ThreadState::postSweep (collectionRate=%d%%)\n",
             static_cast<int>(100 * collectionRate));
#endif

    // ThreadHeap::markedObjectSize() may be underestimated here if any other
    // thread has not yet finished lazy sweeping.
    m_heap->heapStats().setMarkedObjectSizeAtLastCompleteSweep(
        m_heap->heapStats().markedObjectSize());

    DEFINE_STATIC_LOCAL(CustomCountHistogram, objectSizeBeforeGCHistogram,
                        ("BlinkGC.ObjectSizeBeforeGC", 1, 4 * 1024 * 1024, 50));
    objectSizeBeforeGCHistogram.count(m_heap->heapStats().objectSizeAtLastGC() /
                                      1024);
    DEFINE_STATIC_LOCAL(CustomCountHistogram, objectSizeAfterGCHistogram,
                        ("BlinkGC.ObjectSizeAfterGC", 1, 4 * 1024 * 1024, 50));
    objectSizeAfterGCHistogram.count(m_heap->heapStats().markedObjectSize() /
                                     1024);
    DEFINE_STATIC_LOCAL(CustomCountHistogram, collectionRateHistogram,
                        ("BlinkGC.CollectionRate", 1, 100, 20));
    collectionRateHistogram.count(static_cast<int>(100 * collectionRate));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, timeForSweepHistogram,
        ("BlinkGC.TimeForSweepingAllObjects", 1, 10 * 1000, 50));
    timeForSweepHistogram.count(m_accumulatedSweepingTime);

#define COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(GCReason)              \
  case BlinkGC::GCReason: {                                                 \
    DEFINE_STATIC_LOCAL(CustomCountHistogram, histogram,                    \
                        ("BlinkGC.CollectionRate_" #GCReason, 1, 100, 20)); \
    histogram.count(static_cast<int>(100 * collectionRate));                \
    break;                                                                  \
  }

    switch (m_heap->lastGCReason()) {
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(IdleGC)
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(PreciseGC)
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(ConservativeGC)
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(ForcedGC)
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(MemoryPressureGC)
      COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(PageNavigationGC)
      default:
        break;
    }
  }

  switch (gcState()) {
    case Sweeping:
      setGCState(NoGCScheduled);
      break;
    case SweepingAndPreciseGCScheduled:
      setGCState(PreciseGCScheduled);
      break;
    case SweepingAndIdleGCScheduled:
      setGCState(NoGCScheduled);
      scheduleIdleGC();
      break;
    default:
      ASSERT_NOT_REACHED();
  }
}

#if DCHECK_IS_ON()
BasePage* ThreadState::findPageFromAddress(Address address) {
  for (int i = 0; i < BlinkGC::NumberOfArenas; ++i) {
    if (BasePage* page = m_arenas[i]->findPageFromAddress(address))
      return page;
  }
  return nullptr;
}
#endif

bool ThreadState::isAddressInHeapDoesNotContainCache(Address address) {
  // If the cache has been marked as invalidated, it's cleared prior
  // to performing the next GC. Hence, consider the cache as being
  // effectively empty.
  if (m_shouldFlushHeapDoesNotContainCache)
    return false;
  return heap().m_heapDoesNotContainCache->lookup(address);
}

size_t ThreadState::objectPayloadSizeForTesting() {
  size_t objectPayloadSize = 0;
  for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
    objectPayloadSize += m_arenas[i]->objectPayloadSizeForTesting();
  return objectPayloadSize;
}

void ThreadState::safePoint(BlinkGC::StackState stackState) {
  ASSERT(checkThread());
  ThreadHeap::reportMemoryUsageForTracing();

  runScheduledGC(stackState);
  m_stackState = BlinkGC::HeapPointersOnStack;
}

#ifdef ADDRESS_SANITIZER
// When we are running under AddressSanitizer with
// detect_stack_use_after_return=1 then stack marker obtained from
// SafePointScope will point into a fake stack.  Detect this case by checking if
// it falls in between current stack frame and stack start and use an arbitrary
// high enough value for it.  Don't adjust stack marker in any other case to
// match behavior of code running without AddressSanitizer.
NO_SANITIZE_ADDRESS static void* adjustScopeMarkerForAdressSanitizer(
    void* scopeMarker) {
  Address start = reinterpret_cast<Address>(WTF::getStackStart());
  Address end = reinterpret_cast<Address>(&start);
  RELEASE_ASSERT(end < start);

  if (end <= scopeMarker && scopeMarker < start)
    return scopeMarker;

  // 256 is as good an approximation as any else.
  const size_t bytesToCopy = sizeof(Address) * 256;
  if (static_cast<size_t>(start - end) < bytesToCopy)
    return start;

  return end + bytesToCopy;
}
#endif

// TODO(haraken): The first void* pointer is unused. Remove it.
using PushAllRegistersCallback = void (*)(void*, ThreadState*, intptr_t*);
extern "C" void pushAllRegisters(void*, ThreadState*, PushAllRegistersCallback);

static void enterSafePointAfterPushRegisters(void*,
                                             ThreadState* state,
                                             intptr_t* stackEnd) {
  state->recordStackEnd(stackEnd);
  state->copyStackUntilSafePointScope();
}

void ThreadState::enterSafePoint(BlinkGC::StackState stackState,
                                 void* scopeMarker) {
  ASSERT(checkThread());
#ifdef ADDRESS_SANITIZER
  if (stackState == BlinkGC::HeapPointersOnStack)
    scopeMarker = adjustScopeMarkerForAdressSanitizer(scopeMarker);
#endif
  ASSERT(stackState == BlinkGC::NoHeapPointersOnStack || scopeMarker);
  runScheduledGC(stackState);
  m_stackState = stackState;
  m_safePointScopeMarker = scopeMarker;
  pushAllRegisters(nullptr, this, enterSafePointAfterPushRegisters);
}

void ThreadState::leaveSafePoint() {
  ASSERT(checkThread());
  m_stackState = BlinkGC::HeapPointersOnStack;
  clearSafePointScopeMarker();
}

void ThreadState::reportMemoryToV8() {
  if (!m_isolate)
    return;

  size_t currentHeapSize = m_allocatedObjectSize + m_markedObjectSize;
  int64_t diff = static_cast<int64_t>(currentHeapSize) -
                 static_cast<int64_t>(m_reportedMemoryToV8);
  m_isolate->AdjustAmountOfExternalAllocatedMemory(diff);
  m_reportedMemoryToV8 = currentHeapSize;
}

void ThreadState::resetHeapCounters() {
  m_allocatedObjectSize = 0;
  m_markedObjectSize = 0;
}

void ThreadState::increaseAllocatedObjectSize(size_t delta) {
  m_allocatedObjectSize += delta;
  m_heap->heapStats().increaseAllocatedObjectSize(delta);
}

void ThreadState::decreaseAllocatedObjectSize(size_t delta) {
  m_allocatedObjectSize -= delta;
  m_heap->heapStats().decreaseAllocatedObjectSize(delta);
}

void ThreadState::increaseMarkedObjectSize(size_t delta) {
  m_markedObjectSize += delta;
  m_heap->heapStats().increaseMarkedObjectSize(delta);
}

void ThreadState::copyStackUntilSafePointScope() {
  if (!m_safePointScopeMarker || m_stackState == BlinkGC::NoHeapPointersOnStack)
    return;

  Address* to = reinterpret_cast<Address*>(m_safePointScopeMarker);
  Address* from = reinterpret_cast<Address*>(m_endOfStack);
  RELEASE_ASSERT(from < to);
  RELEASE_ASSERT(to <= reinterpret_cast<Address*>(m_startOfStack));
  size_t slotCount = static_cast<size_t>(to - from);
// Catch potential performance issues.
#if defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
  // ASan/LSan use more space on the stack and we therefore
  // increase the allowed stack copying for those builds.
  ASSERT(slotCount < 2048);
#else
  ASSERT(slotCount < 1024);
#endif

  ASSERT(!m_safePointStackCopy.size());
  m_safePointStackCopy.resize(slotCount);
  for (size_t i = 0; i < slotCount; ++i) {
    m_safePointStackCopy[i] = from[i];
  }
}

void ThreadState::registerStaticPersistentNode(
    PersistentNode* node,
    PersistentClearCallback callback) {
#if defined(LEAK_SANITIZER)
  if (m_disabledStaticPersistentsRegistration)
    return;
#endif

  ASSERT(!m_staticPersistents.contains(node));
  m_staticPersistents.insert(node, callback);
}

void ThreadState::releaseStaticPersistentNodes() {
  HashMap<PersistentNode*, ThreadState::PersistentClearCallback>
      staticPersistents;
  staticPersistents.swap(m_staticPersistents);

  PersistentRegion* persistentRegion = getPersistentRegion();
  for (const auto& it : staticPersistents)
    persistentRegion->releasePersistentNode(it.key, it.value);
}

void ThreadState::freePersistentNode(PersistentNode* persistentNode) {
  PersistentRegion* persistentRegion = getPersistentRegion();
  persistentRegion->freePersistentNode(persistentNode);
  // Do not allow static persistents to be freed before
  // they're all released in releaseStaticPersistentNodes().
  //
  // There's no fundamental reason why this couldn't be supported,
  // but no known use for it.
  ASSERT(!m_staticPersistents.contains(persistentNode));
}

#if defined(LEAK_SANITIZER)
void ThreadState::enterStaticReferenceRegistrationDisabledScope() {
  m_disabledStaticPersistentsRegistration++;
}

void ThreadState::leaveStaticReferenceRegistrationDisabledScope() {
  ASSERT(m_disabledStaticPersistentsRegistration);
  m_disabledStaticPersistentsRegistration--;
}
#endif

void ThreadState::invokePreFinalizers() {
  ASSERT(checkThread());
  ASSERT(!sweepForbidden());
  TRACE_EVENT0("blink_gc", "ThreadState::invokePreFinalizers");

  SweepForbiddenScope sweepForbidden(this);
  ScriptForbiddenIfMainThreadScope scriptForbidden;

  double startTime = WTF::currentTimeMS();
  if (!m_orderedPreFinalizers.isEmpty()) {
    // Call the prefinalizers in the opposite order to their registration.
    //
    // The prefinalizer callback wrapper returns |true| when its associated
    // object is unreachable garbage and the prefinalizer callback has run.
    // The registered prefinalizer entry must then be removed and deleted.
    //
    auto it = --m_orderedPreFinalizers.end();
    bool done;
    do {
      auto entry = it;
      done = it == m_orderedPreFinalizers.begin();
      if (!done)
        --it;
      if ((entry->second)(entry->first))
        m_orderedPreFinalizers.remove(entry);
    } while (!done);
  }
  if (isMainThread()) {
    double timeForInvokingPreFinalizers = WTF::currentTimeMS() - startTime;
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, preFinalizersHistogram,
        ("BlinkGC.TimeForInvokingPreFinalizers", 1, 10 * 1000, 50));
    preFinalizersHistogram.count(timeForInvokingPreFinalizers);
  }
}

void ThreadState::clearArenaAges() {
  memset(m_arenaAges, 0, sizeof(size_t) * BlinkGC::NumberOfArenas);
  memset(m_likelyToBePromptlyFreed.get(), 0,
         sizeof(int) * likelyToBePromptlyFreedArraySize);
  m_currentArenaAges = 0;
}

int ThreadState::arenaIndexOfVectorArenaLeastRecentlyExpanded(
    int beginArenaIndex,
    int endArenaIndex) {
  size_t minArenaAge = m_arenaAges[beginArenaIndex];
  int arenaIndexWithMinArenaAge = beginArenaIndex;
  for (int arenaIndex = beginArenaIndex + 1; arenaIndex <= endArenaIndex;
       arenaIndex++) {
    if (m_arenaAges[arenaIndex] < minArenaAge) {
      minArenaAge = m_arenaAges[arenaIndex];
      arenaIndexWithMinArenaAge = arenaIndex;
    }
  }
  ASSERT(isVectorArenaIndex(arenaIndexWithMinArenaAge));
  return arenaIndexWithMinArenaAge;
}

BaseArena* ThreadState::expandedVectorBackingArena(size_t gcInfoIndex) {
  ASSERT(checkThread());
  size_t entryIndex = gcInfoIndex & likelyToBePromptlyFreedArrayMask;
  --m_likelyToBePromptlyFreed[entryIndex];
  int arenaIndex = m_vectorBackingArenaIndex;
  m_arenaAges[arenaIndex] = ++m_currentArenaAges;
  m_vectorBackingArenaIndex = arenaIndexOfVectorArenaLeastRecentlyExpanded(
      BlinkGC::Vector1ArenaIndex, BlinkGC::Vector4ArenaIndex);
  return m_arenas[arenaIndex];
}

void ThreadState::allocationPointAdjusted(int arenaIndex) {
  m_arenaAges[arenaIndex] = ++m_currentArenaAges;
  if (m_vectorBackingArenaIndex == arenaIndex)
    m_vectorBackingArenaIndex = arenaIndexOfVectorArenaLeastRecentlyExpanded(
        BlinkGC::Vector1ArenaIndex, BlinkGC::Vector4ArenaIndex);
}

void ThreadState::promptlyFreed(size_t gcInfoIndex) {
  ASSERT(checkThread());
  size_t entryIndex = gcInfoIndex & likelyToBePromptlyFreedArrayMask;
  // See the comment in vectorBackingArena() for why this is +3.
  m_likelyToBePromptlyFreed[entryIndex] += 3;
}

void ThreadState::takeSnapshot(SnapshotType type) {
  ASSERT(isInGC());

  // 0 is used as index for freelist entries. Objects are indexed 1 to
  // gcInfoIndex.
  GCSnapshotInfo info(GCInfoTable::gcInfoIndex() + 1);
  String threadDumpName = String::format("blink_gc/thread_%lu",
                                         static_cast<unsigned long>(m_thread));
  const String heapsDumpName = threadDumpName + "/heaps";
  const String classesDumpName = threadDumpName + "/classes";

  int numberOfHeapsReported = 0;
#define SNAPSHOT_HEAP(ArenaType)                                        \
  {                                                                     \
    numberOfHeapsReported++;                                            \
    switch (type) {                                                     \
      case SnapshotType::HeapSnapshot:                                  \
        m_arenas[BlinkGC::ArenaType##ArenaIndex]->takeSnapshot(         \
            heapsDumpName + "/" #ArenaType, info);                      \
        break;                                                          \
      case SnapshotType::FreelistSnapshot:                              \
        m_arenas[BlinkGC::ArenaType##ArenaIndex]->takeFreelistSnapshot( \
            heapsDumpName + "/" #ArenaType);                            \
        break;                                                          \
      default:                                                          \
        ASSERT_NOT_REACHED();                                           \
    }                                                                   \
  }

  SNAPSHOT_HEAP(NormalPage1);
  SNAPSHOT_HEAP(NormalPage2);
  SNAPSHOT_HEAP(NormalPage3);
  SNAPSHOT_HEAP(NormalPage4);
  SNAPSHOT_HEAP(EagerSweep);
  SNAPSHOT_HEAP(Vector1);
  SNAPSHOT_HEAP(Vector2);
  SNAPSHOT_HEAP(Vector3);
  SNAPSHOT_HEAP(Vector4);
  SNAPSHOT_HEAP(InlineVector);
  SNAPSHOT_HEAP(HashTable);
  SNAPSHOT_HEAP(LargeObject);
  FOR_EACH_TYPED_ARENA(SNAPSHOT_HEAP);

  ASSERT(numberOfHeapsReported == BlinkGC::NumberOfArenas);

#undef SNAPSHOT_HEAP

  if (type == SnapshotType::FreelistSnapshot)
    return;

  size_t totalLiveCount = 0;
  size_t totalDeadCount = 0;
  size_t totalLiveSize = 0;
  size_t totalDeadSize = 0;
  for (size_t gcInfoIndex = 1; gcInfoIndex <= GCInfoTable::gcInfoIndex();
       ++gcInfoIndex) {
    totalLiveCount += info.liveCount[gcInfoIndex];
    totalDeadCount += info.deadCount[gcInfoIndex];
    totalLiveSize += info.liveSize[gcInfoIndex];
    totalDeadSize += info.deadSize[gcInfoIndex];
  }

  base::trace_event::MemoryAllocatorDump* threadDump =
      BlinkGCMemoryDumpProvider::instance()
          ->createMemoryAllocatorDumpForCurrentGC(threadDumpName);
  threadDump->AddScalar("live_count", "objects", totalLiveCount);
  threadDump->AddScalar("dead_count", "objects", totalDeadCount);
  threadDump->AddScalar("live_size", "bytes", totalLiveSize);
  threadDump->AddScalar("dead_size", "bytes", totalDeadSize);

  base::trace_event::MemoryAllocatorDump* heapsDump =
      BlinkGCMemoryDumpProvider::instance()
          ->createMemoryAllocatorDumpForCurrentGC(heapsDumpName);
  base::trace_event::MemoryAllocatorDump* classesDump =
      BlinkGCMemoryDumpProvider::instance()
          ->createMemoryAllocatorDumpForCurrentGC(classesDumpName);
  BlinkGCMemoryDumpProvider::instance()
      ->currentProcessMemoryDump()
      ->AddOwnershipEdge(classesDump->guid(), heapsDump->guid());
}

void ThreadState::collectGarbage(BlinkGC::StackState stackState,
                                 BlinkGC::GCType gcType,
                                 BlinkGC::GCReason reason) {
  // Nested collectGarbage() invocations aren't supported.
  RELEASE_ASSERT(!isGCForbidden());
  completeSweep();

  GCForbiddenScope gcForbiddenScope(this);

  {
    // Access to the CrossThreadPersistentRegion has to be prevented while in
    // the marking phase because otherwise other threads may allocate or free
    // PersistentNodes and we can't handle that.
    CrossThreadPersistentRegion::LockScope persistentLock(
        ProcessHeap::crossThreadPersistentRegion());
    {
      SafePointScope safePointScope(stackState, this);

      std::unique_ptr<Visitor> visitor;
      if (gcType == BlinkGC::TakeSnapshot) {
        visitor = Visitor::create(this, Visitor::SnapshotMarking);
      } else {
        DCHECK(gcType == BlinkGC::GCWithSweep ||
               gcType == BlinkGC::GCWithoutSweep);
        if (heap().compaction()->shouldCompact(this, gcType, reason)) {
          heap().compaction()->initialize(this);
          visitor = Visitor::create(this, Visitor::GlobalMarkingWithCompaction);
        } else {
          visitor = Visitor::create(this, Visitor::GlobalMarking);
        }
      }

      ScriptForbiddenIfMainThreadScope scriptForbidden;

      TRACE_EVENT2("blink_gc,devtools.timeline", "BlinkGCMarking",
                   "lazySweeping", gcType == BlinkGC::GCWithoutSweep,
                   "gcReason", gcReasonString(reason));
      double startTime = WTF::currentTimeMS();

      if (gcType == BlinkGC::TakeSnapshot)
        BlinkGCMemoryDumpProvider::instance()->clearProcessDumpForCurrentGC();

      // Disallow allocation during garbage collection (but not during the
      // finalization that happens when the visitorScope is torn down).
      NoAllocationScope noAllocationScope(this);

      heap().commitCallbackStacks();
      heap().preGC();

      StackFrameDepthScope stackDepthScope(&heap().stackFrameDepth());

      size_t totalObjectSize = heap().heapStats().allocatedObjectSize() +
                               heap().heapStats().markedObjectSize();
      if (gcType != BlinkGC::TakeSnapshot)
        heap().resetHeapCounters();

      {
        // 1. Trace persistent roots.
        heap().visitPersistentRoots(visitor.get());

        // 2. Trace objects reachable from the stack.  We do this independent of
        // the
        // given stackState since other threads might have a different stack
        // state.
        heap().visitStackRoots(visitor.get());

        // 3. Transitive closure to trace objects including ephemerons.
        heap().processMarkingStack(visitor.get());

        heap().postMarkingProcessing(visitor.get());
        heap().weakProcessing(visitor.get());
      }

      double markingTimeInMilliseconds = WTF::currentTimeMS() - startTime;
      heap().heapStats().setEstimatedMarkingTimePerByte(
          totalObjectSize ? (markingTimeInMilliseconds / 1000 / totalObjectSize)
                          : 0);

#if PRINT_HEAP_STATS
      dataLogF(
          "ThreadHeap::collectGarbage (gcReason=%s, lazySweeping=%d, "
          "time=%.1lfms)\n",
          gcReasonString(reason), gcType == BlinkGC::GCWithoutSweep,
          markingTimeInMilliseconds);
#endif

      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, markingTimeHistogram,
          new CustomCountHistogram("BlinkGC.CollectGarbage", 0, 10 * 1000, 50));
      markingTimeHistogram.count(markingTimeInMilliseconds);
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, totalObjectSpaceHistogram,
          new CustomCountHistogram("BlinkGC.TotalObjectSpace", 0,
                                   4 * 1024 * 1024, 50));
      totalObjectSpaceHistogram.count(ProcessHeap::totalAllocatedObjectSize() /
                                      1024);
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, totalAllocatedSpaceHistogram,
          new CustomCountHistogram("BlinkGC.TotalAllocatedSpace", 0,
                                   4 * 1024 * 1024, 50));
      totalAllocatedSpaceHistogram.count(ProcessHeap::totalAllocatedSpace() /
                                         1024);
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          EnumerationHistogram, gcReasonHistogram,
          new EnumerationHistogram("BlinkGC.GCReason",
                                   BlinkGC::LastGCReason + 1));
      gcReasonHistogram.count(reason);

      heap().m_lastGCReason = reason;

      ThreadHeap::reportMemoryUsageHistogram();
      WTF::Partitions::reportMemoryUsageHistogram();
    }
    heap().postGC(gcType);
  }

  heap().preSweep(gcType);
  heap().decommitCallbackStacks();
}

void ThreadState::collectAllGarbage() {
  // We need to run multiple GCs to collect a chain of persistent handles.
  size_t previousLiveObjects = 0;
  for (int i = 0; i < 5; ++i) {
    collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep,
                   BlinkGC::ForcedGC);
    size_t liveObjects = heap().heapStats().markedObjectSize();
    if (liveObjects == previousLiveObjects)
      break;
    previousLiveObjects = liveObjects;
  }
}

}  // namespace blink
