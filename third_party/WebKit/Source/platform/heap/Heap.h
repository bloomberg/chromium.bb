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

#ifndef Heap_h
#define Heap_h

#include "platform/PlatformExport.h"
#include "platform/heap/GCInfo.h"
#include "platform/heap/HeapPage.h"
#include "platform/heap/PageMemory.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "wtf/AddressSanitizer.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Atomics.h"
#include "wtf/Forward.h"

namespace blink {

class PLATFORM_EXPORT HeapAllocHooks {
public:
    // TODO(hajimehoshi): Pass a type name of the allocated object.
    typedef void AllocationHook(Address, size_t);
    typedef void FreeHook(Address);

    static void setAllocationHook(AllocationHook* hook) { m_allocationHook = hook; }
    static void setFreeHook(FreeHook* hook) { m_freeHook = hook; }

    static void allocationHookIfEnabled(Address address, size_t size)
    {
        AllocationHook* allocationHook = m_allocationHook;
        if (UNLIKELY(!!allocationHook))
            allocationHook(address, size);
    }

    static void freeHookIfEnabled(Address address)
    {
        FreeHook* freeHook = m_freeHook;
        if (UNLIKELY(!!freeHook))
            freeHook(address);
    }

    static void reallocHookIfEnabled(Address oldAddress, Address newAddress, size_t size)
    {
        // Report a reallocation as a free followed by an allocation.
        AllocationHook* allocationHook = m_allocationHook;
        FreeHook* freeHook = m_freeHook;
        if (UNLIKELY(allocationHook && freeHook)) {
            freeHook(oldAddress);
            allocationHook(newAddress, size);
        }
    }

private:
    static AllocationHook* m_allocationHook;
    static FreeHook* m_freeHook;
};

class CrossThreadPersistentRegion;
template<typename T> class Member;
template<typename T> class WeakMember;
template<typename T> class UntracedMember;

template<typename T, bool = NeedsAdjustAndMark<T>::value> class ObjectAliveTrait;

template<typename T>
class ObjectAliveTrait<T, false> {
    STATIC_ONLY(ObjectAliveTrait);
public:
    static bool isHeapObjectAlive(T* object)
    {
        static_assert(sizeof(T), "T must be fully defined");
        return HeapObjectHeader::fromPayload(object)->isMarked();
    }
};

template<typename T>
class ObjectAliveTrait<T, true> {
    STATIC_ONLY(ObjectAliveTrait);
public:
    NO_LAZY_SWEEP_SANITIZE_ADDRESS
    static bool isHeapObjectAlive(T* object)
    {
        static_assert(sizeof(T), "T must be fully defined");
        return object->isHeapObjectAlive();
    }
};

class PLATFORM_EXPORT Heap {
    STATIC_ONLY(Heap);
public:
    static void init();
    static void shutdown();
    static void doShutdown();

    static CrossThreadPersistentRegion& crossThreadPersistentRegion();

#if ENABLE(ASSERT)
    static BasePage* findPageFromAddress(Address);
    static BasePage* findPageFromAddress(const void* pointer) { return findPageFromAddress(reinterpret_cast<Address>(const_cast<void*>(pointer))); }
#endif

    template<typename T>
    static inline bool isHeapObjectAlive(T* object)
    {
        static_assert(sizeof(T), "T must be fully defined");
        // The strongification of collections relies on the fact that once a
        // collection has been strongified, there is no way that it can contain
        // non-live entries, so no entries will be removed. Since you can't set
        // the mark bit on a null pointer, that means that null pointers are
        // always 'alive'.
        if (!object)
            return true;
        return ObjectAliveTrait<T>::isHeapObjectAlive(object);
    }
    template<typename T>
    static inline bool isHeapObjectAlive(const Member<T>& member)
    {
        return isHeapObjectAlive(member.get());
    }
    template<typename T>
    static inline bool isHeapObjectAlive(const WeakMember<T>& member)
    {
        return isHeapObjectAlive(member.get());
    }
    template<typename T>
    static inline bool isHeapObjectAlive(const UntracedMember<T>& member)
    {
        return isHeapObjectAlive(member.get());
    }
    template<typename T>
    static inline bool isHeapObjectAlive(const RawPtr<T>& ptr)
    {
        return isHeapObjectAlive(ptr.get());
    }

