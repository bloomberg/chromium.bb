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

#include "base/sys_info.h"
#include "platform/Histogram.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/heap/BlinkGCMemoryDumpProvider.h"
#include "platform/heap/CallbackStack.h"
#include "platform/heap/MarkingVisitor.h"
#include "platform/heap/PageMemory.h"
#include "platform/heap/PagePool.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/ThreadState.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMemoryAllocatorDump.h"
#include "public/platform/WebProcessMemoryDump.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/DataLog.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/allocator/Partitions.h"

namespace blink {

HeapAllocHooks::AllocationHook* HeapAllocHooks::m_allocationHook = nullptr;
HeapAllocHooks::FreeHook* HeapAllocHooks::m_freeHook = nullptr;

class ParkThreadsScope final {
    STACK_ALLOCATED();
public:
    ParkThreadsScope()
        : m_shouldResumeThreads(false)
    {
    }

    bool parkThreads(ThreadState* state)
    {
        TRACE_EVENT0("blink_gc", "ThreadHeap::ParkThreadsScope");
        const char* samplingState = TRACE_EVENT_GET_SAMPLING_STATE();
        if (state->isMainThread())
            TRACE_EVENT_SET_SAMPLING_STATE("blink_gc", "BlinkGCWaiting");

        // TODO(haraken): In an unlikely coincidence that two threads decide
        // to collect garbage at the same time, avoid doing two GCs in
        // a row and return false.
        double startTime = WTF::currentTimeMS();

        m_shouldResumeThreads = ThreadState::stopThreads();

        double timeForStoppingThreads = WTF::currentTimeMS() - startTime;
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, timeToStopThreadsHistogram, new CustomCountHistogram("BlinkGC.TimeForStoppingThreads", 1, 1000, 50));
        timeToStopThreadsHistogram.count(timeForStoppingThreads);

        if (state->isMainThread())
            TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(samplingState);
        return m_shouldResumeThreads;
    }

    ~ParkThreadsScope()
    {
        // Only cleanup if we parked all threads in which case the GC happened
        // and we need to resume the other threads.
        if (m_shouldResumeThreads)
            ThreadState::resumeThreads();
    }

private:
    bool m_shouldResumeThreads;
};

void ThreadHeap::flushHeapDoesNotContainCache()
{
    s_heapDoesNotContainCache->flush();
}

void ProcessHeap::init()
{
    s_totalAllocatedSpace = 0;
    s_totalAllocatedObjectSize = 0;
    s_totalMarkedObjectSize = 0;
    s_isLowEndDevice = base::SysInfo::IsLowEndDevice();
}

void ProcessHeap::resetHeapCounters()
{
    s_totalAllocatedObjectSize = 0;
    s_totalMarkedObjectSize = 0;
}

void ThreadHeap::init()
{
    ThreadState::init();
    ProcessHeap::init();
    s_markingStack = new CallbackStack();
    s_postMarkingCallbackStack = new CallbackStack();
    s_globalWeakCallbackStack = new CallbackStack();
    // Use smallest supported block size for ephemerons.
    s_ephemeronStack = new CallbackStack(CallbackStack::kMinimalBlockSize);
    s_heapDoesNotContainCache = new HeapDoesNotContainCache();
    s_freePagePool = new FreePagePool();
    s_orphanedPagePool = new OrphanedPagePool();
    s_lastGCReason = BlinkGC::NumberOfGCReason;

    GCInfoTable::init();

    if (Platform::current() && Platform::current()->currentThread())
        Platform::current()->registerMemoryDumpProvider(BlinkGCMemoryDumpProvider::instance(), "BlinkGC");
}

void ThreadHeap::shutdown()
{
    ASSERT(s_markingStack);

    if (Platform::current() && Platform::current()->currentThread())
        Platform::current()->unregisterMemoryDumpProvider(BlinkGCMemoryDumpProvider::instance());

    // The main thread must be the last thread that gets detached.
    RELEASE_ASSERT(ThreadState::attachedThreads().size() == 0);

    delete s_heapDoesNotContainCache;
    s_heapDoesNotContainCache = nullptr;
    delete s_freePagePool;
    s_freePagePool = nullptr;
    delete s_orphanedPagePool;
    s_orphanedPagePool = nullptr;
    delete s_globalWeakCallbackStack;
    s_globalWeakCallbackStack = nullptr;
    delete s_postMarkingCallbackStack;
    s_postMarkingCallbackStack = nullptr;
    delete s_markingStack;
    s_markingStack = nullptr;
    delete s_ephemeronStack;
    s_ephemeronStack = nullptr;
    GCInfoTable::shutdown();
    ThreadState::shutdown();
    ASSERT(ThreadHeap::heapStats().allocatedSpace() == 0);
}

