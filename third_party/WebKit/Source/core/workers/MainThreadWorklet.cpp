// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorklet.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/wtf/WTF.h"

namespace blink {

MainThreadWorklet::MainThreadWorklet(LocalFrame* frame) : Worklet(frame) {}

// Implementation of the "addModule(moduleURL, options)" algorithm:
// https://drafts.css-houdini.org/worklets/#dom-worklet-addmodule
ScriptPromise MainThreadWorklet::addModule(ScriptState* script_state,
                                           const String& module_url) {
  DCHECK(IsMainThread());
  if (!GetExecutionContext()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError,
                                           "This frame is already detached"));
  }

  KURL module_url_record = GetExecutionContext()->CompleteURL(module_url);
  if (!module_url_record.IsValid()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kSyntaxError,
                             "'" + module_url + "' is not a valid URL."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

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
  return promise;
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
