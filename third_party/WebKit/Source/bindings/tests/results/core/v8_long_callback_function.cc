// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/callback_function.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "v8_long_callback_function.h"

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
V8LongCallbackFunction* V8LongCallbackFunction::Create(ScriptState* scriptState, v8::Local<v8::Value> callback) {
  if (IsUndefinedOrNull(callback))
    return nullptr;
  return new V8LongCallbackFunction(scriptState, v8::Local<v8::Function>::Cast(callback));
}

V8LongCallbackFunction::V8LongCallbackFunction(ScriptState* scriptState, v8::Local<v8::Function> callback)
    : CallbackFunctionBase(scriptState, callback) {}

bool V8LongCallbackFunction::call(ScriptWrappable* scriptWrappable, int32_t num1, int32_t num2, int32_t& returnValue) {
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

  v8::Local<v8::Value> v8_num1 = v8::Integer::New(script_state_->GetIsolate(), num1);
  v8::Local<v8::Value> v8_num2 = v8::Integer::New(script_state_->GetIsolate(), num2);
  v8::Local<v8::Value> argv[] = { v8_num1, v8_num2 };
  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);

  v8::Local<v8::Value> v8ReturnValue;
  if (!V8ScriptRunner::CallFunction(callback_.NewLocal(isolate),
                                    context,
                                    thisValue,
                                    2,
                                    argv,
                                    isolate).ToLocal(&v8ReturnValue)) {
    return false;
  }

  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(script_state_->GetIsolate(), v8ReturnValue, exceptionState, kNormalConversion);
  if (exceptionState.HadException())
    return false;
  returnValue = cppValue;
  return true;
}

V8LongCallbackFunction* NativeValueTraits<V8LongCallbackFunction>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  V8LongCallbackFunction* nativeValue = V8LongCallbackFunction::Create(ScriptState::Current(isolate), value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "LongCallbackFunction"));
  }
  return nativeValue;
}

}  // namespace blink
