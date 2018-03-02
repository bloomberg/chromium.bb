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
// TODO(mlippautz):
// - Backing store pointers
// - Weak handling
class MarkingVerifier final : public Visitor {
 public:
  explicit MarkingVerifier(ThreadState* state)
      : Visitor(state), parent_(nullptr) {}
  virtual ~MarkingVerifier() {}

  void VerifyObject(HeapObjectHeader* header) {
    if (header->IsFree())
      return;

    const GCInfo* info = ThreadHeap::GcInfo(header->GcInfoIndex());
    const bool can_verify =
        !info->HasVTable() || blink::VTableInitialized(header->Payload());
    if (can_verify) {
      CHECK(header->IsValid());
      parent_ = header;
      info->trace_(this, header->Payload());
    }
  }

  void Visit(void* object, TraceDescriptor desc) {
    if (parent_->IsMarked()) {
      HeapObjectHeader* child_header =
          HeapObjectHeader::FromPayload(desc.base_object_payload);
      // This CHECKs ensure that any children reachable from marked parents are
      // also marked. If you hit these CHECKs then marking is in an inconsistent
      // state meaning that there are unmarked objects reachable from marked
      // ones.
      CHECK(child_header);
      CHECK(child_header->IsMarked());
    }
  }

  // Unused overrides.
  void RegisterBackingStoreReference(void* slot) final {}
  void RegisterBackingStoreCallback(void* backing_store,
                                    MovingObjectCallback,
                                    void* callback_data) final {}
  void RegisterDelayedMarkNoTracing(const void* pointer) final {}
  void RegisterWeakCallback(void* closure, WeakCallback) final {}

 private:
  HeapObjectHeader* parent_;
};

}  // namespace blink

#endif  // MarkingVerifier_h
