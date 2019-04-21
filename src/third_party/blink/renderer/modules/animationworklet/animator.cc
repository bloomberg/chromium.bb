// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animator.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/modules/v8/effect_proxy_or_worklet_group_effect_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_animate_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_state_callback.h"
#include "third_party/blink/renderer/modules/animationworklet/animator_definition.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "v8/include/v8.h"

namespace blink {

Animator::Animator(v8::Isolate* isolate,
                   AnimatorDefinition* definition,
                   v8::Local<v8::Value> instance,
                   const String& name,
                   WorkletAnimationOptions options,
                   const std::vector<base::Optional<TimeDelta>>& local_times)
    : definition_(definition),
      instance_(isolate, instance),
      name_(name),
      options_(options),
      group_effect_(
          MakeGarbageCollected<WorkletGroupEffectProxy>(local_times)) {
  DCHECK_GE(local_times.size(), 1u);
}

Animator::~Animator() = default;

void Animator::Trace(blink::Visitor* visitor) {
  visitor->Trace(definition_);
  visitor->Trace(instance_);
  visitor->Trace(group_effect_);
}

bool Animator::Animate(
    v8::Isolate* isolate,
    double current_time,
    AnimationWorkletDispatcherOutput::AnimationState* output) {
  v8::Local<v8::Value> instance = instance_.NewLocal(isolate);
  if (IsUndefinedOrNull(instance))
    return false;

  EffectProxyOrWorkletGroupEffectProxy effect;
  if (group_effect_->getChildren().size() == 1) {
    effect.SetEffectProxy(group_effect_->getChildren()[0]);
  } else {
    effect.SetWorkletGroupEffectProxy(group_effect_);
  }

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  if (definition_->AnimateFunction()
          ->Invoke(instance, current_time, effect)
          .IsNothing()) {
    return false;
  }

  output->local_times = GetLocalTimes();
  return true;
}

std::vector<base::Optional<TimeDelta>> Animator::GetLocalTimes() const {
  std::vector<base::Optional<TimeDelta>> local_times;
  local_times.reserve(group_effect_->getChildren().size());
  for (const auto& effect : group_effect_->getChildren()) {
    local_times.push_back(effect->local_time());
  }
  return local_times;
}

bool Animator::IsStateful() const {
  return definition_->IsStateful();
}

v8::Local<v8::Value> Animator::State(v8::Isolate* isolate,
                                     ExceptionState& exception_state) {
  if (!IsStateful())
    return v8::Undefined(isolate);

  v8::Local<v8::Value> instance = instance_.NewLocal(isolate);
  DCHECK(!IsUndefinedOrNull(instance));

  v8::TryCatch try_catch(isolate);
  v8::Maybe<ScriptValue> state = definition_->StateFunction()->Invoke(instance);
  if (try_catch.HasCaught()) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return v8::Undefined(isolate);
  }
  return state.ToChecked().V8Value();
}

}  // namespace blink
