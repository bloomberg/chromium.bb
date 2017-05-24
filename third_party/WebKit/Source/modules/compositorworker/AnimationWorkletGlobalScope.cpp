// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorkletGlobalScope.h"

#include "platform/weborigin/SecurityOrigin.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/ExceptionCode.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8ObjectConstructor.h"

#include <utility>

namespace blink {

AnimationWorkletGlobalScope* AnimationWorkletGlobalScope::Create(
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate,
    WorkerThread* thread) {
  return new AnimationWorkletGlobalScope(
      url, user_agent, std::move(security_origin), isolate, thread);
}

AnimationWorkletGlobalScope::AnimationWorkletGlobalScope(
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate,
    WorkerThread* thread)
    : ThreadedWorkletGlobalScope(url,
                                 user_agent,
                                 std::move(security_origin),
                                 isolate,
                                 thread) {}

AnimationWorkletGlobalScope::~AnimationWorkletGlobalScope() {}

DEFINE_TRACE(AnimationWorkletGlobalScope) {
  visitor->Trace(m_animatorDefinitions);
  visitor->Trace(m_animators);
  ThreadedWorkletGlobalScope::Trace(visitor);
}

void AnimationWorkletGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  // Clear animators and definitions to avoid reference cycle.
  m_animatorDefinitions.clear();
  m_animators.clear();
  ThreadedWorkletGlobalScope::Dispose();
}

void AnimationWorkletGlobalScope::registerAnimator(
    const String& name,
    const ScriptValue& ctorValue,
    ExceptionState& exceptionState) {
  DCHECK(IsContextThread());
  if (m_animatorDefinitions.Contains(name)) {
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
  if (!constructor->Get(context, V8String(isolate, "prototype"))
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
  if (!prototype->Get(context, V8String(isolate, "animate"))
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
  m_animatorDefinitions.Set(name, definition);

  // Immediately instantiate an animator for the registered definition.
  // TODO(majidvp): Remove this once you add alternative way to instantiate
  m_animators.push_back(CreateInstance(name));
}

Animator* AnimationWorkletGlobalScope::CreateInstance(const String& name) {
  DCHECK(IsContextThread());
  AnimatorDefinition* definition = m_animatorDefinitions.at(name);
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

}  // namespace blink