    // Is the finalizable GC object still alive, but slated for lazy sweeping?
    // If a lazy sweep is in progress, returns true if the object was found
    // to be not reachable during the marking phase, but it has yet to be swept
    // and finalized. The predicate returns false in all other cases.
    //
    // Holding a reference to an already-dead object is not a valid state
    // to be in; willObjectBeLazilySwept() has undefined behavior if passed
    // such a reference.
    template<typename T>
    NO_LAZY_SWEEP_SANITIZE_ADDRESS
    static bool willObjectBeLazilySwept(const T* objectPointer)
    {
        static_assert(IsGarbageCollectedType<T>::value, "only objects deriving from GarbageCollected can be used.");
        BasePage* page = pageFromObject(objectPointer);
        if (page->hasBeenSwept())
            return false;
        ASSERT(page->heap()->threadState()->isSweepingInProgress());

        return !Heap::isHeapObjectAlive(const_cast<T*>(objectPointer));
    }

    // Push a trace callback on the marking stack.
    static void pushTraceCallback(void* containerObject, TraceCallback);

    // Push a trace callback on the post-marking callback stack.  These
    // callbacks are called after normal marking (including ephemeron
    // iteration).
    static void pushPostMarkingCallback(void*, TraceCallback);

    // Add a weak pointer callback to the weak callback work list.  General
    // object pointer callbacks are added to a thread local weak callback work
    // list and the callback is called on the thread that owns the object, with
    // the closure pointer as an argument.  Most of the time, the closure and
    // the containerObject can be the same thing, but the containerObject is
    // constrained to be on the heap, since the heap is used to identify the
    // correct thread.
    static void pushThreadLocalWeakCallback(void* closure, void* containerObject, WeakCallback);

    // Similar to the more general pushThreadLocalWeakCallback, but cell
    // pointer callbacks are added to a static callback work list and the weak
    // callback is performed on the thread performing garbage collection.  This
    // is OK because cells are just cleared and no deallocation can happen.
    static void pushGlobalWeakCallback(void** cell, WeakCallback);

    // Pop the top of a marking stack and call the callback with the visitor
    // and the object.  Returns false when there is nothing more to do.
    static bool popAndInvokeTraceCallback(Visitor*);

    // Remove an item from the post-marking callback stack and call
    // the callback with the visitor and the object pointer.  Returns
    // false when there is nothing more to do.
    static bool popAndInvokePostMarkingCallback(Visitor*);

    // Remove an item from the weak callback work list and call the callback
    // with the visitor and the closure pointer.  Returns false when there is
    // nothing more to do.
    static bool popAndInvokeGlobalWeakCallback(Visitor*);

    // Register an ephemeron table for fixed-point iteration.
    static void registerWeakTable(void* containerObject, EphemeronCallback, EphemeronCallback);
#if ENABLE(ASSERT)
    static bool weakTableRegistered(const void*);
#endif

    static inline size_t allocationSizeFromSize(size_t size)
    {
        // Check the size before computing the actual allocation size.  The
        // allocation size calculation can overflow for large sizes and the check
        // therefore has to happen before any calculation on the size.
        RELEASE_ASSERT(size < maxHeapObjectSize);

        // Add space for header.
        size_t allocationSize = size + sizeof(HeapObjectHeader);
        // Align size with allocation granularity.
        allocationSize = (allocationSize + allocationMask) & ~allocationMask;
        return allocationSize;
    }
    static Address allocateOnHeapIndex(ThreadState*, size_t, int heapIndex, size_t gcInfoIndex);
    template<typename T> static Address allocate(size_t, bool eagerlySweep = false);
    template<typename T> static Address reallocate(void* previous, size_t);

    static const char* gcReasonString(BlinkGC::GCReason);
    static void collectGarbage(BlinkGC::StackState, BlinkGC::GCType, BlinkGC::GCReason);
    static void collectGarbageForTerminatingThread(ThreadState*);
    static void collectAllGarbage();

