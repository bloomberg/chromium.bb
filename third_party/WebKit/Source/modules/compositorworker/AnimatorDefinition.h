// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimatorDefinition_h
#define AnimatorDefinition_h

#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"

namespace blink {

// Represents a valid registered Javascript animator.  In particular it owns two
// |v8::Function|s that are the "constructor" and "animate" functions of the
// registered class. It does not do any validation itself and relies on
// |AnimationWorkletGlobalScope::registerAnimator| to validate the provided
// Javascript class before completing the registration.
class MODULES_EXPORT AnimatorDefinition final
    : public GarbageCollectedFinalized<AnimatorDefinition>,
      public TraceWrapperBase {
 public:
  AnimatorDefinition(v8::Isolate*,
                     v8::Local<v8::Function> constructor,
                     v8::Local<v8::Function> animate);
  ~AnimatorDefinition();
  void Trace(blink::Visitor* visitor) {}
  DECLARE_TRACE_WRAPPERS();

  v8::Local<v8::Function> ConstructorLocal(v8::Isolate*);
  v8::Local<v8::Function> AnimateLocal(v8::Isolate*);

 private:
  // This object keeps the constructor function, and animate function alive.
  // It participates in wrapper tracing as it holds onto V8 wrappers.
  TraceWrapperV8Reference<v8::Function> constructor_;
  TraceWrapperV8Reference<v8::Function> animate_;
};

}  // namespace blink

#endif  // AnimatorDefinition_h
