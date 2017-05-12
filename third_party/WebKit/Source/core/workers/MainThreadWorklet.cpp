// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorklet.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/wtf/WTF.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

namespace {

WebURLRequest::FetchCredentialsMode ParseCredentialsOption(
    const String& credentials_option) {
  if (credentials_option == "omit")
    return WebURLRequest::kFetchCredentialsModeOmit;
  if (credentials_option == "same-origin")
    return WebURLRequest::kFetchCredentialsModeSameOrigin;
  if (credentials_option == "include")
    return WebURLRequest::kFetchCredentialsModeInclude;
  NOTREACHED();
  return WebURLRequest::kFetchCredentialsModeOmit;
}

}  // namespace

MainThreadWorklet::MainThreadWorklet(LocalFrame* frame) : Worklet(frame) {}

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
  // TODO(nhiroki): Queue a task instead of executing this here.
  GetWorkletGlobalScopeProxy()->FetchAndInvokeScript(
      module_url_record, credentials_mode, pending_tasks);
}

void MainThreadWorklet::ContextDestroyed(ExecutionContext* execution_context) {
  DCHECK(IsMainThread());
  GetWorkletGlobalScopeProxy()->TerminateWorkletGlobalScope();
}

DEFINE_TRACE(MainThreadWorklet) {
  Worklet::Trace(visitor);
}

}  // namespace blink
