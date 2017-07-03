// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeapAllocator_h
#define HeapAllocator_h

#include "build/build_config.h"
#include "platform/heap/Heap.h"
#include "platform/heap/Persistent.h"
#include "platform/heap/TraceTraits.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/HashCountedSet.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/HashTable.h"
#include "platform/wtf/LinkedHashSet.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/TypeTraits.h"
#include "platform/wtf/Vector.h"

namespace blink {

template <typename T, typename Traits = WTF::VectorTraits<T>>
class HeapVectorBacking {
  DISALLOW_NEW();
  IS_GARBAGE_COLLECTED_TYPE();

 public:
  static void Finalize(void* pointer);
  void FinalizeGarbageCollectedObject() { Finalize(this); }
};

template <typename Table>
class HeapHashTableBacking {
  DISALLOW_NEW();
  IS_GARBAGE_COLLECTED_TYPE();

 public:
  static void Finalize(void* pointer);
  void FinalizeGarbageCollectedObject() { Finalize(this); }
};

// This is a static-only class used as a trait on collections to make them heap
// allocated.  However see also HeapListHashSetAllocator.
class PLATFORM_EXPORT HeapAllocator {
  STATIC_ONLY(HeapAllocator);

 public:
  using Visitor = blink::Visitor;
  static const bool kIsGarbageCollected = true;

  template <typename T>
  static size_t MaxElementCountInBackingStore() {
    return kMaxHeapObjectSize / sizeof(T);
  }

  template <typename T>
  static size_t QuantizedSize(size_t count) {
    CHECK(count <= MaxElementCountInBackingStore<T>());
    return ThreadHeap::AllocationSizeFromSize(count * sizeof(T)) -
           sizeof(HeapObjectHeader);
  }
  template <typename T>
  static T* AllocateVectorBacking(size_t size) {
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
    DCHECK(state->IsAllocationAllowed());
    size_t gc_info_index = GCInfoTrait<HeapVectorBacking<T>>::Index();
    NormalPageArena* arena =
        static_cast<NormalPageArena*>(state->VectorBackingArena(gc_info_index));
    return reinterpret_cast<T*>(arena->AllocateObject(
        ThreadHeap::AllocationSizeFromSize(size), gc_info_index));
  }
  template <typename T>
  static T* AllocateExpandedVectorBacking(size_t size) {
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
    DCHECK(state->IsAllocationAllowed());
    size_t gc_info_index = GCInfoTrait<HeapVectorBacking<T>>::Index();
    NormalPageArena* arena = static_cast<NormalPageArena*>(
        state->ExpandedVectorBackingArena(gc_info_index));
    return reinterpret_cast<T*>(arena->AllocateObject(
        ThreadHeap::AllocationSizeFromSize(size), gc_info_index));
  }
  static void FreeVectorBacking(void*);
  static bool ExpandVectorBacking(void*, size_t);
  static bool ShrinkVectorBacking(void* address,
                                  size_t quantized_current_size,
                                  size_t quantized_shrunk_size);
  template <typename T>
  static T* AllocateInlineVectorBacking(size_t size) {
    size_t gc_info_index = GCInfoTrait<HeapVectorBacking<T>>::Index();
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
    const char* type_name = WTF_HEAP_PROFILER_TYPE_NAME(HeapVectorBacking<T>);
    return reinterpret_cast<T*>(ThreadHeap::AllocateOnArenaIndex(
        state, size, BlinkGC::kInlineVectorArenaIndex, gc_info_index,
        type_name));
  }
  static void FreeInlineVectorBacking(void*);
  static bool ExpandInlineVectorBacking(void*, size_t);
  static bool ShrinkInlineVectorBacking(void* address,
                                        size_t quantized_current_size,
                                        size_t quantized_shrunk_size);

  template <typename T, typename HashTable>
  static T* AllocateHashTableBacking(size_t size) {
    size_t gc_info_index =
        GCInfoTrait<HeapHashTableBacking<HashTable>>::Index();
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
    const char* type_name =
        WTF_HEAP_PROFILER_TYPE_NAME(HeapHashTableBacking<HashTable>);
    return reinterpret_cast<T*>(ThreadHeap::AllocateOnArenaIndex(
        state, size, BlinkGC::kHashTableArenaIndex, gc_info_index, type_name));
  }
  template <typename T, typename HashTable>
  static T* AllocateZeroedHashTableBacking(size_t size) {
    return AllocateHashTableBacking<T, HashTable>(size);
  }
  static void FreeHashTableBacking(void* address);
  static bool ExpandHashTableBacking(void*, size_t);

