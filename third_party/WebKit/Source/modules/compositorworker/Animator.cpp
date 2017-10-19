// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/Animator.h"

#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/ExecutionContext.h"
#include "modules/compositorworker/AnimatorDefinition.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8Binding.h"

namespace blink {

Animator::Animator(v8::Isolate* isolate,
                   AnimatorDefinition* definition,
                   v8::Local<v8::Object> instance)
    : definition_(definition), instance_(isolate, this, instance) {}

Animator::~Animator() {}

void Animator::Trace(blink::Visitor* visitor) {
  visitor->Trace(definition_);
}

void Animator::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(definition_);
  visitor->TraceWrappers(instance_.Cast<v8::Value>());
}

void Animator::Animate(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();

  v8::Local<v8::Object> instance = instance_.NewLocal(isolate);
  v8::Local<v8::Function> animate = definition_->AnimateLocal(isolate);

  if (IsUndefinedOrNull(instance) || IsUndefinedOrNull(animate))
    return;

  ScriptState::Scope scope(script_state);
  v8::TryCatch block(isolate);
  block.SetVerbose(true);

  V8ScriptRunner::CallFunction(animate, ExecutionContext::From(script_state),
                               instance, 0, nullptr, isolate);

  // The animate function may have produced an error!
  // TODO(majidvp): We should probably just throw here.
  if (block.HasCaught())
    return;
}

}  // namespace blink
