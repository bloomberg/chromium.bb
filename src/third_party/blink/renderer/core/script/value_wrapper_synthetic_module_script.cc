// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/value_wrapper_synthetic_module_script.h"

#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet_init.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "v8/include/v8.h"

namespace blink {

// https://whatpr.org/html/4898/webappapis.html#creating-a-css-module-script
ValueWrapperSyntheticModuleScript*
ValueWrapperSyntheticModuleScript::CreateCSSWrapperSyntheticModuleScript(
    const base::Optional<ModuleScriptCreationParams>& params,
    Modulator* settings_object) {
  DCHECK(settings_object->HasValidContext());
  ScriptState* script_state = settings_object->GetScriptState();
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "ModuleScriptLoader",
                                 "CreateCSSWrapperSyntheticModuleScript");
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  auto* context_window = DynamicTo<LocalDOMWindow>(execution_context);
  if (!context_window) {
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        isolate, "Cannot create CSS Module in non-document context");
    return ValueWrapperSyntheticModuleScript::CreateWithError(
        v8::Local<v8::Value>(), settings_object, params->GetResponseUrl(),
        KURL(), ScriptFetchOptions(), error);
  }
  CSSStyleSheetInit* init = CSSStyleSheetInit::Create();
  CSSStyleSheet* style_sheet =
      CSSStyleSheet::Create(*context_window->document(), init, exception_state);
  if (exception_state.HadException()) {
    v8::Local<v8::Value> error = exception_state.GetException();
    exception_state.ClearException();
    return ValueWrapperSyntheticModuleScript::CreateWithError(
        v8::Local<v8::Value>(), settings_object, params->GetResponseUrl(),
        KURL(), ScriptFetchOptions(), error);
  }
  style_sheet->replaceSync(params->GetSourceText().ToString(), exception_state);
  if (exception_state.HadException()) {
    v8::Local<v8::Value> error = exception_state.GetException();
    exception_state.ClearException();
    return ValueWrapperSyntheticModuleScript::CreateWithError(
        v8::Local<v8::Value>(), settings_object, params->GetResponseUrl(),
        KURL(), ScriptFetchOptions(), error);
  }
  v8::Local<v8::Value> v8_value_stylesheet = ToV8(style_sheet, script_state);
  return ValueWrapperSyntheticModuleScript::CreateWithDefaultExport(
      v8_value_stylesheet, settings_object, params->GetResponseUrl(), KURL(),
      ScriptFetchOptions());
}

ValueWrapperSyntheticModuleScript*
ValueWrapperSyntheticModuleScript::CreateJSONWrapperSyntheticModuleScript(
    const base::Optional<ModuleScriptCreationParams>& params,
    Modulator* settings_object) {
  DCHECK(settings_object->HasValidContext());
  ScriptState::Scope scope(settings_object->GetScriptState());
  v8::Local<v8::Context> context =
      settings_object->GetScriptState()->GetContext();
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> original_json =
      V8String(isolate, params->GetSourceText().ToString());
  v8::Local<v8::Value> parsed_json;
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "ModuleScriptLoader",
                                 "CreateJSONWrapperSyntheticModuleScript");
  // Step 1. "Let script be a new module script that this algorithm will
  // subsequently initialize."
  // [spec text]
  // Step 2. "Set script's settings object to settings."
  // [spec text]
  // Step 3. "Set script's base URL and fetch options to null."
  // [spec text]
  // Step 4. "Set script's parse error and error to rethrow to null."
  // [spec text]
  // Step 5. "Let json be ? Call(%JSONParse%, undefined, « source »).
  // If this throws an exception, set script's parse error to that exception,
  // and return script."
  // [spec text]
  if (!v8::JSON::Parse(context, original_json).ToLocal(&parsed_json)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    v8::Local<v8::Value> error = exception_state.GetException();
    exception_state.ClearException();
    return ValueWrapperSyntheticModuleScript::CreateWithError(
        parsed_json, settings_object, params->GetResponseUrl(), KURL(),
        ScriptFetchOptions(), error);
  } else {
    return ValueWrapperSyntheticModuleScript::CreateWithDefaultExport(
        parsed_json, settings_object, params->GetResponseUrl(), KURL(),
        ScriptFetchOptions());
  }
}

