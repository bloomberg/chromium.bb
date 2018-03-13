// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/MarkingVisitor.h"

#include "platform/heap/Heap.h"
#include "platform/heap/ThreadState.h"

namespace blink {

std::unique_ptr<MarkingVisitor> MarkingVisitor::Create(ThreadState* state,
                                                       MarkingMode mode) {
  return std::make_unique<MarkingVisitor>(state, mode);
}

MarkingVisitor::MarkingVisitor(ThreadState* state, MarkingMode marking_mode)
    : Visitor(state), marking_mode_(marking_mode) {
  // See ThreadState::runScheduledGC() why we need to already be in a
  // GCForbiddenScope before any safe point is entered.
  DCHECK(state->IsGCForbidden());
#if DCHECK_IS_ON()
  DCHECK(state->CheckThread());
#endif
}

MarkingVisitor::~MarkingVisitor() = default;

void MarkingVisitor::MarkNoTracingCallback(Visitor* visitor, void* object) {
  reinterpret_cast<MarkingVisitor*>(visitor)->MarkHeaderNoTracing(
      HeapObjectHeader::FromPayload(object));
}

void MarkingVisitor::RegisterWeakCallback(void* closure,
                                          WeakCallback callback) {
  // We don't want to run weak processings when taking a snapshot.
  if (marking_mode_ == kSnapshotMarking)
    return;
  Heap().PushWeakCallback(closure, callback);
}

void MarkingVisitor::RegisterBackingStoreReference(void* slot) {
  if (marking_mode_ != kGlobalMarkingWithCompaction)
    return;
  Heap().RegisterMovingObjectReference(
      reinterpret_cast<MovableReference*>(slot));
}

void MarkingVisitor::RegisterBackingStoreCallback(void* backing_store,
                                                  MovingObjectCallback callback,
                                                  void* callback_data) {
  if (marking_mode_ != kGlobalMarkingWithCompaction)
    return;
  Heap().RegisterMovingObjectCallback(
      reinterpret_cast<MovableReference>(backing_store), callback,
      callback_data);
}

bool MarkingVisitor::RegisterWeakTable(
    const void* closure,
    EphemeronCallback iteration_callback,
    EphemeronCallback iteration_done_callback) {
  Heap().RegisterWeakTable(const_cast<void*>(closure), iteration_callback,
                           iteration_done_callback);
  return true;
}

#if DCHECK_IS_ON()
bool MarkingVisitor::WeakTableRegistered(const void* closure) {
  return Heap().WeakTableRegistered(closure);
}
#endif

}  // namespace blink
