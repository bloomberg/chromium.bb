// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorkletGlobalScope.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/Modulator.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/WorkletModuleResponsesMap.h"
#include "core/workers/WorkletModuleTreeClient.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

MainThreadWorkletGlobalScope::MainThreadWorkletGlobalScope(
    LocalFrame* frame,
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate)
    : WorkletGlobalScope(url,
                         user_agent,
                         std::move(security_origin),
                         isolate,
                         nullptr /* worker_clients */),
      ContextClient(frame) {}

MainThreadWorkletGlobalScope::~MainThreadWorkletGlobalScope() {}

void MainThreadWorkletGlobalScope::ReportFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  UseCounter::Count(GetFrame(), feature);
}

void MainThreadWorkletGlobalScope::ReportDeprecation(WebFeature feature) {
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
    WorkletModuleResponsesMap* module_responses_map,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    RefPtr<WebTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  if (!module_responses_map_proxy_) {
    // |kUnspecedLoading| is used here because this is a part of script module
    // loading and this usage is not explicitly spec'ed.
    module_responses_map_proxy_ = WorkletModuleResponsesMapProxy::Create(
        module_responses_map, outside_settings_task_runner,
        TaskRunnerHelper::Get(TaskType::kUnspecedLoading, this));
  }

  // Step 1: "Let insideSettings be the workletGlobalScope's associated
  // environment settings object."
  // Step 2: "Let script by the result of fetch a worklet script given
  // moduleURLRecord, moduleResponsesMap, credentialOptions, outsideSettings,
  // and insideSettings when it asynchronously completes."
  String nonce = "";
  ParserDisposition parser_state = kNotParserInserted;
  Modulator* modulator = Modulator::From(ScriptController()->GetScriptState());
  ModuleScriptFetchRequest module_request(module_url_record, nonce,
                                          parser_state, credentials_mode);

  // Step 3 to 5 are implemented in
  // WorkletModuleTreeClient::NotifyModuleTreeLoadFinished.
  WorkletModuleTreeClient* client = new WorkletModuleTreeClient(
      modulator, std::move(outside_settings_task_runner), pending_tasks);
  modulator->FetchTree(module_request, client);
}

// TODO(nhiroki): Add tests for termination.
void MainThreadWorkletGlobalScope::Terminate() {
  Dispose();
}

WorkletModuleResponsesMapProxy*
MainThreadWorkletGlobalScope::GetModuleResponsesMapProxy() const {
  DCHECK(module_responses_map_proxy_);
  return module_responses_map_proxy_;
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
  visitor->Trace(module_responses_map_proxy_);
  WorkletGlobalScope::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
