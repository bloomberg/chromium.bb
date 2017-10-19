// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModulatorImplBase.h"

#include "core/dom/DynamicModuleResolver.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ModuleMap.h"
#include "core/dom/ModuleScript.h"
#include "core/dom/ScriptModuleResolverImpl.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleScriptLoaderRegistry.h"
#include "core/loader/modulescript/ModuleTreeLinkerRegistry.h"
#include "platform/WebTaskRunner.h"

namespace blink {

ExecutionContext* ModulatorImplBase::GetExecutionContext() const {
  return ExecutionContext::From(script_state_.get());
}

ModulatorImplBase::ModulatorImplBase(RefPtr<ScriptState> script_state)
    : script_state_(std::move(script_state)),
      task_runner_(
          TaskRunnerHelper::Get(TaskType::kNetworking, script_state_.get())),
      map_(ModuleMap::Create(this)),
      loader_registry_(ModuleScriptLoaderRegistry::Create()),
      tree_linker_registry_(ModuleTreeLinkerRegistry::Create()),
      script_module_resolver_(ScriptModuleResolverImpl::Create(
          this,
          ExecutionContext::From(script_state_.get()))),
      dynamic_module_resolver_(DynamicModuleResolver::Create(this)) {
  DCHECK(script_state_);
  DCHECK(task_runner_);
}

ModulatorImplBase::~ModulatorImplBase() {}

ReferrerPolicy ModulatorImplBase::GetReferrerPolicy() {
  return GetExecutionContext()->GetReferrerPolicy();
}

SecurityOrigin* ModulatorImplBase::GetSecurityOriginForFetch() {
  return GetExecutionContext()->GetSecurityOrigin();
}

void ModulatorImplBase::FetchTree(const ModuleScriptFetchRequest& request,
                                  ModuleTreeClient* client) {
  // Step 1. Perform the internal module script graph fetching procedure given
  // url, settings object, destination, cryptographic nonce, parser state,
  // credentials mode, settings object, a new empty list, "client", and with the
  // top-level module fetch flag set. If the caller of this algorithm specified
  // custom perform the fetch steps, pass those along as well.

  // Note: "Fetch a module script graph" algorithm doesn't have "referrer" as
  // its argument.
  DCHECK(request.GetReferrer().IsNull());

  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  tree_linker_registry_->Fetch(request, this, client);

  // Step 2. When the internal module script graph fetching procedure
  // asynchronously completes with result, asynchronously complete this
  // algorithm with result.
  // Note: We delegate to ModuleTreeLinker to notify ModuleTreeClient.
}

void ModulatorImplBase::FetchDescendantsForInlineScript(
    ModuleScript* module_script,
    ModuleTreeClient* client) {
  tree_linker_registry_->FetchDescendantsForInlineScript(module_script, this,
                                                         client);
}

void ModulatorImplBase::FetchSingle(const ModuleScriptFetchRequest& request,
                                    ModuleGraphLevel level,
                                    SingleModuleClient* client) {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  map_->FetchSingleModuleScript(request, level, client);
}

void ModulatorImplBase::FetchNewSingleModule(
    const ModuleScriptFetchRequest& request,
    ModuleGraphLevel level,
    ModuleScriptLoaderClient* client) {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  loader_registry_->Fetch(request, level, this, client);
}

ModuleScript* ModulatorImplBase::GetFetchedModuleScript(const KURL& url) {
  return map_->GetFetchedModuleScript(url);
}

bool ModulatorImplBase::HasValidContext() {
  return script_state_->ContextIsValid();
}

void ModulatorImplBase::ResolveDynamically(
    const String& specifier,
    const KURL& referrer_url,
    const ReferrerScriptInfo& referrer_info,
    ScriptPromiseResolver* resolver) {
  dynamic_module_resolver_->ResolveDynamically(specifier, referrer_url,
                                               referrer_info, resolver);
}

ScriptModule ModulatorImplBase::CompileModule(
    const String& provided_source,
    const String& url_str,
    AccessControlStatus access_control_status,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    const String& nonce,
    ParserDisposition parser_state,
    const TextPosition& position,
    ExceptionState& exception_state) {
  // Implements Steps 3-5 of
  // https://html.spec.whatwg.org/multipage/webappapis.html#creating-a-module-script

  // Step 3. Let realm be the provided environment settings object's Realm.
  // Note: Realm is v8::Context.

  // Step 4. If scripting is disabled for the given environment settings
  // object's responsible browsing context, then let script source be the empty
  // string. Otherwise, let script source be the provided script source.
  String script_source;
  if (GetExecutionContext()->CanExecuteScripts(kAboutToExecuteScript))
    script_source = provided_source;

  // Step 5. Let result be ParseModule(script source, realm, script).
  ScriptState::Scope scope(script_state_.get());
  return ScriptModule::Compile(script_state_->GetIsolate(), script_source,
                               url_str, access_control_status, credentials_mode,
                               nonce, parser_state, position, exception_state);
}

ScriptValue ModulatorImplBase::InstantiateModule(ScriptModule script_module) {
  ScriptState::Scope scope(script_state_.get());
  return script_module.Instantiate(script_state_.get());
}

ScriptModuleState ModulatorImplBase::GetRecordStatus(
    ScriptModule script_module) {
  ScriptState::Scope scope(script_state_.get());
  return script_module.Status(script_state_.get());
}

ScriptValue ModulatorImplBase::GetError(const ModuleScript* module_script) {
  DCHECK(module_script);
  ScriptState::Scope scope(script_state_.get());
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-error
  // "When a module script is errored, ..." [spec text]

  // "we say that its error is either its pre-instantiation error, when its
  // module record is null, ..." [spec text]
  ScriptModule record = module_script->Record();
  if (record.IsNull()) {
    return ScriptValue(script_state_.get(), module_script->CreateErrorInternal(
                                                script_state_->GetIsolate()));
  }

  // "or its module record's [[ErrorCompletion]] field's [[Value]] field,
  // otherwise." [spec text]
  return ScriptValue(script_state_.get(),
                     record.ErrorCompletion(script_state_.get()));
}

Vector<Modulator::ModuleRequest>
ModulatorImplBase::ModuleRequestsFromScriptModule(ScriptModule script_module) {
  ScriptState::Scope scope(script_state_.get());
  Vector<String> specifiers = script_module.ModuleRequests(script_state_.get());
  Vector<TextPosition> positions =
      script_module.ModuleRequestPositions(script_state_.get());
  DCHECK_EQ(specifiers.size(), positions.size());
  Vector<ModuleRequest> requests;
  requests.ReserveInitialCapacity(specifiers.size());
  for (size_t i = 0; i < specifiers.size(); ++i) {
    requests.emplace_back(specifiers[i], positions[i]);
  }
  return requests;
}

ScriptValue ModulatorImplBase::ExecuteModule(
    const ModuleScript* module_script,
    CaptureEvalErrorFlag capture_error) {
  // https://html.spec.whatwg.org/#run-a-module-script

  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  // Step 1. "If rethrow errors is not given, let it be false." [spec text]

  // Step 2. "Let settings be the settings object of s." [spec text]
  // The settings object is |this|.

  // Step 3. "Check if we can run script with settings.
  //          If this returns "do not run" then return." [spec text]
  if (!GetExecutionContext()->CanExecuteScripts(kAboutToExecuteScript))
    return ScriptValue();

  // Step 4. "Prepare to run script given settings." [spec text]
  // This is placed here to also cover ScriptModule::ReportException().
  ScriptState::Scope scope(script_state_.get());

  // Step 5. "Let evaluationStatus be null." [spec text]
  // |error| corresponds to "evaluationStatus of [[Type]]: throw".
  ScriptValue error;

  // Step 6. "If s is errored, then set evaluationStatus to Completion
  // { [[Type]]: throw, [[Value]]: script's error, [[Target]]: empty }."
  // [spec text]
  if (module_script->IsErrored()) {
    error = GetError(module_script);
  } else {
    // Step 7. "Otherwise:" [spec text]
    // Step 7.1. "Let record be s's module record." [spec text]
    const ScriptModule& record = module_script->Record();
    CHECK(!record.IsNull());

    // Step 7.2 "Let evaluationStatus be record.ModuleEvaluate()." [spec text]
    error = record.Evaluate(script_state_.get(), capture_error);
  }

  // Step 8. "If evaluationStatus is an abrupt completion, then:" [spec text]
  if (!error.IsEmpty()) {
    // Step 8.1. "If rethrow errors is true, rethrow the exception given by
    // evaluationStatus.[[Value]] for s." [spec text]
    if (capture_error == CaptureEvalErrorFlag::kCapture)
      return error;

    // Step 8.2. "Otherwise, report the exception given by
    // evaluationStatus.[[Value]] for script." [spec text]
    ScriptModule::ReportException(script_state_.get(), error.V8Value());

    return ScriptValue();
  }

  // Step 9. "Clean up after running script with settings." [spec text]
  // Implemented as the ScriptState::Scope destructor.
  return ScriptValue();
}

void ModulatorImplBase::Trace(blink::Visitor* visitor) {
  Modulator::Trace(visitor);
  visitor->Trace(map_);
  visitor->Trace(loader_registry_);
  visitor->Trace(tree_linker_registry_);
  visitor->Trace(script_module_resolver_);
  visitor->Trace(dynamic_module_resolver_);
}

void ModulatorImplBase::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(map_);
  visitor->TraceWrappers(tree_linker_registry_);
}

}  // namespace blink
