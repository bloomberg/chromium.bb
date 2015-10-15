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
#include "wtf/DataLog.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/MainThread.h"
#include "wtf/Partitions.h"
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

namespace blink {

class GCForbiddenScope final {
public:
    explicit GCForbiddenScope(ThreadState* state)
        : m_state(state)
    {
        // Prevent nested collectGarbage() invocations.
        m_state->enterGCForbiddenScope();
    }

    ~GCForbiddenScope()
    {
        m_state->leaveGCForbiddenScope();
    }

private:
    ThreadState* m_state;
};

class GCScope final {
public:
    GCScope(ThreadState* state, BlinkGC::StackState stackState, BlinkGC::GCType gcType)
        : m_state(state)
        , m_gcForbiddenScope(state)
        // See collectGarbageForTerminatingThread() comment on why a
        // safepoint scope isn't entered for its GCScope.
        , m_safePointScope(stackState, gcType != BlinkGC::ThreadTerminationGC ? state : nullptr)
        , m_gcType(gcType)
        , m_parkedAllThreads(false)
    {
        TRACE_EVENT0("blink_gc", "Heap::GCScope");
        const char* samplingState = TRACE_EVENT_GET_SAMPLING_STATE();
        if (m_state->isMainThread())
            TRACE_EVENT_SET_SAMPLING_STATE("blink_gc", "BlinkGCWaiting");

        ASSERT(m_state->checkThread());

        // TODO(haraken): In an unlikely coincidence that two threads decide
        // to collect garbage at the same time, avoid doing two GCs in
        // a row.
        if (LIKELY(gcType != BlinkGC::ThreadTerminationGC && ThreadState::stopThreads()))
            m_parkedAllThreads = true;

        switch (gcType) {
        case BlinkGC::GCWithSweep:
        case BlinkGC::GCWithoutSweep:
            m_visitor = adoptPtr(new MarkingVisitor<Visitor::GlobalMarking>());
            break;
        case BlinkGC::TakeSnapshot:
            m_visitor = adoptPtr(new MarkingVisitor<Visitor::SnapshotMarking>());
            break;
        case BlinkGC::ThreadTerminationGC:
            m_visitor = adoptPtr(new MarkingVisitor<Visitor::ThreadLocalMarking>());
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        if (m_state->isMainThread())
            TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(samplingState);
    }

    bool allThreadsParked() const { return m_parkedAllThreads; }
    Visitor* visitor() const { return m_visitor.get(); }

    ~GCScope()
    {
        // Only cleanup if we parked all threads in which case the GC happened
        // and we need to resume the other threads.
        if (LIKELY(m_gcType != BlinkGC::ThreadTerminationGC && m_parkedAllThreads))
            ThreadState::resumeThreads();
    }

private:
    ThreadState* m_state;
    // The ordering of the two scope objects matters: GCs must first be forbidden
    // before entering the safe point scope. Prior to reaching the safe point,
    // ThreadState::runScheduledGC() is called. See its comment why we need
    // to be in a GC forbidden scope when doing so.
    GCForbiddenScope m_gcForbiddenScope;
    SafePointScope m_safePointScope;
    BlinkGC::GCType m_gcType;
    OwnPtr<Visitor> m_visitor;
    bool m_parkedAllThreads; // False if we fail to park all threads
};

void Heap::flushHeapDoesNotContainCache()
{
    s_heapDoesNotContainCache->flush();
}

void Heap::init()
{
    ThreadState::init();
    s_markingStack = new CallbackStack();
    s_postMarkingCallbackStack = new CallbackStack();
    s_globalWeakCallbackStack = new CallbackStack();
    s_ephemeronStack = new CallbackStack();
    s_heapDoesNotContainCache = new HeapDoesNotContainCache();
    s_freePagePool = new FreePagePool();
    s_orphanedPagePool = new OrphanedPagePool();
    s_allocatedSpace = 0;
    s_allocatedObjectSize = 0;
    s_objectSizeAtLastGC = 0;
    s_markedObjectSize = 0;
    s_markedObjectSizeAtLastCompleteSweep = 0;
    s_wrapperCount = 0;
    s_wrapperCountAtLastGC = 0;
    s_collectedWrapperCount = 0;
    s_partitionAllocSizeAtLastGC = WTF::Partitions::totalSizeOfCommittedPages();
    s_estimatedMarkingTimePerByte = 0.0;

    GCInfoTable::init();

    if (Platform::current() && Platform::current()->currentThread())
        Platform::current()->registerMemoryDumpProvider(BlinkGCMemoryDumpProvider::instance());
}

void Heap::shutdown()
{
    if (Platform::current() && Platform::current()->currentThread())
        Platform::current()->unregisterMemoryDumpProvider(BlinkGCMemoryDumpProvider::instance());
    s_shutdownCalled = true;
    ThreadState::shutdownHeapIfNecessary();
}

void Heap::doShutdown()
{
    // We don't want to call doShutdown() twice.
    if (!s_markingStack)
        return;

    ASSERT(!ThreadState::attachedThreads().size());
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
    delete s_regionTree;
    s_regionTree = nullptr;
    GCInfoTable::shutdown();
    ThreadState::shutdown();
    ASSERT(Heap::allocatedSpace() == 0);
}

#if ENABLE(ASSERT)
BasePage* Heap::findPageFromAddress(Address address)
{
    MutexLocker lock(ThreadState::threadAttachMutex());
    for (ThreadState* state : ThreadState::attachedThreads()) {
        if (BasePage* page = state->findPageFromAddress(address))
            return page;
    }
    return nullptr;
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
    ASSERT(ThreadState::current()->isInGC());

    // Trace should never reach an orphaned page.
    ASSERT(!Heap::orphanedPagePool()->contains(object));
    CallbackStack::Item* slot = s_markingStack->allocateEntry();
    *slot = CallbackStack::Item(object, callback);
}

bool Heap::popAndInvokeTraceCallback(Visitor* visitor)
{
    CallbackStack::Item* item = s_markingStack->pop();
    if (!item)
        return false;
    item->call(visitor);
    return true;
}

void Heap::pushPostMarkingCallback(void* object, TraceCallback callback)
{
    ASSERT(ThreadState::current()->isInGC());

    // Trace should never reach an orphaned page.
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

void Heap::pushGlobalWeakCallback(void** cell, WeakCallback callback)
{
    ASSERT(ThreadState::current()->isInGC());

    // Trace should never reach an orphaned page.
    ASSERT(!Heap::orphanedPagePool()->contains(cell));
    CallbackStack::Item* slot = s_globalWeakCallbackStack->allocateEntry();
    *slot = CallbackStack::Item(cell, callback);
}

void Heap::pushThreadLocalWeakCallback(void* closure, void* object, WeakCallback callback)
{
    ASSERT(ThreadState::current()->isInGC());

    // Trace should never reach an orphaned page.
    ASSERT(!Heap::orphanedPagePool()->contains(object));
    ThreadState* state = pageFromObject(object)->heap()->threadState();
    state->pushThreadLocalWeakCallback(closure, callback);
}

bool Heap::popAndInvokeGlobalWeakCallback(Visitor* visitor)
{
    if (CallbackStack::Item* item = s_globalWeakCallbackStack->pop()) {
        item->call(visitor);
        return true;
    }
    return false;
}

void Heap::registerWeakTable(void* table, EphemeronCallback iterationCallback, EphemeronCallback iterationDoneCallback)
{
    ASSERT(ThreadState::current()->isInGC());

    // Trace should never reach an orphaned page.
    ASSERT(!Heap::orphanedPagePool()->contains(table));
    CallbackStack::Item* slot = s_ephemeronStack->allocateEntry();
    *slot = CallbackStack::Item(table, iterationCallback);

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

void Heap::postGC(BlinkGC::GCType gcType)
{
    ASSERT(ThreadState::current()->isInGC());
    for (ThreadState* state : ThreadState::attachedThreads())
        state->postGC(gcType);
}

const char* Heap::gcReasonString(GCReason reason)
{
    switch (reason) {
#define STRINGIFY_REASON(reason) case reason: return #reason;
        STRINGIFY_REASON(IdleGC);
        STRINGIFY_REASON(PreciseGC);
        STRINGIFY_REASON(ConservativeGC);
        STRINGIFY_REASON(ForcedGC);
        STRINGIFY_REASON(MemoryPressureGC);
        STRINGIFY_REASON(PageNavigationGC);
#undef STRINGIFY_REASON
    case NumberOfGCReason: ASSERT_NOT_REACHED();
    }
    return "<Unknown>";
}

void Heap::collectGarbage(BlinkGC::StackState stackState, BlinkGC::GCType gcType, GCReason reason)
{
    ThreadState* state = ThreadState::current();
    // Nested collectGarbage() invocations aren't supported.
    RELEASE_ASSERT(!state->isGCForbidden());
    state->completeSweep();

    GCScope gcScope(state, stackState, gcType);
    // Check if we successfully parked the other threads.  If not we bail out of
    // the GC.
    if (!gcScope.allThreadsParked())
        return;

    if (state->isMainThread())
        ScriptForbiddenScope::enter();

    TRACE_EVENT2("blink_gc", "Heap::collectGarbage",
        "lazySweeping", gcType == BlinkGC::GCWithoutSweep,
        "gcReason", gcReasonString(reason));
    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink_gc", "BlinkGC");
    double timeStamp = WTF::currentTimeMS();

    if (gcType == BlinkGC::TakeSnapshot)
        BlinkGCMemoryDumpProvider::instance()->clearProcessDumpForCurrentGC();

    // Disallow allocation during garbage collection (but not during the
    // finalization that happens when the gcScope is torn down).
    ThreadState::NoAllocationScope noAllocationScope(state);

    preGC();

    StackFrameDepthScope stackDepthScope;

    size_t totalObjectSize = Heap::allocatedObjectSize() + Heap::markedObjectSize();
    if (gcType != BlinkGC::TakeSnapshot)
        Heap::resetHeapCounters();

    // 1. Trace persistent roots.
    ThreadState::visitPersistentRoots(gcScope.visitor());

    // 2. Trace objects reachable from the stack.  We do this independent of the
    // given stackState since other threads might have a different stack state.
    ThreadState::visitStackRoots(gcScope.visitor());

    // 3. Transitive closure to trace objects including ephemerons.
    processMarkingStack(gcScope.visitor());

    postMarkingProcessing(gcScope.visitor());
    globalWeakProcessing(gcScope.visitor());

    // Now we can delete all orphaned pages because there are no dangling
    // pointers to the orphaned pages.  (If we have such dangling pointers,
    // we should have crashed during marking before getting here.)
    orphanedPagePool()->decommitOrphanedPages();

    double markingTimeInMilliseconds = WTF::currentTimeMS() - timeStamp;
    s_estimatedMarkingTimePerByte = totalObjectSize ? (markingTimeInMilliseconds / 1000 / totalObjectSize) : 0;

#if PRINT_HEAP_STATS
    dataLogF("Heap::collectGarbage (gcReason=%s, lazySweeping=%d, time=%.1lfms)\n", gcReasonString(reason), gcType == BlinkGC::GCWithoutSweep, markingTimeInMilliseconds);
#endif

    Platform::current()->histogramCustomCounts("BlinkGC.CollectGarbage", markingTimeInMilliseconds, 0, 10 * 1000, 50);
    Platform::current()->histogramCustomCounts("BlinkGC.TotalObjectSpace", Heap::allocatedObjectSize() / 1024, 0, 4 * 1024 * 1024, 50);
    Platform::current()->histogramCustomCounts("BlinkGC.TotalAllocatedSpace", Heap::allocatedSpace() / 1024, 0, 4 * 1024 * 1024, 50);
    Platform::current()->histogramEnumeration("BlinkGC.GCReason", reason, NumberOfGCReason);
    Heap::reportMemoryUsageHistogram();
    WTF::Partitions::reportMemoryUsageHistogram();

    postGC(gcType);

    if (state->isMainThread())
        ScriptForbiddenScope::exit();
}

void Heap::collectGarbageForTerminatingThread(ThreadState* state)
{
    {
        // A thread-specific termination GC must not allow other global GCs to go
        // ahead while it is running, hence the termination GC does not enter a
        // safepoint. GCScope will not enter also a safepoint scope for
        // ThreadTerminationGC.
        GCScope gcScope(state, BlinkGC::NoHeapPointersOnStack, BlinkGC::ThreadTerminationGC);

        ThreadState::NoAllocationScope noAllocationScope(state);

        state->preGC();
        StackFrameDepthScope stackDepthScope;

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
        state->visitPersistents(gcScope.visitor());

        // 2. Trace objects reachable from the thread's persistent roots
        // including ephemerons.
        processMarkingStack(gcScope.visitor());

        postMarkingProcessing(gcScope.visitor());
        globalWeakProcessing(gcScope.visitor());

        state->postGC(BlinkGC::GCWithSweep);
    }
    state->preSweep();
}

void Heap::processMarkingStack(Visitor* visitor)
{
    // Ephemeron fixed point loop.
    do {
        {
            // Iteratively mark all objects that are reachable from the objects
            // currently pushed onto the marking stack.
            TRACE_EVENT0("blink_gc", "Heap::processMarkingStackSingleThreaded");
            while (popAndInvokeTraceCallback(visitor)) { }
        }

        {
            // Mark any strong pointers that have now become reachable in
            // ephemeron maps.
            TRACE_EVENT0("blink_gc", "Heap::processEphemeronStack");
            s_ephemeronStack->invokeEphemeronCallbacks(visitor);
        }

        // Rerun loop if ephemeron processing queued more objects for tracing.
    } while (!s_markingStack->isEmpty());
}

void Heap::postMarkingProcessing(Visitor* visitor)
{
    TRACE_EVENT0("blink_gc", "Heap::postMarkingProcessing");
    // Call post-marking callbacks including:
    // 1. the ephemeronIterationDone callbacks on weak tables to do cleanup
    //    (specifically to clear the queued bits for weak hash tables), and
    // 2. the markNoTracing callbacks on collection backings to mark them
    //    if they are only reachable from their front objects.
    while (popAndInvokePostMarkingCallback(visitor)) { }

    s_ephemeronStack->clear();

    // Post-marking callbacks should not trace any objects and
    // therefore the marking stack should be empty after the
    // post-marking callbacks.
    ASSERT(s_markingStack->isEmpty());
}

void Heap::globalWeakProcessing(Visitor* visitor)
{
    TRACE_EVENT0("blink_gc", "Heap::globalWeakProcessing");
    // Call weak callbacks on objects that may now be pointing to dead objects.
    while (popAndInvokeGlobalWeakCallback(visitor)) { }

    // It is not permitted to trace pointers of live objects in the weak
    // callback phase, so the marking stack should still be empty here.
    ASSERT(s_markingStack->isEmpty());
}

void Heap::collectAllGarbage()
{
    // We need to run multiple GCs to collect a chain of persistent handles.
    size_t previousLiveObjects = 0;
    for (int i = 0; i < 5; ++i) {
        collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, ForcedGC);
        size_t liveObjects = Heap::markedObjectSize();
        if (liveObjects == previousLiveObjects)
            break;
        previousLiveObjects = liveObjects;
    }
}

double Heap::estimatedMarkingTime()
{
    ASSERT(ThreadState::current()->isMainThread());

    // Use 8 ms as initial estimated marking time.
    // 8 ms is long enough for low-end mobile devices to mark common
    // real-world object graphs.
    if (s_estimatedMarkingTimePerByte == 0)
        return 0.008;

    // Assuming that the collection rate of this GC will be mostly equal to
    // the collection rate of the last GC, estimate the marking time of this GC.
    return s_estimatedMarkingTimePerByte * (Heap::allocatedObjectSize() + Heap::markedObjectSize());
}

void Heap::reportMemoryUsageHistogram()
{
    static size_t supportedMaxSizeInMB = 4 * 1024;
    static size_t observedMaxSizeInMB = 0;

    // We only report the memory in the main thread.
    if (!isMainThread())
        return;
    // +1 is for rounding up the sizeInMB.
    size_t sizeInMB = Heap::allocatedSpace() / 1024 / 1024 + 1;
    if (sizeInMB >= supportedMaxSizeInMB)
        sizeInMB = supportedMaxSizeInMB - 1;
    if (sizeInMB > observedMaxSizeInMB) {
        // Send a UseCounter only when we see the highest memory usage
        // we've ever seen.
        Platform::current()->histogramEnumeration("BlinkGC.CommittedSize", sizeInMB, supportedMaxSizeInMB);
        observedMaxSizeInMB = sizeInMB;
    }
}

void Heap::reportMemoryUsageForTracing()
{
#if PRINT_HEAP_STATS
    // dataLogF("allocatedSpace=%ldMB, allocatedObjectSize=%ldMB, markedObjectSize=%ldMB, partitionAllocSize=%ldMB, wrapperCount=%ld, collectedWrapperCount=%ld\n", Heap::allocatedSpace() / 1024 / 1024, Heap::allocatedObjectSize() / 1024 / 1024, Heap::markedObjectSize() / 1024 / 1024, WTF::Partitions::totalSizeOfCommittedPages() / 1024 / 1024, Heap::wrapperCount(), Heap::collectedWrapperCount());
#endif

    bool gcTracingEnabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED("blink_gc", &gcTracingEnabled);
    if (!gcTracingEnabled)
        return;

    // These values are divided by 1024 to avoid overflow in practical cases (TRACE_COUNTER values are 32-bit ints).
    // They are capped to INT_MAX just in case.
    TRACE_COUNTER1("blink_gc", "Heap::allocatedObjectSizeKB", std::min(Heap::allocatedObjectSize() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1("blink_gc", "Heap::markedObjectSizeKB", std::min(Heap::markedObjectSize() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "Heap::markedObjectSizeAtLastCompleteSweepKB", std::min(Heap::markedObjectSizeAtLastCompleteSweep() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1("blink_gc", "Heap::allocatedSpaceKB", std::min(Heap::allocatedSpace() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "Heap::objectSizeAtLastGCKB", std::min(Heap::objectSizeAtLastGC() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "Heap::wrapperCount", std::min(Heap::wrapperCount(), static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "Heap::wrapperCountAtLastGC", std::min(Heap::wrapperCountAtLastGC(), static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "Heap::collectedWrapperCount", std::min(Heap::collectedWrapperCount(), static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "Heap::partitionAllocSizeAtLastGCKB", std::min(Heap::partitionAllocSizeAtLastGC() / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1("blink_gc", "Partitions::totalSizeOfCommittedPagesKB", std::min(WTF::Partitions::totalSizeOfCommittedPages() / 1024, static_cast<size_t>(INT_MAX)));
}

size_t Heap::objectPayloadSizeForTesting()
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
    MutexLocker locker(regionTreeMutex());
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

    Heap::reportMemoryUsageForTracing();

    s_objectSizeAtLastGC = s_allocatedObjectSize + s_markedObjectSize;
    s_partitionAllocSizeAtLastGC = WTF::Partitions::totalSizeOfCommittedPages();
    s_allocatedObjectSize = 0;
    s_markedObjectSize = 0;
    s_wrapperCountAtLastGC = s_wrapperCount;
    s_collectedWrapperCount = 0;
}

CallbackStack* Heap::s_markingStack;
CallbackStack* Heap::s_postMarkingCallbackStack;
CallbackStack* Heap::s_globalWeakCallbackStack;
CallbackStack* Heap::s_ephemeronStack;
HeapDoesNotContainCache* Heap::s_heapDoesNotContainCache;
bool Heap::s_shutdownCalled = false;
FreePagePool* Heap::s_freePagePool;
OrphanedPagePool* Heap::s_orphanedPagePool;
Heap::RegionTree* Heap::s_regionTree = nullptr;
size_t Heap::s_allocatedSpace = 0;
size_t Heap::s_allocatedObjectSize = 0;
size_t Heap::s_objectSizeAtLastGC = 0;
size_t Heap::s_markedObjectSize = 0;
size_t Heap::s_markedObjectSizeAtLastCompleteSweep = 0;
size_t Heap::s_wrapperCount = 0;
size_t Heap::s_wrapperCountAtLastGC = 0;
size_t Heap::s_collectedWrapperCount = 0;
size_t Heap::s_partitionAllocSizeAtLastGC = 0;
double Heap::s_estimatedMarkingTimePerByte = 0.0;

} // namespace blink
