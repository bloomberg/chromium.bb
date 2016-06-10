// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitor_h
#define ScriptWrappableVisitor_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/WrapperVisitor.h"
#include "wtf/Deque.h"
#include "wtf/Vector.h"
#include <v8.h>


namespace blink {

class HeapObjectHeader;
template<typename T> class Member;

/**
 * ScriptWrappableVisitor is able to trace through the script wrappable
 * references. It is used during V8 garbage collection.  When this visitor is
 * set to the v8::Isolate as its embedder heap tracer, V8 will call it during
 * its garbage collection. At the beginning, it will call TracePrologue, then
 * repeatedly it will call AdvanceTracing, and at the end it will call
 * TraceEpilogue. Everytime V8 finds new wrappers, it will let the tracer know
 * using RegisterV8References.
 */
class CORE_EXPORT ScriptWrappableVisitor : public WrapperVisitor, public v8::EmbedderHeapTracer {
public:
    ScriptWrappableVisitor(v8::Isolate* isolate) : m_isolate(isolate) {};
    ~ScriptWrappableVisitor() override;
    /**
     * Replace all dead objects in the marking deque with nullptr after oilpan
     * gc.
     */
    static void invalidateDeadObjectsInMarkingDeque(v8::Isolate*);
    /**
     * Mark wrappers in all worlds for the given script wrappable as alive in
     * V8.
     */
    static void markWrappersInAllWorlds(const ScriptWrappable*, v8::Isolate*);
    /**
     * Mark given wrapper as alive in V8.
     */
    template <typename T>
    static void markWrapper(const v8::Persistent<T>* handle, v8::Isolate* isolate)
    {
        handle->RegisterExternalReference(isolate);
    }

    void TracePrologue() override;
    void RegisterV8References(const std::vector<std::pair<void*, void*>>& internalFieldsOfPotentialWrappers) override;
    void RegisterV8Reference(const std::pair<void*, void*>& internalFields);
    bool AdvanceTracing(double deadlineInMs, v8::EmbedderHeapTracer::AdvanceTracingActions) override;
    void TraceEpilogue() override;

    template <typename T>
    void markWrapper(const v8::Persistent<T>* handle) const
    {
        markWrapper(handle, m_isolate);
    }

    void dispatchTraceWrappers(const ScriptWrappable*) const override;
#define DECLARE_DISPATCH_TRACE_WRAPPERS(className)                   \
    void dispatchTraceWrappers(const className*) const override;

    WRAPPER_VISITOR_SPECIAL_CLASSES(DECLARE_DISPATCH_TRACE_WRAPPERS);

#undef DECLARE_DISPATCH_TRACE_WRAPPERS
    virtual void dispatchTraceWrappers(const void*) const {}

    void traceWrappers(const ScopedPersistent<v8::Value>*) const override;

    void invalidateDeadObjectsInMarkingDeque();

private:
    bool markWrapperHeader(HeapObjectHeader*) const;
    bool markWrapperHeader(const ScriptWrappable*) const override;
    bool markWrapperHeader(const void* garbageCollected) const override;
    bool m_tracingInProgress = false;
    /**
     * Collection of ScriptWrappables we need to trace from. We assume it is
     * safe to hold on to the raw pointers because:
     *     * oilpan object cannot move
     *     * for the oilpan gc, this deque is part of the root set, so objects
     *       in the deque will not die
     */
    mutable WTF::Deque<const ScriptWrappable*> m_markingDeque;
    /**
     * Collection of headers we need to unmark after the tracing finished. We
     * assume it is safe to hold on to the headers because:
     *     * oilpan objects cannot move
     *     * objects this headers belong to are considered alive by the oilpan
     *       gc (so they cannot be reclaimed). For the oilpan gc, wrappers are
     *       part of the root set and wrapper will keep its ScriptWrappable
     *       alive. Wrapper reachability is a subgraph of oilpan reachability,
     *       therefore anything we find during tracing wrappers will be found by
     *       oilpan gc too.
     */
    mutable WTF::Vector<HeapObjectHeader*> m_headersToUnmark;
    v8::Isolate* m_isolate;
};

}
#endif
