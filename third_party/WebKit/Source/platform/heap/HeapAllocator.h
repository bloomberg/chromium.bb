// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeapAllocator_h
#define HeapAllocator_h

#include "platform/heap/Heap.h"
#include "platform/heap/TraceTraits.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Atomics.h"
#include "wtf/Deque.h"
#include "wtf/HashCountedSet.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/HashTable.h"
#include "wtf/LinkedHashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/TypeTraits.h"
#include "wtf/Vector.h"

namespace blink {

// This is a static-only class used as a trait on collections to make them heap
// allocated.  However see also HeapListHashSetAllocator.
class PLATFORM_EXPORT HeapAllocator {
    STATIC_ONLY(HeapAllocator);
public:
    using Visitor = blink::Visitor;
    static const bool isGarbageCollected = true;

    template<typename T>
    static size_t quantizedSize(size_t count)
    {
        RELEASE_ASSERT(count <= maxHeapObjectSize / sizeof(T));
        return Heap::allocationSizeFromSize(count * sizeof(T)) - sizeof(HeapObjectHeader);
    }
    template <typename T>
    static T* allocateVectorBacking(size_t size)
    {
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        ASSERT(state->isAllocationAllowed());
        size_t gcInfoIndex = GCInfoTrait<HeapVectorBacking<T, VectorTraits<T>>>::index();
        NormalPageHeap* heap = static_cast<NormalPageHeap*>(state->vectorBackingHeap(gcInfoIndex));
        return reinterpret_cast<T*>(heap->allocateObject(Heap::allocationSizeFromSize(size), gcInfoIndex));
    }
    template <typename T>
    static T* allocateExpandedVectorBacking(size_t size)
    {
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        ASSERT(state->isAllocationAllowed());
        size_t gcInfoIndex = GCInfoTrait<HeapVectorBacking<T, VectorTraits<T>>>::index();
        NormalPageHeap* heap = static_cast<NormalPageHeap*>(state->expandedVectorBackingHeap(gcInfoIndex));
        return reinterpret_cast<T*>(heap->allocateObject(Heap::allocationSizeFromSize(size), gcInfoIndex));
    }
    static void freeVectorBacking(void*);
    static bool expandVectorBacking(void*, size_t);
    static bool shrinkVectorBacking(void* address, size_t quantizedCurrentSize, size_t quantizedShrunkSize);
    template <typename T>
    static T* allocateInlineVectorBacking(size_t size)
    {
        size_t gcInfoIndex = GCInfoTrait<HeapVectorBacking<T, VectorTraits<T>>>::index();
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        return reinterpret_cast<T*>(Heap::allocateOnHeapIndex(state, size, BlinkGC::InlineVectorHeapIndex, gcInfoIndex));
    }
    static void freeInlineVectorBacking(void*);
    static bool expandInlineVectorBacking(void*, size_t);
    static bool shrinkInlineVectorBacking(void* address, size_t quantizedCurrentSize, size_t quantizedShrunkSize);

    template <typename T, typename HashTable>
    static T* allocateHashTableBacking(size_t size)
    {
        size_t gcInfoIndex = GCInfoTrait<HeapHashTableBacking<HashTable>>::index();
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        return reinterpret_cast<T*>(Heap::allocateOnHeapIndex(state, size, BlinkGC::HashTableHeapIndex, gcInfoIndex));
    }
    template <typename T, typename HashTable>
    static T* allocateZeroedHashTableBacking(size_t size)
    {
        return allocateHashTableBacking<T, HashTable>(size);
    }
    static void freeHashTableBacking(void* address);
    static bool expandHashTableBacking(void*, size_t);

    template <typename Return, typename Metadata>
    static Return malloc(size_t size, const char* typeName)
    {
        return reinterpret_cast<Return>(Heap::allocate<Metadata>(size, IsEagerlyFinalizedType<Metadata>::value));
    }
    static void free(void* address) { }
    template<typename T>
    static void* newArray(size_t bytes)
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    static void deleteArray(void* ptr)
    {
        ASSERT_NOT_REACHED();
    }

    static bool isAllocationAllowed()
    {
        return ThreadState::current()->isAllocationAllowed();
    }

    template<typename T>
    static bool isHeapObjectAlive(T* object)
    {
        return Heap::isHeapObjectAlive(object);
    }

    template<typename VisitorDispatcher>
    static void markNoTracing(VisitorDispatcher visitor, const void* t) { visitor->markNoTracing(t); }

