// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModuleScript.h"

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ScriptModuleResolver.h"
#include "platform/bindings/ScriptState.h"
#include "v8/include/v8.h"

namespace blink {

ModuleScript* ModuleScript::Create(
    const String& source_text,
    Modulator* modulator,
    const KURL& base_url,
    const String& nonce,
    ParserDisposition parser_state,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    AccessControlStatus access_control_status,
    const TextPosition& start_position) {
  // https://html.spec.whatwg.org/#creating-a-module-script
  // Step 1. "Let script be a new module script that this algorithm will
  // subsequently initialize, with its module record initially set to null."
  // [spec text] Step 2. "Set script's settings object to the environment
  // settings object provided." [spec text] Note: "script's settings object"
  // will be "modulator".
  ScriptState* script_state = modulator->GetScriptState();
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "ModuleScript", "Create");

  // Delegate to Modulator::CompileModule to process Steps 3-5.
  ScriptModule result = modulator->CompileModule(
      source_text, base_url.GetString(), access_control_status,
      credentials_mode, nonce, parser_state, start_position, exception_state);

  // CreateInternal processes Steps 8-13.
  // [nospec] We initialize the other ModuleScript members anyway by running
  // Steps 8-13 before Step 6. In a case that compile failed, we will
  // immediately turn the script into errored state. Thus the members will not
  // be used for the speced algorithms, but may be used from inspector.
  ModuleScript* script =
      CreateInternal(source_text, modulator, result, base_url, nonce,
                     parser_state, credentials_mode, start_position);

  // Step 6. "If result is a List of errors, then:" [spec text]
  if (exception_state.HadException()) {
    DCHECK(result.IsNull());

    // Step 6.1. "Error script with errors[0]." [spec text]
    v8::Local<v8::Value> error = exception_state.GetException();
    exception_state.ClearException();
    script->SetErrorAndClearRecord(ScriptValue(script_state, error));

    // Step 6.2. "Return script." [spec text]
    return script;
  }

  // Step 7. "For each string requested of record.[[RequestedModules]]:" [spec
  // text]
  for (const auto& requested :
       modulator->ModuleRequestsFromScriptModule(result)) {
    // Step 7.1. "Let url be the result of resolving a module specifier given
    // module script and requested."[spec text] Step 7.2. "If url is failure:"
    // [spec text]
    // TODO(kouhei): Cache the url here instead of issuing
    // ResolveModuleSpecifier later again in ModuleTreeLinker.
    if (modulator->ResolveModuleSpecifier(requested.specifier, base_url)
            .IsValid())
      continue;

    // Step 7.2.1. "Let error be a new TypeError exception." [spec text]
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        isolate,
        "Failed to resolve module specifier '" + requested.specifier + "'");

    // Step 7.2.2. "Set the parse error of script to error." [spec text]
    script->SetErrorAndClearRecord(ScriptValue(script_state, error));

    // Step 7.2.3. "Return script." [spec text]
    return script;
  }

  return script;
}

ModuleScript* ModuleScript::CreateForTest(
    Modulator* modulator,
    ScriptModule record,
    const KURL& base_url,
    const String& nonce,
    ParserDisposition parser_state,
    WebURLRequest::FetchCredentialsMode credentials_mode) {
  String dummy_source_text = "";
  return CreateInternal(dummy_source_text, modulator, record, base_url, nonce,
                        parser_state, credentials_mode,
                        TextPosition::MinimumPosition());
}

ModuleScript* ModuleScript::CreateInternal(
    const String& source_text,
    Modulator* modulator,
    ScriptModule result,
    const KURL& base_url,
    const String& nonce,
    ParserDisposition parser_state,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    const TextPosition& start_position) {
  // https://html.spec.whatwg.org/#creating-a-module-script
  // Step 8. Set script's module record to result.
  // Step 9. Set script's base URL to the script base URL provided.
  // Step 10. Set script's cryptographic nonce to the cryptographic nonce
  // provided.
  // Step 11. Set script's parser state to the parser state.
  // Step 12. Set script's credentials mode to the credentials mode provided.
  // Step 13. Return script.
  // [not specced] |source_text| is saved for CSP checks.
  ModuleScript* module_script =
      new ModuleScript(modulator, result, base_url, nonce, parser_state,
                       credentials_mode, source_text, start_position);

  // Step 5, a part of ParseModule(): Passing script as the last parameter
  // here ensures result.[[HostDefined]] will be script.
  modulator->GetScriptModuleResolver()->RegisterModuleScript(module_script);

  return module_script;
}

