// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MarkingVerifier_h
#define MarkingVerifier_h

#include "platform/heap/GarbageCollected.h"
#include "platform/heap/HeapPage.h"
#include "platform/heap/Visitor.h"

namespace blink {

// Marking verifier that checks that a child is marked if its parent is marked.
class MarkingVerifier final : public Visitor {
 public:
  explicit MarkingVerifier(ThreadState* state) : Visitor(state) {}
  virtual ~MarkingVerifier() {}

  void VerifyObject(HeapObjectHeader* header) {
    // Verify only non-free marked objects.
    if (header->IsFree() || !header->IsMarked())
      return;

    const GCInfo* info = ThreadHeap::GcInfo(header->GcInfoIndex());
    const bool can_verify =
        !info->HasVTable() || blink::VTableInitialized(header->Payload());
    if (can_verify) {
      CHECK(header->IsValid());
      info->trace_(this, header->Payload());
    }
  }

  void Visit(void* object, TraceDescriptor desc) final {
    CHECK(object);
    VerifyChild(desc.base_object_payload);
  }

  void VisitWeak(void* object,
                 void** object_slot,
                 TraceDescriptor desc,
                 WeakCallback callback) final {
    CHECK(object);
    VerifyChild(desc.base_object_payload);
  }

  // Unused overrides.
  void VisitBackingStoreStrongly(void* object,
                                 void** object_slot,
                                 TraceDescriptor desc) final {}
  void VisitBackingStoreWeakly(void* object,
                               void** object_slot,
                               TraceDescriptor desc) final {}
  void RegisterBackingStoreCallback(void* backing_store,
                                    MovingObjectCallback,
                                    void* callback_data) final {}
  void RegisterWeakCallback(void* closure, WeakCallback) final {}

 private:
  void VerifyChild(void* base_object_payload) {
    CHECK(base_object_payload);
    HeapObjectHeader* child_header =
        HeapObjectHeader::FromPayload(base_object_payload);
    // This CHECKs ensure that any children reachable from marked parents are
    // also marked. If you hit these CHECKs then marking is in an inconsistent
    // state meaning that there are unmarked objects reachable from marked
    // ones.
    CHECK(child_header);
    CHECK(child_header->IsMarked());
  }
};

}  // namespace blink

#endif  // MarkingVerifier_h
