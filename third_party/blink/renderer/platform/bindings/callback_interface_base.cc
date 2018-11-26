// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/callback_interface_base.h"

namespace blink {

CallbackInterfaceBase::CallbackInterfaceBase(
    v8::Local<v8::Object> callback_object,
    v8::Local<v8::Context> callback_object_creation_context,
    SingleOperationOrNot single_op_or_not) {
  DCHECK(!callback_object.IsEmpty());
  DCHECK(!callback_object_creation_context.IsEmpty());
  DCHECK(callback_object->CreationContext() ==
         callback_object_creation_context);

  callback_relevant_script_state_ =
      ScriptState::From(callback_object_creation_context);
  v8::Isolate* isolate = callback_relevant_script_state_->GetIsolate();

  callback_object_.Set(isolate, callback_object);
  is_callback_object_callable_ =
      (single_op_or_not == kSingleOperation) && callback_object->IsCallable();
  incumbent_script_state_ = ScriptState::From(isolate->GetIncumbentContext());
}

void CallbackInterfaceBase::Trace(Visitor* visitor) {
  visitor->Trace(callback_object_);
  visitor->Trace(callback_relevant_script_state_);
  visitor->Trace(incumbent_script_state_);
}

ScriptState* CallbackInterfaceBase::CallbackRelevantScriptStateOrReportError(
    const char* interface,
    const char* operation) {
  // TODO(yukishiino): Implement this function.
  return callback_relevant_script_state_;
}

ScriptState* CallbackInterfaceBase::CallbackRelevantScriptStateOrThrowException(
    const char* interface,
    const char* operation) {
  // TODO(yukishiino): Implement this function.
  return callback_relevant_script_state_;
}

V8PersistentCallbackInterfaceBase::V8PersistentCallbackInterfaceBase(
    CallbackInterfaceBase* callback_interface)
    : callback_interface_(callback_interface) {
  v8_object_.Reset(callback_interface_->GetIsolate(),
                   callback_interface_->callback_object_.Get());
}

void V8PersistentCallbackInterfaceBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_interface_);
}

}  // namespace blink
