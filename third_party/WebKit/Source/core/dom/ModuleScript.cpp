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
  // Step 1. Let script be a new module script that this algorithm will
  // subsequently initialize.
  // Step 2. Set script's settings object to the environment settings object
  // provided.
  // Note: "script's settings object" will be "modulator".

  // Delegate to Modulator::compileModule to process Steps 3-6.
  ScriptModule result = modulator->CompileModule(
      source_text, base_url.GetString(), access_control_status, start_position);
  // Step 6: "...return null, and abort these steps."
  if (result.IsNull())
    return nullptr;

  return CreateInternal(source_text, modulator, result, base_url, nonce,
                        parser_state, credentials_mode);
}

ModuleScript* ModuleScript::CreateInternal(
    const String& source_text,
    Modulator* modulator,
    ScriptModule result,
    const KURL& base_url,
    const String& nonce,
    ParserDisposition parser_state,
    WebURLRequest::FetchCredentialsMode credentials_mode) {
  // https://html.spec.whatwg.org/#creating-a-module-script
  // Step 7. Set script's module record to result.
  // Step 8. Set script's base URL to the script base URL provided.
  // Step 9. Set script's cryptographic nonce to the cryptographic nonce
  // provided.
  // Step 10. Set script's parser state to the parser state.
  // Step 11. Set script's credentials mode to the credentials mode provided.
  // Step 12. Return script.
  // [not specced] |source_text| is saved for CSP checks.
  ModuleScript* module_script =
      new ModuleScript(modulator, result, base_url, nonce, parser_state,
                       credentials_mode, source_text);

  // Step 5, a part of ParseModule(): Passing script as the last parameter
  // here ensures result.[[HostDefined]] will be script.
  modulator->GetScriptModuleResolver()->RegisterModuleScript(module_script);

  return module_script;
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
                        parser_state, credentials_mode);
}

ModuleScript::ModuleScript(Modulator* settings_object,
                           ScriptModule record,
                           const KURL& base_url,
                           const String& nonce,
                           ParserDisposition parser_state,
                           WebURLRequest::FetchCredentialsMode credentials_mode,
                           const String& source_text)
    : settings_object_(settings_object),
      record_(this),
      base_url_(base_url),
      instantiation_error_(this),
      nonce_(nonce),
      parser_state_(parser_state),
      credentials_mode_(credentials_mode),
      source_text_(source_text) {
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

void ModuleScript::SetInstantiationErrorAndClearRecord(ScriptValue error) {
  // Implements Step 7.1 of:
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure

  // "set script's instantiation state to "errored", ..."
  DCHECK_EQ(instantiation_state_, ModuleInstantiationState::kUninstantiated);
  instantiation_state_ = ModuleInstantiationState::kErrored;

  // "its instantiation error to instantiationStatus.[[Value]], and ..."
  DCHECK(!error.IsEmpty());
  {
    ScriptState::Scope scope(error.GetScriptState());
    instantiation_error_.Set(error.GetIsolate(), error.V8Value());
  }

  // "its module record to null."
  record_.Clear();
}

void ModuleScript::SetInstantiationSuccess() {
  // Implements Step 7.2 of:
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure

  // "set script's instantiation state to "instantiated"."
  DCHECK_EQ(instantiation_state_, ModuleInstantiationState::kUninstantiated);
  instantiation_state_ = ModuleInstantiationState::kInstantiated;
}

DEFINE_TRACE(ModuleScript) {
  visitor->Trace(settings_object_);
  Script::Trace(visitor);
}
DEFINE_TRACE_WRAPPERS(ModuleScript) {
  // TODO(mlippautz): Support TraceWrappers(const
  // TraceWrapperV8Reference<v8::Module>&) to remove the cast.
  visitor->TraceWrappers(record_.Cast<v8::Value>());
  visitor->TraceWrappers(instantiation_error_);
}

bool ModuleScript::IsEmpty() const {
  return false;
}

bool ModuleScript::CheckMIMETypeBeforeRunScript(Document* context_document,
                                                const SecurityOrigin*) const {
  // We don't check MIME type here because we check the MIME type in
  // ModuleScriptLoader::WasModuleLoadSuccessful().
  return true;
}

void ModuleScript::RunScript(LocalFrame* frame, const SecurityOrigin*) const {
  settings_object_->ExecuteModule(this);
}

String ModuleScript::InlineSourceTextForCSP() const {
  return source_text_;
}

}  // namespace blink
