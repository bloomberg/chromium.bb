// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModuleScript.h"

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ScriptModuleResolver.h"
#include "platform/bindings/ScriptState.h"
#include "v8/include/v8.h"

namespace blink {

ModuleScript* ModuleScript::Create(const String& source_text,
                                   Modulator* modulator,
                                   const KURL& base_url,
                                   const ScriptFetchOptions& options,
                                   AccessControlStatus access_control_status,
                                   const TextPosition& start_position) {
  // https://html.spec.whatwg.org/#creating-a-module-script

  // Step 1. "If scripting is disabled for settings's responsible browsing
  // context, then set source to the empty string." [spec text]
  //
  // TODO(hiroshige): Implement this.

  // Step 2. "Let script be a new module script that this algorithm will
  // subsequently initialize." [spec text]

  // Step 3. "Set script's settings object to settings." [spec text]
  //
  // Note: "script's settings object" will be |modulator|.

  // Step 5. "Let result be ParseModule(source, settings's Realm, script)."
  // [spec text]
  ScriptState* script_state = modulator->GetScriptState();
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "ModuleScript", "Create");

  // Delegate to Modulator::CompileModule to process Steps 3-5.
  ScriptModule result = modulator->CompileModule(
      source_text, base_url.GetString(), options, access_control_status,
      start_position, exception_state);

  // CreateInternal processes Steps 4 and 8-10.
  //
  // [nospec] We initialize the other ModuleScript members anyway by running
  // Steps 8-13 before Step 6. In a case that compile failed, we will
  // immediately turn the script into errored state. Thus the members will not
  // be used for the speced algorithms, but may be used from inspector.
  ModuleScript* script = CreateInternal(source_text, modulator, result,
                                        base_url, options, start_position);

  // Step 6. "If result is a list of errors, then:" [spec text]
  if (exception_state.HadException()) {
    DCHECK(result.IsNull());

    // Step 6.1. "Set script's parse error to result[0]." [spec text]
    v8::Local<v8::Value> error = exception_state.GetException();
    exception_state.ClearException();
    script->SetParseErrorAndClearRecord(ScriptValue(script_state, error));

    // Step 6.2. "Return script." [spec text]
    return script;
  }

  // Step 7. "For each string requested of record.[[RequestedModules]]:" [spec
  // text]
  for (const auto& requested :
       modulator->ModuleRequestsFromScriptModule(result)) {
    // Step 7.1. "Let url be the result of resolving a module specifier given
    // module script and requested." [spec text]
    // Step 7.2. "If url is failure, then:" [spec text]
    // TODO(kouhei): Cache the url here instead of issuing
    // ResolveModuleSpecifier later again in ModuleTreeLinker.
    String failure_reason;
    if (Modulator::ResolveModuleSpecifier(requested.specifier, base_url,
                                          &failure_reason)
            .IsValid())
      continue;

    // Step 7.2.1. "Let error be a new TypeError exception." [spec text]
    String error_message = "Failed to resolve module specifier \"" +
                           requested.specifier + "\". " + failure_reason;
    v8::Local<v8::Value> error =
        V8ThrowException::CreateTypeError(isolate, error_message);

    // Step 7.2.2. "Set script's parse error to error." [spec text]
    script->SetParseErrorAndClearRecord(ScriptValue(script_state, error));

    // Step 7.2.3. "Return script." [spec text]
    return script;
  }

  // Step 11. "Return script." [spec text]
  return script;
}

ModuleScript* ModuleScript::CreateForTest(Modulator* modulator,
                                          ScriptModule record,
                                          const KURL& base_url,
                                          const ScriptFetchOptions& options) {
  String dummy_source_text = "";
  return CreateInternal(dummy_source_text, modulator, record, base_url, options,
                        TextPosition::MinimumPosition());
}