CrossThreadPersistentRegion& ProcessHeap::crossThreadPersistentRegion()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CrossThreadPersistentRegion, persistentRegion, new CrossThreadPersistentRegion());
    return persistentRegion;
}

bool ProcessHeap::s_isLowEndDevice = false;
size_t ProcessHeap::s_totalAllocatedSpace = 0;
size_t ProcessHeap::s_totalAllocatedObjectSize = 0;
size_t ProcessHeap::s_totalMarkedObjectSize = 0;

ThreadHeapStats::ThreadHeapStats()
    : m_allocatedSpace(0)
    , m_allocatedObjectSize(0)
    , m_objectSizeAtLastGC(0)
    , m_markedObjectSize(0)
    , m_markedObjectSizeAtLastCompleteSweep(0)
    , m_wrapperCount(0)
    , m_wrapperCountAtLastGC(0)
    , m_collectedWrapperCount(0)
    , m_partitionAllocSizeAtLastGC(WTF::Partitions::totalSizeOfCommittedPages())
    , m_estimatedMarkingTimePerByte(0.0)
{
}

double ThreadHeapStats::estimatedMarkingTime()
{
    // Use 8 ms as initial estimated marking time.
    // 8 ms is long enough for low-end mobile devices to mark common
    // real-world object graphs.
    if (m_estimatedMarkingTimePerByte == 0)
        return 0.008;

    // Assuming that the collection rate of this GC will be mostly equal to
    // the collection rate of the last GC, estimate the marking time of this GC.
    return m_estimatedMarkingTimePerByte * (allocatedObjectSize() + markedObjectSize());
}

void ThreadHeapStats::reset()
{
    m_objectSizeAtLastGC = m_allocatedObjectSize + m_markedObjectSize;
    m_partitionAllocSizeAtLastGC = WTF::Partitions::totalSizeOfCommittedPages();
    m_allocatedObjectSize = 0;
    m_markedObjectSize = 0;
    m_wrapperCountAtLastGC = m_wrapperCount;
    m_collectedWrapperCount = 0;
}

void ThreadHeapStats::increaseAllocatedObjectSize(size_t delta)
{
    atomicAdd(&m_allocatedObjectSize, static_cast<long>(delta));
    ProcessHeap::increaseTotalAllocatedObjectSize(delta);
}

void ThreadHeapStats::decreaseAllocatedObjectSize(size_t delta)
{
    atomicSubtract(&m_allocatedObjectSize, static_cast<long>(delta));
    ProcessHeap::decreaseTotalAllocatedObjectSize(delta);
}

void ThreadHeapStats::increaseMarkedObjectSize(size_t delta)
{
    atomicAdd(&m_markedObjectSize, static_cast<long>(delta));
    ProcessHeap::increaseTotalMarkedObjectSize(delta);
}

void ThreadHeapStats::increaseAllocatedSpace(size_t delta)
{
    atomicAdd(&m_allocatedSpace, static_cast<long>(delta));
    ProcessHeap::increaseTotalAllocatedSpace(delta);
}

void ThreadHeapStats::decreaseAllocatedSpace(size_t delta)
{
    atomicSubtract(&m_allocatedSpace, static_cast<long>(delta));
    ProcessHeap::decreaseTotalAllocatedSpace(delta);
}

#if ENABLE(ASSERT)
BasePage* ThreadHeap::findPageFromAddress(Address address)
{
    MutexLocker lock(ThreadState::threadAttachMutex());
    for (ThreadState* state : ThreadState::attachedThreads()) {
        if (BasePage* page = state->findPageFromAddress(address))
            return page;
    }
    return nullptr;
}
#endif

Address ThreadHeap::checkAndMarkPointer(Visitor* visitor, Address address)
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

