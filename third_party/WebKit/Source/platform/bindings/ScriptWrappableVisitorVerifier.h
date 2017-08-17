// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitorVerifier_h
#define ScriptWrappableVisitorVerifier_h

#include "platform/bindings/ScriptWrappableVisitor.h"

namespace blink {

class ScriptWrappableVisitorVerifier final : public ScriptWrappableVisitor {
 public:
  explicit ScriptWrappableVisitorVerifier(v8::Isolate* isolate)
      : ScriptWrappableVisitor(isolate) {}

  void TraceWrappers(const TraceWrapperV8Reference<v8::Value>&) const final {}
  void MarkWrapper(const v8::PersistentBase<v8::Value>*) const final {}
  bool MarkWrapperHeader(HeapObjectHeader* header) const final {
    if (!visited_headers_.Contains(header)) {
      visited_headers_.insert(header);
      return true;
    }
    return false;
  }
  void MarkWrappersInAllWorlds(const ScriptWrappable*) const final {}

  void PushToMarkingDeque(
      TraceWrappersCallback trace_wrappers_callback,
      HeapObjectHeaderCallback heap_object_header_callback,
      MissedWriteBarrierCallback missed_write_barrier_callback,
      const void* object) const final {
    if (!heap_object_header_callback(object)->IsWrapperHeaderMarked()) {
      // If this branch is hit, it means that a white (not discovered by
      // traceWrappers) object was assigned as a member to a black object
      // (already processed by traceWrappers). Black object will not be
      // processed anymore so White object will remain undetected and
      // therefore its wrapper and all wrappers reachable from it would be
      // collected.

      // This means there is a write barrier missing somewhere. Check the
      // backtrace to see which types are causing this and review all the
      // places where white object is set to a black object.
      missed_write_barrier_callback();
      NOTREACHED();
    }
    trace_wrappers_callback(this, object);
  }

 private:
  mutable WTF::HashSet<HeapObjectHeader*> visited_headers_;
};
}
#endif
