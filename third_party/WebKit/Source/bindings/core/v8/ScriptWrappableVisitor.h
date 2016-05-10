// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitor_h
#define ScriptWrappableVisitor_h

#include "core/CoreExport.h"
#include "platform/heap/WrapperVisitor.h"
#include "wtf/Vector.h"
#include <v8.h>


namespace blink {

class ScriptWrappable;
class HeapObjectHeader;
template<typename T> class Member;

/**
 * ScriptWrappableVisitor is able to trace through the script wrappable
 * references. It is used during V8 garbage collection.  When this visitor is
 * set to the v8::Isolate as its embedder heap tracer, V8 will call it during
 * its garbage collection. At the beginning, it will call TracePrologue, then
 * repeatedly (as v8 discovers more wrappers) it will call TraceWrappersFrom,
 * and at the end it will call TraceEpilogue.
 */
class CORE_EXPORT ScriptWrappableVisitor : public WrapperVisitor, public v8::EmbedderHeapTracer {
public:
    ScriptWrappableVisitor(v8::Isolate* isolate) : m_isolate(isolate) {};
    ~ScriptWrappableVisitor() override;
    /**
     * Mark wrappers in all worlds for the given script wrappable as alive in
     * V8.
     */
    static void markWrappersInAllWorlds(const ScriptWrappable*, v8::Isolate*);
    /**
     * Mark given wrapper as alive in V8.
     */
    static void markWrapper(const v8::Persistent<v8::Object>* handle, v8::Isolate*);

    void TracePrologue() override;
    void TraceWrappersFrom(const std::vector<std::pair<void*, void*>>& internalFieldsOfPotentialWrappers) override;
    void TraceEpilogue() override;

    void traceWrappersFrom(const ScriptWrappable*) const;

    void markWrapper(const v8::Persistent<v8::Object>*) const;
    virtual void dispatchTraceWrappers(const ScriptWrappable*) const;
#define DECLARE_DISPATCH_TRACE_WRAPPERS(className)               \
    virtual void dispatchTraceWrappers(const className*) const;

    WRAPPER_VISITOR_SPECIAL_CLASSES(DECLARE_DISPATCH_TRACE_WRAPPERS);

#undef DECLARE_DISPATCH_TRACE_WRAPPERS
    virtual void dispatchTraceWrappers(const void*) const {}

private:
    virtual bool markWrapperHeader(const ScriptWrappable*, const void*) const;
    virtual bool markWrapperHeader(const void* garbageCollected, const void*) const;
    inline void addHeaderToUnmark(HeapObjectHeader*) const;
    inline void traceWrappersFrom(std::pair<void*, void*> internalFields);
    bool m_tracingInProgress = false;
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
