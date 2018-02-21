// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VisitorImpl_h
#define VisitorImpl_h

#include "platform/heap/Heap.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "platform/wtf/Allocator.h"

namespace blink {

inline void MarkingVisitor::MarkHeader(HeapObjectHeader* header,
                                       TraceCallback callback) {
  DCHECK(header);
  if (header->IsMarked())
    return;

  DCHECK(ThreadState::Current()->IsInGC() ||
         ThreadState::Current()->IsIncrementalMarking());
  DCHECK(GetMarkingMode() != kWeakProcessing);

  const void* object_pointer = header->Payload();
  // A GC should only mark the objects that belong in its heap.
  DCHECK(&PageFromObject(object_pointer)->Arena()->GetThreadState()->Heap() ==
         &Heap());

  header->Mark();

  if (callback)
    Heap().PushTraceCallback(const_cast<void*>(object_pointer), callback);
}

inline void MarkingVisitor::Mark(const void* object_pointer,
                                 TraceCallback callback) {
  if (!object_pointer)
    return;
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(object_pointer);
  MarkHeader(header, callback);
}

inline void MarkingVisitor::MarkHeaderNoTracing(HeapObjectHeader* header) {
  MarkHeader(header, reinterpret_cast<TraceCallback>(0));
}

inline void MarkingVisitor::RegisterDelayedMarkNoTracing(
    const void* object_pointer) {
  DCHECK(GetMarkingMode() != kWeakProcessing);
  Heap().PushPostMarkingCallback(const_cast<void*>(object_pointer),
                                 &MarkNoTracingCallback);
}

inline void MarkingVisitor::RegisterWeakTable(
    const void* closure,
    EphemeronCallback iteration_callback,
    EphemeronCallback iteration_done_callback) {
  DCHECK(GetMarkingMode() != kWeakProcessing);
  Heap().RegisterWeakTable(const_cast<void*>(closure), iteration_callback,
                           iteration_done_callback);
}

#if DCHECK_IS_ON()
inline bool MarkingVisitor::WeakTableRegistered(const void* closure) {
  return Heap().WeakTableRegistered(closure);
}
#endif

inline bool MarkingVisitor::EnsureMarked(const void* object_pointer) {
  if (!object_pointer)
    return false;

  HeapObjectHeader* header = HeapObjectHeader::FromPayload(object_pointer);
  if (header->IsMarked())
    return false;
#if DCHECK_IS_ON()
  MarkNoTracing(object_pointer);
#else
  // Inline what the above markNoTracing() call expands to,
  // so as to make sure that we do get all the benefits (asserts excepted.)
  header->Mark();
#endif
  return true;
}

inline void MarkingVisitor::RegisterWeakCallback(void* closure,
                                                 WeakCallback callback) {
  DCHECK(GetMarkingMode() != kWeakProcessing);
  // We don't want to run weak processings when taking a snapshot.
  if (GetMarkingMode() == kSnapshotMarking)
    return;
  Heap().PushWeakCallback(closure, callback);
}

inline void MarkingVisitor::RegisterBackingStoreReference(void* slot) {
  if (GetMarkingMode() != kGlobalMarkingWithCompaction)
    return;
  Heap().RegisterMovingObjectReference(
      reinterpret_cast<MovableReference*>(slot));
}

inline void MarkingVisitor::RegisterBackingStoreCallback(
    void* backing_store,
    MovingObjectCallback callback,
    void* callback_data) {
  if (GetMarkingMode() != kGlobalMarkingWithCompaction)
    return;
  Heap().RegisterMovingObjectCallback(
      reinterpret_cast<MovableReference>(backing_store), callback,
      callback_data);
}

}  // namespace blink

#endif