    static void processMarkingStack(Visitor*);
    static void postMarkingProcessing(Visitor*);
    static void globalWeakProcessing(Visitor*);
    static void setForcePreciseGCForTesting();

    static void preGC();
    static void postGC(BlinkGC::GCType);

    // Conservatively checks whether an address is a pointer in any of the
    // thread heaps.  If so marks the object pointed to as live.
    static Address checkAndMarkPointer(Visitor*, Address);

    static size_t objectPayloadSizeForTesting();

    static void flushHeapDoesNotContainCache();

    static FreePagePool* freePagePool() { return s_freePagePool; }
    static OrphanedPagePool* orphanedPagePool() { return s_orphanedPagePool; }

    // This look-up uses the region search tree and a negative contains cache to
    // provide an efficient mapping from arbitrary addresses to the containing
    // heap-page if one exists.
    static BasePage* lookup(Address);
    static void addPageMemoryRegion(PageMemoryRegion*);
    static void removePageMemoryRegion(PageMemoryRegion*);

    static const GCInfo* gcInfo(size_t gcInfoIndex)
    {
        ASSERT(gcInfoIndex >= 1);
        ASSERT(gcInfoIndex < GCInfoTable::maxIndex);
        ASSERT(s_gcInfoTable);
        const GCInfo* info = s_gcInfoTable[gcInfoIndex];
        ASSERT(info);
        return info;
    }

    static void setMarkedObjectSizeAtLastCompleteSweep(size_t size) { releaseStore(&s_markedObjectSizeAtLastCompleteSweep, size); }
    static size_t markedObjectSizeAtLastCompleteSweep() { return acquireLoad(&s_markedObjectSizeAtLastCompleteSweep); }
    static void increaseAllocatedObjectSize(size_t delta) { atomicAdd(&s_allocatedObjectSize, static_cast<long>(delta)); }
    static void decreaseAllocatedObjectSize(size_t delta) { atomicSubtract(&s_allocatedObjectSize, static_cast<long>(delta)); }
    static size_t allocatedObjectSize() { return acquireLoad(&s_allocatedObjectSize); }
    static void increaseMarkedObjectSize(size_t delta) { atomicAdd(&s_markedObjectSize, static_cast<long>(delta)); }
    static size_t markedObjectSize() { return acquireLoad(&s_markedObjectSize); }
    static void increaseAllocatedSpace(size_t delta) { atomicAdd(&s_allocatedSpace, static_cast<long>(delta)); }
    static void decreaseAllocatedSpace(size_t delta) { atomicSubtract(&s_allocatedSpace, static_cast<long>(delta)); }
    static size_t allocatedSpace() { return acquireLoad(&s_allocatedSpace); }
    static size_t objectSizeAtLastGC() { return acquireLoad(&s_objectSizeAtLastGC); }
    static void increaseWrapperCount(size_t delta) { atomicAdd(&s_wrapperCount, static_cast<long>(delta)); }
    static void decreaseWrapperCount(size_t delta) { atomicSubtract(&s_wrapperCount, static_cast<long>(delta)); }
    static size_t wrapperCount() { return acquireLoad(&s_wrapperCount); }
    static size_t wrapperCountAtLastGC() { return acquireLoad(&s_wrapperCountAtLastGC); }
    static void increaseCollectedWrapperCount(size_t delta) { atomicAdd(&s_collectedWrapperCount, static_cast<long>(delta)); }
    static size_t collectedWrapperCount() { return acquireLoad(&s_collectedWrapperCount); }
    static size_t partitionAllocSizeAtLastGC() { return acquireLoad(&s_partitionAllocSizeAtLastGC); }

    static double estimatedMarkingTime();
    static void reportMemoryUsageHistogram();
    static void reportMemoryUsageForTracing();
    static bool isLowEndDevice() { return s_isLowEndDevice; }

#if ENABLE(ASSERT)
    static uint16_t gcGeneration() { return s_gcGeneration; }
#endif

private:
    // Reset counters that track live and allocated-since-last-GC sizes.
    static void resetHeapCounters();

    static int heapIndexForObjectSize(size_t);
    static bool isNormalHeapIndex(int);

