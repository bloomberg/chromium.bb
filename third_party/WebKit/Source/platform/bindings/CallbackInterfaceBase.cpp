// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/CallbackInterfaceBase.h"

namespace blink {

CallbackInterfaceBase::CallbackInterfaceBase(
    v8::Local<v8::Object> callback_object,
    SingleOperationOrNot single_op_or_not) {
  DCHECK(!callback_object.IsEmpty());

  callback_relevant_script_state_ =
      ScriptState::From(callback_object->CreationContext());
  v8::Isolate* isolate = callback_relevant_script_state_->GetIsolate();

  callback_object_.Set(isolate, callback_object);
  is_callback_object_callable_ =
      (single_op_or_not == kSingleOperation) && callback_object->IsCallable();
  incumbent_script_state_ = ScriptState::From(isolate->GetIncumbentContext());
}

}  // namespace blink
