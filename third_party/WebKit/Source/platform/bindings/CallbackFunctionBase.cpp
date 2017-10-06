// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/CallbackFunctionBase.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

CallbackFunctionBase::CallbackFunctionBase(ScriptState* scriptState,
                                           v8::Local<v8::Function> callback)
    : script_state_(scriptState),
      callback_(scriptState->GetIsolate(), this, callback) {
  DCHECK(!callback_.IsEmpty());
}

CallbackFunctionBase::~CallbackFunctionBase() = default;

DEFINE_TRACE_WRAPPERS(CallbackFunctionBase) {
  visitor->TraceWrappers(callback_);
}

}  // namespace blink
