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
#include "platform/heap/ThreadState.h"

#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "wtf/ThreadingPrimitives.h"

#if OS(WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#elif defined(__GLIBC__)
extern "C" void* __libc_stack_end;  // NOLINT
#endif

namespace WebCore {

static void* getStackStart()
{
#if defined(__GLIBC__) || OS(ANDROID)
    pthread_attr_t attr;
    if (!pthread_getattr_np(pthread_self(), &attr)) {
        void* base;
        size_t size;
        int error = pthread_attr_getstack(&attr, &base, &size);
        RELEASE_ASSERT(!error);
        pthread_attr_destroy(&attr);
        return reinterpret_cast<Address>(base) + size;
    }
#if defined(__GLIBC__)
    // pthread_getattr_np can fail for the main thread. In this case
    // just like NaCl we rely on the __libc_stack_end to give us
    // the start of the stack.
    // See https://code.google.com/p/nativeclient/issues/detail?id=3431.
    return __libc_stack_end;
#else
    ASSERT_NOT_REACHED();
    return 0;
#endif
#elif OS(MACOSX)
    return pthread_get_stackaddr_np(pthread_self());
#elif OS(WIN) && COMPILER(MSVC)
    // On Windows stack limits for the current thread are available in
    // the thread information block (TIB). Its fields can be accessed through
    // FS segment register on x86 and GS segment register on x86_64.
#ifdef _WIN64
    return reinterpret_cast<void*>(__readgsqword(offsetof(NT_TIB64, StackBase)));
#else
    return reinterpret_cast<void*>(__readfsdword(offsetof(NT_TIB, StackBase)));
#endif
#else
#error Unsupported getStackStart on this platform.
#endif
}


WTF::ThreadSpecific<ThreadState*>* ThreadState::s_threadSpecific = 0;
uint8_t ThreadState::s_mainThreadStateStorage[sizeof(ThreadState)];
SafePointBarrier* ThreadState::s_safePointBarrier = 0;
bool ThreadState::s_inGC = false;

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
        releaseStore(&m_canResume, 0);

