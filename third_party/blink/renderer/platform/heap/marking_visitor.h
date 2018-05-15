// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VISITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VISITOR_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class BasePage;

// Visitor used to mark Oilpan objects.
class PLATFORM_EXPORT MarkingVisitor final : public Visitor {
 public:
  enum MarkingMode {
    // This is a default visitor. This is used for MarkingType=kAtomicMarking
    // and MarkingType=kIncrementalMarking.
    kGlobalMarking,
    // This visitor just marks objects and ignores weak processing.
    // This is used for MarkingType=kTakeSnapshot.
    kSnapshotMarking,
    // Perform global marking along with preparing for additional sweep
    // compaction of heap arenas afterwards. Compared to the GlobalMarking
    // visitor, this visitor will also register references to objects
    // that might be moved during arena compaction -- the compaction
    // pass will then fix up those references when the object move goes
    // ahead.
    kGlobalMarkingWithCompaction,
  };

  static std::unique_ptr<MarkingVisitor> Create(ThreadState*, MarkingMode);

  // Write barrier that adds |value| to the set of marked objects. The barrier
  // bails out if marking is off or the object is not yet marked.
  inline static void WriteBarrier(void* value);

  // Eagerly traces an already marked backing store ensuring that all its
  // children are discovered by the marker. The barrier bails out if marking
  // is off and on individual objects reachable if they are already marked. The
  // barrier uses the callback function through GcInfo, so it will not inline
  // any templated type-specific code.
  inline static void TraceMarkedBackingStore(void* value);

  MarkingVisitor(ThreadState*, MarkingMode);
  ~MarkingVisitor() override;

  // Marking implementation.

  // Conservatively marks an object if pointed to by Address.
  void ConservativelyMarkAddress(BasePage*, Address);
#if DCHECK_IS_ON()
  void ConservativelyMarkAddress(BasePage*,
                                 Address,
                                 MarkedPointerCallbackForTesting);
#endif  // DCHECK_IS_ON()

  // Marks an object and adds a tracing callback for processing of the object.
  inline void MarkHeader(HeapObjectHeader*, TraceCallback);

  // Try to mark an object without tracing. Returns true when the object was not
  // marked upon calling.
  inline bool MarkHeaderNoTracing(HeapObjectHeader*);

  // Implementation of the visitor interface. See above for descriptions.

  void Visit(void* object, TraceDescriptor desc) final {
    DCHECK(object);
    if (desc.base_object_payload == BlinkGC::kNotFullyConstructedObject) {
      // This means that the objects are not-yet-fully-constructed. See comments
      // on GarbageCollectedMixin for how those objects are handled.
      not_fully_constructed_worklist_.Push(object);
      return;
    }
    // Default mark method of the trait just calls the two-argument mark
    // method on the visitor. The second argument is the static trace method
    // of the trait, which by default calls the instance method
    // trace(Visitor*) on the object.
    //
    // If the trait allows it, invoke the trace callback right here on the
    // not-yet-marked object.
    if (desc.can_trace_eagerly) {
      // Protect against too deep trace call chains, and the
      // unbounded system stack usage they can bring about.
      //
      // Assert against deep stacks so as to flush them out,
      // but test and appropriately handle them should they occur
      // in release builds.
      //
      // If you hit this assert, it means that you're creating an object
      // graph that causes too many recursions, which might cause a stack
      // overflow. To break the recursions, you need to add
      // WILL_NOT_BE_EAGERLY_TRACED_CLASS() to classes that hold pointers
      // that lead to many recursions.
      DCHECK(Heap().GetStackFrameDepth().IsAcceptableStackUse());
      if (LIKELY(Heap().GetStackFrameDepth().IsSafeToRecurse())) {
        if (MarkHeaderNoTracing(
                HeapObjectHeader::FromPayload(desc.base_object_payload))) {
          desc.callback(this, desc.base_object_payload);
        }
        return;
      }
    }
    MarkHeader(HeapObjectHeader::FromPayload(desc.base_object_payload),
               desc.callback);
  }

