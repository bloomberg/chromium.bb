// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/animationworklet/AnimationWorkletGlobalScope.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8ObjectParser.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/dom/ExceptionCode.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

namespace {

// Once this goes out of scope it clears any animators that have not been
// animated.
class ScopedAnimatorsSweeper {
  STACK_ALLOCATED();

 public:
  using AnimatorMap = HeapHashMap<int, TraceWrapperMember<Animator>>;
  explicit ScopedAnimatorsSweeper(AnimatorMap& animators)
      : animators_(animators) {
    for (const auto& entry : animators_) {
      Animator* animator = entry.value;
      animator->clear_did_animate();
    }
  }
  ~ScopedAnimatorsSweeper() {
    // Clear any animator that has not been animated.
    // TODO(majidvp): Reconsider this once we add specific entry to mutator
    // input that explicitly inform us that an animator is deleted.
    Vector<int> to_be_removed;
    for (const auto& entry : animators_) {
      int id = entry.key;
      Animator* animator = entry.value;
      if (!animator->did_animate())
        to_be_removed.push_back(id);
    }
    animators_.RemoveAll(to_be_removed);
  }

 private:
  AnimatorMap& animators_;
};

}  // namespace

AnimationWorkletGlobalScope* AnimationWorkletGlobalScope::Create(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    v8::Isolate* isolate,
    WorkerThread* thread) {
  return new AnimationWorkletGlobalScope(std::move(creation_params), isolate,
                                         thread);
}

AnimationWorkletGlobalScope::AnimationWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    v8::Isolate* isolate,
    WorkerThread* thread)
    : ThreadedWorkletGlobalScope(std::move(creation_params), isolate, thread) {
  if (AnimationWorkletProxyClient* proxy_client =
          AnimationWorkletProxyClient::From(Clients()))
    proxy_client->SetGlobalScope(this);
}

AnimationWorkletGlobalScope::~AnimationWorkletGlobalScope() = default;

void AnimationWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(animator_definitions_);
  visitor->Trace(animators_);
  ThreadedWorkletGlobalScope::Trace(visitor);
}

void AnimationWorkletGlobalScope::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  for (auto animator : animators_)
    visitor->TraceWrappers(animator.value);

  for (auto definition : animator_definitions_)
    visitor->TraceWrappers(definition.value);

  ThreadedWorkletGlobalScope::TraceWrappers(visitor);
}

void AnimationWorkletGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  if (AnimationWorkletProxyClient* proxy_client =
          AnimationWorkletProxyClient::From(Clients()))
    proxy_client->Dispose();
  ThreadedWorkletGlobalScope::Dispose();
}

Animator* AnimationWorkletGlobalScope::GetAnimatorFor(int animation_id,
                                                      const String& name) {
  Animator* animator = animators_.at(animation_id);
  if (!animator) {
    // This is a new animation so we should create an animator for it.
    animator = CreateInstance(name);
    if (!animator)
      return nullptr;

    animators_.Set(animation_id, animator);
  }

  return animator;
}

std::unique_ptr<CompositorMutatorOutputState>
AnimationWorkletGlobalScope::Mutate(
    const CompositorMutatorInputState& mutator_input) {
  DCHECK(IsContextThread());

  // Clean any animator that is not updated
  ScopedAnimatorsSweeper sweeper(animators_);

  ScriptState* script_state = ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);

  std::unique_ptr<CompositorMutatorOutputState> result =
      std::make_unique<CompositorMutatorOutputState>();

  for (const CompositorMutatorInputState::AnimationState& animation_input :
       mutator_input.animations) {
    int id = animation_input.animation_id;
    const String name = String::FromUTF8(animation_input.name.data(),
                                         animation_input.name.size());

    Animator* animator = GetAnimatorFor(id, name);
    // TODO(majidvp): This means there is an animatorName for which
    // definition was not registered. We should handle this case gracefully.
    // http://crbug.com/776017
    if (!animator)
      continue;

    CompositorMutatorOutputState::AnimationState animation_output;
    if (animator->Animate(script_state, animation_input, &animation_output)) {
      animation_output.animation_id = id;
      result->animations.push_back(std::move(animation_output));
    }
  }

  return result;
}

void AnimationWorkletGlobalScope::registerAnimator(
    const String& name,
    const ScriptValue& constructor_value,
    ExceptionState& exception_state) {
  DCHECK(IsContextThread());
  if (animator_definitions_.Contains(name)) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  if (name.IsEmpty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  v8::Isolate* isolate = ScriptController()->GetScriptState()->GetIsolate();
  v8::Local<v8::Context> context = ScriptController()->GetContext();

  DCHECK(constructor_value.V8Value()->IsFunction());
  v8::Local<v8::Function> constructor =
      v8::Local<v8::Function>::Cast(constructor_value.V8Value());

  v8::Local<v8::Object> prototype;
  if (!V8ObjectParser::ParsePrototype(context, constructor, &prototype,
                                      &exception_state))
    return;

  v8::Local<v8::Function> animate;
  if (!V8ObjectParser::ParseFunction(context, prototype, "animate", &animate,
                                     &exception_state))
    return;

  AnimatorDefinition* definition =
      new AnimatorDefinition(isolate, constructor, animate);

  animator_definitions_.Set(name, definition);
}

Animator* AnimationWorkletGlobalScope::CreateInstance(const String& name) {
  DCHECK(IsContextThread());
  AnimatorDefinition* definition = animator_definitions_.at(name);
  if (!definition)
    return nullptr;

  v8::Isolate* isolate = ScriptController()->GetScriptState()->GetIsolate();
  v8::Local<v8::Function> constructor = definition->ConstructorLocal(isolate);
  DCHECK(!IsUndefinedOrNull(constructor));

  v8::Local<v8::Object> instance;
  if (!V8ObjectConstructor::NewInstance(isolate, constructor)
           .ToLocal(&instance))
    return nullptr;

  return new Animator(isolate, definition, instance);
}

AnimatorDefinition* AnimationWorkletGlobalScope::FindDefinitionForTest(
    const String& name) {
  return animator_definitions_.at(name);
}

}  // namespace blink
