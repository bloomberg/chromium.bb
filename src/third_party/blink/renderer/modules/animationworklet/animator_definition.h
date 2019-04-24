// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATOR_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATOR_DEFINITION_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class V8AnimateCallback;
class V8AnimatorConstructor;
class V8Function;

// Represents a valid registered Javascript animator.  In particular it owns two
// |v8::Function|s that are the "constructor" and "animate" functions of the
// registered class. It does not do any validation itself and relies on
// |AnimationWorkletGlobalScope::registerAnimator| to validate the provided
// Javascript class before completing the registration.
class MODULES_EXPORT AnimatorDefinition final
    : public GarbageCollectedFinalized<AnimatorDefinition>,
      public NameClient {
 public:
  explicit AnimatorDefinition(V8AnimatorConstructor* constructor,
                              V8AnimateCallback* animate,
                              V8Function* state);
  ~AnimatorDefinition();
  virtual void Trace(blink::Visitor* visitor);
  const char* NameInHeapSnapshot() const override {
    return "AnimatorDefinition";
  }

  V8AnimatorConstructor* ConstructorFunction() const { return constructor_; }
  V8AnimateCallback* AnimateFunction() const { return animate_; }
  bool IsStateful() const { return state_; }

 private:
  // This object keeps the constructor function, animate, and state function
  // alive. It participates in wrapper tracing as it holds onto V8 wrappers.
  TraceWrapperMember<V8AnimatorConstructor> constructor_;
  TraceWrapperMember<V8AnimateCallback> animate_;
  TraceWrapperMember<V8Function> state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATOR_DEFINITION_H_
