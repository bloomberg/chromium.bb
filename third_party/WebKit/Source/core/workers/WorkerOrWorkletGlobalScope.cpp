// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerOrWorkletGlobalScope.h"

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/Deprecation.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/Functional.h"

namespace blink {

WorkerOrWorkletGlobalScope::WorkerOrWorkletGlobalScope(
    v8::Isolate* isolate,
    WorkerClients* worker_clients)
    : worker_clients_(worker_clients),
      script_controller_(
          WorkerOrWorkletScriptController::Create(this, isolate)),
      used_features_(UseCounter::kNumberOfFeatures) {
  if (worker_clients_)
    worker_clients_->ReattachThread();
}

WorkerOrWorkletGlobalScope::~WorkerOrWorkletGlobalScope() = default;

void WorkerOrWorkletGlobalScope::CountFeature(UseCounter::Feature feature) {
  DCHECK_NE(UseCounter::kOBSOLETE_PageDestruction, feature);
  DCHECK_GT(UseCounter::kNumberOfFeatures, feature);
  if (used_features_.QuickGet(feature))
    return;
  used_features_.QuickSet(feature);
  ReportFeature(feature);
}

void WorkerOrWorkletGlobalScope::CountDeprecation(UseCounter::Feature feature) {
  DCHECK_NE(UseCounter::kOBSOLETE_PageDestruction, feature);
  DCHECK_GT(UseCounter::kNumberOfFeatures, feature);
  if (used_features_.QuickGet(feature))
    return;
  used_features_.QuickSet(feature);

  // Adds a deprecation message to the console.
  DCHECK(!Deprecation::DeprecationMessage(feature).IsEmpty());
  AddConsoleMessage(
      ConsoleMessage::Create(kDeprecationMessageSource, kWarningMessageLevel,
                             Deprecation::DeprecationMessage(feature)));

  ReportDeprecation(feature);
}

WorkerFetchContext* WorkerOrWorkletGlobalScope::GetFetchContext() {
  DCHECK(RuntimeEnabledFeatures::offMainThreadFetchEnabled());
  DCHECK(!IsMainThreadWorkletGlobalScope());
  if (fetch_context_)
    return fetch_context_;
  fetch_context_ = WorkerFetchContext::Create(*this);
  return fetch_context_;
}

bool WorkerOrWorkletGlobalScope::IsJSExecutionForbidden() const {
  return script_controller_->IsExecutionForbidden();
}

void WorkerOrWorkletGlobalScope::DisableEval(const String& error_message) {
  script_controller_->DisableEval(error_message);
}

void WorkerOrWorkletGlobalScope::PostTask(
    TaskType type,
    const WebTraceLocation& location,
    std::unique_ptr<ExecutionContextTask> task,
    const String& task_name_for_instrumentation) {
  if (!GetThread())
    return;

  bool is_instrumented = !task_name_for_instrumentation.IsEmpty();
  if (is_instrumented) {
    probe::AsyncTaskScheduled(this, "Worker task", task.get());
  }

  TaskRunnerHelper::Get(type, this)
      ->PostTask(location, CrossThreadBind(&WorkerOrWorkletGlobalScope::RunTask,
                                           WrapCrossThreadWeakPersistent(this),
                                           WTF::Passed(std::move(task)),
                                           is_instrumented));
}

bool WorkerOrWorkletGlobalScope::CanExecuteScripts(
    ReasonForCallingCanExecuteScripts) {
  return !IsJSExecutionForbidden();
}

void WorkerOrWorkletGlobalScope::Dispose() {
  DCHECK(script_controller_);
  script_controller_->Dispose();
  script_controller_.Clear();
}

DEFINE_TRACE(WorkerOrWorkletGlobalScope) {
  visitor->Trace(fetch_context_);
  visitor->Trace(script_controller_);
  ExecutionContext::Trace(visitor);
}

void WorkerOrWorkletGlobalScope::RunTask(
    std::unique_ptr<ExecutionContextTask> task,
    bool is_instrumented) {
  DCHECK(GetThread()->IsCurrentThread());
  probe::AsyncTask async_task(this, task.get(), nullptr, is_instrumented);
  task->PerformTask(this);
}

}  // namespace blink
