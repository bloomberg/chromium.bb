// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/callback_interface.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "V8TestCallbackInterface.h"

#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8TestInterfaceEmpty.h"
#include "core/dom/ExecutionContext.h"

namespace blink {

void V8TestCallbackInterface::voidMethod() {
  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState())) {
    return;
  }

  ScriptState::Scope scope(CallbackRelevantScriptState());

  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> *argv = 0;

  v8::Isolate* isolate = GetIsolate();

  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);

  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallFunction(
          CallbackObject().As<v8::Function>(),
          ExecutionContext::From(CallbackRelevantScriptState()),
          v8::Undefined(isolate),
          0,
          argv,
          isolate).ToLocal(&call_result)) {
    return;
  }

  // TODO(yukishiino): This function throws the return value away, and it's
  // wrong. This function should return the return value converting it from
  // v8::Value to an native type.
  ALLOW_UNUSED_LOCAL(call_result);
  return;
}

bool V8TestCallbackInterface::booleanMethod() {
  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState())) {
    return true;
  }

  ScriptState::Scope scope(CallbackRelevantScriptState());

  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> *argv = 0;

  v8::Isolate* isolate = GetIsolate();

  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);

  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallFunction(
          CallbackObject().As<v8::Function>(),
          ExecutionContext::From(CallbackRelevantScriptState()),
          v8::Undefined(isolate),
          0,
          argv,
          isolate).ToLocal(&call_result)) {
    return false;
  }

  // TODO(yukishiino): This function throws the return value away, and it's
  // wrong. This function should return the return value converting it from
  // v8::Value to an native type.
  ALLOW_UNUSED_LOCAL(call_result);
  return true;
}

void V8TestCallbackInterface::voidMethodBooleanArg(bool boolArg) {
  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState())) {
    return;
  }

  ScriptState::Scope scope(CallbackRelevantScriptState());

  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> boolArgHandle = v8::Boolean::New(GetIsolate(), boolArg);
  v8::Local<v8::Value> argv[] = { boolArgHandle };

  v8::Isolate* isolate = GetIsolate();

  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);

  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallFunction(
          CallbackObject().As<v8::Function>(),
          ExecutionContext::From(CallbackRelevantScriptState()),
          v8::Undefined(isolate),
          1,
          argv,
          isolate).ToLocal(&call_result)) {
    return;
  }

  // TODO(yukishiino): This function throws the return value away, and it's
  // wrong. This function should return the return value converting it from
  // v8::Value to an native type.
  ALLOW_UNUSED_LOCAL(call_result);
  return;
}

void V8TestCallbackInterface::voidMethodSequenceArg(const HeapVector<Member<TestInterfaceEmpty>>& sequenceArg) {
  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState())) {
    return;
  }

  ScriptState::Scope scope(CallbackRelevantScriptState());

  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> sequenceArgHandle = ToV8(sequenceArg, argument_creation_context, GetIsolate());
  v8::Local<v8::Value> argv[] = { sequenceArgHandle };

  v8::Isolate* isolate = GetIsolate();

  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);

  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallFunction(
          CallbackObject().As<v8::Function>(),
          ExecutionContext::From(CallbackRelevantScriptState()),
          v8::Undefined(isolate),
          1,
          argv,
          isolate).ToLocal(&call_result)) {
    return;
  }

  // TODO(yukishiino): This function throws the return value away, and it's
  // wrong. This function should return the return value converting it from
  // v8::Value to an native type.
  ALLOW_UNUSED_LOCAL(call_result);
  return;
}

void V8TestCallbackInterface::voidMethodFloatArg(float floatArg) {
  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState())) {
    return;
  }

  ScriptState::Scope scope(CallbackRelevantScriptState());

  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> floatArgHandle = v8::Number::New(GetIsolate(), floatArg);
  v8::Local<v8::Value> argv[] = { floatArgHandle };

  v8::Isolate* isolate = GetIsolate();

  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);

  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallFunction(
          CallbackObject().As<v8::Function>(),
          ExecutionContext::From(CallbackRelevantScriptState()),
          v8::Undefined(isolate),
          1,
          argv,
          isolate).ToLocal(&call_result)) {
    return;
  }

  // TODO(yukishiino): This function throws the return value away, and it's
  // wrong. This function should return the return value converting it from
  // v8::Value to an native type.
  ALLOW_UNUSED_LOCAL(call_result);
  return;
}

void V8TestCallbackInterface::voidMethodTestInterfaceEmptyArg(TestInterfaceEmpty* testInterfaceEmptyArg) {
  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState())) {
    return;
  }

  ScriptState::Scope scope(CallbackRelevantScriptState());

  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> testInterfaceEmptyArgHandle = ToV8(testInterfaceEmptyArg, argument_creation_context, GetIsolate());
  v8::Local<v8::Value> argv[] = { testInterfaceEmptyArgHandle };

  v8::Isolate* isolate = GetIsolate();

  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);

  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallFunction(
          CallbackObject().As<v8::Function>(),
          ExecutionContext::From(CallbackRelevantScriptState()),
          v8::Undefined(isolate),
          1,
          argv,
          isolate).ToLocal(&call_result)) {
    return;
  }

  // TODO(yukishiino): This function throws the return value away, and it's
  // wrong. This function should return the return value converting it from
  // v8::Value to an native type.
  ALLOW_UNUSED_LOCAL(call_result);
  return;
}

void V8TestCallbackInterface::voidMethodTestInterfaceEmptyStringArg(TestInterfaceEmpty* testInterfaceEmptyArg, const String& stringArg) {
  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState())) {
    return;
  }

  ScriptState::Scope scope(CallbackRelevantScriptState());

  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> testInterfaceEmptyArgHandle = ToV8(testInterfaceEmptyArg, argument_creation_context, GetIsolate());
  v8::Local<v8::Value> stringArgHandle = V8String(GetIsolate(), stringArg);
  v8::Local<v8::Value> argv[] = { testInterfaceEmptyArgHandle, stringArgHandle };

  v8::Isolate* isolate = GetIsolate();

  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);

  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallFunction(
          CallbackObject().As<v8::Function>(),
          ExecutionContext::From(CallbackRelevantScriptState()),
          v8::Undefined(isolate),
          2,
          argv,
          isolate).ToLocal(&call_result)) {
    return;
  }

  // TODO(yukishiino): This function throws the return value away, and it's
  // wrong. This function should return the return value converting it from
  // v8::Value to an native type.
  ALLOW_UNUSED_LOCAL(call_result);
  return;
}

void V8TestCallbackInterface::callbackWithThisValueVoidMethodStringArg(ScriptValue thisValue, const String& stringArg) {
  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState())) {
    return;
  }

  ScriptState::Scope scope(CallbackRelevantScriptState());

  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> stringArgHandle = V8String(GetIsolate(), stringArg);
  v8::Local<v8::Value> argv[] = { stringArgHandle };

  v8::Isolate* isolate = GetIsolate();

  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);

  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallFunction(
          CallbackObject().As<v8::Function>(),
          ExecutionContext::From(CallbackRelevantScriptState()),
          v8::Undefined(isolate),
          1,
          argv,
          isolate).ToLocal(&call_result)) {
    return;
  }

  // TODO(yukishiino): This function throws the return value away, and it's
  // wrong. This function should return the return value converting it from
  // v8::Value to an native type.
  ALLOW_UNUSED_LOCAL(call_result);
  return;
}

}  // namespace blink