  void VisitWeak(void* object,
                 void** object_slot,
                 TraceDescriptor desc,
                 WeakCallback callback) final {
    RegisterWeakCallback(object_slot, callback);
  }

  void VisitBackingStoreStrongly(void* object,
                                 void** object_slot,
                                 TraceDescriptor desc) final {
    RegisterBackingStoreReference(object_slot);
    Visit(object, desc);
  }

  // All work is registered through RegisterWeakCallback.
  void VisitBackingStoreWeakly(void* object,
                               void** object_slot,
                               TraceDescriptor desc,
                               WeakCallback callback,
                               void* parameter) final {
    RegisterWeakCallback(parameter, callback);
  }

  // Used to only mark the backing store when it has been registered for weak
  // processing. In this case, the contents are processed separately using
  // the corresponding traits but the backing store requires marking.
  void VisitBackingStoreOnly(void* object, void** object_slot) final {
    MarkHeaderNoTracing(HeapObjectHeader::FromPayload(object));
    RegisterBackingStoreReference(object_slot);
  }

  void RegisterBackingStoreCallback(void* backing_store,
                                    MovingObjectCallback,
                                    void* callback_data) final;
  bool RegisterWeakTable(const void* closure,
                         EphemeronCallback iteration_callback) final;
  void RegisterWeakCallback(void* closure, WeakCallback) final;

 private:
  void RegisterBackingStoreReference(void* slot);

  void ConservativelyMarkHeader(HeapObjectHeader*);

  MarkingWorklist::View marking_worklist_;
  NotFullyConstructedWorklist::View not_fully_constructed_worklist_;
  WeakCallbackWorklist::View weak_callback_worklist_;
  const MarkingMode marking_mode_;
};

inline bool MarkingVisitor::MarkHeaderNoTracing(HeapObjectHeader* header) {
  DCHECK(header);
  DCHECK(State()->InAtomicMarkingPause() || State()->IsIncrementalMarking());
  // A GC should only mark the objects that belong in its heap.
  DCHECK_EQ(State(),
            PageFromObject(header->Payload())->Arena()->GetThreadState());
  // Never mark free space objects. This would e.g. hint to marking a promptly
  // freed backing store.
  DCHECK(!header->IsFree());

  return header->TryMark();
}

inline void MarkingVisitor::MarkHeader(HeapObjectHeader* header,
                                       TraceCallback callback) {
  DCHECK(header);
  DCHECK(callback);

  if (MarkHeaderNoTracing(header)) {
    marking_worklist_.Push(
        {reinterpret_cast<void*>(header->Payload()), callback});
  }
}

inline void MarkingVisitor::WriteBarrier(void* value) {
#if BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)
  if (!ThreadState::IsAnyIncrementalMarking() || !value)
    return;

  ThreadState* const thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  thread_state->Heap().WriteBarrier(value);
#endif  // BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)
}

inline void MarkingVisitor::TraceMarkedBackingStore(void* value) {
#if BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)
  if (!ThreadState::IsAnyIncrementalMarking() || !value)
    return;

  ThreadState* const thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  // |value| is pointing to the start of a backing store.
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(value);
  CHECK(header->IsMarked());
  DCHECK(thread_state->CurrentVisitor());
  // This check ensures that the visitor will not eagerly recurse into children
  // but rather push all blink::GarbageCollected objects and only eagerly trace
  // non-managed objects.
  DCHECK(!thread_state->Heap().GetStackFrameDepth().IsEnabled());
  // No weak handling for write barriers. Modifying weakly reachable objects
  // strongifies them for the current cycle.
  ThreadHeap::GcInfo(header->GcInfoIndex())
      ->trace_(thread_state->CurrentVisitor(), value);
#endif  // BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VISITOR_H_
