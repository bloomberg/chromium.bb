// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/marking_visitor.h"

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

namespace {

ALWAYS_INLINE bool IsHashTableDeleteValue(const void* value) {
  return value == reinterpret_cast<void*>(-1);
}

}  // namespace

MarkingVisitor::MarkingVisitor(ThreadState* state, MarkingMode marking_mode)
    : Visitor(state),
      marking_worklist_(Heap().GetMarkingWorklist(),
                        WorklistTaskId::MainThread),
      not_fully_constructed_worklist_(Heap().GetNotFullyConstructedWorklist(),
                                      WorklistTaskId::MainThread),
      weak_callback_worklist_(Heap().GetWeakCallbackWorklist(),
                              WorklistTaskId::MainThread),
      marking_mode_(marking_mode) {
  DCHECK(state->InAtomicMarkingPause());
#if DCHECK_IS_ON()
  DCHECK(state->CheckThread());
#endif  // DCHECK_IS_ON
}

MarkingVisitor::~MarkingVisitor() = default;

void MarkingVisitor::DynamicallyMarkAddress(Address address) {
  HeapObjectHeader* const header = HeapObjectHeader::FromInnerAddress(address);
  DCHECK(header);
  DCHECK(!header->IsInConstruction());
  const GCInfo* gc_info =
      GCInfoTable::Get().GCInfoFromIndex(header->GcInfoIndex());
  MarkHeader(header, gc_info->trace);
}

void MarkingVisitor::ConservativelyMarkAddress(BasePage* page,
                                               Address address) {
#if DCHECK_IS_ON()
  DCHECK(page->Contains(address));
#endif
  HeapObjectHeader* const header =
      page->IsLargeObjectPage()
          ? static_cast<LargeObjectPage*>(page)->ObjectHeader()
          : static_cast<NormalPage*>(page)->FindHeaderFromAddress(address);
  if (!header || header->IsMarked())
    return;

  // Simple case for fully constructed objects.
  const GCInfo* gc_info =
      GCInfoTable::Get().GCInfoFromIndex(header->GcInfoIndex());
  if (!header->IsInConstruction()) {
    MarkHeader(header, gc_info->trace);
    return;
  }

  // This case is reached for not-fully-constructed objects with vtables.
  // We can differentiate multiple cases:
  // 1. No vtable set up. Example:
  //      class A : public GarbageCollected<A> { virtual void f() = 0; };
  //      class B : public A { B() : A(foo()) {}; };
  //    The vtable for A is not set up if foo() allocates and triggers a GC.
  //
  // 2. Vtables properly set up (non-mixin case).
  // 3. Vtables not properly set up (mixin) if GC is allowed during mixin
  //    construction.
  //
  // We use a simple conservative approach for these cases as they are not
  // performance critical.
  MarkHeaderNoTracing(header);
  Address* payload = reinterpret_cast<Address*>(header->Payload());
  const size_t payload_size = header->PayloadSize();
  for (size_t i = 0; i < (payload_size / sizeof(Address)); ++i) {
    if (payload[i])
      Heap().CheckAndMarkPointer(this, payload[i]);
  }
}

void MarkingVisitor::RegisterWeakCallback(void* object, WeakCallback callback) {
  // We don't want to run weak processings when taking a snapshot.
  if (marking_mode_ == kSnapshotMarking)
    return;
  weak_callback_worklist_.Push({object, callback});
}

void MarkingVisitor::RegisterBackingStoreReference(void** slot) {
  if (marking_mode_ != kGlobalMarkingWithCompaction)
    return;
  Heap().RegisterMovingObjectReference(
      reinterpret_cast<MovableReference*>(slot));
}

void MarkingVisitor::RegisterBackingStoreCallback(void** slot,
                                                  MovingObjectCallback callback,
                                                  void* callback_data) {
  if (marking_mode_ != kGlobalMarkingWithCompaction)
    return;
  Heap().RegisterMovingObjectCallback(reinterpret_cast<MovableReference*>(slot),
                                      callback, callback_data);
}

bool MarkingVisitor::RegisterWeakTable(const void* closure,
                                       EphemeronCallback iteration_callback) {
  Heap().RegisterWeakTable(const_cast<void*>(closure), iteration_callback);
  return true;
}

void MarkingVisitor::WriteBarrierSlow(void* value) {
  if (!value || IsHashTableDeleteValue(value))
    return;

  ThreadState* const thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  thread_state->Heap().WriteBarrier(value);
}

void MarkingVisitor::TraceMarkedBackingStoreSlow(void* value) {
  if (!value)
    return;

  ThreadState* const thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  // |value| is pointing to the start of a backing store.
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(value);
  CHECK(header->IsMarked());
  DCHECK(thread_state->CurrentVisitor());
  // No weak handling for write barriers. Modifying weakly reachable objects
  // strongifies them for the current cycle.
  GCInfoTable::Get()
      .GCInfoFromIndex(header->GcInfoIndex())
      ->trace(thread_state->CurrentVisitor(), value);
}

}  // namespace blink