    template<typename VisitorDispatcher, typename T, typename Traits>
    static void trace(VisitorDispatcher visitor, T& t)
    {
        TraceCollectionIfEnabled<WTF::NeedsTracingTrait<Traits>::value, Traits::weakHandlingFlag, WTF::WeakPointersActWeak, T, Traits>::trace(visitor, t);
    }

    template<typename VisitorDispatcher>
    static void registerDelayedMarkNoTracing(VisitorDispatcher visitor, const void* object)
    {
        visitor->registerDelayedMarkNoTracing(object);
    }

    template<typename VisitorDispatcher>
    static void registerWeakMembers(VisitorDispatcher visitor, const void* closure, const void* object, WeakCallback callback)
    {
        visitor->registerWeakMembers(closure, object, callback);
    }

    template<typename VisitorDispatcher>
    static void registerWeakTable(VisitorDispatcher visitor, const void* closure, EphemeronCallback iterationCallback, EphemeronCallback iterationDoneCallback)
    {
        visitor->registerWeakTable(closure, iterationCallback, iterationDoneCallback);
    }

#if ENABLE(ASSERT)
    template<typename VisitorDispatcher>
    static bool weakTableRegistered(VisitorDispatcher visitor, const void* closure)
    {
        return visitor->weakTableRegistered(closure);
    }
#endif

    template<typename T>
    struct ResultType {
        using Type = T*;
    };

    template<typename T>
    struct OtherType {
        using Type = T*;
    };

    template<typename T>
    static T& getOther(T* other)
    {
        return *other;
    }

    static void enterGCForbiddenScope()
    {
        ThreadState::current()->enterGCForbiddenScope();
    }

    static void leaveGCForbiddenScope()
    {
        ThreadState::current()->leaveGCForbiddenScope();
    }

private:
    static void backingFree(void*);
    static bool backingExpand(void*, size_t);
    static bool backingShrink(void*, size_t quantizedCurrentSize, size_t quantizedShrunkSize);

    template<typename T, size_t u, typename V> friend class WTF::Vector;
    template<typename T, typename U, typename V, typename W> friend class WTF::HashSet;
    template<typename T, typename U, typename V, typename W, typename X, typename Y> friend class WTF::HashMap;
};

template<typename VisitorDispatcher, typename Value>
static void traceListHashSetValue(VisitorDispatcher visitor, Value& value)
{
    // We use the default hash traits for the value in the node, because
    // ListHashSet does not let you specify any specific ones.
    // We don't allow ListHashSet of WeakMember, so we set that one false
    // (there's an assert elsewhere), but we have to specify some value for the
    // strongify template argument, so we specify WTF::WeakPointersActWeak,
    // arbitrarily.
    TraceCollectionIfEnabled<WTF::NeedsTracingTrait<WTF::HashTraits<Value>>::value, WTF::NoWeakHandlingInCollections, WTF::WeakPointersActWeak, Value, WTF::HashTraits<Value>>::trace(visitor, value);
}

// The inline capacity is just a dummy template argument to match the off-heap
// allocator.
// This inherits from the static-only HeapAllocator trait class, but we do
// declare pointers to instances.  These pointers are always null, and no
// objects are instantiated.
template<typename ValueArg, size_t inlineCapacity>
class HeapListHashSetAllocator : public HeapAllocator {
    DISALLOW_NEW();
public:
    using TableAllocator = HeapAllocator;
    using Node = WTF::ListHashSetNode<ValueArg, HeapListHashSetAllocator>;

    class AllocatorProvider {
        DISALLOW_NEW();
    public:
        // For the heap allocation we don't need an actual allocator object, so
        // we just return null.
        HeapListHashSetAllocator* get() const { return 0; }

        // No allocator object is needed.
        void createAllocatorIfNeeded() { }
        void releaseAllocator() { }

        // There is no allocator object in the HeapListHashSet (unlike in the
        // regular ListHashSet) so there is nothing to swap.
        void swap(AllocatorProvider& other) { }
    };

    void deallocate(void* dummy) { }

    // This is not a static method even though it could be, because it needs to
    // match the one that the (off-heap) ListHashSetAllocator has.  The 'this'
    // pointer will always be null.
    void* allocateNode()
    {
        // Consider using a LinkedHashSet instead if this compile-time assert fails:
        static_assert(!WTF::IsWeak<ValueArg>::value, "weak pointers in a ListHashSet will result in null entries in the set");

        return malloc<void*, Node>(sizeof(Node), nullptr /* Oilpan does not use the heap profiler at the moment. */);
    }

