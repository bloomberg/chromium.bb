// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletGlobalScope.h"

#include <memory>
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/probe/CoreProbes.h"
#include "core/script/Modulator.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkletModuleResponsesMap.h"
#include "core/workers/WorkletModuleTreeClient.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "public/platform/TaskType.h"

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
  SetReferrerPolicy(creation_params->referrer_policy);

  // https://drafts.css-houdini.org/worklets/#creating-a-workletglobalscope
  // Step 6: "Invoke the initialize a global object's CSP list algorithm given
  // workletGlobalScope."
  ApplyContentSecurityPolicyFromVector(
      *creation_params->content_security_policy_parsed_headers);
}

WorkletGlobalScope::~WorkletGlobalScope() = default;

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
    network::mojom::FetchCredentialsMode credentials_mode,
    scoped_refptr<WebTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsContextThread());
  if (!module_responses_map_proxy_) {
    // |kUnspecedLoading| is used here because this is a part of script module
    // loading and this usage is not explicitly spec'ed.
    module_responses_map_proxy_ = WorkletModuleResponsesMapProxy::Create(
        module_responses_map, outside_settings_task_runner,
        GetTaskRunner(TaskType::kUnspecedLoading));
  }

  // Step 1: "Let insideSettings be the workletGlobalScope's associated
  // environment settings object."
  // Step 2: "Let script by the result of fetch a worklet script given
  // moduleURLRecord, moduleResponsesMap, credentialOptions, outsideSettings,
  // and insideSettings when it asynchronously completes."

  Modulator* modulator = Modulator::From(ScriptController()->GetScriptState());

  // Step 3 to 5 are implemented in
  // WorkletModuleTreeClient::NotifyModuleTreeLoadFinished.
  WorkletModuleTreeClient* client = new WorkletModuleTreeClient(
      modulator, std::move(outside_settings_task_runner), pending_tasks);

  FetchModuleScript(module_url_record, credentials_mode, client);
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
  WorkerOrWorkletGlobalScope::Trace(visitor);
}

void WorkletGlobalScope::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  WorkerOrWorkletGlobalScope::TraceWrappers(visitor);
}

}  // namespace blink
