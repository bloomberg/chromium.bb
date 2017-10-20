// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletGlobalScope.h"

#include "platform/weborigin/SecurityOrigin.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/dom/ExceptionCode.h"
#include "core/workers/WorkerClients.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8ObjectConstructor.h"

#include <utility>

namespace blink {

namespace {

// Once this goes our of scope it clears any animators that has not been
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
    const KURL& url,
    const String& user_agent,
    RefPtr<SecurityOrigin> document_security_origin,
    v8::Isolate* isolate,
    WorkerThread* thread,
    WorkerClients* worker_clients) {
  return new AnimationWorkletGlobalScope(url, user_agent,
                                         std::move(document_security_origin),
                                         isolate, thread, worker_clients);
}

AnimationWorkletGlobalScope::AnimationWorkletGlobalScope(
    const KURL& url,
    const String& user_agent,
    RefPtr<SecurityOrigin> document_security_origin,
    v8::Isolate* isolate,
    WorkerThread* thread,
    WorkerClients* worker_clients)
    : ThreadedWorkletGlobalScope(url,
                                 user_agent,
                                 std::move(document_security_origin),
                                 isolate,
                                 thread,
                                 worker_clients) {
  if (AnimationWorkletProxyClient* proxy_client =
          AnimationWorkletProxyClient::From(Clients()))
    proxy_client->SetGlobalScope(this);
}

AnimationWorkletGlobalScope::~AnimationWorkletGlobalScope() {}

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

Animator* AnimationWorkletGlobalScope::GetAnimatorFor(int player_id,
                                                      const String& name) {
  Animator* animator = animators_.at(player_id);
  if (!animator) {
    // This is a new player so we should create an animator for it.
    animator = CreateInstance(name);
    if (!animator)
      return nullptr;

    animators_.Set(player_id, animator);
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
      WTF::MakeUnique<CompositorMutatorOutputState>();

  for (const CompositorMutatorInputState::AnimationState& animation_input :
       mutator_input.animations) {
    int id = animation_input.animation_player_id;
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
      animation_output.animation_player_id = id;
      result->animations.push_back(std::move(animation_output));
    }
  }

  return result;
}

void AnimationWorkletGlobalScope::registerAnimator(
    const String& name,
    const ScriptValue& ctorValue,
    ExceptionState& exceptionState) {
  DCHECK(IsContextThread());
  if (animator_definitions_.Contains(name)) {
    exceptionState.ThrowDOMException(
        kNotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  if (name.IsEmpty()) {
    exceptionState.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  v8::Isolate* isolate = ScriptController()->GetScriptState()->GetIsolate();
  v8::Local<v8::Context> context = ScriptController()->GetContext();

  DCHECK(ctorValue.V8Value()->IsFunction());
  v8::Local<v8::Function> constructor =
      v8::Local<v8::Function>::Cast(ctorValue.V8Value());

  v8::Local<v8::Value> prototypeValue;
  if (!constructor->Get(context, V8AtomicString(isolate, "prototype"))
           .ToLocal(&prototypeValue))
    return;

  if (IsUndefinedOrNull(prototypeValue)) {
    exceptionState.ThrowTypeError(
        "The 'prototype' object on the class does not exist.");
    return;
  }

  if (!prototypeValue->IsObject()) {
    exceptionState.ThrowTypeError(
        "The 'prototype' property on the class is not an object.");
    return;
  }

  v8::Local<v8::Object> prototype = v8::Local<v8::Object>::Cast(prototypeValue);

  v8::Local<v8::Value> animateValue;
  if (!prototype->Get(context, V8AtomicString(isolate, "animate"))
           .ToLocal(&animateValue))
    return;

  if (IsUndefinedOrNull(animateValue)) {
    exceptionState.ThrowTypeError(
        "The 'animate' function on the prototype does not exist.");
    return;
  }

  if (!animateValue->IsFunction()) {
    exceptionState.ThrowTypeError(
        "The 'animate' property on the prototype is not a function.");
    return;
  }

  v8::Local<v8::Function> animate = v8::Local<v8::Function>::Cast(animateValue);

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
