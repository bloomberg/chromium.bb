// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeathAwareScriptWrappable_h
#define DeathAwareScriptWrappable_h

#include <signal.h>
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Heap.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DeathAwareScriptWrappable
    : public GarbageCollectedFinalized<DeathAwareScriptWrappable>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  static DeathAwareScriptWrappable* instance_;
  static bool has_died_;

 public:
  typedef TraceWrapperMember<DeathAwareScriptWrappable> Wrapper;

  virtual ~DeathAwareScriptWrappable() {
    if (this == instance_) {
      has_died_ = true;
    }
  }

  static DeathAwareScriptWrappable* Create() {
    return new DeathAwareScriptWrappable();
  }

  static bool HasDied() { return has_died_; }
  static void ObserveDeathsOf(DeathAwareScriptWrappable* instance) {
    has_died_ = false;
    instance_ = instance;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(raw_dependency_);
    visitor->Trace(wrapped_dependency_);
    visitor->Trace(wrapped_vector_dependency_);
    visitor->Trace(wrapped_hash_map_dependency_);
  }

  DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() {
    visitor->TraceWrappersWithManualWriteBarrier(raw_dependency_);
    visitor->TraceWrappers(wrapped_dependency_);
    for (auto dep : wrapped_vector_dependency_) {
      visitor->TraceWrappers(dep);
    }
    for (auto pair : wrapped_hash_map_dependency_) {
      visitor->TraceWrappers(pair.key);
      visitor->TraceWrappers(pair.value);
    }
  }

  void SetRawDependency(DeathAwareScriptWrappable* dependency) {
    raw_dependency_ = dependency;
    ScriptWrappableVisitor::WriteBarrier(dependency);
  }

  void SetWrappedDependency(DeathAwareScriptWrappable* dependency) {
    wrapped_dependency_ = dependency;
  }

  void AddWrappedVectorDependency(DeathAwareScriptWrappable* dependency) {
    wrapped_vector_dependency_.push_back(dependency);
  }

  void AddWrappedHashMapDependency(DeathAwareScriptWrappable* key,
                                   DeathAwareScriptWrappable* value) {
    wrapped_hash_map_dependency_.insert(key, value);
  }

 private:
  DeathAwareScriptWrappable() {}

  Member<DeathAwareScriptWrappable> raw_dependency_;
  Wrapper wrapped_dependency_;
  HeapVector<Wrapper> wrapped_vector_dependency_;
  HeapHashMap<Wrapper, Wrapper> wrapped_hash_map_dependency_;
};

}  // namespace blink

#endif  // DeathAwareScriptWrappable_h
