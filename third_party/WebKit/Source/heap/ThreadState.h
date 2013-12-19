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

#ifndef ThreadState_h
#define ThreadState_h

#include "wtf/HashSet.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"

namespace WebCore {

class HeapContainsCache;

// A HeapStats structure keeps track of the amount of memory allocated
// for a Blink heap and how much of that memory is used for actual
// Blink objects. These stats are used in the heuristics to determine
// when to perform garbage collections.
class HeapStats {
public:
    size_t totalObjectSpace() const { return m_totalObjectSpace; }
    size_t totalAllocatedSpace() const { return m_totalAllocatedSpace; }

    void add(HeapStats* other)
    {
        m_totalObjectSpace += other->m_totalObjectSpace;
        m_totalAllocatedSpace += other->m_totalAllocatedSpace;
    }

    void inline increaseObjectSpace(size_t newObjectSpace)
    {
        m_totalObjectSpace += newObjectSpace;
    }

    void inline decreaseObjectSpace(size_t deadObjectSpace)
    {
        m_totalObjectSpace -= deadObjectSpace;
    }

    void inline increaseAllocatedSpace(size_t newAllocatedSpace)
    {
        m_totalAllocatedSpace += newAllocatedSpace;
    }

    void inline decreaseAllocatedSpace(size_t deadAllocatedSpace)
    {
        m_totalAllocatedSpace -= deadAllocatedSpace;
    }

    void clear()
    {
        m_totalObjectSpace = 0;
        m_totalAllocatedSpace = 0;
    }

    bool operator==(const HeapStats& other);

private:
    size_t m_totalObjectSpace; // Actually contains objects that may be live, not including headers.
    size_t m_totalAllocatedSpace; // Allocated from the OS.

    friend class HeapTester;
};

class ThreadState {
public:
    // When garbage collecting we need to know whether or not there
    // can be pointers to Blink GC managed objects on the stack for
    // each thread. When threads reach a safe point they record
    // whether or not they have pointers on the stack.
    enum StackState {
        NoHeapPointersOnStack,
        HeapPointersOnStack
    };

    // Initialize threading infrastructure. Should be called from the main
    // thread.
    static void init(intptr_t* startOfStack);
    static void shutdown();

    // Associate ThreadState object with the current thread. After this
    // call thread can start using the garbage collected heap infrastructure.
    // It also has to periodically check for safepoints.
    static void attach(intptr_t* startOfStack);

    // Disassociate attached ThreadState from the current thread. The thread
    // can no longer use the garbage collected heap after this call.
    static void detach();

    static ThreadState* current() { return **s_threadSpecific; }
    static ThreadState* mainThreadState()
    {
        return reinterpret_cast<ThreadState*>(s_mainThreadStateStorage);
    }

    static bool isMainThread() { return current() == mainThreadState(); }

    inline void checkThread() const
    {
        ASSERT(m_thread == currentThread());
    }

    // shouldGC and shouldForceConservativeGC implement the heuristics
    // that are used to determine when to collect garbage. If
    // shouldForceConservativeGC returns true, we force the garbage
    // collection immediately. Otherwise, if shouldGC returns true, we
    // record that we should garbage collect the next time we return
    // to the event loop. If both return false, we don't need to
    // collect garbage at this point.
    bool shouldGC();
    bool shouldForceConservativeGC();

    // If gcRequested returns true when a thread returns to its event
    // loop the thread will initiate a garbage collection.
    //
    // FIXME: Implement.
    void setGCRequested(bool) { ASSERT_NOT_REACHED(); }
    bool gcRequested() { ASSERT_NOT_REACHED(); return false; }

    HeapContainsCache* heapContainsCache() { return m_heapContainsCache; }

    HeapStats& stats() { return m_stats; }
    HeapStats& statsAfterLastGC() { return m_statsAfterLastGC; }

private:
    explicit ThreadState(intptr_t* startOfStack);
    ~ThreadState();

    typedef HashSet<ThreadState*> AttachedThreadStateSet;
    static AttachedThreadStateSet& attachedThreads();

    static WTF::ThreadSpecific<ThreadState*>* s_threadSpecific;

    // We can't create a static member of type ThreadState here
    // because it will introduce global constructor and destructor.
    // We would like to manage lifetime of the ThreadState attached
    // to the main thread explicitly instead and still use normal
    // constructor and destructor for the ThreadState class.
    // For this we reserve static storage for the main ThreadState
    // and lazily construct ThreadState in it using placement new.
    static uint8_t s_mainThreadStateStorage[];

    ThreadIdentifier m_thread;
    intptr_t* m_startOfStack;
    HeapContainsCache* m_heapContainsCache;
    HeapStats m_stats;
    HeapStats m_statsAfterLastGC;
};

}

#endif // ThreadState_h
