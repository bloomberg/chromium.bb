// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitorVerifier_h
#define ScriptWrappableVisitorVerifier_h

#include "platform/bindings/ScriptWrappableVisitor.h"

namespace blink {

class ScriptWrappableVisitorVerifier : public ScriptWrappableVisitor {
 public:
  explicit ScriptWrappableVisitorVerifier(v8::Isolate* isolate)
      : ScriptWrappableVisitor(isolate) {}

  void DispatchTraceWrappers(const TraceWrapperBase* t) const override {
    t->TraceWrappers(this);
  }

  void TraceWrappers(const TraceWrapperV8Reference<v8::Value>&) const override {
  }
  void MarkWrapper(const v8::PersistentBase<v8::Value>*) const override {}

  void PushToMarkingDeque(
      void (*trace_wrappers_callback)(const ScriptWrappableVisitor*,
                                      const void*),
      HeapObjectHeader* (*heap_object_header_callback)(const void*),
      void (*missed_write_barrier_callback)(void),
      const void* object) const override {
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

  bool MarkWrapperHeader(HeapObjectHeader* header) const override {
    if (!visited_headers_.Contains(header)) {
      visited_headers_.insert(header);
      return true;
    }
    return false;
  }
  void MarkWrappersInAllWorlds(const ScriptWrappable*) const override {}

 private:
  mutable WTF::HashSet<HeapObjectHeader*> visited_headers_;
};
}
#endif
