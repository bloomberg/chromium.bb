// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitorVerifier_h
#define ScriptWrappableVisitorVerifier_h

#include "platform/bindings/ScriptWrappableVisitor.h"

namespace blink {

// This visitor should be applied on wrapper members of each marked object
// after marking is complete. The Visit method checks that the given wrapper
// is also marked.
class ScriptWrappableVisitorVerifier final : public ScriptWrappableVisitor {
 protected:
  void Visit(const TraceWrapperV8Reference<v8::Value>&) const final {}
  void Visit(const WrapperDescriptor& wrapper_descriptor) const final {
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
  void Visit(DOMWrapperMap<ScriptWrappable>*,
             const ScriptWrappable* key) const final {}
};
}
#endif
