// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletModuleTreeClient.h"

#include "core/dom/ModuleScript.h"

namespace blink {

WorkletModuleTreeClient::WorkletModuleTreeClient(
    Modulator* modulator,
    WorkletPendingTasks* pending_tasks)
    : modulator_(modulator), pending_tasks_(pending_tasks) {}

// Implementation of the second half of the "fetch and invoke a worklet script"
// algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
void WorkletModuleTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  DCHECK(IsMainThread());
  if (!module_script) {
    // Step 3: "If script is null, then queue a task on outsideSettings's
    // responsible event loop to run these steps:"
    // The steps are implemented in WorkletPendingTasks::Abort().
    // TODO(nhiroki): Queue a task instead of executing this here.
    pending_tasks_->Abort();
    return;
  }

  // Step 4: "Run a module script given script."
  modulator_->ExecuteModule(module_script);

  // Step 5: "Queue a task on outsideSettings's responsible event loop to run
  // these steps:"
  // The steps are implemented in WorkletPendingTasks::DecrementCounter().
  // TODO(nhiroki): Queue a task instead of executing this here.
  pending_tasks_->DecrementCounter();
};

DEFINE_TRACE(WorkletModuleTreeClient) {
  visitor->Trace(modulator_);
  visitor->Trace(pending_tasks_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace blink