void ThreadHeap::pushTraceCallback(void* object, TraceCallback callback)
{
    ASSERT(ThreadState::current()->isInGC());

    // Trace should never reach an orphaned page.
    ASSERT(!ThreadHeap::getOrphanedPagePool()->contains(object));
    CallbackStack::Item* slot = s_markingStack->allocateEntry();
    *slot = CallbackStack::Item(object, callback);
}

bool ThreadHeap::popAndInvokeTraceCallback(Visitor* visitor)
{
    CallbackStack::Item* item = s_markingStack->pop();
    if (!item)
        return false;
    item->call(visitor);
    return true;
}

void ThreadHeap::pushPostMarkingCallback(void* object, TraceCallback callback)
{
    ASSERT(ThreadState::current()->isInGC());

    // Trace should never reach an orphaned page.
    ASSERT(!ThreadHeap::getOrphanedPagePool()->contains(object));
    CallbackStack::Item* slot = s_postMarkingCallbackStack->allocateEntry();
    *slot = CallbackStack::Item(object, callback);
}

bool ThreadHeap::popAndInvokePostMarkingCallback(Visitor* visitor)
{
    if (CallbackStack::Item* item = s_postMarkingCallbackStack->pop()) {
        item->call(visitor);
        return true;
    }
    return false;
}

void ThreadHeap::pushGlobalWeakCallback(void** cell, WeakCallback callback)
{
    ASSERT(ThreadState::current()->isInGC());

    // Trace should never reach an orphaned page.
    ASSERT(!ThreadHeap::getOrphanedPagePool()->contains(cell));
    CallbackStack::Item* slot = s_globalWeakCallbackStack->allocateEntry();
    *slot = CallbackStack::Item(cell, callback);
}

void ThreadHeap::pushThreadLocalWeakCallback(void* closure, void* object, WeakCallback callback)
{
    ASSERT(ThreadState::current()->isInGC());

    // Trace should never reach an orphaned page.
    ASSERT(!ThreadHeap::getOrphanedPagePool()->contains(object));
    ThreadState* state = pageFromObject(object)->arena()->getThreadState();
    state->pushThreadLocalWeakCallback(closure, callback);
}

bool ThreadHeap::popAndInvokeGlobalWeakCallback(Visitor* visitor)
{
    if (CallbackStack::Item* item = s_globalWeakCallbackStack->pop()) {
        item->call(visitor);
        return true;
    }
    return false;
}

void ThreadHeap::registerWeakTable(void* table, EphemeronCallback iterationCallback, EphemeronCallback iterationDoneCallback)
{
    ASSERT(ThreadState::current()->isInGC());

    // Trace should never reach an orphaned page.
    ASSERT(!ThreadHeap::getOrphanedPagePool()->contains(table));
    CallbackStack::Item* slot = s_ephemeronStack->allocateEntry();
    *slot = CallbackStack::Item(table, iterationCallback);

    // Register a post-marking callback to tell the tables that
    // ephemeron iteration is complete.
    pushPostMarkingCallback(table, iterationDoneCallback);
}

#if ENABLE(ASSERT)
bool ThreadHeap::weakTableRegistered(const void* table)
{
    ASSERT(s_ephemeronStack);
    return s_ephemeronStack->hasCallbackForObject(table);
}
#endif

void ThreadHeap::decommitCallbackStacks()
{
    s_markingStack->decommit();
    s_postMarkingCallbackStack->decommit();
    s_globalWeakCallbackStack->decommit();
    s_ephemeronStack->decommit();
}

void ThreadHeap::preGC()
{
    ASSERT(!ThreadState::current()->isInGC());
    for (ThreadState* state : ThreadState::attachedThreads())
        state->preGC();
}

void ThreadHeap::postGC(BlinkGC::GCType gcType)
{
    ASSERT(ThreadState::current()->isInGC());
    for (ThreadState* state : ThreadState::attachedThreads())
        state->postGC(gcType);
}

const char* ThreadHeap::gcReasonString(BlinkGC::GCReason reason)
{
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
    default:
        ASSERT_NOT_REACHED();
    }
    return "<Unknown>";
}

