// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/callback_function.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "v8_void_callback_function_enum_arg.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/Assertions.h"

namespace blink {

// static
V8VoidCallbackFunctionEnumArg* V8VoidCallbackFunctionEnumArg::Create(ScriptState* scriptState, v8::Local<v8::Value> callback) {
  if (IsUndefinedOrNull(callback))
    return nullptr;
  return new V8VoidCallbackFunctionEnumArg(scriptState, v8::Local<v8::Function>::Cast(callback));
}

V8VoidCallbackFunctionEnumArg::V8VoidCallbackFunctionEnumArg(ScriptState* scriptState, v8::Local<v8::Function> callback)
    : CallbackFunctionBase(scriptState, callback) {}

bool V8VoidCallbackFunctionEnumArg::call(ScriptWrappable* scriptWrappable, const String& arg) {
  if (callback_.IsEmpty())
    return false;

  if (!script_state_->ContextIsValid())
    return false;

  // TODO(bashi): Make sure that using DummyExceptionStateForTesting is OK.
  // crbug.com/653769
  DummyExceptionStateForTesting exceptionState;

  const char* valid_arg_values[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(arg, valid_arg_values, WTF_ARRAY_LENGTH(valid_arg_values), "TestEnum", exceptionState)) {
    NOTREACHED();
    return false;
  }

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

  v8::Local<v8::Value> v8_arg = V8String(script_state_->GetIsolate(), arg);
  v8::Local<v8::Value> argv[] = { v8_arg };
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

V8VoidCallbackFunctionEnumArg* NativeValueTraits<V8VoidCallbackFunctionEnumArg>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  V8VoidCallbackFunctionEnumArg* nativeValue = V8VoidCallbackFunctionEnumArg::Create(ScriptState::Current(isolate), value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "VoidCallbackFunctionEnumArg"));
  }
  return nativeValue;
}

}  // namespace blink
