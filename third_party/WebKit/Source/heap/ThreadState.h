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

#include "heap/HeapExport.h"
#include "wtf/HashSet.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"

namespace WebCore {

class BaseHeap;
class BaseHeapPage;
class FinalizedHeapObjectHeader;
class HeapContainsCache;
class HeapObjectHeader;
class PersistentNode;
class Visitor;
template<typename Header> class ThreadHeap;

typedef uint8_t* Address;

// ThreadAffinity indicates which threads objects can be used on. We
// distinguish between objects that can be used on the main thread
// only and objects that can be used on any thread.
//
// For objects that can only be used on the main thread we avoid going
// through thread-local storage to get to the thread state.
//
// FIXME: We should evaluate the performance gain. Having
// ThreadAffinity is complicating the implementation and we should get
// rid of it if it is fast enough to go through thread-local storage
// always.
enum ThreadAffinity {
    AnyThread,
    MainThreadOnly,
};

// By default all types are considered to be used on the main thread only.
template<typename T>
struct ThreadingTrait {
    static const ThreadAffinity Affinity = MainThreadOnly;
};

// List of typed heaps used to generate the implementation of typed
// heaps.
//
// To create a new typed heap add a H(<ClassName>) to the
// FOR_EACH_TYPED_HEAP macro below.
#define FOR_EACH_TYPED_HEAP(H)  \
    H(Node)

#define TypedHeapEnumName(Type) Type##Heap,

enum TypedHeaps {
    GeneralHeap,
    FOR_EACH_TYPED_HEAP(TypedHeapEnumName)
    NumberOfHeaps
};

// Trait to give an index in the thread state to all the
// type-specialized heaps. The general heap is at index 0 in the
// thread state. The index for other type-specialized heaps are given
// by the TypedHeaps enum above.
template<typename T>
struct HeapTrait {
    static const int index = GeneralHeap;
    typedef ThreadHeap<FinalizedHeapObjectHeader> HeapType;
};

#define DEFINE_HEAP_INDEX_TRAIT(Type)                  \
    class Type;                                        \
    template<>                                         \
    struct HeapTrait<class Type> {                     \
        static const int index = Type##Heap;           \
        typedef ThreadHeap<HeapObjectHeader> HeapType; \
    };

FOR_EACH_TYPED_HEAP(DEFINE_HEAP_INDEX_TRAIT)

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

    bool operator==(const HeapStats& other)
    {
        return m_totalAllocatedSpace == other.m_totalAllocatedSpace
            && m_totalObjectSpace == other.m_totalObjectSpace;
    }

private:
    size_t m_totalObjectSpace; // Actually contains objects that may be live, not including headers.
    size_t m_totalAllocatedSpace; // Allocated from the OS.

    friend class HeapTester;
};

class HEAP_EXPORT ThreadState {
public:
    // When garbage collecting we need to know whether or not there
    // can be pointers to Blink GC managed objects on the stack for
    // each thread. When threads reach a safe point they record
    // whether or not they have pointers on the stack.
    enum StackState {
        NoHeapPointersOnStack,
        HeapPointersOnStack
    };

    // The set of ThreadStates for all threads attached to the Blink
    // garbage collector.
    typedef HashSet<ThreadState*> AttachedThreadStateSet;
    static AttachedThreadStateSet& attachedThreads();

    // Initialize threading infrastructure. Should be called from the main
    // thread.
    static void init(intptr_t* startOfStack);
    static void shutdown();

    // Trace all GC roots, called when marking the managed heap objects.
    static void visitRoots(Visitor*);

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
    bool gcRequested();
    void setGCRequested();
    void clearGCRequested();

    // Support for disallowing allocation. Mainly used for sanity
    // checks asserts.
    bool isAllocationAllowed() const { return !isAtSafePoint() && !m_noAllocationCount; }
    void enterNoAllocationScope() { m_noAllocationCount++; }
    void leaveNoAllocationScope() { m_noAllocationCount--; }

    // Before performing GC the thread-specific heap state should be
    // made consistent for garbage collection.
    bool isConsistentForGC();

    void prepareForGC();

    bool sweepRequested();
    void setSweepRequested();
    void clearSweepRequested();

    bool isAtSafePoint() const { return m_atSafePoint; }

    // Get one of the heap structures for this thread.
    //
    // The heap is split into multiple heap parts based on object
    // types. To get the index for a given type, use
    // HeapTrait<Type>::index.
    BaseHeap* heap(int index) const { return m_heaps[index]; }

    // Infrastructure to determine if an address is within one of the
    // address ranges for the Blink heap.
    HeapContainsCache* heapContainsCache() { return m_heapContainsCache; }
    bool contains(Address);
    bool contains(void* pointer) { return contains(reinterpret_cast<Address>(pointer)); }
    bool contains(const void* pointer) { return contains(const_cast<void*>(pointer)); }

    // Finds the Blink HeapPage in this thread-specific heap
    // corresponding to a given address. Return 0 if the address is
    // not contained in any of the pages.
    BaseHeapPage* heapPageFromAddress(Address);

    // List of persistent roots allocated on the given thread.
    PersistentNode* roots() const { return m_persistents; }

    // Visit all persistents allocated on this thread.
    void visitPersistents(Visitor*);

    void getStats(HeapStats&);
    HeapStats& stats() { return m_stats; }
    HeapStats& statsAfterLastGC() { return m_statsAfterLastGC; }

private:
    explicit ThreadState(intptr_t* startOfStack);
    ~ThreadState();

    static WTF::ThreadSpecific<ThreadState*>* s_threadSpecific;

    // We can't create a static member of type ThreadState here
    // because it will introduce global constructor and destructor.
    // We would like to manage lifetime of the ThreadState attached
    // to the main thread explicitly instead and still use normal
    // constructor and destructor for the ThreadState class.
    // For this we reserve static storage for the main ThreadState
    // and lazily construct ThreadState in it using placement new.
    static uint8_t s_mainThreadStateStorage[];

    void trace(Visitor*);

    ThreadIdentifier m_thread;
    PersistentNode* m_persistents;
    intptr_t* m_startOfStack;
    bool m_gcRequested;
    size_t m_noAllocationCount;
    volatile int m_sweepRequested;
    bool m_atSafePoint;
    BaseHeap* m_heaps[NumberOfHeaps];
    HeapContainsCache* m_heapContainsCache;
    HeapStats m_stats;
    HeapStats m_statsAfterLastGC;
};

template<ThreadAffinity affinity> class ThreadStateFor;

template<> class ThreadStateFor<MainThreadOnly> {
public:
    static ThreadState* state()
    {
        // This specialization must only be used from the main thread.
        ASSERT(ThreadState::isMainThread());
        return ThreadState::mainThreadState();
    }
};

template<> class ThreadStateFor<AnyThread> {
public:
    static ThreadState* state() { return ThreadState::current(); }
};

}

#endif // ThreadState_h
