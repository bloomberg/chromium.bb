// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/callback_function.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "v8_void_callback_function_modules.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"

namespace blink {

bool V8VoidCallbackFunctionModules::call(ScriptWrappable* callback_this_value) {
  // This function implements "invoke" steps in
  // "3.10. Invoking callback functions".
  // https://heycam.github.io/webidl/#es-invoking-callback-functions

  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState())) {
    return false;
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
    return false;
  }
  v8::Context::BackupIncumbentScope backup_incumbent_scope(
      IncumbentScriptState()->GetContext());

  v8::TryCatch try_catch(GetIsolate());
  try_catch.SetVerbose(true);

  v8::Local<v8::Value> this_arg = ToV8(callback_this_value,
                                       CallbackRelevantScriptState());

  // step 10. Let esArgs be the result of converting args to an ECMAScript
  //   arguments list. If this throws an exception, set completion to the
  //   completion value representing the thrown exception and jump to the step
  //   labeled return.
  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> *argv = nullptr;

  // step 11. Let callResult be Call(X, thisArg, esArgs).
  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallFunction(
          CallbackFunction(),
          ExecutionContext::From(CallbackRelevantScriptState()),
          this_arg,
          0,
          argv,
          GetIsolate()).ToLocal(&call_result)) {
    // step 12. If callResult is an abrupt completion, set completion to
    //   callResult and jump to the step labeled return.
    return false;
  }

  // step 13. Set completion to the result of converting callResult.[[Value]] to
  //   an IDL value of the same type as the operation's return type.
  return true;
}

}  // namespace blink