    template<typename VisitorDispatcher>
    static void traceValue(VisitorDispatcher visitor, Node* node)
    {
        traceListHashSetValue(visitor, node->m_value);
    }
};

template<typename T, typename Traits = WTF::VectorTraits<T>> class HeapVectorBacking {
    DISALLOW_NEW();
public:
    static void finalize(void* pointer);
    void finalizeGarbageCollectedObject() { finalize(this); }
};

template<typename T, typename Traits>
void HeapVectorBacking<T, Traits>::finalize(void* pointer)
{
    static_assert(Traits::needsDestruction, "Only vector buffers with items requiring destruction should be finalized");
    // See the comment in HeapVectorBacking::trace.
    static_assert(Traits::canClearUnusedSlotsWithMemset || std::is_polymorphic<T>::value, "HeapVectorBacking doesn't support objects that cannot be cleared as unused with memset or don't have a vtable");

    ASSERT(!WTF::IsTriviallyDestructible<T>::value);
    HeapObjectHeader* header = HeapObjectHeader::fromPayload(pointer);
    ASSERT(header->checkHeader());
    // Use the payload size as recorded by the heap to determine how many
    // elements to finalize.
    size_t length = header->payloadSize() / sizeof(T);
    T* buffer = reinterpret_cast<T*>(pointer);
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    // As commented above, HeapVectorBacking calls finalizers for unused slots
    // (which are already zeroed out).
    ANNOTATE_CHANGE_SIZE(buffer, length, 0, length);
#endif
    if (std::is_polymorphic<T>::value) {
        for (unsigned i = 0; i < length; ++i) {
            if (blink::vTableInitialized(&buffer[i]))
                buffer[i].~T();
        }
    } else {
        for (unsigned i = 0; i < length; ++i) {
            buffer[i].~T();
        }
    }
}

template<typename Table> class HeapHashTableBacking {
    DISALLOW_NEW();
public:
    static void finalize(void* pointer);
    void finalizeGarbageCollectedObject() { finalize(this); }
};

template<typename Table>
void HeapHashTableBacking<Table>::finalize(void* pointer)
{
    using Value = typename Table::ValueType;
    ASSERT(!WTF::IsTriviallyDestructible<Value>::value);
    HeapObjectHeader* header = HeapObjectHeader::fromPayload(pointer);
    ASSERT(header->checkHeader());
    // Use the payload size as recorded by the heap to determine how many
    // elements to finalize.
    size_t length = header->payloadSize() / sizeof(Value);
    Value* table = reinterpret_cast<Value*>(pointer);
    for (unsigned i = 0; i < length; ++i) {
        if (!Table::isEmptyOrDeletedBucket(table[i]))
            table[i].~Value();
    }
}

template<
    typename KeyArg,
    typename MappedArg,
    typename HashArg = typename DefaultHash<KeyArg>::Hash,
    typename KeyTraitsArg = HashTraits<KeyArg>,
    typename MappedTraitsArg = HashTraits<MappedArg>>
class HeapHashMap : public HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg, HeapAllocator> {
    IS_GARBAGE_COLLECTED_TYPE();
};

template<
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg>>
class HeapHashSet : public HashSet<ValueArg, HashArg, TraitsArg, HeapAllocator> {
    IS_GARBAGE_COLLECTED_TYPE();
};

template<
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg>>
class HeapLinkedHashSet : public LinkedHashSet<ValueArg, HashArg, TraitsArg, HeapAllocator> {
    IS_GARBAGE_COLLECTED_TYPE();
};

template<
    typename ValueArg,
    size_t inlineCapacity = 0, // The inlineCapacity is just a dummy to match ListHashSet (off-heap).
    typename HashArg = typename DefaultHash<ValueArg>::Hash>
class HeapListHashSet : public ListHashSet<ValueArg, inlineCapacity, HashArg, HeapListHashSetAllocator<ValueArg, inlineCapacity>> {
    IS_GARBAGE_COLLECTED_TYPE();
};

template<
    typename Value,
    typename HashFunctions = typename DefaultHash<Value>::Hash,
    typename Traits = HashTraits<Value>>
class HeapHashCountedSet : public HashCountedSet<Value, HashFunctions, Traits, HeapAllocator> {
    IS_GARBAGE_COLLECTED_TYPE();
};

template<typename T, size_t inlineCapacity = 0>
class HeapVector : public Vector<T, inlineCapacity, HeapAllocator> {
    IS_GARBAGE_COLLECTED_TYPE();
public:
    HeapVector() { }

