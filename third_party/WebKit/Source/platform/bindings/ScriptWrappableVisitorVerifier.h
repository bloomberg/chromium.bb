// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitorVerifier_h
#define ScriptWrappableVisitorVerifier_h

#include "platform/bindings/ScriptWrappableVisitor.h"

namespace blink {

class ScriptWrappableVisitorVerifier final : public ScriptWrappableVisitor {
 public:
  // The verifier deque should contain all objects encountered during marking.
  // For each object in the deque the verifier checks that all children of
  // the object are marked.
  ScriptWrappableVisitorVerifier(
      v8::Isolate* isolate,
      const WTF::Deque<WrapperMarkingData>* verifier_deque)
      : ScriptWrappableVisitor(isolate), verifier_deque_(verifier_deque) {}

  void MarkWrappersInAllWorlds(const ScriptWrappable*) const final {}

  void Verify() {
    for (auto& marking_data : *verifier_deque_) {
      // Check that all children of this object are marked.
      marking_data.TraceWrappers(this);
    }
  }

 protected:
  void Visit(const TraceWrapperV8Reference<v8::Value>&) const final {}
  void Visit(const WrapperDescriptor& wrapper_descriptor) const override {
    HeapObjectHeader* header = wrapper_descriptor.heap_object_header_callback(
        wrapper_descriptor.traceable);
    if (!header->IsWrapperHeaderMarked()) {
      // If this branch is hit, it means that a white (not discovered by
      // traceWrappers) object was assigned as a member to a black object
      // (already processed by traceWrappers). Black object will not be
      // processed anymore so White object will remain undetected and
      // therefore its wrapper and all wrappers reachable from it would be
      // collected.

      // This means there is a write barrier missing somewhere. Check the
      // backtrace to see which types are causing this and review all the
      // places where white object is set to a black object.
      wrapper_descriptor.missed_write_barrier_callback();
      NOTREACHED();
    }
  }

 private:
  const WTF::Deque<WrapperMarkingData>* verifier_deque_;
};
}
#endif
