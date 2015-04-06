// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GCInfo_h
#define GCInfo_h

#include "platform/heap/Visitor.h"
#include "wtf/Assertions.h"
#include "wtf/Atomics.h"
#include "wtf/TypeTraits.h"

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

} // namespace blink

#endif
