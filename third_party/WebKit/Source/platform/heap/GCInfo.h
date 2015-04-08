// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GCInfo_h
#define GCInfo_h

#include "platform/heap/Visitor.h"
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

// s_gcInfoTable holds the per-class GCInfo descriptors; each heap
// object header keeps its index into this table.
extern PLATFORM_EXPORT GCInfo const** s_gcInfoTable;

// GCInfo contains meta-data associated with objects allocated in the
// Blink heap. This meta-data consists of a function pointer used to
// trace the pointers in the object during garbage collection, an
// indication of whether or not the object needs a finalization
// callback, and a function pointer used to finalize the object when
// the garbage collector determines that the object is no longer
// reachable. There is a GCInfo struct for each class that directly
// inherits from GarbageCollected or GarbageCollectedFinalized.
struct GCInfo {
    bool hasFinalizer() const { return m_nonTrivialFinalizer; }
    bool hasVTable() const { return m_hasVTable; }
    TraceCallback m_trace;
    FinalizationCallback m_finalize;
    bool m_nonTrivialFinalizer;
    bool m_hasVTable;
#if ENABLE(GC_PROFILING)
    // |m_className| is held as a reference to prevent dtor being called at exit.
    const String& m_className;
#endif
};

#if ENABLE(ASSERT)
PLATFORM_EXPORT void assertObjectHasGCInfo(const void*, size_t gcInfoIndex);
#endif

class GCInfoTable {
public:
    PLATFORM_EXPORT static void ensureGCInfoIndex(const GCInfo*, size_t*);

    static void init();
    static void shutdown();

    // The (max + 1) GCInfo index supported.
    // We assume that 14 bits is enough to represent all possible types: during
    // telemetry runs, we see about 1000 different types, looking at the output
    // of the oilpan gc clang plugin, there appear to be at most about 6000
    // types, so 14 bits should be more than twice as many bits as we will ever
    // encounter.
    static const size_t maxIndex = 1 << 14;

private:
    static void resize();

    static int s_gcInfoIndex;
    static size_t s_gcInfoTableSize;
};

// This macro should be used when returning a unique 14 bit integer
// for a given gcInfo.
#define RETURN_GCINFO_INDEX()                                  \
    static size_t gcInfoIndex = 0;                             \
    ASSERT(s_gcInfoTable);                                     \
    if (!acquireLoad(&gcInfoIndex))                            \
        GCInfoTable::ensureGCInfoIndex(&gcInfo, &gcInfoIndex); \
    ASSERT(gcInfoIndex >= 1);                                  \
    ASSERT(gcInfoIndex < GCInfoTable::maxIndex);               \
    return gcInfoIndex;