void ThreadHeap::collectGarbage(BlinkGC::StackState stackState, BlinkGC::GCType gcType, BlinkGC::GCReason reason)
{
    ASSERT(gcType != BlinkGC::ThreadTerminationGC);

    ThreadState* state = ThreadState::current();
    // Nested collectGarbage() invocations aren't supported.
    RELEASE_ASSERT(!state->isGCForbidden());
    state->completeSweep();

    OwnPtr<Visitor> visitor = Visitor::create(state, gcType);

    SafePointScope safePointScope(stackState, state);

    // Resume all parked threads upon leaving this scope.
    ParkThreadsScope parkThreadsScope;

    // Try to park the other threads. If we're unable to, bail out of the GC.
    if (!parkThreadsScope.parkThreads(state))
        return;

    ScriptForbiddenIfMainThreadScope scriptForbidden;

    TRACE_EVENT2("blink_gc,devtools.timeline", "BlinkGCMarking",
        "lazySweeping", gcType == BlinkGC::GCWithoutSweep,
        "gcReason", gcReasonString(reason));
    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink_gc", "BlinkGC");
    double startTime = WTF::currentTimeMS();

    if (gcType == BlinkGC::TakeSnapshot)
        BlinkGCMemoryDumpProvider::instance()->clearProcessDumpForCurrentGC();

    // Disallow allocation during garbage collection (but not during the
    // finalization that happens when the visitorScope is torn down).
    ThreadState::NoAllocationScope noAllocationScope(state);

    preGC();

    StackFrameDepthScope stackDepthScope;

    size_t totalObjectSize = ThreadHeap::heapStats().allocatedObjectSize() + ThreadHeap::heapStats().markedObjectSize();
    if (gcType != BlinkGC::TakeSnapshot)
        ThreadHeap::resetHeapCounters();

    // 1. Trace persistent roots.
    ThreadState::visitPersistentRoots(visitor.get());

    // 2. Trace objects reachable from the stack.  We do this independent of the
    // given stackState since other threads might have a different stack state.
    ThreadState::visitStackRoots(visitor.get());

    // 3. Transitive closure to trace objects including ephemerons.
    processMarkingStack(visitor.get());

    postMarkingProcessing(visitor.get());
    globalWeakProcessing(visitor.get());

    // Now we can delete all orphaned pages because there are no dangling
    // pointers to the orphaned pages.  (If we have such dangling pointers,
    // we should have crashed during marking before getting here.)
    getOrphanedPagePool()->decommitOrphanedPages();

    double markingTimeInMilliseconds = WTF::currentTimeMS() - startTime;
    ThreadHeap::heapStats().setEstimatedMarkingTimePerByte(totalObjectSize ? (markingTimeInMilliseconds / 1000 / totalObjectSize) : 0);

#if PRINT_HEAP_STATS
    dataLogF("ThreadHeap::collectGarbage (gcReason=%s, lazySweeping=%d, time=%.1lfms)\n", gcReasonString(reason), gcType == BlinkGC::GCWithoutSweep, markingTimeInMilliseconds);
#endif

    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, markingTimeHistogram, new CustomCountHistogram("BlinkGC.CollectGarbage", 0, 10 * 1000, 50));
    markingTimeHistogram.count(markingTimeInMilliseconds);
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, totalObjectSpaceHistogram, new CustomCountHistogram("BlinkGC.TotalObjectSpace", 0, 4 * 1024 * 1024, 50));
    totalObjectSpaceHistogram.count(ProcessHeap::totalAllocatedObjectSize() / 1024);
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, totalAllocatedSpaceHistogram, new CustomCountHistogram("BlinkGC.TotalAllocatedSpace", 0, 4 * 1024 * 1024, 50));
    totalAllocatedSpaceHistogram.count(ProcessHeap::totalAllocatedSpace() / 1024);
    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, gcReasonHistogram, new EnumerationHistogram("BlinkGC.GCReason", BlinkGC::NumberOfGCReason));
    gcReasonHistogram.count(reason);

    s_lastGCReason = reason;

    ThreadHeap::reportMemoryUsageHistogram();
    WTF::Partitions::reportMemoryUsageHistogram();

    postGC(gcType);
    ThreadHeap::decommitCallbackStacks();
}

