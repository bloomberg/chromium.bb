// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModulatorImpl.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/ModuleMap.h"
#include "core/dom/ModuleScript.h"
#include "core/dom/ScriptModuleResolverImpl.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleScriptLoaderRegistry.h"
#include "core/loader/modulescript/ModuleTreeLinkerRegistry.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

ModulatorImpl* ModulatorImpl::Create(RefPtr<ScriptState> script_state,
                                     ResourceFetcher* resource_fetcher) {
  return new ModulatorImpl(std::move(script_state), resource_fetcher);
}

ModulatorImpl::ModulatorImpl(RefPtr<ScriptState> script_state,
                             ResourceFetcher* fetcher)
    : script_state_(std::move(script_state)),
      task_runner_(
          TaskRunnerHelper::Get(TaskType::kNetworking, script_state_.Get())),
      fetcher_(fetcher),
      map_(ModuleMap::Create(this)),
      loader_registry_(ModuleScriptLoaderRegistry::Create()),
      tree_linker_registry_(ModuleTreeLinkerRegistry::Create()),
      script_module_resolver_(ScriptModuleResolverImpl::Create(
          this,
          ExecutionContext::From(script_state_.Get()))) {
  DCHECK(script_state_);
  DCHECK(task_runner_);
  DCHECK(fetcher_);
}

ModulatorImpl::~ModulatorImpl() {}

ReferrerPolicy ModulatorImpl::GetReferrerPolicy() {
  return GetExecutionContext()->GetReferrerPolicy();
}

SecurityOrigin* ModulatorImpl::GetSecurityOrigin() {
  return GetExecutionContext()->GetSecurityOrigin();
}

void ModulatorImpl::FetchTree(const ModuleScriptFetchRequest& request,
                              ModuleTreeClient* client) {
  // Step 1. Perform the internal module script graph fetching procedure given
  // url, settings object, destination, cryptographic nonce, parser state,
  // credentials mode, settings object, a new empty list, "client", and with the
  // top-level module fetch flag set. If the caller of this algorithm specified
  // custom perform the fetch steps, pass those along as well.

  // Note: "Fetch a module script graph" algorithm doesn't have "referrer" as
  // its argument.
  DCHECK(request.GetReferrer().IsNull());

  AncestorList empty_ancestor_list;
  FetchTreeInternal(request, empty_ancestor_list,
                    ModuleGraphLevel::kTopLevelModuleFetch, nullptr, client);

  // Step 2. When the internal module script graph fetching procedure
  // asynchronously completes with result, asynchronously complete this
  // algorithm with result.
  // Note: We delegate to ModuleTreeLinker to notify ModuleTreeClient.
}

void ModulatorImpl::FetchTreeInternal(const ModuleScriptFetchRequest& request,
                                      const AncestorList& ancestor_list,
                                      ModuleGraphLevel level,
                                      ModuleTreeReachedUrlSet* reached_url_set,
                                      ModuleTreeClient* client) {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  tree_linker_registry_->Fetch(request, ancestor_list, level, this,
                               reached_url_set, client);
}

void ModulatorImpl::FetchDescendantsForInlineScript(ModuleScript* module_script,
                                                    ModuleTreeClient* client) {
  tree_linker_registry_->FetchDescendantsForInlineScript(module_script, this,
                                                         client);
}

void ModulatorImpl::FetchSingle(const ModuleScriptFetchRequest& request,
                                ModuleGraphLevel level,
                                SingleModuleClient* client) {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  map_->FetchSingleModuleScript(request, level, client);
}

void ModulatorImpl::FetchNewSingleModule(
    const ModuleScriptFetchRequest& request,
    ModuleGraphLevel level,
    ModuleScriptLoaderClient* client) {
  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  loader_registry_->Fetch(request, level, this, fetcher_.Get(), client);
}

ModuleScript* ModulatorImpl::GetFetchedModuleScript(const KURL& url) {
  return map_->GetFetchedModuleScript(url);
}

bool ModulatorImpl::HasValidContext() {
  return script_state_->ContextIsValid();
}

ScriptModule ModulatorImpl::CompileModule(
    const String& provided_source,
    const String& url_str,
    AccessControlStatus access_control_status,
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
  ScriptState::Scope scope(script_state_.Get());
  return ScriptModule::Compile(script_state_->GetIsolate(), script_source,
                               url_str, access_control_status, position,
                               exception_state);
}