template<typename T>
struct GCInfoAtBase {
    static size_t index()
    {
        static const GCInfo gcInfo = {
            TraceTrait<T>::trace,
            FinalizerTrait<T>::finalize,
            FinalizerTrait<T>::nonTrivialFinalizer,
            WTF::IsPolymorphic<T>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<T>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, bool = WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, GarbageCollected>::value> struct GetGarbageCollectedBase;

template<typename T>
struct GetGarbageCollectedBase<T, true> {
    typedef typename T::GarbageCollectedBase type;
};

template<typename T>
struct GetGarbageCollectedBase<T, false> {
    typedef T type;
};

template<typename T>
struct GCInfoTrait {
    static size_t index()
    {
        return GCInfoAtBase<typename GetGarbageCollectedBase<T>::type>::index();
    }
};

class HeapAllocator;
template<typename ValueArg, size_t inlineCapacity> class HeapListHashSetAllocator;
template<typename T, typename Traits> class HeapVectorBacking;
template<typename Table> class HeapHashTableBacking;

// The standard implementation of GCInfoTrait<T>::index() just returns a static
// from the class T, but we can't do that for HashMap, HashSet, Vector, etc.
// because they are in WTF and know nothing of GCInfos. Instead we have a
// specialization of GCInfoTrait for these four classes here.

template<typename Key, typename Value, typename T, typename U, typename V>
struct GCInfoTrait<HashMap<Key, Value, T, U, V, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = HashMap<Key, Value, T, U, V, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            nullptr,
            false, // HashMap needs no finalizer.
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename U, typename V>
struct GCInfoTrait<HashSet<T, U, V, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = HashSet<T, U, V, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            nullptr,
            false, // HashSet needs no finalizer.
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename U, typename V>
struct GCInfoTrait<LinkedHashSet<T, U, V, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = LinkedHashSet<T, U, V, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            LinkedHashSet<T, U, V, HeapAllocator>::finalize,
            true, // Needs finalization. The anchor needs to unlink itself from the chain.
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename ValueArg, size_t inlineCapacity, typename U>
struct GCInfoTrait<ListHashSet<ValueArg, inlineCapacity, U, HeapListHashSetAllocator<ValueArg, inlineCapacity>>> {
    static size_t index()
    {
        using TargetType = WTF::ListHashSet<ValueArg, inlineCapacity, U, HeapListHashSetAllocator<ValueArg, inlineCapacity>>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            nullptr,
            false, // ListHashSet needs no finalization though its backing might.
            false, // no vtable.
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename Allocator>
struct GCInfoTrait<WTF::ListHashSetNode<T, Allocator>> {
    static size_t index()
    {
        using TargetType = WTF::ListHashSetNode<T, Allocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            TargetType::finalize,
            !WTF::IsTriviallyDestructible<T>::value, // The node needs destruction if its data does.
            false, // no vtable.
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T>
struct GCInfoTrait<Vector<T, 0, HeapAllocator>> {
    static size_t index()
    {
#if ENABLE(GC_PROFILING)
        using TargetType = Vector<T, 0, HeapAllocator>;
#endif
        static const GCInfo gcInfo = {
            TraceTrait<Vector<T, 0, HeapAllocator>>::trace,
            nullptr,
            false, // Vector needs no finalizer if it has no inline capacity.
            WTF::IsPolymorphic<Vector<T, 0, HeapAllocator>>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, size_t inlineCapacity>
struct FinalizerTrait<Vector<T, inlineCapacity, HeapAllocator>> : public FinalizerTraitImpl<Vector<T, inlineCapacity, HeapAllocator>, true> { };

template<typename T, size_t inlineCapacity>
struct GCInfoTrait<Vector<T, inlineCapacity, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = Vector<T, inlineCapacity, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            FinalizerTrait<TargetType>::finalize,
            // Finalizer is needed to destruct things stored in the inline capacity.
            inlineCapacity && VectorTraits<T>::needsDestruction,
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T>
struct GCInfoTrait<Deque<T, 0, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = Deque<T, 0, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            nullptr,
            false, // Deque needs no finalizer if it has no inline capacity.
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename U, typename V>
struct GCInfoTrait<HashCountedSet<T, U, V, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = HashCountedSet<T, U, V, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            nullptr,
            false, // HashCountedSet is just a HashTable, and needs no finalizer.
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, size_t inlineCapacity>
struct FinalizerTrait<Deque<T, inlineCapacity, HeapAllocator>> : public FinalizerTraitImpl<Deque<T, inlineCapacity, HeapAllocator>, true> { };

template<typename T, size_t inlineCapacity>
struct GCInfoTrait<Deque<T, inlineCapacity, HeapAllocator>> {
    static size_t index()
    {
        using TargetType = Deque<T, inlineCapacity, HeapAllocator>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            FinalizerTrait<TargetType>::finalize,
            // Finalizer is needed to destruct things stored in the inline capacity.
            inlineCapacity && VectorTraits<T>::needsDestruction,
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename Traits>
struct GCInfoTrait<HeapVectorBacking<T, Traits>> {
    static size_t index()
    {
        using TargetType = HeapVectorBacking<T, Traits>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            FinalizerTrait<TargetType>::finalize,
            Traits::needsDestruction,
            false, // We don't support embedded objects in HeapVectors with vtables.
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename Table>
struct GCInfoTrait<HeapHashTableBacking<Table>> {
    static size_t index()
    {
        using TargetType = HeapHashTableBacking<Table>;
        static const GCInfo gcInfo = {
            TraceTrait<TargetType>::trace,
            HeapHashTableBacking<Table>::finalize,
            !WTF::IsTriviallyDestructible<typename Table::ValueType>::value,
            WTF::IsPolymorphic<TargetType>::value,
#if ENABLE(GC_PROFILING)
            TypenameStringTrait<TargetType>::get()
#endif
        };
        RETURN_GCINFO_INDEX();
    }
};

template<typename T, typename U, typename V, typename W, typename X> class HeapHashMap;
template<typename T, typename U, typename V> class HeapHashSet;
template<typename T, typename U, typename V> class HeapLinkedHashSet;
template<typename T, size_t inlineCapacity, typename U> class HeapListHashSet;
template<typename T, size_t inlineCapacity> class HeapVector;
template<typename T, size_t inlineCapacity> class HeapDeque;
template<typename T, typename U, typename V> class HeapHashCountedSet;

template<typename T, typename U, typename V, typename W, typename X>
struct GCInfoTrait<HeapHashMap<T, U, V, W, X>> : public GCInfoTrait<HashMap<T, U, V, W, X, HeapAllocator>> { };
template<typename T, typename U, typename V>
struct GCInfoTrait<HeapHashSet<T, U, V>> : public GCInfoTrait<HashSet<T, U, V, HeapAllocator>> { };
template<typename T, typename U, typename V>
struct GCInfoTrait<HeapLinkedHashSet<T, U, V>> : public GCInfoTrait<LinkedHashSet<T, U, V, HeapAllocator>> { };
template<typename T, size_t inlineCapacity, typename U>
struct GCInfoTrait<HeapListHashSet<T, inlineCapacity, U>> : public GCInfoTrait<ListHashSet<T, inlineCapacity, U, HeapListHashSetAllocator<T, inlineCapacity>>> { };
template<typename T, size_t inlineCapacity>
struct GCInfoTrait<HeapVector<T, inlineCapacity>> : public GCInfoTrait<Vector<T, inlineCapacity, HeapAllocator>> { };
template<typename T, size_t inlineCapacity>
struct GCInfoTrait<HeapDeque<T, inlineCapacity>> : public GCInfoTrait<Deque<T, inlineCapacity, HeapAllocator>> { };
template<typename T, typename U, typename V>
struct GCInfoTrait<HeapHashCountedSet<T, U, V>> : public GCInfoTrait<HashCountedSet<T, U, V, HeapAllocator>> { };

} // namespace blink

#endif
