// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorklet.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/WTF.h"

namespace blink {

ThreadedWorklet::ThreadedWorklet(LocalFrame* frame) : Worklet(frame) {}

// Implementation of the second half of the "addModule(moduleURL, options)"
// algorithm:
// https://drafts.css-houdini.org/worklets/#dom-worklet-addmodule
// TODO(nhiroki): MainThreadWorklet::FetchAndInvokeScript will be moved to the
// parent class (i.e., Worklet) and this function will be replaced with it.
// (https://crbug.com/727194)
void ThreadedWorklet::FetchAndInvokeScript(const KURL& module_url_record,
                                           const WorkletOptions& options,
                                           ScriptPromiseResolver* resolver) {
  DCHECK(IsMainThread());
  if (!GetExecutionContext())
    return;

  if (!IsInitialized())
    Initialize();

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
  // TODO(nhiroki): These steps will be supported after MainThreadWorklet and
  // ThreadedWorklet are merged into Worklet (https://crbug.com/727194)

  // Step 11: "Let pendingTaskStruct be a new pending tasks struct with counter
  // initialized to the length of worklet's WorkletGlobalScopes."
  const int kNumberOfGlobalScopes = 1;
  WorkletPendingTasks* pending_tasks =
      new WorkletPendingTasks(kNumberOfGlobalScopes, resolver);

  // Step 12: "For each workletGlobalScope in the worklet's
  // WorkletGlobalScopes, queue a task on the workletGlobalScope to fetch and
  // invoke a worklet script given workletGlobalScope, moduleURLRecord,
  // moduleResponsesMap, credentialOptions, outsideSettings, pendingTaskStruct,
  // and promise."
  // TODO(nhiroki): Queue a task instead of executing this here.
  GetWorkletGlobalScopeProxy()->FetchAndInvokeScript(
      module_url_record, credentials_mode,
      std::move(outside_settings_task_runner), pending_tasks);
}

bool ThreadedWorklet::NeedsToCreateGlobalScope() {
  NOTREACHED();
  return false;
}

WorkletGlobalScopeProxy* ThreadedWorklet::CreateGlobalScope() {
  NOTREACHED();
  return nullptr;
}

void ThreadedWorklet::ContextDestroyed(ExecutionContext* execution_context) {
  DCHECK(IsMainThread());
  if (IsInitialized())
    GetWorkletGlobalScopeProxy()->TerminateWorkletGlobalScope();
}

DEFINE_TRACE(ThreadedWorklet) {
  Worklet::Trace(visitor);
}

}  // namespace blink
