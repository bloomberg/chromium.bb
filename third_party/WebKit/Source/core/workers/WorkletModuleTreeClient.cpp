// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletModuleTreeClient.h"

#include "core/dom/ModuleScript.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkletGlobalScope.h"
#include "platform/CrossThreadFunctional.h"

namespace blink {

WorkletModuleTreeClient::WorkletModuleTreeClient(
    Modulator* modulator,
    RefPtr<WebTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks)
    : modulator_(modulator),
      outside_settings_task_runner_(std::move(outside_settings_task_runner)),
      pending_tasks_(pending_tasks) {}

// Implementation of the second half of the "fetch and invoke a worklet script"
// algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
void WorkletModuleTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  if (!module_script) {
    // Step 3: "If script is null, then queue a task on outsideSettings's
    // responsible event loop to run these steps:"
    // The steps are implemented in WorkletPendingTasks::Abort().
    outside_settings_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&WorkletPendingTasks::Abort,
                        WrapCrossThreadPersistent(pending_tasks_.Get())));
    return;
  }

  // TODO(nhiroki): Call WorkerReportingProxy::WillEvaluateWorkerScript() or
  // something like that (e.g., WillEvaluateModuleScript()).
  // Step 4: "Run a module script given script."
  modulator_->ExecuteModule(module_script);
  WorkletGlobalScope* global_scope = ToWorkletGlobalScope(
      ExecutionContext::From(modulator_->GetScriptState()));
  global_scope->ReportingProxy().DidEvaluateModuleScript(
      !module_script->IsErrored());

  // Step 5: "Queue a task on outsideSettings's responsible event loop to run
  // these steps:"
  // The steps are implemented in WorkletPendingTasks::DecrementCounter().
  outside_settings_task_runner_->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkletPendingTasks::DecrementCounter,
                      WrapCrossThreadPersistent(pending_tasks_.Get())));
};

DEFINE_TRACE(WorkletModuleTreeClient) {
  visitor->Trace(modulator_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace blink
