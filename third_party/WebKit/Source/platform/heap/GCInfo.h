// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GCInfo_h
#define GCInfo_h

#include "platform/heap/Visitor.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Atomics.h"
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

// The FinalizerTraitImpl specifies how to finalize objects. Objects that
// inherit from GarbageCollectedFinalized are finalized by calling their
// |Finalize| method which by default will call the destructor on the object.
template <typename T, bool isGarbageCollectedFinalized>
struct FinalizerTraitImpl;

template <typename T>
struct FinalizerTraitImpl<T, true> {
  STATIC_ONLY(FinalizerTraitImpl);
  static void Finalize(void* obj) {
    static_assert(sizeof(T), "T must be fully defined");
    static_cast<T*>(obj)->FinalizeGarbageCollectedObject();
  };
};

template <typename T>
struct FinalizerTraitImpl<T, false> {
  STATIC_ONLY(FinalizerTraitImpl);
  static void Finalize(void* obj) {
    static_assert(sizeof(T), "T must be fully defined");
  };
};

// The FinalizerTrait is used to determine if a type requires finalization and
// what finalization means.
//
// By default classes that inherit from GarbageCollectedFinalized need
// finalization and finalization means calling the |Finalize| method of the
// object. The FinalizerTrait can be specialized if the default behavior is not
// desired.
template <typename T>
struct FinalizerTrait {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer =
      WTF::IsSubclassOfTemplate<typename std::remove_const<T>::type,
                                GarbageCollectedFinalized>::value;
  static void Finalize(void* obj) {
    FinalizerTraitImpl<T, kNonTrivialFinalizer>::Finalize(obj);
  }
};

class HeapAllocator;
template <typename ValueArg, size_t inlineCapacity>
class HeapListHashSetAllocator;
template <typename T, typename Traits>
class HeapVectorBacking;
template <typename Table>
class HeapHashTableBacking;