  template <typename Return, typename Metadata>
  static Return Malloc(size_t size, const char* type_name) {
    return reinterpret_cast<Return>(ThreadHeap::Allocate<Metadata>(
        size, IsEagerlyFinalizedType<Metadata>::value));
  }

#if defined(OS_WIN) && defined(COMPILER_MSVC)
  // MSVC eagerly instantiates the unused 'operator delete',
  // provide a version that asserts and fails at run-time if
  // used.
  // Elsewhere we expect compilation to fail if 'delete' is
  // attempted used and instantiated with a HeapAllocator-based
  // object, as HeapAllocator::free is not provided.
  static void Free(void*) { NOTREACHED(); }
#endif

  template <typename T>
  static void* NewArray(size_t bytes) {
    NOTREACHED();
    return 0;
  }

  static void DeleteArray(void* ptr) { NOTREACHED(); }

  static bool IsAllocationAllowed() {
    return ThreadState::Current()->IsAllocationAllowed();
  }

  static bool IsObjectResurrectionForbidden() {
    return ThreadState::Current()->IsObjectResurrectionForbidden();
  }

  template <typename T>
  static bool IsHeapObjectAlive(T* object) {
    return ThreadHeap::IsHeapObjectAlive(object);
  }

  template <typename VisitorDispatcher>
  static void MarkNoTracing(VisitorDispatcher visitor, const void* t) {
    visitor->MarkNoTracing(t);
  }

  template <typename VisitorDispatcher, typename T, typename Traits>
  static void Trace(VisitorDispatcher visitor, T& t) {
    TraceCollectionIfEnabled<WTF::IsTraceableInCollectionTrait<Traits>::value,
                             Traits::kWeakHandlingFlag,
                             WTF::kWeakPointersActWeak, T,
                             Traits>::Trace(visitor, t);
  }

  template <typename VisitorDispatcher>
  static void RegisterDelayedMarkNoTracing(VisitorDispatcher visitor,
                                           const void* object) {
    visitor->RegisterDelayedMarkNoTracing(object);
  }

  template <typename VisitorDispatcher>
  static void RegisterWeakMembers(VisitorDispatcher visitor,
                                  const void* closure,
                                  WeakCallback callback) {
    visitor->RegisterWeakCallback(const_cast<void*>(closure), callback);
  }

  template <typename VisitorDispatcher>
  static void RegisterWeakTable(VisitorDispatcher visitor,
                                const void* closure,
                                EphemeronCallback iteration_callback,
                                EphemeronCallback iteration_done_callback) {
    visitor->RegisterWeakTable(closure, iteration_callback,
                               iteration_done_callback);
  }

#if DCHECK_IS_ON()
  template <typename VisitorDispatcher>
  static bool WeakTableRegistered(VisitorDispatcher visitor,
                                  const void* closure) {
    return visitor->WeakTableRegistered(closure);
  }
#endif

  template <typename T, typename VisitorDispatcher>
  static void RegisterBackingStoreReference(VisitorDispatcher visitor,
                                            T** slot) {
    visitor->RegisterBackingStoreReference(slot);
  }

  template <typename T, typename VisitorDispatcher>
  static void RegisterBackingStoreCallback(VisitorDispatcher visitor,
                                           T* backing_store,
                                           MovingObjectCallback callback,
                                           void* callback_data) {
    visitor->RegisterBackingStoreCallback(backing_store, callback,
                                          callback_data);
  }

  static void EnterGCForbiddenScope() {
    ThreadState::Current()->EnterGCForbiddenScope();
  }

  static void LeaveGCForbiddenScope() {
    ThreadState::Current()->LeaveGCForbiddenScope();
  }

 private:
  static void BackingFree(void*);
  static bool BackingExpand(void*, size_t);
  static bool BackingShrink(void*,
                            size_t quantized_current_size,
                            size_t quantized_shrunk_size);