ScriptValue ModulatorImpl::InstantiateModule(ScriptModule script_module) {
  ScriptState::Scope scope(script_state_.Get());
  return script_module.Instantiate(script_state_.Get());
}

ScriptModuleState ModulatorImpl::GetRecordStatus(ScriptModule script_module) {
  ScriptState::Scope scope(script_state_.Get());
  return script_module.Status(script_state_.Get());
}

ScriptValue ModulatorImpl::GetError(const ModuleScript* module_script) {
  DCHECK(module_script);
  ScriptState::Scope scope(script_state_.Get());
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-module-script-error
  // "When a module script is errored, ..." [spec text]

  // "we say that its error is either its pre-instantiation error, when its
  // module record is null, ..." [spec text]
  ScriptModule record = module_script->Record();
  if (record.IsNull()) {
    return ScriptValue(script_state_.Get(), module_script->CreateErrorInternal(
                                                script_state_->GetIsolate()));
  }

  // "or its module record's [[ErrorCompletion]] field's [[Value]] field,
  // otherwise." [spec text]
  return ScriptValue(script_state_.Get(),
                     record.ErrorCompletion(script_state_.Get()));
}

Vector<Modulator::ModuleRequest> ModulatorImpl::ModuleRequestsFromScriptModule(
    ScriptModule script_module) {
  ScriptState::Scope scope(script_state_.Get());
  Vector<String> specifiers = script_module.ModuleRequests(script_state_.Get());
  Vector<TextPosition> positions =
      script_module.ModuleRequestPositions(script_state_.Get());
  DCHECK_EQ(specifiers.size(), positions.size());
  Vector<ModuleRequest> requests;
  requests.ReserveInitialCapacity(specifiers.size());
  for (size_t i = 0; i < specifiers.size(); ++i) {
    requests.emplace_back(specifiers[i], positions[i]);
  }
  return requests;
}

inline ExecutionContext* ModulatorImpl::GetExecutionContext() const {
  return ExecutionContext::From(script_state_.Get());
}

void ModulatorImpl::ExecuteModule(const ModuleScript* module_script) {
  // https://html.spec.whatwg.org/#run-a-module-script

  // We ensure module-related code is not executed without the flag.
  // https://crbug.com/715376
  CHECK(RuntimeEnabledFeatures::ModuleScriptsEnabled());

  // Step 1. "Let settings be the settings object of s." [spec text]
  // The settings object is |this|.

  // Step 2. "Check if we can run script with settings.
  //          If this returns "do not run" then abort these steps." [spec text]
  if (!GetExecutionContext()->CanExecuteScripts(kAboutToExecuteScript))
    return;

  // Step 4. "Prepare to run script given settings." [spec text]
  // This is placed here to also cover ScriptModule::ReportException().
  ScriptState::Scope scope(script_state_.Get());

  // Step 3. "If s is errored, then report the exception given by s's error for
  // s and abort these steps." [spec text]
  if (module_script->IsErrored()) {
    ScriptValue error = GetError(module_script);
    ScriptModule::ReportException(script_state_.Get(), error.V8Value(),
                                  module_script->BaseURL().GetString(),
                                  module_script->StartPosition());
    return;
  }

  // Step 5. "Let record be s's module record." [spec text]
  const ScriptModule& record = module_script->Record();
  CHECK(!record.IsNull());

  // Step 6. "Let evaluationStatus be record.ModuleEvaluation()." [spec text]
  record.Evaluate(script_state_.Get());

  // Step 7. "If evaluationStatus is an abrupt completion, then report the
  // exception given by evaluationStatus.[[Value]] for s." [spec text]
  // TODO(kouhei): Implement this.

  // Step 8. "Clean up after running script with settings." [spec text]
  // Implemented as the ScriptState::Scope destructor.
}

DEFINE_TRACE(ModulatorImpl) {
  Modulator::Trace(visitor);
  visitor->Trace(fetcher_);
  visitor->Trace(map_);
  visitor->Trace(loader_registry_);
  visitor->Trace(tree_linker_registry_);
  visitor->Trace(script_module_resolver_);
}

DEFINE_TRACE_WRAPPERS(ModulatorImpl) {
  visitor->TraceWrappers(map_);
  visitor->TraceWrappers(tree_linker_registry_);
}

}  // namespace blink