template <typename T, typename U, typename V>
struct FinalizerTrait<LinkedHashSet<T, U, V, HeapAllocator>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer = true;
  static void Finalize(void* obj) {
    FinalizerTraitImpl<LinkedHashSet<T, U, V, HeapAllocator>,
                       kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename T, typename Allocator>
struct FinalizerTrait<WTF::ListHashSetNode<T, Allocator>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer =
      !WTF::IsTriviallyDestructible<T>::value;
  static void Finalize(void* obj) {
    FinalizerTraitImpl<WTF::ListHashSetNode<T, Allocator>,
                       kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename T, size_t inlineCapacity>
struct FinalizerTrait<Vector<T, inlineCapacity, HeapAllocator>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer =
      inlineCapacity && VectorTraits<T>::kNeedsDestruction;
  static void Finalize(void* obj) {
    FinalizerTraitImpl<Vector<T, inlineCapacity, HeapAllocator>,
                       kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename T, size_t inlineCapacity>
struct FinalizerTrait<Deque<T, inlineCapacity, HeapAllocator>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer =
      inlineCapacity && VectorTraits<T>::kNeedsDestruction;
  static void Finalize(void* obj) {
    FinalizerTraitImpl<Deque<T, inlineCapacity, HeapAllocator>,
                       kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename Table>
struct FinalizerTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer =
      !WTF::IsTriviallyDestructible<typename Table::ValueType>::value;
  static void Finalize(void* obj) {
    FinalizerTraitImpl<HeapHashTableBacking<Table>,
                       kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename T, typename Traits>
struct FinalizerTrait<HeapVectorBacking<T, Traits>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer = Traits::kNeedsDestruction;
  static void Finalize(void* obj) {
    FinalizerTraitImpl<HeapVectorBacking<T, Traits>,
                       kNonTrivialFinalizer>::Finalize(obj);
  }
};

// GCInfo contains meta-data associated with object classes allocated in the
// Blink heap. This meta-data consists of a function pointer used to trace the
// pointers in the class instance during garbage collection, an indication of
// whether or not the instance needs a finalization callback, and a function
// pointer used to finalize the instance when the garbage collector determines
// that the instance is no longer reachable. There is a GCInfo struct for each
// class that directly inherits from GarbageCollected or
// GarbageCollectedFinalized.
struct GCInfo {
  bool HasFinalizer() const { return non_trivial_finalizer_; }
  bool HasVTable() const { return has_v_table_; }
  TraceCallback trace_;
  FinalizationCallback finalize_;
  bool non_trivial_finalizer_;
  bool has_v_table_;
};

// s_gcInfoTable holds the per-class GCInfo descriptors; each HeapObjectHeader
// keeps an index into this table.
extern PLATFORM_EXPORT GCInfo const** g_gc_info_table;

#if DCHECK_IS_ON()
PLATFORM_EXPORT void AssertObjectHasGCInfo(const void*, size_t gc_info_index);
#endif

class GCInfoTable {
  STATIC_ONLY(GCInfoTable);

 public:
  PLATFORM_EXPORT static void EnsureGCInfoIndex(const GCInfo*, size_t*);

  static void Init();

  static size_t GcInfoIndex() { return gc_info_index_; }

  // The (max + 1) GCInfo index supported.
  //
  // We assume that 14 bits is enough to represent all possible types: during
  // telemetry runs, we see about 1,000 different types; looking at the output
  // of the Oilpan GC Clang plugin, there appear to be at most about 6,000
  // types. Thus 14 bits should be more than twice as many bits as we will ever
  // need.
  static const size_t kMaxIndex = 1 << 14;

 private:
  static void Resize();

  static size_t gc_info_index_;
  static size_t gc_info_table_size_;
};

// GCInfoAtBaseType should be used when returning a unique 14 bit integer
// for a given gcInfo.
template <typename T>
struct GCInfoAtBaseType {
  STATIC_ONLY(GCInfoAtBaseType);
  static size_t Index() {
    static_assert(sizeof(T), "T must be fully defined");
    static const GCInfo kGcInfo = {
        TraceTrait<T>::Trace, FinalizerTrait<T>::Finalize,
        FinalizerTrait<T>::kNonTrivialFinalizer, std::is_polymorphic<T>::value,
    };

    static size_t gc_info_index = 0;
    DCHECK(g_gc_info_table);
    if (!AcquireLoad(&gc_info_index))
      GCInfoTable::EnsureGCInfoIndex(&kGcInfo, &gc_info_index);
    DCHECK_GE(gc_info_index, 1u);
    DCHECK(gc_info_index < GCInfoTable::kMaxIndex);
    return gc_info_index;
  }
};

template <typename T,
          bool = WTF::IsSubclassOfTemplate<typename std::remove_const<T>::type,
                                           GarbageCollected>::value>
struct GetGarbageCollectedType;

template <typename T>
struct GetGarbageCollectedType<T, true> {
  STATIC_ONLY(GetGarbageCollectedType);
  using type = typename T::GarbageCollectedType;
};

template <typename T>
struct GetGarbageCollectedType<T, false> {
  STATIC_ONLY(GetGarbageCollectedType);
  using type = T;
};

template <typename T>
struct GCInfoTrait {
  STATIC_ONLY(GCInfoTrait);
  static size_t Index() {
    return GCInfoAtBaseType<typename GetGarbageCollectedType<T>::type>::Index();
  }
};

template <typename U>
class GCInfoTrait<const U> : public GCInfoTrait<U> {};

template <typename T, typename U, typename V, typename W, typename X>
class HeapHashMap;
template <typename T, typename U, typename V>
class HeapHashSet;
template <typename T, typename U, typename V>
class HeapLinkedHashSet;
template <typename T, size_t inlineCapacity, typename U>
class HeapListHashSet;
template <typename T, size_t inlineCapacity>
class HeapVector;
template <typename T, size_t inlineCapacity>
class HeapDeque;
template <typename T, typename U, typename V>
class HeapHashCountedSet;

template <typename T, typename U, typename V, typename W, typename X>
struct GCInfoTrait<HeapHashMap<T, U, V, W, X>>
    : public GCInfoTrait<HashMap<T, U, V, W, X, HeapAllocator>> {};
template <typename T, typename U, typename V>
struct GCInfoTrait<HeapHashSet<T, U, V>>
    : public GCInfoTrait<HashSet<T, U, V, HeapAllocator>> {};
template <typename T, typename U, typename V>
struct GCInfoTrait<HeapLinkedHashSet<T, U, V>>
    : public GCInfoTrait<LinkedHashSet<T, U, V, HeapAllocator>> {};
template <typename T, size_t inlineCapacity, typename U>
struct GCInfoTrait<HeapListHashSet<T, inlineCapacity, U>>
    : public GCInfoTrait<
          ListHashSet<T,
                      inlineCapacity,
                      U,
                      HeapListHashSetAllocator<T, inlineCapacity>>> {};
template <typename T, size_t inlineCapacity>
struct GCInfoTrait<HeapVector<T, inlineCapacity>>
    : public GCInfoTrait<Vector<T, inlineCapacity, HeapAllocator>> {};
template <typename T, size_t inlineCapacity>
struct GCInfoTrait<HeapDeque<T, inlineCapacity>>
    : public GCInfoTrait<Deque<T, inlineCapacity, HeapAllocator>> {};
template <typename T, typename U, typename V>
struct GCInfoTrait<HeapHashCountedSet<T, U, V>>
    : public GCInfoTrait<HashCountedSet<T, U, V, HeapAllocator>> {};

}  // namespace blink

#endif
