// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/CallbackFunctionBase.h"

namespace blink {

CallbackFunctionBase::CallbackFunctionBase(
    v8::Local<v8::Function> callback_function) {
  DCHECK(!callback_function.IsEmpty());

  callback_relevant_script_state_ =
      ScriptState::From(callback_function->CreationContext());
  v8::Isolate* isolate = callback_relevant_script_state_->GetIsolate();

  callback_function_.Set(isolate, callback_function);
  incumbent_script_state_ = ScriptState::From(isolate->GetIncumbentContext());
}

void CallbackFunctionBase::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(callback_function_);
}

}  // namespace blink
