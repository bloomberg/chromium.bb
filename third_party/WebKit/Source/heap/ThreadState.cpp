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
#include "heap/ThreadState.h"

#include "heap/Handle.h"
#include "heap/Heap.h"
#include "wtf/ThreadingPrimitives.h"

namespace WebCore {

WTF::ThreadSpecific<ThreadState*>* ThreadState::s_threadSpecific = 0;
uint8_t ThreadState::s_mainThreadStateStorage[sizeof(ThreadState)];
SafePointBarrier* ThreadState::s_safePointBarrier = 0;

static Mutex& threadAttachMutex()
{
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
    return mutex;
}

typedef void (*PushAllRegistersCallback)(SafePointBarrier*, ThreadState*, intptr_t*);
extern "C" void pushAllRegisters(SafePointBarrier*, ThreadState*, PushAllRegistersCallback);

class SafePointBarrier {
public:
    SafePointBarrier() : m_canResume(1), m_unparkedThreadCount(0) { }
    ~SafePointBarrier() { }

    // Request other attached threads that are not at safe points to park themselves on safepoints.
    void parkOthers()
    {
        ASSERT(ThreadState::current()->isAtSafePoint());

        // Lock threadAttachMutex() to prevent threads from attaching.
        threadAttachMutex().lock();

        ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();

        MutexLocker locker(m_mutex);
        atomicAdd(&m_unparkedThreadCount, threads.size());
        atomicSetOneToZero(&m_canResume);

        for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it) {
            if ((*it)->interruptor())
                (*it)->interruptor()->requestInterrupt();
        }

        while (m_unparkedThreadCount > 0)
            m_parked.wait(m_mutex);
    }

    void resumeOthers()
    {
        ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
        atomicSubtract(&m_unparkedThreadCount, threads.size());
        atomicTestAndSetToOne(&m_canResume);
        {
            // FIXME: Resumed threads will all contend for
            // m_mutex just to unlock it later which is a waste of
            // resources.
            MutexLocker locker(m_mutex);
            m_resume.broadcast();
        }

        for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it) {
            if ((*it)->interruptor())
                (*it)->interruptor()->clearInterrupt();
        }

        threadAttachMutex().unlock();
        ASSERT(ThreadState::current()->isAtSafePoint());
    }

    void doPark(ThreadState* state, intptr_t* stackEnd)
    {
        state->recordStackEnd(stackEnd);
        MutexLocker locker(m_mutex);
        if (!atomicDecrement(&m_unparkedThreadCount))
            m_parked.signal();
        while (!m_canResume)
            m_resume.wait(m_mutex);
        atomicIncrement(&m_unparkedThreadCount);
    }

    void checkAndPark(ThreadState* state)
    {
        ASSERT(!state->isSweepInProgress());
        if (!m_canResume) {
            pushAllRegisters(this, state, parkAfterPushRegisters);
            state->performPendingSweep();
        }
    }

    void doEnterSafePoint(ThreadState* state, intptr_t* stackEnd)
    {
        state->recordStackEnd(stackEnd);
        // m_unparkedThreadCount tracks amount of unparked threads. It is
        // positive if and only if we have requested other threads to park
        // at safe-points in preparation for GC. The last thread to park
        // itself will make the counter hit zero and should notify GC thread
        // that it is safe to proceed.
        // If no other thread is waiting for other threads to park then
        // this counter can be negative: if N threads are at safe-points
        // the counter will be -N.
        if (!atomicDecrement(&m_unparkedThreadCount)) {
            MutexLocker locker(m_mutex);
            m_parked.signal(); // Safe point reached.
        }
        state->copyStackUntilSafePointScope();
    }

    void enterSafePoint(ThreadState* state)
    {
        ASSERT(!state->isSweepInProgress());
        pushAllRegisters(this, state, enterSafePointAfterPushRegisters);
    }

    void leaveSafePoint(ThreadState* state)
    {
        if (atomicIncrement(&m_unparkedThreadCount) > 0)
            checkAndPark(state);
        state->performPendingSweep();
    }

private:
    static void parkAfterPushRegisters(SafePointBarrier* barrier, ThreadState* state, intptr_t* stackEnd)
    {
        barrier->doPark(state, stackEnd);
    }

    static void enterSafePointAfterPushRegisters(SafePointBarrier* barrier, ThreadState* state, intptr_t* stackEnd)
    {
        barrier->doEnterSafePoint(state, stackEnd);
    }

    volatile int m_canResume;
    volatile int m_unparkedThreadCount;
    Mutex m_mutex;
    ThreadCondition m_parked;
    ThreadCondition m_resume;
};

