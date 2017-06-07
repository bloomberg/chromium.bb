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

// This class represents a valid registered Javascript animator. Note that it
// assumed the argument passed to its constructor have been validated to have
// proper type.
// It can be used to instantiate new animators and also to call the Javascript
// 'animate' callback on a given instance.
class MODULES_EXPORT AnimatorDefinition final
    : public GarbageCollectedFinalized<AnimatorDefinition>,
      public TraceWrapperBase {
 public:
  AnimatorDefinition(v8::Isolate*,
                     v8::Local<v8::Function> constructor,
                     v8::Local<v8::Function> animate);
  ~AnimatorDefinition();
  DEFINE_INLINE_TRACE() {}
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
