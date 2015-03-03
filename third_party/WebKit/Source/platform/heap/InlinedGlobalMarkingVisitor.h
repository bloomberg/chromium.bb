// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlinedGlobalMarkingVisitor_h
#define InlinedGlobalMarkingVisitor_h

#include "platform/heap/MarkingVisitorImpl.h"

namespace blink {

class InlinedGlobalMarkingVisitor final : public VisitorHelper<InlinedGlobalMarkingVisitor>, public MarkingVisitorImpl<InlinedGlobalMarkingVisitor> {
public:
    friend class VisitorHelper<InlinedGlobalMarkingVisitor>;
    using Helper = VisitorHelper<InlinedGlobalMarkingVisitor>;
    friend class MarkingVisitorImpl<InlinedGlobalMarkingVisitor>;
    using Impl = MarkingVisitorImpl<InlinedGlobalMarkingVisitor>;

    explicit InlinedGlobalMarkingVisitor(Visitor* visitor)
        : m_visitor(visitor)
    {
        ASSERT(visitor->isGlobalMarkingVisitor());
    }

    // Hack to unify interface to visitor->trace().
    // Without this hack, we need to use visitor.trace() for
    // trace(InlinedGlobalMarkingVisitor) and visitor->trace() for trace(Visitor*).
    InlinedGlobalMarkingVisitor* operator->() { return this; }

    // FIXME: This is a temporary hack to cheat old Blink GC plugin checks.
    // Old GC Plugin doesn't accept calling Helper::trace
    // as a valid mark. This manual redirect worksaround the issue by
    // making the method declaration on Visitor class.
    template <typename T>
    void trace(const T& t)
    {
        Helper::trace(t);
    }

    using Helper::mark;

    inline void mark(const void* objectPointer, TraceCallback callback)
    {
        Impl::mark(objectPointer, callback);
    }

    using Impl::registerDelayedMarkNoTracing;
    using Impl::registerWeakTable;

#if ENABLE(ASSERT)
    using Impl::weakTableRegistered;
#endif

    using Helper::registerWeakMembers;
    inline void registerWeakMembers(const void* closure, const void* objectPointer, WeakPointerCallback callback)
    {
        Impl::registerWeakMembers(closure, objectPointer, callback);
    }

    using Impl::ensureMarked;

    inline bool canTraceEagerly() { return Visitor::canTraceEagerly(); }

    using Impl::isMarked;

    Visitor* getUninlined() { return m_visitor; }

protected:
    // Methods to be called from MarkingVisitorImpl.

    inline bool shouldMarkObject(const void*)
    {
        // As this is global marking visitor, we need to mark all objects.
        return true;
    }

#if ENABLE(GC_PROFILING)
    inline void recordObjectGraphEdge(const void* objectPointer)
    {
        m_visitor->recordObjectGraphEdge(objectPointer);
    }
#endif

private:
    static InlinedGlobalMarkingVisitor fromHelper(Helper* helper)
    {
        return *static_cast<InlinedGlobalMarkingVisitor*>(helper);
    }

#if ENABLE(ASSERT)
    inline void checkMarkingAllowed() { m_visitor->checkMarkingAllowed(); }
#endif

    Visitor* m_visitor;
};

// If T does not support trace(InlinedGlobalMarkingVisitor).
template <typename T>
struct TraceCompatibilityAdaptor<T, false> {
    inline static void trace(blink::Visitor* visitor, T* self)
    {
        self->trace(visitor);
    }

    inline static void trace(InlinedGlobalMarkingVisitor visitor, T* self)
    {
        // We revert to dynamic trace(Visitor*) for tracing T.
        self->trace(visitor.getUninlined());
    }
};

// If T supports trace(InlinedGlobalMarkingVisitor).
template <typename T>
struct TraceCompatibilityAdaptor<T, true> {
    inline static void trace(blink::Visitor* visitor, T* self)
    {
        if (visitor->isGlobalMarkingVisitor()) {
            // Switch to inlined global marking dispatch.
            self->trace(InlinedGlobalMarkingVisitor(visitor));
        } else {
            self->trace(visitor);
        }
    }

    inline static void trace(InlinedGlobalMarkingVisitor visitor, T* self)
    {
        self->trace(visitor);
    }
};

#if ENABLE(INLINED_TRACE)
inline void GarbageCollectedMixin::trace(InlinedGlobalMarkingVisitor)
{
}
#endif

} // namespace blink

#endif
