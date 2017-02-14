// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VisitorImpl_h
#define VisitorImpl_h

#include "platform/heap/Heap.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "wtf/Allocator.h"

namespace blink {

inline void Visitor::markHeader(HeapObjectHeader* header,
                                const void* objectPointer,
                                TraceCallback callback) {
  DCHECK(header);
  DCHECK(objectPointer);

  if (header->isMarked())
    return;

  DCHECK(ThreadState::current()->isInGC());
  DCHECK(getMarkingMode() != WeakProcessing);

  // A GC should only mark the objects that belong in its heap.
  DCHECK(&pageFromObject(objectPointer)->arena()->getThreadState()->heap() ==
         &heap());

  header->mark();

  if (callback)
    heap().pushTraceCallback(const_cast<void*>(objectPointer), callback);
}

inline void Visitor::markHeader(HeapObjectHeader* header,
                                TraceCallback callback) {
  markHeader(header, header->payload(), callback);
}

inline void Visitor::mark(const void* objectPointer, TraceCallback callback) {
  if (!objectPointer)
    return;
  HeapObjectHeader* header = HeapObjectHeader::fromPayload(objectPointer);
  markHeader(header, header->payload(), callback);
}

inline void Visitor::markHeaderNoTracing(HeapObjectHeader* header) {
  markHeader(header, header->payload(), reinterpret_cast<TraceCallback>(0));
}

inline void Visitor::registerDelayedMarkNoTracing(const void* objectPointer) {
  DCHECK(getMarkingMode() != WeakProcessing);
  heap().pushPostMarkingCallback(const_cast<void*>(objectPointer),
                                 &markNoTracingCallback);
}

inline void Visitor::registerWeakTable(
    const void* closure,
    EphemeronCallback iterationCallback,
    EphemeronCallback iterationDoneCallback) {
  DCHECK(getMarkingMode() != WeakProcessing);
  heap().registerWeakTable(const_cast<void*>(closure), iterationCallback,
                           iterationDoneCallback);
}

#if DCHECK_IS_ON()
inline bool Visitor::weakTableRegistered(const void* closure) {
  return heap().weakTableRegistered(closure);
}
#endif

inline bool Visitor::ensureMarked(const void* objectPointer) {
  if (!objectPointer)
    return false;

  HeapObjectHeader* header = HeapObjectHeader::fromPayload(objectPointer);
  if (header->isMarked())
    return false;
#if DCHECK_IS_ON()
  markNoTracing(objectPointer);
#else
  // Inline what the above markNoTracing() call expands to,
  // so as to make sure that we do get all the benefits (asserts excepted.)
  header->mark();
#endif
  return true;
}

inline void Visitor::registerWeakCallback(void* closure,
                                          WeakCallback callback) {
  DCHECK(getMarkingMode() != WeakProcessing);
  // We don't want to run weak processings when taking a snapshot.
  if (getMarkingMode() == SnapshotMarking)
    return;
  heap().pushWeakCallback(closure, callback);
}

inline void Visitor::registerBackingStoreReference(void* slot) {
  if (getMarkingMode() != GlobalMarkingWithCompaction)
    return;
  heap().registerMovingObjectReference(
      reinterpret_cast<MovableReference*>(slot));
}

inline void Visitor::registerBackingStoreCallback(void* backingStore,
                                                  MovingObjectCallback callback,
                                                  void* callbackData) {
  if (getMarkingMode() != GlobalMarkingWithCompaction)
    return;
  heap().registerMovingObjectCallback(
      reinterpret_cast<MovableReference>(backingStore), callback, callbackData);
}

}  // namespace blink

#endif