ModuleScript* ModuleScript::CreateInternal(const String& source_text,
                                           Modulator* modulator,
                                           ScriptModule result,
                                           const KURL& base_url,
                                           const ScriptFetchOptions& options,
                                           const TextPosition& start_position) {
  // https://html.spec.whatwg.org/#creating-a-module-script
  // Step 4. Set script's parse error and error to rethrow to null.
  // Step 8. Set script's record to result.
  // Step 9. Set script's base URL to baseURL.
  // Step 10. Set script's fetch options to options.
  // [nospec] |source_text| is saved for CSP checks.
  ModuleScript* module_script = new ModuleScript(
      modulator, result, base_url, options, source_text, start_position);

  // Step 5, a part of ParseModule(): Passing script as the last parameter
  // here ensures result.[[HostDefined]] will be script.
  modulator->GetScriptModuleResolver()->RegisterModuleScript(module_script);

  return module_script;
}

ModuleScript::ModuleScript(Modulator* settings_object,
                           ScriptModule record,
                           const KURL& base_url,
                           const ScriptFetchOptions& fetch_options,
                           const String& source_text,
                           const TextPosition& start_position)
    : Script(fetch_options),
      settings_object_(settings_object),
      record_(this),
      base_url_(base_url),
      parse_error_(this),
      error_to_rethrow_(this),
      source_text_(source_text),
      start_position_(start_position) {
  if (record.IsNull()) {
    // We allow empty records for module infra tests which never touch records.
    // This should never happen outside unit tests.
    return;
  }

  DCHECK(settings_object);
  v8::Isolate* isolate = settings_object_->GetScriptState()->GetIsolate();
  v8::HandleScope scope(isolate);
  record_.Set(isolate, record.NewLocal(isolate));
}

ScriptModule ModuleScript::Record() const {
  if (record_.IsEmpty())
    return ScriptModule();

  v8::Isolate* isolate = settings_object_->GetScriptState()->GetIsolate();
  v8::HandleScope scope(isolate);
  return ScriptModule(isolate, record_.NewLocal(isolate));
}

bool ModuleScript::HasEmptyRecord() const {
  return record_.IsEmpty();
}

void ModuleScript::SetParseErrorAndClearRecord(ScriptValue error) {
  DCHECK(!error.IsEmpty());

  record_.Clear();
  ScriptState::Scope scope(error.GetScriptState());
  parse_error_.Set(error.GetIsolate(), error.V8Value());
}

ScriptValue ModuleScript::CreateParseError() const {
  ScriptState* script_state = settings_object_->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);
  ScriptValue error(script_state, parse_error_.NewLocal(isolate));
  DCHECK(!error.IsEmpty());
  return error;
}

void ModuleScript::SetErrorToRethrow(ScriptValue error) {
  ScriptState::Scope scope(error.GetScriptState());
  error_to_rethrow_.Set(error.GetIsolate(), error.V8Value());
}

ScriptValue ModuleScript::CreateErrorToRethrow() const {
  ScriptState* script_state = settings_object_->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);
  ScriptValue error(script_state, error_to_rethrow_.NewLocal(isolate));
  DCHECK(!error.IsEmpty());
  return error;
}

void ModuleScript::Trace(blink::Visitor* visitor) {
  visitor->Trace(settings_object_);
  Script::Trace(visitor);
}
void ModuleScript::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  // TODO(mlippautz): Support TraceWrappers(const
  // TraceWrapperV8Reference<v8::Module>&) to remove the cast.
  visitor->TraceWrappers(record_.UnsafeCast<v8::Value>());
  visitor->TraceWrappers(parse_error_);
  visitor->TraceWrappers(error_to_rethrow_);
}

void ModuleScript::RunScript(LocalFrame* frame, const SecurityOrigin*) const {
  DVLOG(1) << *this << "::RunScript()";
  settings_object_->ExecuteModule(this,
                                  Modulator::CaptureEvalErrorFlag::kReport);
}

String ModuleScript::InlineSourceTextForCSP() const {
  return source_text_;
}

std::ostream& operator<<(std::ostream& stream,
                         const ModuleScript& module_script) {
  stream << "ModuleScript[" << &module_script;
  if (module_script.HasEmptyRecord())
    stream << ", empty-record";

  if (module_script.HasErrorToRethrow())
    stream << ", error-to-rethrow";

  if (module_script.HasParseError())
    stream << ", parse-error";

  return stream << "]";
}

}  // namespace blink