  template <typename T, size_t u, typename V>
  friend class WTF::Vector;
  template <typename T, typename U, typename V, typename W>
  friend class WTF::HashSet;
  template <typename T,
            typename U,
            typename V,
            typename W,
            typename X,
            typename Y>
  friend class WTF::HashMap;
};

template <typename VisitorDispatcher, typename Value>
static void TraceListHashSetValue(VisitorDispatcher visitor, Value& value) {
  // We use the default hash traits for the value in the node, because
  // ListHashSet does not let you specify any specific ones.
  // We don't allow ListHashSet of WeakMember, so we set that one false
  // (there's an assert elsewhere), but we have to specify some value for the
  // strongify template argument, so we specify WTF::WeakPointersActWeak,
  // arbitrarily.
  TraceCollectionIfEnabled<
      WTF::IsTraceableInCollectionTrait<WTF::HashTraits<Value>>::value,
      WTF::kNoWeakHandlingInCollections, WTF::kWeakPointersActWeak, Value,
      WTF::HashTraits<Value>>::Trace(visitor, value);
}

// The inline capacity is just a dummy template argument to match the off-heap
// allocator.
// This inherits from the static-only HeapAllocator trait class, but we do
// declare pointers to instances.  These pointers are always null, and no
// objects are instantiated.
template <typename ValueArg, size_t inlineCapacity>
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
    HeapListHashSetAllocator* Get() const { return 0; }

    // No allocator object is needed.
    void CreateAllocatorIfNeeded() {}
    void ReleaseAllocator() {}

    // There is no allocator object in the HeapListHashSet (unlike in the
    // regular ListHashSet) so there is nothing to swap.
    void Swap(AllocatorProvider& other) {}
  };

  void Deallocate(void* dummy) {}

  // This is not a static method even though it could be, because it needs to
  // match the one that the (off-heap) ListHashSetAllocator has.  The 'this'
  // pointer will always be null.
  void* AllocateNode() {
    // Consider using a LinkedHashSet instead if this compile-time assert fails:
    static_assert(!WTF::IsWeak<ValueArg>::value,
                  "weak pointers in a ListHashSet will result in null entries "
                  "in the set");

    return Malloc<void*, Node>(
        sizeof(Node),
        nullptr /* Oilpan does not use the heap profiler at the moment. */);
  }

  template <typename VisitorDispatcher>
  static void TraceValue(VisitorDispatcher visitor, Node* node) {
    TraceListHashSetValue(visitor, node->value_);
  }
};

template <typename T, typename Traits>
void HeapVectorBacking<T, Traits>::Finalize(void* pointer) {
  static_assert(Traits::kNeedsDestruction,
                "Only vector buffers with items requiring destruction should "
                "be finalized");
  // See the comment in HeapVectorBacking::trace.
  static_assert(
      Traits::kCanClearUnusedSlotsWithMemset || std::is_polymorphic<T>::value,
      "HeapVectorBacking doesn't support objects that cannot be cleared as "
      "unused with memset or don't have a vtable");

  DCHECK(!WTF::IsTriviallyDestructible<T>::value);
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(pointer);
  // Use the payload size as recorded by the heap to determine how many
  // elements to finalize.
  size_t length = header->PayloadSize() / sizeof(T);
  T* buffer = reinterpret_cast<T*>(pointer);
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  // As commented above, HeapVectorBacking calls finalizers for unused slots
  // (which are already zeroed out).
  ANNOTATE_CHANGE_SIZE(buffer, length, 0, length);
#endif
  if (std::is_polymorphic<T>::value) {
    char* pointer = reinterpret_cast<char*>(buffer);
    for (unsigned i = 0; i < length; ++i) {
      char* element = pointer + i * sizeof(T);
      if (blink::VTableInitialized(element))
        reinterpret_cast<T*>(element)->~T();
    }
  } else {
    for (unsigned i = 0; i < length; ++i) {
      buffer[i].~T();
    }
  }
}

template <typename Table>
void HeapHashTableBacking<Table>::Finalize(void* pointer) {
  using Value = typename Table::ValueType;
  DCHECK(!WTF::IsTriviallyDestructible<Value>::value);
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(pointer);
  // Use the payload size as recorded by the heap to determine how many
  // elements to finalize.
  size_t length = header->PayloadSize() / sizeof(Value);
  Value* table = reinterpret_cast<Value*>(pointer);
  for (unsigned i = 0; i < length; ++i) {
    if (!Table::IsEmptyOrDeletedBucket(table[i]))
      table[i].~Value();
  }
}

template <typename KeyArg,
          typename MappedArg,
          typename HashArg = typename DefaultHash<KeyArg>::Hash,
          typename KeyTraitsArg = HashTraits<KeyArg>,
          typename MappedTraitsArg = HashTraits<MappedArg>>
