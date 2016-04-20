// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitor_h
#define ScriptWrappableVisitor_h

#include "platform/heap/HeapAllocator.h"
#include <v8.h>
#include <vector>


namespace blink {

class HeapObjectHeader;
class ScriptWrappable;
class ScriptWrappableHeapTracer;
class NodeRareData;

/**
 * Declares non-virtual traceWrappers method. Should be used on
 * non-ScriptWrappable classes which should participate in wrapper tracing (e.g.
 * NodeRareData):
 *
 *     class NodeRareData {
 *     public:
 *         DECLARE_TRACE_WRAPPERS();
 *     }
 */
#define DECLARE_TRACE_WRAPPERS()                               \
    void traceWrappers(const ScriptWrappableVisitor*) const

/**
 * Declares virtual traceWrappers method. It is used in ScriptWrappable, can be
 * used to override the method in the subclasses, and can be used by
 * non-ScriptWrappable classes which expect to be inherited.
 */
#define DECLARE_VIRTUAL_TRACE_WRAPPERS()                       \
    virtual DECLARE_TRACE_WRAPPERS()

/**
 * Provides definition of traceWrappers method. Custom code will usually call
 * visitor->traceWrappers with all objects which could contribute to the set of
 * reachable wrappers:
 *
 *     DEFINE_TRACE_WRAPPERS(NodeRareData)
 *     {
 *         visitor->traceWrappers(m_nodeLists);
 *         visitor->traceWrappers(m_mutationObserverData);
 *     }
 */
#define DEFINE_TRACE_WRAPPERS(T)                              \
    void T::traceWrappers(const ScriptWrappableVisitor* visitor) const

/**
 * ScriptWrappableVisitor is able to trace through the script wrappable
 * references. It is used during V8 garbage collection.  When this visitor is
 * set to the v8::Isolate as its embedder heap tracer, V8 will call it during
 * its garbage collection. At the beginning, it will call TracePrologue, then
 * repeatedly (as v8 discovers more wrappers) it will call TraceWrappersFrom,
 * and at the end it will call TraceEpilogue.
 */
class ScriptWrappableVisitor : public v8::EmbedderHeapTracer {
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
    static void markWrapper(const v8::Persistent<v8::Object>& handle, v8::Isolate*);

    void TracePrologue() override;
    void TraceWrappersFrom(const std::vector<std::pair<void*, void*>>& internalFieldsOfPotentialWrappers) override;
    void TraceEpilogue() override;

    inline void addHeaderToUnmark(HeapObjectHeader*) const;
    void traceWrappers(const ScriptWrappable* wrappable) const;
    void traceWrappers(const ScriptWrappable& wrappable) const;
private:
    inline void traceWrappersFrom(std::pair<void*, void*> internalFields);
    inline void markHeader(const ScriptWrappable* scriptWrappable) const;
    inline void markHeader(const void* garbageCollected) const;
    inline bool isHeaderMarked(const void* garbageCollected) const;
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
