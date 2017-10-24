// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletGlobalScope.h"

#include <memory>
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Modulator.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkletModuleResponsesMap.h"
#include "core/workers/WorkletModuleTreeClient.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

// Partial implementation of the "set up a worklet environment settings object"
// algorithm:
// https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
WorkletGlobalScope::WorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    v8::Isolate* isolate,
    WorkerReportingProxy& reporting_proxy)
    : WorkerOrWorkletGlobalScope(isolate,
                                 creation_params->worker_clients,
                                 reporting_proxy),
      url_(creation_params->script_url),
      user_agent_(creation_params->user_agent),
      document_security_origin_(creation_params->starter_origin) {
  // Step 2: "Let inheritedAPIBaseURL be outsideSettings's API base URL."
  // |url_| is the inheritedAPIBaseURL passed from the parent Document.

  // Step 3: "Let origin be a unique opaque origin."
  SetSecurityOrigin(SecurityOrigin::CreateUnique());

  // Step 5: "Let inheritedReferrerPolicy be outsideSettings's referrer policy."
  // TODO(nhiroki): Set the referrer policy (https://crbug.com/773921).
}

WorkletGlobalScope::~WorkletGlobalScope() = default;

void WorkletGlobalScope::EvaluateClassicScript(
    const KURL& script_url,
    String source_code,
    std::unique_ptr<Vector<char>> cached_meta_data) {
  // Worklet should evaluate a script as a module script (as opposed to a
  // classic script).
  NOTREACHED();
}

v8::Local<v8::Object> WorkletGlobalScope::Wrap(
    v8::Isolate*,
    v8::Local<v8::Object> creation_context) {
  LOG(FATAL) << "WorkletGlobalScope must never be wrapped with wrap method. "
                "The global object of ECMAScript environment is used as the "
                "wrapper.";
  return v8::Local<v8::Object>();
}

v8::Local<v8::Object> WorkletGlobalScope::AssociateWithWrapper(
    v8::Isolate*,
    const WrapperTypeInfo*,
    v8::Local<v8::Object> wrapper) {
  LOG(FATAL) << "WorkletGlobalScope must never be wrapped with wrap method. "
                "The global object of ECMAScript environment is used as the "
                "wrapper.";
  return v8::Local<v8::Object>();
}

bool WorkletGlobalScope::HasPendingActivity() const {
  // The worklet global scope wrapper is kept alive as longs as its execution
  // context is active.
  return !ExecutionContext::IsContextDestroyed();
}

ExecutionContext* WorkletGlobalScope::GetExecutionContext() const {
  return const_cast<WorkletGlobalScope*>(this);
}

bool WorkletGlobalScope::IsSecureContext(String& error_message) const {
  // Until there are APIs that are available in worklets and that
  // require a privileged context test that checks ancestors, just do
  // a simple check here.
  if (GetSecurityOrigin()->IsPotentiallyTrustworthy())
    return true;
  error_message = GetSecurityOrigin()->IsPotentiallyTrustworthyErrorMessage();
  return false;
}

// Implementation of the first half of the "fetch and invoke a worklet script"
// algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
void WorkletGlobalScope::FetchAndInvokeScript(
    const KURL& module_url_record,
    WorkletModuleResponsesMap* module_responses_map,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    scoped_refptr<WebTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsContextThread());
  if (!module_responses_map_proxy_) {
    // |kUnspecedLoading| is used here because this is a part of script module
    // loading and this usage is not explicitly spec'ed.
    module_responses_map_proxy_ = WorkletModuleResponsesMapProxy::Create(
        module_responses_map, outside_settings_task_runner,
        TaskRunnerHelper::Get(TaskType::kUnspecedLoading, this));
  }

  // Step 1: "Let insideSettings be the workletGlobalScope's associated
  // environment settings object."
  // Step 2: "Let script by the result of fetch a worklet script given
  // moduleURLRecord, moduleResponsesMap, credentialOptions, outsideSettings,
  // and insideSettings when it asynchronously completes."
  String nonce;
  ParserDisposition parser_state = kNotParserInserted;
  Modulator* modulator = Modulator::From(ScriptController()->GetScriptState());
  // TODO(nhiroki, ikilpatrick): Update spec to use #script-fetch-options.
  ScriptFetchOptions options(nonce, parser_state, credentials_mode);
  ModuleScriptFetchRequest module_request(module_url_record, options);

  // Step 3 to 5 are implemented in
  // WorkletModuleTreeClient::NotifyModuleTreeLoadFinished.
  WorkletModuleTreeClient* client = new WorkletModuleTreeClient(
      modulator, std::move(outside_settings_task_runner), pending_tasks);
  modulator->FetchTree(module_request, client);
}

WorkletModuleResponsesMapProxy* WorkletGlobalScope::ModuleResponsesMapProxy()
    const {
  DCHECK(IsContextThread());
  DCHECK(module_responses_map_proxy_);
  return module_responses_map_proxy_;
}

void WorkletGlobalScope::SetModuleResponsesMapProxyForTesting(
    WorkletModuleResponsesMapProxy* proxy) {
  DCHECK(!module_responses_map_proxy_);
  module_responses_map_proxy_ = proxy;
}

void WorkletGlobalScope::SetModulator(Modulator* modulator) {
  modulator_ = modulator;
}

KURL WorkletGlobalScope::CompleteURL(const String& url) const {
  // Always return a null URL when passed a null string.
  // TODO(ikilpatrick): Should we change the KURL constructor to have this
  // behavior?
  if (url.IsNull())
    return KURL();
  // Always use UTF-8 in Worklets.
  return KURL(BaseURL(), url);
}

void WorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(module_responses_map_proxy_);
  visitor->Trace(modulator_);
  ScriptWrappable::Trace(visitor);
  SecurityContext::Trace(visitor);
  WorkerOrWorkletGlobalScope::Trace(visitor);
}

void WorkletGlobalScope::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(modulator_);
}

}  // namespace blink
