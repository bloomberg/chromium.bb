// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MarkingVisitorImpl_h
#define MarkingVisitorImpl_h

#include "platform/heap/Heap.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "wtf/Functional.h"
#include "wtf/HashFunctions.h"
#include "wtf/Locker.h"
#include "wtf/RawPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/TypeTraits.h"

namespace blink {

template <typename Derived>
class MarkingVisitorImpl {
protected:
    inline void visitHeader(HeapObjectHeader* header, const void* objectPointer, TraceCallback callback)
    {
        ASSERT(header);
        ASSERT(objectPointer);
        // Check that we are not marking objects that are outside
        // the heap by calling Heap::contains.  However we cannot
        // call Heap::contains when outside a GC and we call mark
        // when doing weakness for ephemerons.  Hence we only check
        // when called within.
        ASSERT(!ThreadState::current()->isInGC() || Heap::containedInHeapOrOrphanedPage(header));

        if (!toDerived()->shouldMarkObject(objectPointer))
            return;

        // If you hit this ASSERT, it means that there is a dangling pointer
        // from a live thread heap to a dead thread heap.  We must eliminate
        // the dangling pointer.
        // Release builds don't have the ASSERT, but it is OK because
        // release builds will crash in the following header->isMarked()
        // because all the entries of the orphaned heaps are zapped.
        ASSERT(!pageFromObject(objectPointer)->orphaned());

        if (header->isMarked())
            return;
        header->mark();

#if ENABLE(GC_PROFILING)
        toDerived()->recordObjectGraphEdge(objectPointer);
#endif

        if (callback)
            Heap::pushTraceCallback(const_cast<void*>(objectPointer), callback);
    }

    inline void mark(const void* objectPointer, TraceCallback callback)
    {
        if (!objectPointer)
            return;
        HeapObjectHeader* header = HeapObjectHeader::fromPayload(objectPointer);
        visitHeader(header, header->payload(), callback);
    }

    inline void registerDelayedMarkNoTracing(const void* objectPointer)
    {
        Heap::pushPostMarkingCallback(const_cast<void*>(objectPointer), &markNoTracingCallback);
    }

    inline void registerWeakMembers(const void* closure, const void* objectPointer, WeakPointerCallback callback)
    {
        Heap::pushWeakPointerCallback(const_cast<void*>(closure), const_cast<void*>(objectPointer), callback);
    }

    inline void registerWeakTable(const void* closure, EphemeronCallback iterationCallback, EphemeronCallback iterationDoneCallback)
    {
        Heap::registerWeakTable(const_cast<void*>(closure), iterationCallback, iterationDoneCallback);
    }

#if ENABLE(ASSERT)
    inline bool weakTableRegistered(const void* closure)
    {
        return Heap::weakTableRegistered(closure);
    }
#endif

    inline bool isMarked(const void* objectPointer)
    {
        return HeapObjectHeader::fromPayload(objectPointer)->isMarked();
    }

    inline bool ensureMarked(const void* objectPointer)
    {
        if (!objectPointer)
            return false;
        if (!toDerived()->shouldMarkObject(objectPointer))
            return false;
#if ENABLE(ASSERT)
        if (isMarked(objectPointer))
            return false;

        toDerived()->markNoTracing(objectPointer);
#else
        // Inline what the above markNoTracing() call expands to,
        // so as to make sure that we do get all the benefits.
        HeapObjectHeader* header = HeapObjectHeader::fromPayload(objectPointer);
        if (header->isMarked())
            return false;
        header->mark();
#endif
        return true;
    }

    Derived* toDerived()
    {
        return static_cast<Derived*>(this);
    }

protected:
    inline void registerWeakCellWithCallback(void** cell, WeakPointerCallback callback)
    {
        Heap::pushWeakCellPointerCallback(cell, callback);
    }

private:
    static void markNoTracingCallback(Visitor* visitor, void* object)
    {
        visitor->markNoTracing(object);
    }
};

} // namespace blink

#endif