class HeapHashMap : public HashMap<KeyArg,
                                   MappedArg,
                                   HashArg,
                                   KeyTraitsArg,
                                   MappedTraitsArg,
                                   HeapAllocator> {
  IS_GARBAGE_COLLECTED_TYPE();
  static_assert(WTF::IsTraceable<KeyArg>::value ||
                    WTF::IsTraceable<MappedArg>::value,
                "For hash maps without traceable elements, use HashMap<> "
                "instead of HeapHashMap<>");
};

template <typename ValueArg,
          typename HashArg = typename DefaultHash<ValueArg>::Hash,
          typename TraitsArg = HashTraits<ValueArg>>
class HeapHashSet
    : public HashSet<ValueArg, HashArg, TraitsArg, HeapAllocator> {
  IS_GARBAGE_COLLECTED_TYPE();
  static_assert(WTF::IsTraceable<ValueArg>::value,
                "For hash sets without traceable elements, use HashSet<> "
                "instead of HeapHashSet<>");
};

template <typename ValueArg,
          typename HashArg = typename DefaultHash<ValueArg>::Hash,
          typename TraitsArg = HashTraits<ValueArg>>
class HeapLinkedHashSet
    : public LinkedHashSet<ValueArg, HashArg, TraitsArg, HeapAllocator> {
  IS_GARBAGE_COLLECTED_TYPE();
  static_assert(WTF::IsTraceable<ValueArg>::value,
                "For sets without traceable elements, use LinkedHashSet<> "
                "instead of HeapLinkedHashSet<>");
};

template <typename ValueArg,
          size_t inlineCapacity = 0,  // The inlineCapacity is just a dummy to
                                      // match ListHashSet (off-heap).
          typename HashArg = typename DefaultHash<ValueArg>::Hash>
class HeapListHashSet
    : public ListHashSet<ValueArg,
                         inlineCapacity,
                         HashArg,
                         HeapListHashSetAllocator<ValueArg, inlineCapacity>> {
  IS_GARBAGE_COLLECTED_TYPE();
  static_assert(WTF::IsTraceable<ValueArg>::value,
                "For sets without traceable elements, use ListHashSet<> "
                "instead of HeapListHashSet<>");
};

template <typename Value,
          typename HashFunctions = typename DefaultHash<Value>::Hash,
          typename Traits = HashTraits<Value>>
class HeapHashCountedSet
    : public HashCountedSet<Value, HashFunctions, Traits, HeapAllocator> {
  IS_GARBAGE_COLLECTED_TYPE();
  static_assert(WTF::IsTraceable<Value>::value,
                "For counted sets without traceable elements, use "
                "HashCountedSet<> instead of HeapHashCountedSet<>");
};

template <typename T, size_t inlineCapacity = 0>
class HeapVector : public Vector<T, inlineCapacity, HeapAllocator> {
  IS_GARBAGE_COLLECTED_TYPE();

 public:
  HeapVector() {
    static_assert(WTF::IsTraceable<T>::value,
                  "For vectors without traceable elements, use Vector<> "
                  "instead of HeapVector<>");
  }

  explicit HeapVector(size_t size)
      : Vector<T, inlineCapacity, HeapAllocator>(size) {}

  HeapVector(size_t size, const T& val)
      : Vector<T, inlineCapacity, HeapAllocator>(size, val) {}

  template <size_t otherCapacity>
  HeapVector(const HeapVector<T, otherCapacity>& other)
      : Vector<T, inlineCapacity, HeapAllocator>(other) {}
};

template <typename T, size_t inlineCapacity = 0>
class HeapDeque : public Deque<T, inlineCapacity, HeapAllocator> {
  IS_GARBAGE_COLLECTED_TYPE();

 public:
  HeapDeque() {
    static_assert(WTF::IsTraceable<T>::value,
                  "For vectors without traceable elements, use Deque<> instead "
                  "of HeapDeque<>");
  }

  explicit HeapDeque(size_t size)
      : Deque<T, inlineCapacity, HeapAllocator>(size) {}

  HeapDeque(size_t size, const T& val)
      : Deque<T, inlineCapacity, HeapAllocator>(size, val) {}

  HeapDeque& operator=(const HeapDeque& other) {
    HeapDeque<T> copy(other);
    Deque<T, inlineCapacity, HeapAllocator>::Swap(copy);
    return *this;
  }

  template <size_t otherCapacity>
  HeapDeque(const HeapDeque<T, otherCapacity>& other)
      : Deque<T, inlineCapacity, HeapAllocator>(other) {}
};

}  // namespace blink