ThreadState::ThreadState(intptr_t* startOfStack)
    : m_thread(currentThread())
    , m_startOfStack(startOfStack)
    , m_endOfStack(startOfStack)
    , m_safePointScopeMarker(0)
    , m_atSafePoint(false)
    , m_interruptor(0)
    , m_gcRequested(false)
    , m_sweepInProgress(false)
    , m_noAllocationCount(0)
    , m_heapContainsCache(new HeapContainsCache())
{
    ASSERT(!**s_threadSpecific);
    **s_threadSpecific = this;

    m_persistents = new PersistentAnchor();

    // First allocate the general heap, second iterate through to
    // allocate the type specific heaps
    m_heaps[GeneralHeap] = new ThreadHeap<FinalizedHeapObjectHeader>(this);
    for (int i = GeneralHeap + 1; i < NumberOfHeaps; i++)
        m_heaps[i] = new ThreadHeap<HeapObjectHeader>(this);
}

ThreadState::~ThreadState()
{
    checkThread();
    for (int i = GeneralHeap; i < NumberOfHeaps; i++)
        delete m_heaps[i];
    **s_threadSpecific = 0;
}

void ThreadState::init(intptr_t* startOfStack)
{
    s_threadSpecific = new WTF::ThreadSpecific<ThreadState*>();
    s_safePointBarrier = new SafePointBarrier;
    new(s_mainThreadStateStorage) ThreadState(startOfStack);
    attachedThreads().add(mainThreadState());
}

void ThreadState::shutdown()
{
    mainThreadState()->~ThreadState();
}

void ThreadState::attach(intptr_t* startOfStack)
{
    MutexLocker locker(threadAttachMutex());
    ThreadState* state = new ThreadState(startOfStack);
    attachedThreads().add(state);
}

void ThreadState::detach()
{
    ThreadState* state = current();
    MutexLocker locker(threadAttachMutex());
    attachedThreads().remove(state);
    delete state;
}

void ThreadState::visitRoots(Visitor* visitor)
{
    AttachedThreadStateSet& threads = attachedThreads();
    for (AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it)
        (*it)->trace(visitor);
}

void ThreadState::visitPersistents(Visitor* visitor)
{
    for (PersistentNode* current = m_persistents->m_next; current != m_persistents; current = current->m_next) {
        current->trace(visitor);
    }
}

void ThreadState::trace(Visitor* visitor)
{
    visitPersistents(visitor);
}

// Trigger garbage collection on a 50% increase in size, but not for
// less than 2 pages.
static bool increasedEnoughToGC(size_t newSize, size_t oldSize)
{
    if (newSize < 2 * blinkPagePayloadSize())
        return false;
    return newSize > oldSize + (oldSize >> 1);
}

// FIXME: The heuristics are local for a thread at this
// point. Consider using heuristics that take memory for all threads
// into account.
bool ThreadState::shouldGC()
{
    return increasedEnoughToGC(m_stats.totalObjectSpace(), m_statsAfterLastGC.totalObjectSpace());
}

// Trigger conservative garbage collection on a 100% increase in size,
// but not for less than 2 pages.
static bool increasedEnoughToForceConservativeGC(size_t newSize, size_t oldSize)
{
    if (newSize < 2 * blinkPagePayloadSize())
        return false;
    return newSize > 2 * oldSize;
}

// FIXME: The heuristics are local for a thread at this
// point. Consider using heuristics that take memory for all threads
// into account.
bool ThreadState::shouldForceConservativeGC()
{
    return increasedEnoughToForceConservativeGC(m_stats.totalObjectSpace(), m_statsAfterLastGC.totalObjectSpace());
}

bool ThreadState::sweepRequested()
{
    checkThread();
    return m_sweepRequested;
}

void ThreadState::setSweepRequested()
{
    // Sweep requested is set from the thread that initiates garbage
    // collection which could be different from the thread for this
    // thread state. Therefore the setting of m_sweepRequested needs a
    // barrier.
    atomicTestAndSetToOne(&m_sweepRequested);
}

void ThreadState::clearSweepRequested()
{
    checkThread();
    m_sweepRequested = 0;
}

bool ThreadState::gcRequested()
{
    checkThread();
    return m_gcRequested;
}

void ThreadState::setGCRequested()
{
    checkThread();
    m_gcRequested = true;
}

void ThreadState::clearGCRequested()
{
    checkThread();
    m_gcRequested = false;
}

bool ThreadState::isConsistentForGC()
{
    for (int i = 0; i < NumberOfHeaps; i++) {
        if (!m_heaps[i]->isConsistentForGC())
            return false;
    }
    return true;
}