    static CallbackStack* s_markingStack;
    static CallbackStack* s_postMarkingCallbackStack;
    static CallbackStack* s_globalWeakCallbackStack;
    static CallbackStack* s_ephemeronStack;
    static HeapDoesNotContainCache* s_heapDoesNotContainCache;
    static bool s_shutdownCalled;
    static FreePagePool* s_freePagePool;
    static OrphanedPagePool* s_orphanedPagePool;
    static RegionTree* s_regionTree;
    static size_t s_allocatedSpace;
    static size_t s_allocatedObjectSize;
    static size_t s_objectSizeAtLastGC;
    static size_t s_markedObjectSize;
    static size_t s_markedObjectSizeAtLastCompleteSweep;
    static size_t s_wrapperCount;
    static size_t s_wrapperCountAtLastGC;
    static size_t s_collectedWrapperCount;
    static size_t s_partitionAllocSizeAtLastGC;
    static double s_estimatedMarkingTimePerByte;
    static bool s_isLowEndDevice;
#if ENABLE(ASSERT)
    static uint16_t s_gcGeneration;
#endif

    friend class ThreadState;
};

template<typename T>
struct IsEagerlyFinalizedType {
    STATIC_ONLY(IsEagerlyFinalizedType);
private:
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename U> static YesType checkMarker(typename U::IsEagerlyFinalizedMarker*);
    template <typename U> static NoType checkMarker(...);

public:
    static const bool value = sizeof(checkMarker<T>(nullptr)) == sizeof(YesType);
};

template<typename T> class GarbageCollected {
    IS_GARBAGE_COLLECTED_TYPE();
    WTF_MAKE_NONCOPYABLE(GarbageCollected);

    // For now direct allocation of arrays on the heap is not allowed.
    void* operator new[](size_t size);

#if OS(WIN) && COMPILER(MSVC)
    // Due to some quirkiness in the MSVC compiler we have to provide
    // the delete[] operator in the GarbageCollected subclasses as it
    // is called when a class is exported in a DLL.
protected:
    void operator delete[](void* p)
    {
        ASSERT_NOT_REACHED();
    }
#else
    void operator delete[](void* p);
#endif

public:
    using GarbageCollectedType = T;

    void* operator new(size_t size)
    {
        return allocateObject(size, IsEagerlyFinalizedType<T>::value);
    }

    static void* allocateObject(size_t size, bool eagerlySweep)
    {
        return Heap::allocate<T>(size, eagerlySweep);
    }

    void operator delete(void* p)
    {
        ASSERT_NOT_REACHED();
    }

protected:
    GarbageCollected()
    {
    }
};

// Assigning class types to their heaps.
//
// We use sized heaps for most 'normal' objects to improve memory locality.
// It seems that the same type of objects are likely to be accessed together,
// which means that we want to group objects by type. That's one reason
// why we provide dedicated heaps for popular types (e.g., Node, CSSValue),
// but it's not practical to prepare dedicated heaps for all types.
// Thus we group objects by their sizes, hoping that this will approximately
// group objects by their types.
//
// An exception to the use of sized heaps is made for class types that
// require prompt finalization after a garbage collection. That is, their
// instances have to be finalized early and cannot be delayed until lazy
// sweeping kicks in for their heap and page. The EAGERLY_FINALIZE()
// macro is used to declare a class (and its derived classes) as being
// in need of eager finalization. Must be defined with 'public' visibility
// for a class.
//

inline int Heap::heapIndexForObjectSize(size_t size)
{
    if (size < 64) {
        if (size < 32)
            return BlinkGC::NormalPage1HeapIndex;
        return BlinkGC::NormalPage2HeapIndex;
    }
    if (size < 128)
        return BlinkGC::NormalPage3HeapIndex;
    return BlinkGC::NormalPage4HeapIndex;
}

inline bool Heap::isNormalHeapIndex(int index)
{
    return index >= BlinkGC::NormalPage1HeapIndex && index <= BlinkGC::NormalPage4HeapIndex;
}

#define DECLARE_EAGER_FINALIZATION_OPERATOR_NEW() \
public:                                           \
    GC_PLUGIN_IGNORE("491488")                    \
    void* operator new(size_t size)               \
    {                                             \
        return allocateObject(size, true);        \
    }

#define IS_EAGERLY_FINALIZED() (pageFromObject(this)->heap()->heapIndex() == BlinkGC::EagerSweepHeapIndex)
#if ENABLE(ASSERT) && ENABLE(OILPAN)
class VerifyEagerFinalization {
    DISALLOW_NEW();
public:
    ~VerifyEagerFinalization()
    {
        // If this assert triggers, the class annotated as eagerly
        // finalized ended up not being allocated on the heap
        // set aside for eager finalization. The reason is most
        // likely that the effective 'operator new' overload for
        // this class' leftmost base is for a class that is not
        // eagerly finalized. Declaring and defining an 'operator new'
        // for this class is what's required -- consider using
        // DECLARE_EAGER_FINALIZATION_OPERATOR_NEW().
        ASSERT(IS_EAGERLY_FINALIZED());
    }
};
#define EAGERLY_FINALIZE()                             \
private:                                               \
    VerifyEagerFinalization m_verifyEagerFinalization; \
public:                                                \
    typedef int IsEagerlyFinalizedMarker
#else
#define EAGERLY_FINALIZE() typedef int IsEagerlyFinalizedMarker
#endif

#if !ENABLE(OILPAN)
#define EAGERLY_FINALIZE_WILL_BE_REMOVED() EAGERLY_FINALIZE()
#else
#define EAGERLY_FINALIZE_WILL_BE_REMOVED()
#endif

inline Address Heap::allocateOnHeapIndex(ThreadState* state, size_t size, int heapIndex, size_t gcInfoIndex)
{
    ASSERT(state->isAllocationAllowed());
    ASSERT(heapIndex != BlinkGC::LargeObjectHeapIndex);
    NormalPageHeap* heap = static_cast<NormalPageHeap*>(state->heap(heapIndex));
    return heap->allocateObject(allocationSizeFromSize(size), gcInfoIndex);
}

template<typename T>
Address Heap::allocate(size_t size, bool eagerlySweep)
{
    ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
    Address address = Heap::allocateOnHeapIndex(state, size, eagerlySweep ? BlinkGC::EagerSweepHeapIndex : Heap::heapIndexForObjectSize(size), GCInfoTrait<T>::index());
    HeapAllocHooks::allocationHookIfEnabled(address, size);
    return address;
}

template<typename T>
Address Heap::reallocate(void* previous, size_t size)
{
    // Not intended to be a full C realloc() substitute;
    // realloc(nullptr, size) is not a supported alias for malloc(size).

    // TODO(sof): promptly free the previous object.
    if (!size) {
        // If the new size is 0 this is considered equivalent to free(previous).
        return nullptr;
    }

    ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
    HeapObjectHeader* previousHeader = HeapObjectHeader::fromPayload(previous);
    BasePage* page = pageFromObject(previousHeader);
    ASSERT(page);
    int heapIndex = page->heap()->heapIndex();
    // Recompute the effective heap index if previous allocation
    // was on the normal heaps or a large object.
    if (isNormalHeapIndex(heapIndex) || heapIndex == BlinkGC::LargeObjectHeapIndex)
        heapIndex = heapIndexForObjectSize(size);

    // TODO(haraken): We don't support reallocate() for finalizable objects.
    ASSERT(!Heap::gcInfo(previousHeader->gcInfoIndex())->hasFinalizer());
    ASSERT(previousHeader->gcInfoIndex() == GCInfoTrait<T>::index());
    Address address = Heap::allocateOnHeapIndex(state, size, heapIndex, GCInfoTrait<T>::index());
    size_t copySize = previousHeader->payloadSize();
    if (copySize > size)
        copySize = size;
    memcpy(address, previous, copySize);
    HeapAllocHooks::reallocHookIfEnabled(static_cast<Address>(previous), address, size);
    return address;
}

template<typename Derived>
template<typename T>
void VisitorHelper<Derived>::handleWeakCell(Visitor* self, void* object)
{
    T** cell = reinterpret_cast<T**>(object);
    if (*cell && !ObjectAliveTrait<T>::isHeapObjectAlive(*cell))
        *cell = nullptr;
}

} // namespace blink

#endif // Heap_h
