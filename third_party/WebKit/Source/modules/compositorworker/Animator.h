// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Animator_h
#define Animator_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"

namespace blink {

class AnimatorDefinition;
class ScriptState;

// Represents an animator instance. It owns the underlying |v8::Object| for the
// instance and knows how to invoke the |animate| function on it.
// See also |AnimationWorkletGlobalScope::CreateInstance|.
class Animator final : public GarbageCollectedFinalized<Animator>,
                       public TraceWrapperBase {
 public:
  Animator(v8::Isolate*, AnimatorDefinition*, v8::Local<v8::Object> instance);
  ~Animator();
  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

  void Animate(ScriptState*) const;

 private:
  // This object keeps the definition object, and animator instance alive.
  // It participates in wrapper tracing as it holds onto V8 wrappers.
  TraceWrapperMember<AnimatorDefinition> definition_;
  TraceWrapperV8Reference<v8::Object> instance_;
};

}  // namespace blink

#endif  // Animator_h