ValueWrapperSyntheticModuleScript*
ValueWrapperSyntheticModuleScript::CreateWithDefaultExport(
    v8::Local<v8::Value> value,
    Modulator* settings_object,
    const KURL& source_url,
    const KURL& base_url,
    const ScriptFetchOptions& fetch_options,
    const TextPosition& start_position) {
  v8::Isolate* isolate = settings_object->GetScriptState()->GetIsolate();
  std::vector<v8::Local<v8::String>> export_names{V8String(isolate, "default")};
  v8::Local<v8::Module> v8_synthetic_module = v8::Module::CreateSyntheticModule(
      isolate, V8String(isolate, source_url.GetString()), export_names,
      ValueWrapperSyntheticModuleScript::EvaluationSteps);
  // Step 6. "Set script's record to the result of creating a synthetic module
  // record with a default export of json with settings."
  // [spec text]
  ValueWrapperSyntheticModuleScript* value_wrapper_module_script =
      MakeGarbageCollected<ValueWrapperSyntheticModuleScript>(
          settings_object, v8_synthetic_module, source_url, base_url,
          fetch_options, value, start_position);
  settings_object->GetModuleRecordResolver()->RegisterModuleScript(
      value_wrapper_module_script);
  // Step 7. "Return script."
  // [spec text]
  return value_wrapper_module_script;
}

ValueWrapperSyntheticModuleScript*
ValueWrapperSyntheticModuleScript::CreateWithError(
    v8::Local<v8::Value> value,
    Modulator* settings_object,
    const KURL& source_url,
    const KURL& base_url,
    const ScriptFetchOptions& fetch_options,
    v8::Local<v8::Value> error,
    const TextPosition& start_position) {
  ValueWrapperSyntheticModuleScript* value_wrapper_module_script =
      MakeGarbageCollected<ValueWrapperSyntheticModuleScript>(
          settings_object, v8::Local<v8::Module>(), source_url, base_url,
          fetch_options, value, start_position);
  settings_object->GetModuleRecordResolver()->RegisterModuleScript(
      value_wrapper_module_script);
  value_wrapper_module_script->SetParseErrorAndClearRecord(
      ScriptValue(settings_object->GetScriptState()->GetIsolate(), error));
  // Step 7. "Return script."
  // [spec text]
  return value_wrapper_module_script;
}

ValueWrapperSyntheticModuleScript::ValueWrapperSyntheticModuleScript(
    Modulator* settings_object,
    v8::Local<v8::Module> record,
    const KURL& source_url,
    const KURL& base_url,
    const ScriptFetchOptions& fetch_options,
    v8::Local<v8::Value> value,
    const TextPosition& start_position)
    : ModuleScript(settings_object,
                   record,
                   source_url,
                   base_url,
                   fetch_options),
      export_value_(v8::Isolate::GetCurrent(), value) {}

// This is the definition of [[EvaluationSteps]] As per the synthetic module
// spec  https://heycam.github.io/webidl/#synthetic-module-records
// It is responsible for setting the default export of the provided module to
// the value wrapped by the ValueWrapperSyntheticModuleScript
v8::MaybeLocal<v8::Value> ValueWrapperSyntheticModuleScript::EvaluationSteps(
    v8::Local<v8::Context> context,
    v8::Local<v8::Module> module) {
  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(context);
  Modulator* modulator = Modulator::From(script_state);
  ModuleRecordResolver* module_record_resolver =
      modulator->GetModuleRecordResolver();
  const ValueWrapperSyntheticModuleScript*
      value_wrapper_synthetic_module_script =
          static_cast<const ValueWrapperSyntheticModuleScript*>(
              module_record_resolver->GetModuleScriptFromModuleRecord(module));
  v8::TryCatch try_catch(isolate);
  v8::Maybe<bool> result = module->SetSyntheticModuleExport(
      isolate, V8String(isolate, "default"),
      value_wrapper_synthetic_module_script->export_value_.NewLocal(isolate));

  // Setting the default export should never fail.
  DCHECK(!try_catch.HasCaught());
  DCHECK(!result.IsNothing() && result.FromJust());

  return v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
}

void ValueWrapperSyntheticModuleScript::Trace(Visitor* visitor) {
  visitor->Trace(export_value_);
  ModuleScript::Trace(visitor);
}

}  // namespace blink
