// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletModuleTreeClient.h"

#include "core/script/ModuleScript.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkletGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "public/platform/TaskType.h"

namespace blink {

WorkletModuleTreeClient::WorkletModuleTreeClient(
    Modulator* modulator,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
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
    PostCrossThreadTask(
        *outside_settings_task_runner_, FROM_HERE,
        CrossThreadBind(&WorkletPendingTasks::Abort,
                        WrapCrossThreadPersistent(pending_tasks_.Get())));
    return;
  }

  // "Note: Specifically, if a script fails to parse or fails to load over the
  // network, it will reject the promise. If the script throws an error while
  // first evaluating the promise it will resolve as classes may have been
  // registered correctly."
  // https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
  //
  // When a network failure happens, |module_script| should be nullptr and the
  // case should already be handled above.
  //
  // Check whether a syntax error happens.
  if (module_script->HasErrorToRethrow()) {
    PostCrossThreadTask(
        *outside_settings_task_runner_, FROM_HERE,
        CrossThreadBind(&WorkletPendingTasks::Abort,
                        WrapCrossThreadPersistent(pending_tasks_.Get())));
    return;
  }

  // TODO(nhiroki): Call WorkerReportingProxy::WillEvaluateWorkerScript() or
  // something like that (e.g., WillEvaluateModuleScript()).
  // Step 4: "Run a module script given script."
  ScriptValue error = modulator_->ExecuteModule(
      module_script, Modulator::CaptureEvalErrorFlag::kReport);

  WorkletGlobalScope* global_scope = ToWorkletGlobalScope(
      ExecutionContext::From(modulator_->GetScriptState()));

  global_scope->ReportingProxy().DidEvaluateModuleScript(error.IsEmpty());

  // Step 5: "Queue a task on outsideSettings's responsible event loop to run
  // these steps:"
  // The steps are implemented in WorkletPendingTasks::DecrementCounter().
  PostCrossThreadTask(
      *outside_settings_task_runner_, FROM_HERE,
      CrossThreadBind(&WorkletPendingTasks::DecrementCounter,
                      WrapCrossThreadPersistent(pending_tasks_.Get())));
};

void WorkletModuleTreeClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(modulator_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace blink