namespace WTF {

template <typename T>
struct VectorTraits<blink::Member<T>> : VectorTraitsBase<blink::Member<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T>
struct VectorTraits<blink::SameThreadCheckedMember<T>>
    : VectorTraitsBase<blink::SameThreadCheckedMember<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
  static const bool kCanSwapUsingCopyOrMove = false;
};

template <typename T>
struct VectorTraits<blink::TraceWrapperMember<T>>
    : VectorTraitsBase<blink::TraceWrapperMember<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
  static const bool kCanSwapUsingCopyOrMove = false;
};

template <typename T>
struct VectorTraits<blink::WeakMember<T>>
    : VectorTraitsBase<blink::WeakMember<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T>
struct VectorTraits<blink::UntracedMember<T>>
    : VectorTraitsBase<blink::UntracedMember<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <
    typename T,
    blink::WeaknessPersistentConfiguration weaknessConfiguration,
    blink::CrossThreadnessPersistentConfiguration crossThreadnessConfiguration>
struct VectorTraits<blink::PersistentBase<T,
                                          weaknessConfiguration,
                                          crossThreadnessConfiguration>>
    : VectorTraitsBase<blink::PersistentBase<T,
                                             weaknessConfiguration,
                                             crossThreadnessConfiguration>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = true;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = false;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T>
struct VectorTraits<blink::HeapVector<T, 0>>
    : VectorTraitsBase<blink::HeapVector<T, 0>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T>
struct VectorTraits<blink::HeapDeque<T, 0>>
    : VectorTraitsBase<blink::HeapDeque<T, 0>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T, size_t inlineCapacity>
struct VectorTraits<blink::HeapVector<T, inlineCapacity>>
    : VectorTraitsBase<blink::HeapVector<T, inlineCapacity>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = VectorTraits<T>::needsDestruction;
  static const bool kCanInitializeWithMemset =
      VectorTraits<T>::canInitializeWithMemset;
  static const bool kCanClearUnusedSlotsWithMemset =
      VectorTraits<T>::canClearUnusedSlotsWithMemset;
  static const bool kCanMoveWithMemcpy = VectorTraits<T>::canMoveWithMemcpy;
};

template <typename T, size_t inlineCapacity>
struct VectorTraits<blink::HeapDeque<T, inlineCapacity>>
    : VectorTraitsBase<blink::HeapDeque<T, inlineCapacity>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = VectorTraits<T>::needsDestruction;
  static const bool kCanInitializeWithMemset =
      VectorTraits<T>::canInitializeWithMemset;
  static const bool kCanClearUnusedSlotsWithMemset =
      VectorTraits<T>::canClearUnusedSlotsWithMemset;
  static const bool kCanMoveWithMemcpy = VectorTraits<T>::canMoveWithMemcpy;
};

template <typename T>
struct HashTraits<blink::Member<T>> : SimpleClassHashTraits<blink::Member<T>> {
  STATIC_ONLY(HashTraits);
  // FIXME: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = blink::Member<T>*;
  using IteratorConstGetType = const blink::Member<T>*;
  using IteratorReferenceType = blink::Member<T>&;
  using IteratorConstReferenceType = const blink::Member<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, blink::Member<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::Member<T>& value) { return value; }
};

template <typename T>
struct HashTraits<blink::SameThreadCheckedMember<T>>
    : SimpleClassHashTraits<blink::SameThreadCheckedMember<T>> {
  STATIC_ONLY(HashTraits);
  // FIXME: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = blink::SameThreadCheckedMember<T>*;
  using IteratorConstGetType = const blink::SameThreadCheckedMember<T>*;
  using IteratorReferenceType = blink::SameThreadCheckedMember<T>&;
  using IteratorConstReferenceType = const blink::SameThreadCheckedMember<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value,
                    blink::SameThreadCheckedMember<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::SameThreadCheckedMember<T>& value) {
    return value;
  }

  static blink::SameThreadCheckedMember<T> EmptyValue() {
    return blink::SameThreadCheckedMember<T>(nullptr, nullptr);
  }
};

template <typename T>
struct HashTraits<blink::TraceWrapperMember<T>>
    : SimpleClassHashTraits<blink::TraceWrapperMember<T>> {
  STATIC_ONLY(HashTraits);
  // FIXME: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = blink::TraceWrapperMember<T>*;
  using IteratorConstGetType = const blink::TraceWrapperMember<T>*;
  using IteratorReferenceType = blink::TraceWrapperMember<T>&;
  using IteratorConstReferenceType = const blink::TraceWrapperMember<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, blink::TraceWrapperMember<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::TraceWrapperMember<T>& value) {
    return value;
  }

  static blink::TraceWrapperMember<T> EmptyValue() {
    return blink::TraceWrapperMember<T>(nullptr, nullptr);
  }
};

