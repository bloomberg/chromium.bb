// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8IntersectionObserverCallback.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8IntersectionObserver.h"
#include "core/dom/ExecutionContext.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/Assertions.h"

namespace blink {

V8IntersectionObserverCallback::V8IntersectionObserverCallback(
    v8::Local<v8::Function> callback,
    v8::Local<v8::Object> owner,
    ScriptState* script_state)
    : callback_(script_state->GetIsolate(), callback),
      script_state_(script_state) {
  V8PrivateProperty::GetIntersectionObserverCallback(script_state->GetIsolate())
      .Set(owner, callback);
  callback_.SetPhantom();
}

V8IntersectionObserverCallback::~V8IntersectionObserverCallback() {}

void V8IntersectionObserverCallback::HandleEvent(
    const HeapVector<Member<IntersectionObserverEntry>>& entries,
    IntersectionObserver& observer) {
  ExecutionContext* execution_context =
      ExecutionContext::From(script_state_.Get());
  if (!execution_context || execution_context->IsContextSuspended() ||
      execution_context->IsContextDestroyed())
    return;
  if (!script_state_->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state_.Get());

  if (callback_.IsEmpty())
    return;
  v8::Local<v8::Value> observer_handle =
      ToV8(&observer, script_state_->GetContext()->Global(),
           script_state_->GetIsolate());
  if (!observer_handle->IsObject())
    return;

  v8::Local<v8::Object> this_object =
      v8::Local<v8::Object>::Cast(observer_handle);
  v8::Local<v8::Value> entries_handle =
      ToV8(entries, script_state_->GetContext()->Global(),
           script_state_->GetIsolate());
  if (entries_handle.IsEmpty()) {
    return;
  }
  v8::Local<v8::Value> argv[] = {entries_handle, observer_handle};

  v8::TryCatch exception_catcher(script_state_->GetIsolate());
  exception_catcher.SetVerbose(true);
  V8ScriptRunner::CallFunction(callback_.NewLocal(script_state_->GetIsolate()),
                               ExecutionContext::From(script_state_.Get()),
                               this_object, 2, argv,
                               script_state_->GetIsolate());
}

DEFINE_TRACE(V8IntersectionObserverCallback) {
  IntersectionObserverCallback::Trace(visitor);
}

}  // namespace blink
