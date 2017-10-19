// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimatorDefinition.h"

#include "core/dom/ExecutionContext.h"
#include "modules/compositorworker/Animator.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8Binding.h"
#include "platform/bindings/V8ObjectConstructor.h"

namespace blink {

AnimatorDefinition::AnimatorDefinition(v8::Isolate* isolate,
                                       v8::Local<v8::Function> constructor,
                                       v8::Local<v8::Function> animate)
    : constructor_(isolate, this, constructor),
      animate_(isolate, this, animate) {}

AnimatorDefinition::~AnimatorDefinition() {}

void AnimatorDefinition::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(constructor_.Cast<v8::Value>());
  visitor->TraceWrappers(animate_.Cast<v8::Value>());
}

v8::Local<v8::Function> AnimatorDefinition::ConstructorLocal(
    v8::Isolate* isolate) {
  return constructor_.NewLocal(isolate);
}

v8::Local<v8::Function> AnimatorDefinition::AnimateLocal(v8::Isolate* isolate) {
  return animate_.NewLocal(isolate);
}

}  // namespace blink