void ThreadHeap::collectGarbageForTerminatingThread(ThreadState* state)
{
    {
        // A thread-specific termination GC must not allow other global GCs to go
        // ahead while it is running, hence the termination GC does not enter a
        // safepoint. VisitorScope will not enter also a safepoint scope for
        // ThreadTerminationGC.
        OwnPtr<Visitor> visitor = Visitor::create(state, BlinkGC::ThreadTerminationGC);

        ThreadState::NoAllocationScope noAllocationScope(state);

        state->preGC();

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
        state->visitPersistents(visitor.get());

        // 2. Trace objects reachable from the thread's persistent roots
        // including ephemerons.
        processMarkingStack(visitor.get());

        postMarkingProcessing(visitor.get());
        globalWeakProcessing(visitor.get());

        state->postGC(BlinkGC::GCWithSweep);
        ThreadHeap::decommitCallbackStacks();
    }
    state->preSweep();
}

void ThreadHeap::processMarkingStack(Visitor* visitor)
{
    // Ephemeron fixed point loop.
    do {
        {
            // Iteratively mark all objects that are reachable from the objects
            // currently pushed onto the marking stack.
            TRACE_EVENT0("blink_gc", "ThreadHeap::processMarkingStackSingleThreaded");
            while (popAndInvokeTraceCallback(visitor)) { }
        }

        {
            // Mark any strong pointers that have now become reachable in
            // ephemeron maps.
            TRACE_EVENT0("blink_gc", "ThreadHeap::processEphemeronStack");
            s_ephemeronStack->invokeEphemeronCallbacks(visitor);
        }

        // Rerun loop if ephemeron processing queued more objects for tracing.
    } while (!s_markingStack->isEmpty());
}

void ThreadHeap::postMarkingProcessing(Visitor* visitor)
{
    TRACE_EVENT0("blink_gc", "ThreadHeap::postMarkingProcessing");
    // Call post-marking callbacks including:
    // 1. the ephemeronIterationDone callbacks on weak tables to do cleanup
    //    (specifically to clear the queued bits for weak hash tables), and
    // 2. the markNoTracing callbacks on collection backings to mark them
    //    if they are only reachable from their front objects.
    while (popAndInvokePostMarkingCallback(visitor)) { }

    // Post-marking callbacks should not trace any objects and
    // therefore the marking stack should be empty after the
    // post-marking callbacks.
    ASSERT(s_markingStack->isEmpty());
}

void ThreadHeap::globalWeakProcessing(Visitor* visitor)
{
    TRACE_EVENT0("blink_gc", "ThreadHeap::globalWeakProcessing");
    double startTime = WTF::currentTimeMS();

    // Call weak callbacks on objects that may now be pointing to dead objects.
    while (popAndInvokeGlobalWeakCallback(visitor)) { }

    // It is not permitted to trace pointers of live objects in the weak
    // callback phase, so the marking stack should still be empty here.
    ASSERT(s_markingStack->isEmpty());

    double timeForGlobalWeakProcessing = WTF::currentTimeMS() - startTime;
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, globalWeakTimeHistogram, new CustomCountHistogram("BlinkGC.TimeForGlobalWeakProcessing", 1, 10 * 1000, 50));
    globalWeakTimeHistogram.count(timeForGlobalWeakProcessing);
}

void ThreadHeap::collectAllGarbage()
{
    // We need to run multiple GCs to collect a chain of persistent handles.
    size_t previousLiveObjects = 0;
    for (int i = 0; i < 5; ++i) {
        collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);
        size_t liveObjects = ThreadHeap::heapStats().markedObjectSize();
        if (liveObjects == previousLiveObjects)
            break;
        previousLiveObjects = liveObjects;
    }
}

void ThreadHeap::reportMemoryUsageHistogram()
{
    static size_t supportedMaxSizeInMB = 4 * 1024;
    static size_t observedMaxSizeInMB = 0;

    // We only report the memory in the main thread.
    if (!isMainThread())
        return;
    // +1 is for rounding up the sizeInMB.
    size_t sizeInMB = ThreadHeap::heapStats().allocatedSpace() / 1024 / 1024 + 1;
    if (sizeInMB >= supportedMaxSizeInMB)
        sizeInMB = supportedMaxSizeInMB - 1;
    if (sizeInMB > observedMaxSizeInMB) {
        // Send a UseCounter only when we see the highest memory usage
        // we've ever seen.
        DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, commitedSizeHistogram, new EnumerationHistogram("BlinkGC.CommittedSize", supportedMaxSizeInMB));
        commitedSizeHistogram.count(sizeInMB);
        observedMaxSizeInMB = sizeInMB;
    }
}

