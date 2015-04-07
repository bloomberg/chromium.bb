// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MarkingVisitor_h
#define MarkingVisitor_h

#include "platform/heap/MarkingVisitorImpl.h"

namespace blink {

template <Visitor::MarkingMode Mode>
class MarkingVisitor final : public Visitor, public MarkingVisitorImpl<MarkingVisitor<Mode>> {
public:
    using Impl = MarkingVisitorImpl<MarkingVisitor<Mode>>;
    friend class MarkingVisitorImpl<MarkingVisitor<Mode>>;

#if ENABLE(GC_PROFILING)
    using LiveObjectSet = HashSet<uintptr_t>;
    using LiveObjectMap = HashMap<String, LiveObjectSet>;
    using ObjectGraph = HashMap<uintptr_t, std::pair<uintptr_t, String>>;
#endif

    MarkingVisitor()
        : Visitor(Mode)
    {
    }

    virtual void markHeader(HeapObjectHeader* header, TraceCallback callback) override
    {
        Impl::markHeader(header, header->payload(), callback);
    }

    virtual void mark(const void* objectPointer, TraceCallback callback) override
    {
        Impl::mark(objectPointer, callback);
    }

    virtual void registerDelayedMarkNoTracing(const void* object) override
    {
        Impl::registerDelayedMarkNoTracing(object);
    }

    virtual void registerWeakMembers(const void* closure, const void* objectPointer, WeakPointerCallback callback) override
    {
        Impl::registerWeakMembers(closure, objectPointer, callback);
    }

    virtual void registerWeakTable(const void* closure, EphemeronCallback iterationCallback, EphemeronCallback iterationDoneCallback)
    {
        Impl::registerWeakTable(closure, iterationCallback, iterationDoneCallback);
    }

#if ENABLE(ASSERT)
    virtual bool weakTableRegistered(const void* closure)
    {
        return Impl::weakTableRegistered(closure);
    }
#endif

    virtual bool isMarked(const void* objectPointer) override
    {
        return Impl::isMarked(objectPointer);
    }

    virtual bool ensureMarked(const void* objectPointer) override
    {
        return Impl::ensureMarked(objectPointer);
    }

#if ENABLE(GC_PROFILING)
    virtual void recordObjectGraphEdge(const void* objectPointer) override
    {
        MutexLocker locker(objectGraphMutex());
        String className(classOf(objectPointer));
        {
            LiveObjectMap::AddResult result = currentlyLive().add(className, LiveObjectSet());
            result.storedValue->value.add(reinterpret_cast<uintptr_t>(objectPointer));
        }
        ObjectGraph::AddResult result = objectGraph().add(reinterpret_cast<uintptr_t>(objectPointer), std::make_pair(reinterpret_cast<uintptr_t>(m_hostObject), m_hostName));
        ASSERT(result.isNewEntry);
        // fprintf(stderr, "%s[%p] -> %s[%p]\n", m_hostName.ascii().data(), m_hostObject, className.ascii().data(), objectPointer);
    }

    void reportStats()
    {
        fprintf(stderr, "\n---------- AFTER MARKING -------------------\n");
        for (LiveObjectMap::iterator it = currentlyLive().begin(), end = currentlyLive().end(); it != end; ++it) {
            fprintf(stderr, "%s %u", it->key.ascii().data(), it->value.size());

            if (it->key == "blink::Document")
                reportStillAlive(it->value, previouslyLive().get(it->key));

            fprintf(stderr, "\n");
        }

        previouslyLive().swap(currentlyLive());
        currentlyLive().clear();

        for (uintptr_t object : objectsToFindPath()) {
            dumpPathToObjectFromObjectGraph(objectGraph(), object);
        }
    }

    static void reportStillAlive(LiveObjectSet current, LiveObjectSet previous)
    {
        int count = 0;

        fprintf(stderr, " [previously %u]", previous.size());
        for (uintptr_t object : current) {
            if (previous.find(object) == previous.end())
                continue;
            count++;
        }

        if (!count)
            return;

        fprintf(stderr, " {survived 2GCs %d: ", count);
        for (uintptr_t object : current) {
            if (previous.find(object) == previous.end())
                continue;
            fprintf(stderr, "%ld", object);
            if (--count)
                fprintf(stderr, ", ");
        }
        ASSERT(!count);
        fprintf(stderr, "}");
    }

    static void dumpPathToObjectFromObjectGraph(const ObjectGraph& graph, uintptr_t target)
    {
        ObjectGraph::const_iterator it = graph.find(target);
        if (it == graph.end())
            return;
        fprintf(stderr, "Path to %lx of %s\n", target, classOf(reinterpret_cast<const void*>(target)).ascii().data());
        while (it != graph.end()) {
            fprintf(stderr, "<- %lx of %s\n", it->value.first, it->value.second.utf8().data());
            it = graph.find(it->value.first);
        }
        fprintf(stderr, "\n");
    }

    static void dumpPathToObjectOnNextGC(void* p)
    {
        objectsToFindPath().add(reinterpret_cast<uintptr_t>(p));
    }

    static Mutex& objectGraphMutex()
    {
        AtomicallyInitializedStaticReference(Mutex, mutex, new Mutex);
        return mutex;
    }

    static LiveObjectMap& previouslyLive()
    {
        DEFINE_STATIC_LOCAL(LiveObjectMap, map, ());
        return map;
    }

    static LiveObjectMap& currentlyLive()
    {
        DEFINE_STATIC_LOCAL(LiveObjectMap, map, ());
        return map;
    }

    static ObjectGraph& objectGraph()
    {
        DEFINE_STATIC_LOCAL(ObjectGraph, graph, ());
        return graph;
    }

    static HashSet<uintptr_t>& objectsToFindPath()
    {
        DEFINE_STATIC_LOCAL(HashSet<uintptr_t>, set, ());
        return set;
    }
#endif

protected:
    virtual void registerWeakCellWithCallback(void** cell, WeakPointerCallback callback) override
    {
        Impl::registerWeakCellWithCallback(cell, callback);
    }

    inline bool shouldMarkObject(const void* objectPointer)
    {
        if (Mode != ThreadLocalMarking)
            return true;

        BasePage* page = pageFromObject(objectPointer);
        ASSERT(!page->orphaned());
        // When doing a thread local GC, the marker checks if
        // the object resides in another thread's heap. If it
        // does, the object should not be marked & traced.
        return page->terminating();
    }

#if ENABLE(ASSERT)
    virtual void checkMarkingAllowed() override
    {
        ASSERT(ThreadState::current()->isInGC());
    }
#endif
};

} // namespace blink

#endif
