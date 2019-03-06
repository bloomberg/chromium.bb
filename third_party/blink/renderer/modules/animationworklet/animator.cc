// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animator.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/modules/v8/effect_proxy_or_worklet_group_effect_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_animate_callback.h"
#include "third_party/blink/renderer/modules/animationworklet/animator_definition.h"

namespace blink {

Animator::Animator(v8::Isolate* isolate,
                   AnimatorDefinition* definition,
                   v8::Local<v8::Value> instance,
                   int num_effects)
    : definition_(definition),
      instance_(isolate, instance),
      group_effect_(
          MakeGarbageCollected<WorkletGroupEffectProxy>(num_effects)) {
  DCHECK_GE(num_effects, 1);
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

}  // namespace blink
