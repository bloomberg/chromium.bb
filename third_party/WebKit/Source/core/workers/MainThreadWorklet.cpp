// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorklet.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/wtf/WTF.h"

namespace blink {

MainThreadWorklet::MainThreadWorklet(LocalFrame* frame) : Worklet(frame) {}

// Implementation of the first half of the "addModule(moduleURL, options)"
// algorithm:
// https://drafts.css-houdini.org/worklets/#dom-worklet-addmodule
ScriptPromise MainThreadWorklet::addModule(ScriptState* script_state,
                                           const String& module_url) {
  DCHECK(IsMainThread());
  if (!GetExecutionContext()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError,
                                           "This frame is already detached"));
  }

  // Step 1: "Let promise be a new promise."
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // Step 2: "Let worklet be the current Worklet."
  // |this| is the current Worklet.

  // Step 3: "Let moduleURLRecord be the result of parsing the moduleURL
  // argument relative to the relevant settings object of this."
  KURL module_url_record = GetExecutionContext()->CompleteURL(module_url);

  // Step 4: "If moduleURLRecord is failure, then reject promise with a
  // "SyntaxError" DOMException and return promise."
  if (!module_url_record.IsValid()) {
    resolver->Reject(DOMException::Create(
        kSyntaxError, "'" + module_url + "' is not a valid URL."));
    return promise;
  }

  // Step 5: "Return promise, and then continue running this algorithm in
  // parallel."
  // |kUnspecedLoading| is used here because this is a part of script module
  // loading.
  TaskRunnerHelper::Get(TaskType::kUnspecedLoading, script_state)
      ->PostTask(BLINK_FROM_HERE,
                 WTF::Bind(&MainThreadWorklet::FetchAndInvokeScript,
                           WrapPersistent(this), module_url_record,
                           WrapPersistent(resolver)));
  return promise;
}

// Implementation of the second half of the "addModule(moduleURL, options)"
// algorithm:
// https://drafts.css-houdini.org/worklets/#dom-worklet-addmodule
void MainThreadWorklet::FetchAndInvokeScript(const KURL& module_url_record,
                                             ScriptPromiseResolver* resolver) {
  DCHECK(IsMainThread());
  if (!GetExecutionContext())
    return;

  // Step 6: "Let credentialOptions be the credentials member of options."
  // TODO(nhiroki): Implement credentialOptions (https://crbug.com/710837).

  // Step 7: "Let outsideSettings be the relevant settings object of this."
  // TODO(nhiroki): outsideSettings will be used for posting a task to the
  // document's responsible event loop. We could use a task runner for the
  // purpose.

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
  // TODO(nhiroki): Create WorkletGlobalScopes at this point.

  // Step 11: "Let pendingTaskStruct be a new pending tasks struct with counter
  // initialized to the length of worklet's WorkletGlobalScopes."
  // TODO(nhiroki): Introduce the concept of "worklet's WorkletGlobalScopes" and
  // use the length of it here.
  constexpr int number_of_global_scopes = 1;
  WorkletPendingTasks* pending_tasks =
      new WorkletPendingTasks(number_of_global_scopes, resolver);

  // Step 12: "For each workletGlobalScope in the worklet's
  // WorkletGlobalScopes, queue a task on the workletGlobalScope to fetch and
  // invoke a worklet script given workletGlobalScope, moduleURLRecord,
  // moduleResponsesMap, credentialOptions, outsideSettings, pendingTaskStruct,
  // and promise."
  // TODO(nhiroki): Pass the remaining parameters (e.g., credentialOptions).
  // TODO(nhiroki): Queue a task instead of executing this here.
  GetWorkletGlobalScopeProxy()->FetchAndInvokeScript(module_url_record,
                                                     pending_tasks);
}

void MainThreadWorklet::ContextDestroyed(ExecutionContext* execution_context) {
  DCHECK(IsMainThread());
  GetWorkletGlobalScopeProxy()->TerminateWorkletGlobalScope();
  Worklet::ContextDestroyed(execution_context);
}

DEFINE_TRACE(MainThreadWorklet) {
  Worklet::Trace(visitor);
}

}  // namespace blink