        ThreadState* current = ThreadState::current();
        for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it) {
            if (*it == current)
                continue;

            const Vector<ThreadState::Interruptor*>& interruptors = (*it)->interruptors();
            for (size_t i = 0; i < interruptors.size(); i++)
                interruptors[i]->requestInterrupt();
        }

        while (acquireLoad(&m_unparkedThreadCount) > 0)
            m_parked.wait(m_mutex);
    }

    void resumeOthers()
    {
        ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
        atomicSubtract(&m_unparkedThreadCount, threads.size());
        releaseStore(&m_canResume, 1);
        {
            // FIXME: Resumed threads will all contend for
            // m_mutex just to unlock it later which is a waste of
            // resources.
            MutexLocker locker(m_mutex);
            m_resume.broadcast();
        }

        ThreadState* current = ThreadState::current();
        for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it) {
            if (*it == current)
                continue;

            const Vector<ThreadState::Interruptor*>& interruptors = (*it)->interruptors();
            for (size_t i = 0; i < interruptors.size(); i++)
                interruptors[i]->clearInterrupt();
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
        while (!acquireLoad(&m_canResume))
            m_resume.wait(m_mutex);
        atomicIncrement(&m_unparkedThreadCount);
    }

    void checkAndPark(ThreadState* state)
    {
        ASSERT(!state->isSweepInProgress());
        if (!acquireLoad(&m_canResume)) {
            pushAllRegisters(this, state, parkAfterPushRegisters);
            state->performPendingSweep();
        }
    }

    void doEnterSafePoint(ThreadState* state, intptr_t* stackEnd)
    {
        state->recordStackEnd(stackEnd);
        state->copyStackUntilSafePointScope();
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

ThreadState::ThreadState()
    : m_thread(currentThread())
    , m_persistents(adoptPtr(new PersistentAnchor()))
    , m_startOfStack(reinterpret_cast<intptr_t*>(getStackStart()))
    , m_endOfStack(reinterpret_cast<intptr_t*>(getStackStart()))
    , m_safePointScopeMarker(0)
    , m_atSafePoint(false)
    , m_interruptors()
    , m_gcRequested(false)
    , m_forcePreciseGCForTesting(false)
    , m_sweepRequested(0)
    , m_sweepInProgress(false)
    , m_noAllocationCount(0)
    , m_inGC(false)
    , m_heapContainsCache(adoptPtr(new HeapContainsCache()))
    , m_isCleaningUp(false)
#if defined(ADDRESS_SANITIZER) && !OS(WIN)
    , m_asanFakeStack(__asan_get_current_fake_stack())
#endif
{
    ASSERT(!**s_threadSpecific);
    **s_threadSpecific = this;

    m_stats.clear();
    m_statsAfterLastGC.clear();
    // First allocate the general heap, second iterate through to
    // allocate the type specific heaps
    m_heaps[GeneralHeap] = new ThreadHeap<FinalizedHeapObjectHeader>(this);
    for (int i = GeneralHeap + 1; i < NumberOfHeaps; i++)
        m_heaps[i] = new ThreadHeap<HeapObjectHeader>(this);

    CallbackStack::init(&m_weakCallbackStack);
}

ThreadState::~ThreadState()
{
    checkThread();
    CallbackStack::shutdown(&m_weakCallbackStack);
    for (int i = GeneralHeap; i < NumberOfHeaps; i++)
        delete m_heaps[i];
    deleteAllValues(m_interruptors);
    **s_threadSpecific = 0;
}

void ThreadState::init()
{
    s_threadSpecific = new WTF::ThreadSpecific<ThreadState*>();
    s_safePointBarrier = new SafePointBarrier;
    new(s_mainThreadStateStorage) ThreadState();
    attachedThreads().add(mainThreadState());
}

void ThreadState::shutdown()
{
    mainThreadState()->~ThreadState();
}

void ThreadState::attach()
{
    MutexLocker locker(threadAttachMutex());
    ThreadState* state = new ThreadState();
    attachedThreads().add(state);
}

void ThreadState::cleanup()
{
    // From here on ignore all conservatively discovered
    // pointers into the heap owned by this thread.
    m_isCleaningUp = true;

    for (size_t i = 0; i < m_cleanupTasks.size(); i++)
        m_cleanupTasks[i]->preCleanup();

    // After this GC we expect heap to be empty because
    // preCleanup tasks should have cleared all persistent
    // handles that were externally owned.
    Heap::collectAllGarbage();

    // Verify that all heaps are empty now.
    for (int i = 0; i < NumberOfHeaps; i++)
        m_heaps[i]->assertEmpty();

    for (size_t i = 0; i < m_cleanupTasks.size(); i++)
        m_cleanupTasks[i]->postCleanup();

    m_cleanupTasks.clear();
}

void ThreadState::detach()
{
    ThreadState* state = current();
    state->cleanup();

    // Enter safe point before trying to acquire threadAttachMutex
    // to avoid dead lock if another thread is preparing for GC, has acquired
    // threadAttachMutex and waiting for other threads to pause or reach a
    // safepoint.
    if (!state->isAtSafePoint())
        state->enterSafePointWithoutPointers();
    MutexLocker locker(threadAttachMutex());
    state->leaveSafePoint();
    attachedThreads().remove(state);
    delete state;
}

void ThreadState::visitRoots(Visitor* visitor)
{
    {
        // All threads are at safepoints so this is not strictly necessary.
        // However we acquire the mutex to make mutation and traversal of this
        // list symmetrical.
        MutexLocker locker(globalRootsMutex());
        globalRoots()->trace(visitor);
    }

    AttachedThreadStateSet& threads = attachedThreads();
    for (AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it)
        (*it)->trace(visitor);
}

NO_SANITIZE_ADDRESS
void ThreadState::visitAsanFakeStackForPointer(Visitor* visitor, Address ptr)
{
#if defined(ADDRESS_SANITIZER) && !OS(WIN)
    Address* start = reinterpret_cast<Address*>(m_startOfStack);
    Address* end = reinterpret_cast<Address*>(m_endOfStack);
    Address* fakeFrameStart = 0;
    Address* fakeFrameEnd = 0;
    Address* maybeFakeFrame = reinterpret_cast<Address*>(ptr);
    Address* realFrameForFakeFrame =
        reinterpret_cast<Address*>(
            __asan_addr_is_in_fake_stack(
                m_asanFakeStack, maybeFakeFrame,
                reinterpret_cast<void**>(&fakeFrameStart),
                reinterpret_cast<void**>(&fakeFrameEnd)));
    if (realFrameForFakeFrame) {
        // This is a fake frame from the asan fake stack.
        if (realFrameForFakeFrame > end && start > realFrameForFakeFrame) {
            // The real stack address for the asan fake frame is
            // within the stack range that we need to scan so we need
            // to visit the values in the fake frame.
            for (Address* p = fakeFrameStart; p < fakeFrameEnd; p++)
                Heap::checkAndMarkPointer(visitor, *p);
        }
    }
#endif
}

NO_SANITIZE_ADDRESS
void ThreadState::visitStack(Visitor* visitor)
{
    Address* start = reinterpret_cast<Address*>(m_startOfStack);
    // If there is a safepoint scope marker we should stop the stack
    // scanning there to not touch active parts of the stack. Anything
    // interesting beyond that point is in the safepoint stack copy.
    // If there is no scope marker the thread is blocked and we should
    // scan all the way to the recorded end stack pointer.
    Address* end = reinterpret_cast<Address*>(m_endOfStack);
    Address* safePointScopeMarker = reinterpret_cast<Address*>(m_safePointScopeMarker);
    Address* current = safePointScopeMarker ? safePointScopeMarker : end;

    // Ensure that current is aligned by address size otherwise the loop below
    // will read past start address.
    current = reinterpret_cast<Address*>(reinterpret_cast<intptr_t>(current) & ~(sizeof(Address) - 1));

    for (; current < start; ++current) {
        Heap::checkAndMarkPointer(visitor, *current);
        visitAsanFakeStackForPointer(visitor, *current);
    }

    for (Vector<Address>::iterator it = m_safePointStackCopy.begin(); it != m_safePointStackCopy.end(); ++it) {
        Heap::checkAndMarkPointer(visitor, *it);
        visitAsanFakeStackForPointer(visitor, *it);
    }
}

void ThreadState::visitPersistents(Visitor* visitor)
{
    m_persistents->trace(visitor);
}

void ThreadState::trace(Visitor* visitor)
{
    if (m_stackState == HeapPointersOnStack)
        visitStack(visitor);
    visitPersistents(visitor);
}

bool ThreadState::checkAndMarkPointer(Visitor* visitor, Address address)
{
    // If thread is cleaning up ignore conservative pointers.
    if (m_isCleaningUp)
        return false;

    BaseHeapPage* page = heapPageFromAddress(address);
    if (page)
        return page->checkAndMarkPointer(visitor, address);
    // Not in heap pages, check large objects
    for (int i = 0; i < NumberOfHeaps; i++) {
        if (m_heaps[i]->checkAndMarkLargeHeapObject(visitor, address))
            return true;
    }
    return false;
}

void ThreadState::pushWeakObjectPointerCallback(void* object, WeakPointerCallback callback)
{
    CallbackStack::Item* slot = m_weakCallbackStack->allocateEntry(&m_weakCallbackStack);
    *slot = CallbackStack::Item(object, callback);
}

bool ThreadState::popAndInvokeWeakPointerCallback(Visitor* visitor)
{
    return m_weakCallbackStack->popAndInvokeCallback(&m_weakCallbackStack, visitor);
}

PersistentNode* ThreadState::globalRoots()
{
    AtomicallyInitializedStatic(PersistentNode*, anchor = new PersistentAnchor);
    return anchor;
}

Mutex& ThreadState::globalRootsMutex()
{
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
    return mutex;
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
    // Do not GC during sweeping. We allow allocation during
    // finalization, but those allocations are not allowed
    // to lead to nested garbage collections.
    return !m_sweepInProgress && increasedEnoughToGC(m_stats.totalObjectSpace(), m_statsAfterLastGC.totalObjectSpace());
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
    // Do not GC during sweeping. We allow allocation during
    // finalization, but those allocations are not allowed
    // to lead to nested garbage collections.
    return !m_sweepInProgress && increasedEnoughToForceConservativeGC(m_stats.totalObjectSpace(), m_statsAfterLastGC.totalObjectSpace());
}

bool ThreadState::sweepRequested()
{
    ASSERT(isAnyThreadInGC() || checkThread());
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

void ThreadState::performPendingGC(StackState stackState)
{
    if (stackState == NoHeapPointersOnStack) {
        if (forcePreciseGCForTesting()) {
            setForcePreciseGCForTesting(false);
            Heap::collectAllGarbage();
        } else if (gcRequested()) {
            Heap::collectGarbage(NoHeapPointersOnStack);
        }
    }
}

void ThreadState::setForcePreciseGCForTesting(bool value)
{
    checkThread();
    m_forcePreciseGCForTesting = value;
}

bool ThreadState::forcePreciseGCForTesting()
{
    checkThread();
    return m_forcePreciseGCForTesting;
}

bool ThreadState::isConsistentForGC()
{
    for (int i = 0; i < NumberOfHeaps; i++) {
        if (!m_heaps[i]->isConsistentForGC())
            return false;
    }
    return true;
}

void ThreadState::makeConsistentForGC()
{
    for (int i = 0; i < NumberOfHeaps; i++)
        m_heaps[i]->makeConsistentForGC();
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

BaseHeapPage* ThreadState::contains(Address address)
{
    // Check heap contains cache first.
    BaseHeapPage* page = heapPageFromAddress(address);
    if (page)
        return page;
    // If no heap page was found check large objects.
    for (int i = 0; i < NumberOfHeaps; i++) {
        page = m_heaps[i]->largeHeapObjectFromAddress(address);
        if (page)
            return page;
    }
    return 0;
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

void ThreadState::safePoint(StackState stackState)
{
    checkThread();
    performPendingGC(stackState);
    m_stackState = stackState;
    s_safePointBarrier->checkAndPark(this);
    m_stackState = HeapPointersOnStack;
}

#ifdef ADDRESS_SANITIZER
// When we are running under AddressSanitizer with detect_stack_use_after_return=1
// then stack marker obtained from SafePointScope will point into a fake stack.
// Detect this case by checking if it falls in between current stack frame
// and stack start and use an arbitrary high enough value for it.
// Don't adjust stack marker in any other case to match behavior of code running
// without AddressSanitizer.
NO_SANITIZE_ADDRESS static void* adjustScopeMarkerForAdressSanitizer(void* scopeMarker)
{
    Address start = reinterpret_cast<Address>(getStackStart());
    Address end = reinterpret_cast<Address>(&start);
    RELEASE_ASSERT(end < start);

    if (end <= scopeMarker && scopeMarker < start)
        return scopeMarker;

    // 256 is as good an approximation as any else.
    const size_t bytesToCopy = sizeof(Address) * 256;
    if (start - end < bytesToCopy)
        return start;

    return end + bytesToCopy;
}
#endif

void ThreadState::enterSafePoint(StackState stackState, void* scopeMarker)
{
#ifdef ADDRESS_SANITIZER
    if (stackState == HeapPointersOnStack)
        scopeMarker = adjustScopeMarkerForAdressSanitizer(scopeMarker);
#endif
    ASSERT(stackState == NoHeapPointersOnStack || scopeMarker);
    performPendingGC(stackState);
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
    s_safePointBarrier->leaveSafePoint(this);
    m_atSafePoint = false;
    m_stackState = HeapPointersOnStack;
    clearSafePointScopeMarker();
    performPendingSweep();
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
    m_safePointStackCopy.resize(slotCount);
    for (size_t i = 0; i < slotCount; ++i) {
        m_safePointStackCopy[i] = from[i];
    }
}

void ThreadState::performPendingSweep()
{
    if (sweepRequested()) {
        m_sweepInProgress = true;
        // Disallow allocation during weak processing.
        enterNoAllocationScope();
        // Perform thread-specific weak processing.
        while (popAndInvokeWeakPointerCallback(Heap::s_markingVisitor)) { }
        leaveNoAllocationScope();
        // Perform sweeping and finalization.
        m_stats.clear(); // Sweeping will recalculate the stats
        for (int i = 0; i < NumberOfHeaps; i++)
            m_heaps[i]->sweep();
        getStats(m_statsAfterLastGC);
        m_sweepInProgress = false;
        clearGCRequested();
        clearSweepRequested();
    }
}

void ThreadState::addInterruptor(Interruptor* interruptor)
{
    SafePointScope scope(HeapPointersOnStack, SafePointScope::AllowNesting);

    {
        MutexLocker locker(threadAttachMutex());
        m_interruptors.append(interruptor);
    }
}

void ThreadState::removeInterruptor(Interruptor* interruptor)
{
    SafePointScope scope(HeapPointersOnStack, SafePointScope::AllowNesting);

    {
        MutexLocker locker(threadAttachMutex());
        size_t index = m_interruptors.find(interruptor);
        RELEASE_ASSERT(index >= 0);
        m_interruptors.remove(index);
    }
}

void ThreadState::Interruptor::onInterrupted()
{
    ThreadState* state = ThreadState::current();
    ASSERT(state);
    ASSERT(!state->isAtSafePoint());
    state->safePoint(HeapPointersOnStack);
}

ThreadState::AttachedThreadStateSet& ThreadState::attachedThreads()
{
    DEFINE_STATIC_LOCAL(AttachedThreadStateSet, threads, ());
    return threads;
}

}
