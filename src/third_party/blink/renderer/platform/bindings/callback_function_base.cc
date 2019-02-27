// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

CallbackFunctionBase::CallbackFunctionBase(
    v8::Local<v8::Function> callback_function) {
  DCHECK(!callback_function.IsEmpty());

  v8::Local<v8::Context> creation_context =
      callback_function->CreationContext();
  if (!ScriptState::AccessCheck(creation_context)) {
      const String& message =
        "callback created in invalid context";
      V8ThrowException::ThrowAccessError(
        v8::Isolate::GetCurrent(), message);
      return;
  }

  callback_relevant_script_state_ =
      ScriptState::From(callback_function->CreationContext());
  v8::Isolate* isolate = callback_relevant_script_state_->GetIsolate();

  callback_function_.Set(isolate, callback_function);
  incumbent_script_state_ = ScriptState::From(isolate->GetIncumbentContext());
}

void CallbackFunctionBase::Trace(Visitor* visitor) {
  visitor->Trace(callback_function_);
  visitor->Trace(callback_relevant_script_state_);
  visitor->Trace(incumbent_script_state_);
}

V8PersistentCallbackFunctionBase::V8PersistentCallbackFunctionBase(
    CallbackFunctionBase* callback_function)
    : callback_function_(callback_function) {
  if (callback_function_->callback_function_.IsEmpty()) {
    const String& message =
      "callback created in invalid context";
    V8ThrowException::ThrowAccessError(
      v8::Isolate::GetCurrent(), message);
    return;
  }

  v8_function_.Reset(callback_function_->GetIsolate(),
                     callback_function_->callback_function_.Get());
}

void V8PersistentCallbackFunctionBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_function_);
}

}  // namespace blink