    explicit HeapVector(size_t size) : Vector<T, inlineCapacity, HeapAllocator>(size)
    {
    }

    HeapVector(size_t size, const T& val) : Vector<T, inlineCapacity, HeapAllocator>(size, val)
    {
    }

    template<size_t otherCapacity>
    HeapVector(const HeapVector<T, otherCapacity>& other)
        : Vector<T, inlineCapacity, HeapAllocator>(other)
    {
    }
};

template<typename T, size_t inlineCapacity = 0>
class HeapDeque : public Deque<T, inlineCapacity, HeapAllocator> {
    IS_GARBAGE_COLLECTED_TYPE();
public:
    HeapDeque() { }

    explicit HeapDeque(size_t size) : Deque<T, inlineCapacity, HeapAllocator>(size)
    {
    }

    HeapDeque(size_t size, const T& val) : Deque<T, inlineCapacity, HeapAllocator>(size, val)
    {
    }

    // FIXME: Doesn't work if there is an inline buffer, due to crbug.com/360572
    HeapDeque<T, 0>& operator=(const HeapDeque& other)
    {
        HeapDeque<T> copy(other);
        Deque<T, inlineCapacity, HeapAllocator>::swap(copy);
        return *this;
    }

    template<size_t otherCapacity>
    HeapDeque(const HeapDeque<T, otherCapacity>& other)
        : Deque<T, inlineCapacity, HeapAllocator>(other)
    {
    }
};

} // namespace blink

