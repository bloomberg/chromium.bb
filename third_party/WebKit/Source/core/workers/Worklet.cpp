// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/Worklet.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/WebTaskRunner.h"
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

Worklet::Worklet(LocalFrame* frame)
    : ContextLifecycleObserver(frame->GetDocument()),
      module_responses_map_(new WorkletModuleResponsesMap) {
  DCHECK(IsMainThread());
}

Worklet::~Worklet() {
  for (const auto& proxy : proxies_)
    proxy->WorkletObjectDestroyed();
}

// Implementation of the first half of the "addModule(moduleURL, options)"
// algorithm:
// https://drafts.css-houdini.org/worklets/#dom-worklet-addmodule
ScriptPromise Worklet::addModule(ScriptState* script_state,
                                 const String& module_url,
                                 const WorkletOptions& options) {
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
      ->PostTask(
          BLINK_FROM_HERE,
          WTF::Bind(&Worklet::FetchAndInvokeScript, WrapPersistent(this),
                    module_url_record, options, WrapPersistent(resolver)));
  return promise;
}

void Worklet::ContextDestroyed(ExecutionContext* execution_context) {
  DCHECK(IsMainThread());
  for (const auto& proxy : proxies_)
    proxy->TerminateWorkletGlobalScope();
}

WorkletGlobalScopeProxy* Worklet::FindAvailableGlobalScope() const {
  DCHECK(IsMainThread());
  return proxies_.at(SelectGlobalScope());
}

// Implementation of the second half of the "addModule(moduleURL, options)"
// algorithm:
// https://drafts.css-houdini.org/worklets/#dom-worklet-addmodule
void Worklet::FetchAndInvokeScript(const KURL& module_url_record,
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
  WorkletModuleResponsesMap* module_responses_map = module_responses_map_;

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
    proxies_.push_back(CreateGlobalScope());

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
    proxy->FetchAndInvokeScript(module_url_record, module_responses_map,
                                credentials_mode, outside_settings_task_runner,
                                pending_tasks);
  }
}

size_t Worklet::SelectGlobalScope() const {
  DCHECK_EQ(GetNumberOfGlobalScopes(), 1u);
  return 0u;
}

DEFINE_TRACE(Worklet) {
  visitor->Trace(proxies_);
  visitor->Trace(module_responses_map_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
