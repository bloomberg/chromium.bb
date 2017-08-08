// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8IDBObserver.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/modules/v8/IDBObserverCallback.h"
#include "platform/bindings/V8DOMWrapper.h"
#include "platform/bindings/V8PrivateProperty.h"

namespace blink {

void V8IDBObserver::constructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "IDBObserver");

  if (UNLIKELY(info.Length() < 1)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  v8::Local<v8::Object> wrapper = info.Holder();

  if (!info[0]->IsFunction()) {
    exception_state.ThrowTypeError(
        "The callback provided as parameter 1 is not a function.");
    return;
  }

  if (info.Length() > 1 && !IsUndefinedOrNull(info[1]) &&
      !info[1]->IsObject()) {
    exception_state.ThrowTypeError("parameter 2 ('options') is not an object.");
    return;
  }

  if (exception_state.HadException())
    return;

  ScriptState* script_state = ScriptState::ForRelevantRealm(info);
  v8::Local<v8::Function> v8_callback = v8::Local<v8::Function>::Cast(info[0]);
  IDBObserverCallback* callback =
      IDBObserverCallback::Create(script_state, v8_callback);
  IDBObserver* observer = IDBObserver::Create(callback);
  if (exception_state.HadException())
    return;
  DCHECK(observer);
  // TODO(bashi): Don't set private property (and remove this custom
  // constructor) when we can trace correctly.
  V8PrivateProperty::GetIDBObserverCallback(isolate).Set(wrapper, v8_callback);
  V8SetReturnValue(info, V8DOMWrapper::AssociateObjectWithWrapper(
                             isolate, observer, &wrapperTypeInfo, wrapper));
}

}  // namespace blink