namespace WTF {

template <typename T> struct VectorTraits<blink::Member<T>> : VectorTraitsBase<blink::Member<T>> {
    STATIC_ONLY(VectorTraits);
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canClearUnusedSlotsWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T> struct VectorTraits<blink::WeakMember<T>> : VectorTraitsBase<blink::WeakMember<T>> {
    STATIC_ONLY(VectorTraits);
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canClearUnusedSlotsWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T> struct VectorTraits<blink::UntracedMember<T>> : VectorTraitsBase<blink::UntracedMember<T>> {
    STATIC_ONLY(VectorTraits);
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canClearUnusedSlotsWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T> struct VectorTraits<blink::HeapVector<T, 0>> : VectorTraitsBase<blink::HeapVector<T, 0>> {
    STATIC_ONLY(VectorTraits);
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canClearUnusedSlotsWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T> struct VectorTraits<blink::HeapDeque<T, 0>> : VectorTraitsBase<blink::HeapDeque<T, 0>> {
    STATIC_ONLY(VectorTraits);
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canClearUnusedSlotsWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T, size_t inlineCapacity> struct VectorTraits<blink::HeapVector<T, inlineCapacity>> : VectorTraitsBase<blink::HeapVector<T, inlineCapacity>> {
    STATIC_ONLY(VectorTraits);
    static const bool needsDestruction = VectorTraits<T>::needsDestruction;
    static const bool canInitializeWithMemset = VectorTraits<T>::canInitializeWithMemset;
    static const bool canClearUnusedSlotsWithMemset = VectorTraits<T>::canClearUnusedSlotsWithMemset;
    static const bool canMoveWithMemcpy = VectorTraits<T>::canMoveWithMemcpy;
};

template <typename T, size_t inlineCapacity> struct VectorTraits<blink::HeapDeque<T, inlineCapacity>> : VectorTraitsBase<blink::HeapDeque<T, inlineCapacity>> {
    STATIC_ONLY(VectorTraits);
    static const bool needsDestruction = VectorTraits<T>::needsDestruction;
    static const bool canInitializeWithMemset = VectorTraits<T>::canInitializeWithMemset;
    static const bool canClearUnusedSlotsWithMemset = VectorTraits<T>::canClearUnusedSlotsWithMemset;
    static const bool canMoveWithMemcpy = VectorTraits<T>::canMoveWithMemcpy;
};

template<typename T> struct HashTraits<blink::Member<T>> : SimpleClassHashTraits<blink::Member<T>> {
    STATIC_ONLY(HashTraits);
    // FIXME: The distinction between PeekInType and PassInType is there for
    // the sake of the reference counting handles. When they are gone the two
    // types can be merged into PassInType.
    // FIXME: Implement proper const'ness for iterator types. Requires support
    // in the marking Visitor.
    using PeekInType = RawPtr<T>;
    using PassInType = RawPtr<T>;
    using IteratorGetType = blink::Member<T>*;
    using IteratorConstGetType = const blink::Member<T>*;
    using IteratorReferenceType = blink::Member<T>&;
    using IteratorConstReferenceType = const blink::Member<T>&;
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return *x; }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return *x; }
    // FIXME: Similarly, there is no need for a distinction between PeekOutType
    // and PassOutType without reference counting.
    using PeekOutType = T*;
    using PassOutType = T*;

    template<typename U>
    static void store(const U& value, blink::Member<T>& storage) { storage = value; }

    static PeekOutType peek(const blink::Member<T>& value) { return value; }
    static PassOutType passOut(const blink::Member<T>& value) { return value; }
};

template<typename T> struct HashTraits<blink::WeakMember<T>> : SimpleClassHashTraits<blink::WeakMember<T>> {
    STATIC_ONLY(HashTraits);
    static const bool needsDestruction = false;
    // FIXME: The distinction between PeekInType and PassInType is there for
    // the sake of the reference counting handles. When they are gone the two
    // types can be merged into PassInType.
    // FIXME: Implement proper const'ness for iterator types. Requires support
    // in the marking Visitor.
    using PeekInType = RawPtr<T>;
    using PassInType = RawPtr<T>;
    using IteratorGetType = blink::WeakMember<T>*;
    using IteratorConstGetType = const blink::WeakMember<T>*;
    using IteratorReferenceType = blink::WeakMember<T>&;
    using IteratorConstReferenceType = const blink::WeakMember<T>&;
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return *x; }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return *x; }
    // FIXME: Similarly, there is no need for a distinction between PeekOutType
    // and PassOutType without reference counting.
    using PeekOutType = T*;
    using PassOutType = T*;

    template<typename U>
    static void store(const U& value, blink::WeakMember<T>& storage) { storage = value; }

    static PeekOutType peek(const blink::WeakMember<T>& value) { return value; }
    static PassOutType passOut(const blink::WeakMember<T>& value) { return value; }

    template<typename VisitorDispatcher>
    static bool traceInCollection(VisitorDispatcher visitor, blink::WeakMember<T>& weakMember, ShouldWeakPointersBeMarkedStrongly strongify)
    {
        if (strongify == WeakPointersActStrong) {
            visitor->trace(weakMember.get()); // Strongified visit.
            return false;
        }
        return !blink::Heap::isHeapObjectAlive(weakMember);
    }
};

template<typename T> struct HashTraits<blink::UntracedMember<T>> : SimpleClassHashTraits<blink::UntracedMember<T>> {
    STATIC_ONLY(HashTraits);
    static const bool needsDestruction = false;
    // FIXME: The distinction between PeekInType and PassInType is there for
    // the sake of the reference counting handles. When they are gone the two
    // types can be merged into PassInType.
    // FIXME: Implement proper const'ness for iterator types.
    using PeekInType = RawPtr<T>;
    using PassInType = RawPtr<T>;
    using IteratorGetType = blink::UntracedMember<T>*;
    using IteratorConstGetType = const blink::UntracedMember<T>*;
    using IteratorReferenceType = blink::UntracedMember<T>&;
    using IteratorConstReferenceType = const blink::UntracedMember<T>&;
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return *x; }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return *x; }
    // FIXME: Similarly, there is no need for a distinction between PeekOutType
    // and PassOutType without reference counting.
    using PeekOutType = T*;
    using PassOutType = T*;

    template<typename U>
    static void store(const U& value, blink::UntracedMember<T>& storage) { storage = value; }

    static PeekOutType peek(const blink::UntracedMember<T>& value) { return value; }
    static PassOutType passOut(const blink::UntracedMember<T>& value) { return value; }
};

template<typename T, size_t inlineCapacity>
struct NeedsTracing<ListHashSetNode<T, blink::HeapListHashSetAllocator<T, inlineCapacity>> *> {
    STATIC_ONLY(NeedsTracing);
    static_assert(sizeof(T), "T must be fully defined");
    // All heap allocated node pointers need visiting to keep the nodes alive,
    // regardless of whether they contain pointers to other heap allocated
    // objects.
    static const bool value = true;
};

} // namespace WTF

#endif
