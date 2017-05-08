// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorkletGlobalScope.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/probe/CoreProbes.h"

namespace blink {

MainThreadWorkletGlobalScope::MainThreadWorkletGlobalScope(
    LocalFrame* frame,
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate)
    : WorkletGlobalScope(url, user_agent, std::move(security_origin), isolate),
      ContextClient(frame) {}

MainThreadWorkletGlobalScope::~MainThreadWorkletGlobalScope() {}

void MainThreadWorkletGlobalScope::ReportFeature(UseCounter::Feature feature) {
  DCHECK(IsMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  UseCounter::Count(GetFrame(), feature);
}

void MainThreadWorkletGlobalScope::ReportDeprecation(
    UseCounter::Feature feature) {
  DCHECK(IsMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  Deprecation::CountDeprecation(GetFrame(), feature);
}

WorkerThread* MainThreadWorkletGlobalScope::GetThread() const {
  NOTREACHED();
  return nullptr;
}

// Implementation of the first half of the "fetch and invoke a worklet script"
// algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
void MainThreadWorkletGlobalScope::FetchAndInvokeScript(
    const KURL& module_url_record,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  // Step 1: "Let insideSettings be the workletGlobalScope's associated
  // environment settings object."
  // Step 2: "Let script by the result of fetch a worklet script given
  // moduleURLRecord, moduleResponsesMap, credentialOptions, outsideSettings,
  // and insideSettings when it asynchronously completes."
  // TODO(nhiroki): Replace this with module script loading.
  WorkletScriptLoader* script_loader =
      WorkletScriptLoader::Create(GetFrame()->GetDocument()->Fetcher(), this);
  loader_map_.Set(script_loader, pending_tasks);
  script_loader->FetchScript(module_url_record);
}

// TODO(nhiroki): Add tests for termination.
void MainThreadWorkletGlobalScope::Terminate() {
  for (auto it = loader_map_.begin(); it != loader_map_.end();) {
    WorkletScriptLoader* script_loader = it->key;
    // Cancel() eventually calls NotifyWorkletScriptLoadingFinished() and
    // removes |it| from |loader_map_|, so increment it in advance.
    ++it;
    script_loader->Cancel();
  }
  Dispose();
}

// Implementation of the second half of the "fetch and invoke a worklet script"
// algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
void MainThreadWorkletGlobalScope::NotifyWorkletScriptLoadingFinished(
    WorkletScriptLoader* script_loader,
    const ScriptSourceCode& source_code) {
  DCHECK(IsMainThread());
  auto it = loader_map_.find(script_loader);
  DCHECK(it != loader_map_.end());
  WorkletPendingTasks* pending_tasks = it->value;
  loader_map_.erase(it);

  if (!script_loader->WasScriptLoadSuccessful()) {
    // Step 3: "If script is null, then queue a task on outsideSettings's
    // responsible event loop to run these steps:"
    // The steps are implemented in WorkletPendingTasks::Abort().
    // TODO(nhiroki): Queue a task instead of executing this here.
    pending_tasks->Abort();
    return;
  }

  // Step 4: "Run a module script given script."
  ScriptController()->Evaluate(source_code);

  // Step 5: "Queue a task on outsideSettings's responsible event loop to run
  // these steps:"
  // The steps are implemented in WorkletPendingTasks::DecrementCounter().
  // TODO(nhiroki): Queue a task instead of executing this here.
  pending_tasks->DecrementCounter();
}

void MainThreadWorkletGlobalScope::AddConsoleMessage(
    ConsoleMessage* console_message) {
  GetFrame()->Console().AddMessage(console_message);
}

void MainThreadWorkletGlobalScope::ExceptionThrown(ErrorEvent* event) {
  MainThreadDebugger::Instance()->ExceptionThrown(this, event);
}

CoreProbeSink* MainThreadWorkletGlobalScope::GetProbeSink() {
  return probe::ToCoreProbeSink(GetFrame());
}

DEFINE_TRACE(MainThreadWorkletGlobalScope) {
  visitor->Trace(loader_map_);
  WorkletGlobalScope::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