template <typename T>
struct HashTraits<blink::WeakMember<T>>
    : SimpleClassHashTraits<blink::WeakMember<T>> {
  STATIC_ONLY(HashTraits);
  static const bool kNeedsDestruction = false;
  // FIXME: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = blink::WeakMember<T>*;
  using IteratorConstGetType = const blink::WeakMember<T>*;
  using IteratorReferenceType = blink::WeakMember<T>&;
  using IteratorConstReferenceType = const blink::WeakMember<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, blink::WeakMember<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::WeakMember<T>& value) { return value; }

  template <typename VisitorDispatcher>
  static bool TraceInCollection(VisitorDispatcher visitor,
                                blink::WeakMember<T>& weak_member,
                                ShouldWeakPointersBeMarkedStrongly strongify) {
    if (strongify == kWeakPointersActStrong) {
      visitor->Trace(weak_member.Get());  // Strongified visit.
      return false;
    }
    return !blink::ThreadHeap::IsHeapObjectAlive(weak_member);
  }
};

template <typename T>
struct HashTraits<blink::UntracedMember<T>>
    : SimpleClassHashTraits<blink::UntracedMember<T>> {
  STATIC_ONLY(HashTraits);
  static const bool kNeedsDestruction = false;
  // FIXME: Implement proper const'ness for iterator types.
  using PeekInType = T*;
  using IteratorGetType = blink::UntracedMember<T>*;
  using IteratorConstGetType = const blink::UntracedMember<T>*;
  using IteratorReferenceType = blink::UntracedMember<T>&;
  using IteratorConstReferenceType = const blink::UntracedMember<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }
  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, blink::UntracedMember<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::UntracedMember<T>& value) {
    return value;
  }
};

template <typename T, size_t inlineCapacity>
struct IsTraceable<
    ListHashSetNode<T, blink::HeapListHashSetAllocator<T, inlineCapacity>>*> {
  STATIC_ONLY(IsTraceable);
  static_assert(sizeof(T), "T must be fully defined");
  // All heap allocated node pointers need visiting to keep the nodes alive,
  // regardless of whether they contain pointers to other heap allocated
  // objects.
  static const bool value = true;
};

template <typename T, size_t inlineCapacity>
struct IsGarbageCollectedType<
    ListHashSetNode<T, blink::HeapListHashSetAllocator<T, inlineCapacity>>> {
  static const bool value = true;
};

template <typename Set>
struct IsGarbageCollectedType<ListHashSetIterator<Set>> {
  static const bool value = IsGarbageCollectedType<Set>::value;
};

template <typename Set>
struct IsGarbageCollectedType<ListHashSetConstIterator<Set>> {
  static const bool value = IsGarbageCollectedType<Set>::value;
};

template <typename Set>
struct IsGarbageCollectedType<ListHashSetReverseIterator<Set>> {
  static const bool value = IsGarbageCollectedType<Set>::value;
};

template <typename Set>
struct IsGarbageCollectedType<ListHashSetConstReverseIterator<Set>> {
  static const bool value = IsGarbageCollectedType<Set>::value;
};

template <typename T, typename H>
struct HandleHashTraits : SimpleClassHashTraits<H> {
  STATIC_ONLY(HandleHashTraits);
  // TODO: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = H*;
  using IteratorConstGetType = const H*;
  using IteratorReferenceType = H&;
  using IteratorConstReferenceType = const H&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, H& storage) {
    storage = value;
  }

  static PeekOutType Peek(const H& value) { return value; }
};

template <typename T>
struct HashTraits<blink::Persistent<T>>
    : HandleHashTraits<T, blink::Persistent<T>> {};

template <typename T>
struct HashTraits<blink::CrossThreadPersistent<T>>
    : HandleHashTraits<T, blink::CrossThreadPersistent<T>> {};

template <typename Value,
          typename HashFunctions,
          typename Traits,
          typename VectorType>
inline void CopyToVector(
    const blink::HeapHashCountedSet<Value, HashFunctions, Traits>& set,
    VectorType& vector) {
  CopyToVector(static_cast<const HashCountedSet<Value, HashFunctions, Traits,
                                                blink::HeapAllocator>&>(set),
               vector);
}

}  // namespace WTF

#endif
