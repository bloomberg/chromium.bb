// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MarkingVisitor_h
#define MarkingVisitor_h

#include "platform/heap/Heap.h"
#include "platform/heap/HeapPage.h"
#include "platform/heap/Visitor.h"

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

  MarkingVisitor(ThreadState*, MarkingMode);
  virtual ~MarkingVisitor();

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
    DCHECK(desc.base_object_payload);
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

  // Used to delay the marking of objects until the usual marking including
  // ephemeron iteration is done. This is used to delay the marking of
  // collection backing stores until we know if they are reachable from
  // locations other than the collection front object. If collection backings
  // are reachable from other locations we strongify them to avoid issues with
  // iterators and weak processing.
  void VisitBackingStoreWeakly(void* object,
                               void** object_slot,
                               TraceDescriptor desc) final {
    RegisterBackingStoreReference(object_slot);
    Heap().PushPostMarkingCallback(object, &MarkNoTracingCallback);
  }

  void RegisterBackingStoreCallback(void* backing_store,
                                    MovingObjectCallback,
                                    void* callback_data) final;
  bool RegisterWeakTable(const void* closure,
                         EphemeronCallback iteration_callback,
                         EphemeronCallback iteration_done_callback) final;
#if DCHECK_IS_ON()
  bool WeakTableRegistered(const void* closure) final;
#endif
  void RegisterWeakCallback(void* closure, WeakCallback) final;

 private:
  static void MarkNoTracingCallback(Visitor*, void*);

  void RegisterBackingStoreReference(void* slot);

  void ConservativelyMarkHeader(HeapObjectHeader*);

  const MarkingMode marking_mode_;
};

inline bool MarkingVisitor::MarkHeaderNoTracing(HeapObjectHeader* header) {
  DCHECK(header);
  DCHECK(State()->IsInGC() || State()->IsIncrementalMarking());
  // A GC should only mark the objects that belong in its heap.
  DCHECK_EQ(State(),
            PageFromObject(header->Payload())->Arena()->GetThreadState());

  return header->TryMark();
}

inline void MarkingVisitor::MarkHeader(HeapObjectHeader* header,
                                       TraceCallback callback) {
  DCHECK(header);
  DCHECK(callback);

  if (MarkHeaderNoTracing(header)) {
    Heap().PushTraceCallback(reinterpret_cast<void*>(header->Payload()),
                             callback);
  }
}

}  // namespace blink

#endif  // MarkingVisitor_h
