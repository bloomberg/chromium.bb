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

#ifndef Visitor_h
#define Visitor_h

#include "wtf/Assertions.h"

#define TRACE_GC_MARKING 0
#define TRACE_GC_FINALIZATION 0

#if TRACE_GC_MARKING || TRACE_GC_FINALIZATION
#define TRACE_GC_USING_CLASSOF 1
#else
#define TRACE_GC_USING_CLASSOF 0
#endif

namespace WebCore {

typedef void (*FinalizationCallback)(void*);

class FinalizedHeapObjectHeader;
class HeapObjectHeader;
class Visitor;

typedef void (*VisitorCallback)(Visitor*, void* self);
typedef VisitorCallback TraceCallback;
typedef VisitorCallback WeakPointerCallback;

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
    const char* m_typeMarker;
    TraceCallback m_trace;
    FinalizationCallback m_finalize;
    bool m_nonTrivialFinalizer;
#if TRACE_GC_USING_CLASSOF
    ClassOfCallback m_classOf;
#endif
};

// Macros to declare and define GCInfo structures for objects
// allocated in the Blink garbage-collected heap.
#define DECLARE_GC_INFO                                                       \
public:                                                                       \
    static const GCInfo s_gcInfo;                                             \
    template<typename Any> friend struct FinalizerTrait;                      \
private:                                                                      \

#define DEFINE_GC_INFO(Type)                                                  \
const GCInfo Type::s_gcInfo = {                                               \
    #Type,                                                                    \
    TraceTrait<Type>::trace,                                                  \
    FinalizerTrait<Type>::finalize,                                           \
    FinalizerTrait<Type>::nonTrivialFinalizer,                                \
    CLASSOF_FUNC(Type)                                                        \
};                                                                            \

// Trait to get the GCInfo structure for types that have their
// instances allocated in the Blink garbage-collected heap.
template<typename T>
struct GCInfoTrait {
    static const GCInfo* get()
    {
        return &T::s_gcInfo;
    }
};

template<typename T>
const char* getTypeMarker()
{
    return GCInfoTrait<T>::get()->m_typeMarker;
}

// Visitor is used to traverse the Blink object graph. Used for the
// marking phase of the mark-sweep garbage collector.
//
// Pointers are marked and pushed on the marking stack by calling the
// |mark| method with the pointer as an argument.
//
// Pointers within objects are traced by calling the |trace| methods
// with the object as an argument. Tracing objects will mark all of the
// contained pointers and push them on the marking stack.
class Visitor {
public:
    template<typename T>
    void mark(T* t)
    {
        // FIXME: Implement.
        ASSERT_NOT_REACHED();
    }

    // This method marks an object and adds it to the set of objects
    // that should have their trace method called. Since not all
    // objects have vtables we have to have the callback as an
    // explicit argument, but we can use the templated one-argument
    // mark method above to automatically provide the callback
    // function.
    virtual void mark(const void*, TraceCallback) = 0;

    // Used to mark objects during conservative scanning.
    virtual void mark(HeapObjectHeader*, TraceCallback) = 0;
    virtual void mark(FinalizedHeapObjectHeader*, TraceCallback) = 0;
};

}

#endif
