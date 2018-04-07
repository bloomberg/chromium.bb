// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/callback_function.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "v8_void_callback_function_typedef.h"

#include "bindings/core/v8/exception_state.h"
#include "bindings/core/v8/generated_code_helper.h"
#include "bindings/core/v8/idl_types.h"
#include "bindings/core/v8/native_value_traits_impl.h"
#include "bindings/core/v8/to_v8_for_core.h"
#include "bindings/core/v8/v8_binding_for_core.h"
#include "core/execution_context/execution_context.h"

namespace blink {

v8::Maybe<void> V8VoidCallbackFunctionTypedef::Invoke(ScriptWrappable* callback_this_value, const String& arg) {
  // This function implements "invoke" steps in
  // "3.10. Invoking callback functions".
  // https://heycam.github.io/webidl/#es-invoking-callback-functions

  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState())) {
    // Wrapper-tracing for the callback function makes the function object and
    // its creation context alive. Thus it's safe to use the creation context
    // of the callback function here.
    v8::HandleScope handle_scope(GetIsolate());
    CHECK(!CallbackFunction().IsEmpty());
    v8::Context::Scope context_scope(CallbackFunction()->CreationContext());
    V8ThrowException::ThrowError(
        GetIsolate(),
        ExceptionMessages::FailedToExecute(
            "invoke",
            "VoidCallbackFunctionTypedef",
            "The provided callback is no longer runnable."));
    return v8::Nothing<void>();
  }

  // step 4. If ! IsCallable(F) is false:
  //
  // As Blink no longer supports [TreatNonObjectAsNull], there must be no such a
  // case.
#if DCHECK_IS_ON()
  {
    v8::HandleScope handle_scope(GetIsolate());
    DCHECK(CallbackFunction()->IsFunction());
  }
#endif

  // step 8. Prepare to run script with relevant settings.
  ScriptState::Scope callback_relevant_context_scope(
      CallbackRelevantScriptState());
  // step 9. Prepare to run a callback with stored settings.
  if (IncumbentScriptState()->GetContext().IsEmpty()) {
    V8ThrowException::ThrowError(
        GetIsolate(),
        ExceptionMessages::FailedToExecute(
            "invoke",
            "VoidCallbackFunctionTypedef",
            "The provided callback is no longer runnable."));
    return v8::Nothing<void>();
  }
  v8::Context::BackupIncumbentScope backup_incumbent_scope(
      IncumbentScriptState()->GetContext());

  v8::Local<v8::Value> this_arg = ToV8(callback_this_value,
                                       CallbackRelevantScriptState());

  // step 10. Let esArgs be the result of converting args to an ECMAScript
  //   arguments list. If this throws an exception, set completion to the
  //   completion value representing the thrown exception and jump to the step
  //   labeled return.
  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> v8_arg = V8String(GetIsolate(), arg);
  v8::Local<v8::Value> argv[] = { v8_arg };

  // step 11. Let callResult be Call(X, thisArg, esArgs).
  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallFunction(
          CallbackFunction(),
          ExecutionContext::From(CallbackRelevantScriptState()),
          this_arg,
          1,
          argv,
          GetIsolate()).ToLocal(&call_result)) {
    // step 12. If callResult is an abrupt completion, set completion to
    //   callResult and jump to the step labeled return.
    return v8::Nothing<void>();
  }

  // step 13. Set completion to the result of converting callResult.[[Value]] to
  //   an IDL value of the same type as the operation's return type.
  return v8::JustVoid();
}

void V8VoidCallbackFunctionTypedef::InvokeAndReportException(ScriptWrappable* callback_this_value, const String& arg) {
  v8::TryCatch try_catch(GetIsolate());
  try_catch.SetVerbose(true);

  v8::Maybe<void> maybe_result =
      Invoke(callback_this_value, arg);
  // An exception if any is killed with the v8::TryCatch above.
  ALLOW_UNUSED_LOCAL(maybe_result);
}

CORE_TEMPLATE_EXPORT
v8::Maybe<void> V8PersistentCallbackFunction<V8VoidCallbackFunctionTypedef>::Invoke(ScriptWrappable* callback_this_value, const String& arg) {
  return Proxy()->Invoke(
      callback_this_value, arg);
}

CORE_TEMPLATE_EXPORT
void V8PersistentCallbackFunction<V8VoidCallbackFunctionTypedef>::InvokeAndReportException(ScriptWrappable* callback_this_value, const String& arg) {
  Proxy()->InvokeAndReportException(
      callback_this_value, arg);
}

}  // namespace blink
