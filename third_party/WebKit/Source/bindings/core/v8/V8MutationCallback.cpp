/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/V8MutationCallback.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8MutationObserver.h"
#include "bindings/core/v8/V8MutationRecord.h"
#include "core/dom/ExecutionContext.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/Assertions.h"

namespace blink {

V8MutationCallback::V8MutationCallback(v8::Local<v8::Function> callback,
                                       v8::Local<v8::Object> owner,
                                       ScriptState* script_state)
    : callback_(script_state->GetIsolate(), this, callback),
      script_state_(script_state) {
  V8PrivateProperty::GetMutationObserverCallback(script_state->GetIsolate())
      .Set(owner, callback);
}

V8MutationCallback::~V8MutationCallback() {}

void V8MutationCallback::Call(
    const HeapVector<Member<MutationRecord>>& mutations,
    MutationObserver* observer) {
  if (callback_.IsEmpty())
    return;

  if (!script_state_->ContextIsValid())
    return;

  v8::Isolate* isolate = script_state_->GetIsolate();
  ExecutionContext* execution_context =
      ExecutionContext::From(script_state_.Get());
  if (!execution_context || execution_context->IsContextSuspended() ||
      execution_context->IsContextDestroyed())
    return;
  ScriptState::Scope scope(script_state_.Get());

  v8::Local<v8::Value> observer_handle =
      ToV8(observer, script_state_->GetContext()->Global(), isolate);
  if (!observer_handle->IsObject())
    return;

  v8::Local<v8::Object> this_object =
      v8::Local<v8::Object>::Cast(observer_handle);
  v8::Local<v8::Value> v8_mutations =
      ToV8(mutations, script_state_->GetContext()->Global(), isolate);
  if (v8_mutations.IsEmpty())
    return;
  v8::Local<v8::Value> argv[] = {v8_mutations, observer_handle};

  v8::TryCatch exception_catcher(isolate);
  exception_catcher.SetVerbose(true);
  V8ScriptRunner::CallFunction(callback_.NewLocal(isolate),
                               GetExecutionContext(), this_object,
                               WTF_ARRAY_LENGTH(argv), argv, isolate);
}

DEFINE_TRACE(V8MutationCallback) {
  MutationCallback::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(V8MutationCallback) {
  visitor->TraceWrappers(callback_.Cast<v8::Value>());
  MutationCallback::TraceWrappers(visitor);
}

}  // namespace blink