void ThreadState::prepareForGC()
{
    for (int i = 0; i < NumberOfHeaps; i++) {
        BaseHeap* heap = m_heaps[i];
        heap->makeConsistentForGC();
        // If there are parked threads with outstanding sweep requests, clear their mark bits.
        // This happens if a thread did not have time to wake up and sweep,
        // before the next GC arrived.
        if (sweepRequested())
            heap->clearMarks();
    }
    setSweepRequested();
}

BaseHeapPage* ThreadState::heapPageFromAddress(Address address)
{
    BaseHeapPage* page;
    bool found = heapContainsCache()->lookup(address, &page);
    if (found)
        return page;

    for (int i = 0; i < NumberOfHeaps; i++) {
        page = m_heaps[i]->heapPageFromAddress(address);
#ifndef NDEBUG
        Address blinkPageAddr = roundToBlinkPageStart(address);
#endif
        ASSERT(page == m_heaps[i]->heapPageFromAddress(blinkPageAddr));
        ASSERT(page == m_heaps[i]->heapPageFromAddress(blinkPageAddr + blinkPageSize - 1));
        if (page)
            break;
    }
    heapContainsCache()->addEntry(address, page);
    return page; // 0 if not found.
}

bool ThreadState::contains(Address address)
{
    // Check heap contains cache first.
    BaseHeapPage* page = heapPageFromAddress(address);
    if (page)
        return true;
    // If no heap page was found check large objects.
    for (int i = 0; i < NumberOfHeaps; i++) {
        if (m_heaps[i]->largeHeapObjectFromAddress(address))
            return true;
    }
    return false;
}

void ThreadState::getStats(HeapStats& stats)
{
    stats = m_stats;
#ifndef NDEBUG
    if (isConsistentForGC()) {
        HeapStats scannedStats;
        scannedStats.clear();
        for (int i = 0; i < NumberOfHeaps; i++)
            m_heaps[i]->getScannedStats(scannedStats);
        ASSERT(scannedStats == stats);
    }
#endif
}

void ThreadState::stopThreads()
{
    s_safePointBarrier->parkOthers();
}

void ThreadState::resumeThreads()
{
    s_safePointBarrier->resumeOthers();
}

void ThreadState::safePoint()
{
    s_safePointBarrier->checkAndPark(this);
}

void ThreadState::enterSafePoint(StackState stackState, void* scopeMarker)
{
    ASSERT(stackState == NoHeapPointersOnStack || scopeMarker);
    if (stackState == NoHeapPointersOnStack && gcRequested())
        Heap::collectGarbage(NoHeapPointersOnStack);
    checkThread();
    ASSERT(!m_atSafePoint);
    m_atSafePoint = true;
    m_stackState = stackState;
    m_safePointScopeMarker = scopeMarker;
    s_safePointBarrier->enterSafePoint(this);
}

void ThreadState::leaveSafePoint()
{
    checkThread();
    ASSERT(m_atSafePoint);
    m_atSafePoint = false;
    m_stackState = HeapPointersOnStack;
    clearSafePointScopeMarker();
    s_safePointBarrier->leaveSafePoint(this);
}

void ThreadState::copyStackUntilSafePointScope()
{
    if (!m_safePointScopeMarker || m_stackState == NoHeapPointersOnStack)
        return;

    Address* to = reinterpret_cast<Address*>(m_safePointScopeMarker);
    Address* from = reinterpret_cast<Address*>(m_endOfStack);
    RELEASE_ASSERT(from < to);
    RELEASE_ASSERT(to < reinterpret_cast<Address*>(m_startOfStack));
    size_t slotCount = static_cast<size_t>(to - from);
    ASSERT(slotCount < 1024); // Catch potential performance issues.

    ASSERT(!m_safePointStackCopy.size());
    m_safePointStackCopy.append(reinterpret_cast<Address*>(from), slotCount);
}

void ThreadState::performPendingSweep()
{
    if (sweepRequested()) {
        m_sweepInProgress = true;
        // FIXME: implement sweep.
        m_sweepInProgress = false;
        clearGCRequested();
        clearSweepRequested();
    }
}

void ThreadState::setInterruptor(Interruptor* interruptor)
{
    SafePointScope scope(HeapPointersOnStack, SafePointScope::AllowNesting);

    {
        MutexLocker locker(threadAttachMutex());
        delete m_interruptor;
        m_interruptor = interruptor;
    }
}

void ThreadState::Interruptor::onInterrupted()
{
    ThreadState* state = ThreadState::current();
    ASSERT(state);
    ASSERT(!state->isAtSafePoint());
    state->safePoint();
}

ThreadState::AttachedThreadStateSet& ThreadState::attachedThreads()
{
    DEFINE_STATIC_LOCAL(AttachedThreadStateSet, threads, ());
    return threads;
}

}
