// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/callback_function.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "v8_void_callback_function_interface_arg.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8HTMLDivElement.h"
#include "core/dom/ExecutionContext.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/Assertions.h"

namespace blink {

// static
V8VoidCallbackFunctionInterfaceArg* V8VoidCallbackFunctionInterfaceArg::Create(ScriptState* scriptState, v8::Local<v8::Value> callback) {
  if (IsUndefinedOrNull(callback))
    return nullptr;
  return new V8VoidCallbackFunctionInterfaceArg(scriptState, v8::Local<v8::Function>::Cast(callback));
}

V8VoidCallbackFunctionInterfaceArg::V8VoidCallbackFunctionInterfaceArg(ScriptState* scriptState, v8::Local<v8::Function> callback)
    : CallbackFunctionBase(scriptState, callback) {}

bool V8VoidCallbackFunctionInterfaceArg::call(ScriptWrappable* scriptWrappable, HTMLDivElement* divElement) {
  if (callback_.IsEmpty())
    return false;

  if (!script_state_->ContextIsValid())
    return false;

  // TODO(bashi): Make sure that using DummyExceptionStateForTesting is OK.
  // crbug.com/653769
  DummyExceptionStateForTesting exceptionState;

  ExecutionContext* context = ExecutionContext::From(script_state_.get());
  DCHECK(context);
  if (context->IsContextSuspended() || context->IsContextDestroyed())
    return false;

  ScriptState::Scope scope(script_state_.get());
  v8::Isolate* isolate = script_state_->GetIsolate();

  v8::Local<v8::Value> thisValue = ToV8(
      scriptWrappable,
      script_state_->GetContext()->Global(),
      isolate);

  v8::Local<v8::Value> v8_divElement = ToV8(divElement, script_state_->GetContext()->Global(), script_state_->GetIsolate());
  v8::Local<v8::Value> argv[] = { v8_divElement };
  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);

  v8::Local<v8::Value> v8ReturnValue;
  if (!V8ScriptRunner::CallFunction(callback_.NewLocal(isolate),
                                    context,
                                    thisValue,
                                    1,
                                    argv,
                                    isolate).ToLocal(&v8ReturnValue)) {
    return false;
  }

  return true;
}

V8VoidCallbackFunctionInterfaceArg* NativeValueTraits<V8VoidCallbackFunctionInterfaceArg>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  V8VoidCallbackFunctionInterfaceArg* nativeValue = V8VoidCallbackFunctionInterfaceArg::Create(ScriptState::Current(isolate), value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "VoidCallbackFunctionInterfaceArg"));
  }
  return nativeValue;
}

}  // namespace blink
