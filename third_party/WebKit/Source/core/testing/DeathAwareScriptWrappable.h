// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeathAwareScriptWrappable_h
#define DeathAwareScriptWrappable_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "platform/heap/Heap.h"
#include "wtf/text/WTFString.h"
#include <signal.h>

namespace blink {

class DeathAwareScriptWrappable
    : public GarbageCollectedFinalized<DeathAwareScriptWrappable>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  static DeathAwareScriptWrappable* s_instance;
  static bool s_hasDied;

 public:
  virtual ~DeathAwareScriptWrappable() {
    if (this == s_instance) {
      s_hasDied = true;
    }
  }

  static DeathAwareScriptWrappable* create() {
    return new DeathAwareScriptWrappable();
  }

  static bool hasDied() { return s_hasDied; }
  static void observeDeathsOf(DeathAwareScriptWrappable* instance) {
    s_hasDied = false;
    s_instance = instance;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(m_dependency); }

  DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() {
    visitor->traceWrappers(m_dependency);
  }

  void setDependency(DeathAwareScriptWrappable* dependency) {
    ScriptWrappableVisitor::writeBarrier(this, dependency);
    m_dependency = dependency;
  }

 private:
  Member<DeathAwareScriptWrappable> m_dependency;
};

}  // namespace blink

#endif  // DeathAwareScriptWrappable_h
