// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitorVerifier_h
#define ScriptWrappableVisitorVerifier_h

#include "bindings/core/v8/ScriptWrappableVisitor.h"

namespace blink {

class ScriptWrappableVisitorVerifier : public WrapperVisitor {
 public:
  void dispatchTraceWrappers(const TraceWrapperBase* t) const override {
    t->traceWrappers(this);
  }

  void traceWrappers(const TraceWrapperV8Reference<v8::Value>&) const override {
  }
  void markWrapper(const v8::PersistentBase<v8::Value>*) const override {}

  bool pushToMarkingDeque(
      void (*traceWrappersCallback)(const WrapperVisitor*, const void*),
      HeapObjectHeader* (*heapObjectHeaderCallback)(const void*),
      void (*missedWriteBarrierCallback)(void),
      const void* object) const override {
    if (!heapObjectHeaderCallback(object)->isWrapperHeaderMarked()) {
      // If this branch is hit, it means that a white (not discovered by
      // traceWrappers) object was assigned as a member to a black object
      // (already processed by traceWrappers). Black object will not be
      // processed anymore so White object will remain undetected and
      // therefore its wrapper and all wrappers reachable from it would be
      // collected.

      // This means there is a write barrier missing somewhere. Check the
      // backtrace to see which types are causing this and review all the
      // places where white object is set to a black object.
      missedWriteBarrierCallback();
      NOTREACHED();
    }
    traceWrappersCallback(this, object);
    return true;
  }

  bool markWrapperHeader(HeapObjectHeader* header) const override {
    if (!m_visitedHeaders.contains(header)) {
      m_visitedHeaders.insert(header);
      return true;
    }
    return false;
  }
  void markWrappersInAllWorlds(const ScriptWrappable*) const override {}

 private:
  mutable WTF::HashSet<HeapObjectHeader*> m_visitedHeaders;
};
}
#endif