void ThreadHeap::reportMemoryUsageForTracing()
{
#if PRINT_HEAP_STATS
    // dataLogF("allocatedSpace=%ldMB, allocatedObjectSize=%ldMB, markedObjectSize=%ldMB, partitionAllocSize=%ldMB, wrapperCount=%ld, collectedWrapperCount=%ld\n", ThreadHeap::allocatedSpace() / 1024 / 1024, ThreadHeap::allocatedObjectSize() / 1024 / 1024, ThreadHeap::markedObjectSize() / 1024 / 1024, WTF::Partitions::totalSizeOfCommittedPages() / 1024 / 1024, ThreadHeap::wrapperCount(), ThreadHeap::collectedWrapperCount());
#endif

    bool gcTracingEnabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED("blink_gc", &gcTracingEnabled);
    if (!gcTracingEnabled)
        return;

    // These values are divided by 1024 to avoid overflow in practical cases (TRACE_COUNTER values are 32-bit ints).
    // They are capped to INT_MAX just in case.
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::allocatedObjectSizeKB", std::min(ThreadHeap::heapStats().allocatedObjectSize() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::markedObjectSizeKB", std::min(ThreadHeap::heapStats().markedObjectSize() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::markedObjectSizeAtLastCompleteSweepKB", std::min(ThreadHeap::heapStats().markedObjectSizeAtLastCompleteSweep() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::allocatedSpaceKB", std::min(ThreadHeap::heapStats().allocatedSpace() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::objectSizeAtLastGCKB", std::min(ThreadHeap::heapStats().objectSizeAtLastGC() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::wrapperCount", std::min(ThreadHeap::heapStats().wrapperCount(), static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::heapStats().wrapperCountAtLastGC", std::min(ThreadHeap::heapStats().wrapperCountAtLastGC(), static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::collectedWrapperCount", std::min(ThreadHeap::heapStats().collectedWrapperCount(), static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadHeap::partitionAllocSizeAtLastGCKB", std::min(ThreadHeap::heapStats().partitionAllocSizeAtLastGC() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "Partitions::totalSizeOfCommittedPagesKB", std::min(WTF::Partitions::totalSizeOfCommittedPages() / 1024, static_cast<size_t>(INT_MAX)));
}

size_t ThreadHeap::objectPayloadSizeForTesting()
{
    size_t objectPayloadSize = 0;
    for (ThreadState* state : ThreadState::attachedThreads()) {
        state->setGCState(ThreadState::GCRunning);
        state->makeConsistentForGC();
        objectPayloadSize += state->objectPayloadSizeForTesting();
        state->setGCState(ThreadState::EagerSweepScheduled);
        state->setGCState(ThreadState::Sweeping);
        state->setGCState(ThreadState::NoGCScheduled);
    }
    return objectPayloadSize;
}

RegionTree* ThreadHeap::getRegionTree()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(RegionTree, tree, new RegionTree);
    return &tree;
}

BasePage* ThreadHeap::lookup(Address address)
{
    ASSERT(ThreadState::current()->isInGC());
    if (PageMemoryRegion* region = ThreadHeap::getRegionTree()->lookup(address)) {
        BasePage* page = region->pageFromAddress(address);
        return page && !page->orphaned() ? page : nullptr;
    }
    return nullptr;
}

void ThreadHeap::resetHeapCounters()
{
    ASSERT(ThreadState::current()->isInGC());

    ThreadHeap::reportMemoryUsageForTracing();

    ProcessHeap::resetHeapCounters();
    ThreadHeap::heapStats().reset();
    for (ThreadState* state : ThreadState::attachedThreads())
        state->resetHeapCounters();
}

ThreadHeapStats& ThreadHeap::heapStats()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadHeapStats, stats, new ThreadHeapStats());
    return stats;
}

CallbackStack* ThreadHeap::s_markingStack;
CallbackStack* ThreadHeap::s_postMarkingCallbackStack;
CallbackStack* ThreadHeap::s_globalWeakCallbackStack;
CallbackStack* ThreadHeap::s_ephemeronStack;
HeapDoesNotContainCache* ThreadHeap::s_heapDoesNotContainCache;
FreePagePool* ThreadHeap::s_freePagePool;
OrphanedPagePool* ThreadHeap::s_orphanedPagePool;

BlinkGC::GCReason ThreadHeap::s_lastGCReason = BlinkGC::NumberOfGCReason;

} // namespace blink