ModuleScript::ModuleScript(Modulator* settings_object,
                           ScriptModule record,
                           const KURL& base_url,
                           const String& nonce,
                           ParserDisposition parser_state,
                           WebURLRequest::FetchCredentialsMode credentials_mode,
                           const String& source_text,
                           const TextPosition& start_position)
    : settings_object_(settings_object),
      record_(this),
      base_url_(base_url),
      preinstantiation_error_(this),
      nonce_(nonce),
      parser_state_(parser_state),
      credentials_mode_(credentials_mode),
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

ScriptModuleState ModuleScript::RecordStatus() const {
  DCHECK(!record_.IsEmpty());
  return settings_object_->GetRecordStatus(Record());
}

// https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-has-instantiated
bool ModuleScript::HasInstantiated() const {
  // "We say that a module script has instantiated if ..." [spec text]

  // "its module record is not null, and ..." [spec text]
  if (record_.IsEmpty())
    return false;

  // "its module record's [[Status]] field is ..." [spec text]
  ScriptModuleState status = RecordStatus();

  // "either "instantiated" or "evaluated"." [spec text]
  return status == ScriptModuleState::kInstantiated ||
         status == ScriptModuleState::kEvaluated;
}

// https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-is-errored
bool ModuleScript::IsErrored() const {
  // "We say that a module script is errored ..." [spec text]

  // "if either its module record is null, ..." [spec text]
  if (record_.IsEmpty())
    return true;

  // "or its module record's [[Status]] field has the value "errored"." [spec
  // text]
  return RecordStatus() == ScriptModuleState::kErrored;
}

void ModuleScript::SetErrorAndClearRecord(ScriptValue error) {
  DVLOG(1) << *this << "::SetErrorAndClearRecord()";

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-set-pre-instantiation-error
  // Step 1. "If script's module record is not null, ..." [spec text]
  if (!record_.IsEmpty()) {
    // "set its [[HostDefined]] field to undefined." [spec text]
    if (ScriptModuleResolver* resolver =
            settings_object_->GetScriptModuleResolver())
      resolver->UnregisterModuleScript(this);
  }

  // Step 2. "Set script's module record to null." [spec text]
  record_.Clear();

  // Step 3. "Set script's pre-instantiation error to error." [spec text]
  DCHECK(!error.IsEmpty());
  {
    ScriptState::Scope scope(error.GetScriptState());
    preinstantiation_error_.Set(error.GetIsolate(), error.V8Value());
  }
}

DEFINE_TRACE(ModuleScript) {
  visitor->Trace(settings_object_);
  Script::Trace(visitor);
}
DEFINE_TRACE_WRAPPERS(ModuleScript) {
  // TODO(mlippautz): Support TraceWrappers(const
  // TraceWrapperV8Reference<v8::Module>&) to remove the cast.
  visitor->TraceWrappers(record_.UnsafeCast<v8::Value>());
  visitor->TraceWrappers(preinstantiation_error_);
}

bool ModuleScript::CheckMIMETypeBeforeRunScript(Document* context_document,
                                                const SecurityOrigin*) const {
  // We don't check MIME type here because we check the MIME type in
  // ModuleScriptLoader::WasModuleLoadSuccessful().
  return true;
}

void ModuleScript::RunScript(LocalFrame* frame, const SecurityOrigin*) const {
  DVLOG(1) << *this << "::RunScript()";
  settings_object_->ExecuteModule(this);
}

String ModuleScript::InlineSourceTextForCSP() const {
  return source_text_;
}

std::ostream& operator<<(std::ostream& stream,
                         const ModuleScript& module_script) {
  stream << "ModuleScript[" << &module_script << ", ";
  if (module_script.HasEmptyRecord()) {
    stream << "errored (empty record)";
  } else {
    stream << "record's [[Status]] = "
           << ScriptModuleStateToString(module_script.RecordStatus());
  }

  return stream << "]";
}

}  // namespace blink
