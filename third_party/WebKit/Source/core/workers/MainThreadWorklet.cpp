// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorklet.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/WTF.h"

namespace blink {

MainThreadWorklet::MainThreadWorklet(LocalFrame* frame) : Worklet(frame) {}

WorkletGlobalScopeProxy* MainThreadWorklet::FindAvailableGlobalScope() const {
  DCHECK(IsMainThread());
  // TODO(nhiroki): Support the case where there are multiple global scopes.
  DCHECK_EQ(1u, GetNumberOfGlobalScopes());
  return proxies_.begin()->get();
}

// Implementation of the second half of the "addModule(moduleURL, options)"
// algorithm:
// https://drafts.css-houdini.org/worklets/#dom-worklet-addmodule
void MainThreadWorklet::FetchAndInvokeScript(const KURL& module_url_record,
                                             const WorkletOptions& options,
                                             ScriptPromiseResolver* resolver) {
  DCHECK(IsMainThread());
  if (!GetExecutionContext())
    return;

  // Step 6: "Let credentialOptions be the credentials member of options."
  // TODO(nhiroki): Add tests for credentialOptions (https://crbug.com/710837).
  WebURLRequest::FetchCredentialsMode credentials_mode =
      ParseCredentialsOption(options.credentials());

  // Step 7: "Let outsideSettings be the relevant settings object of this."
  // In the specification, outsideSettings is used for posting a task to the
  // document's responsible event loop. In our implementation, we use the
  // document's UnspecedLoading task runner as that is what we commonly use for
  // module loading.
  RefPtr<WebTaskRunner> outside_settings_task_runner =
      TaskRunnerHelper::Get(TaskType::kUnspecedLoading, GetExecutionContext());

  // Step 8: "Let moduleResponsesMap be worklet's module responses map."
  // TODO(nhiroki): Implement moduleResponsesMap (https://crbug.com/627945).

  // Step 9: "Let workletGlobalScopeType be worklet's worklet global scope
  // type."
  // workletGlobalScopeType is encoded into the class name (e.g., PaintWorklet).

  // Step 10: "If the worklet's WorkletGlobalScopes is empty, run the following
  // steps:"
  //   10.1: "Create a WorkletGlobalScope given workletGlobalScopeType,
  //          moduleResponsesMap, and outsideSettings."
  //   10.2: "Add the WorkletGlobalScope to worklet's WorkletGlobalScopes."
  // "Depending on the type of worklet the user agent may create additional
  // WorkletGlobalScopes at this time."
  while (NeedsToCreateGlobalScope())
    proxies_.insert(CreateGlobalScope());
  DCHECK_EQ(1u, GetNumberOfGlobalScopes());

  // Step 11: "Let pendingTaskStruct be a new pending tasks struct with counter
  // initialized to the length of worklet's WorkletGlobalScopes."
  WorkletPendingTasks* pending_tasks =
      new WorkletPendingTasks(GetNumberOfGlobalScopes(), resolver);

  // Step 12: "For each workletGlobalScope in the worklet's
  // WorkletGlobalScopes, queue a task on the workletGlobalScope to fetch and
  // invoke a worklet script given workletGlobalScope, moduleURLRecord,
  // moduleResponsesMap, credentialOptions, outsideSettings, pendingTaskStruct,
  // and promise."
  // TODO(nhiroki): Queue a task instead of executing this here.
  for (const auto& proxy : proxies_) {
    proxy->FetchAndInvokeScript(module_url_record, credentials_mode,
                                outside_settings_task_runner, pending_tasks);
  }
}

void MainThreadWorklet::ContextDestroyed(ExecutionContext* execution_context) {
  DCHECK(IsMainThread());
  for (const auto& proxy : proxies_)
    proxy->TerminateWorkletGlobalScope();
}

DEFINE_TRACE(MainThreadWorklet) {
  Worklet::Trace(visitor);
}

}  // namespace blink
