// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8PromiseRejectionEvent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

void V8PromiseRejectionEvent::promiseAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  // This attribute returns a Promise.
  // Per https://heycam.github.io/webidl/#dfn-attribute-getter, all exceptions
  // must be turned into a Promise rejection. Returning a Promise type requires
  // us to disable some of V8's type checks, so we have to manually check that
  // info.Holder() really points to an instance of the type.
  PromiseRejectionEvent* event =
      V8PromiseRejectionEvent::toImplWithTypeCheck(isolate, info.Holder());
  if (!event) {
    ExceptionState exception_state(isolate, ExceptionState::kGetterContext,
                                   "PromiseRejectionEvent", "promise");
    ExceptionToRejectPromiseScope rejectPromiseScope(info, exception_state);
    exception_state.ThrowTypeError("Illegal invocation");
    return;
  }

  ScriptPromise promise = event->promise(ScriptState::Current(isolate));
  if (promise.IsEmpty()) {
    V8SetReturnValue(info, v8::Null(isolate));
    return;
  }

  V8SetReturnValue(info, promise.V8Value());
}

}  // namespace blink
