// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerOrWorkletGlobalScope.h"

#include "core/dom/ExecutionContextTask.h"
#include "core/frame/Deprecation.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "wtf/Functional.h"

namespace blink {

WorkerOrWorkletGlobalScope::WorkerOrWorkletGlobalScope()
    : deprecation_warning_bits_(UseCounter::kNumberOfFeatures) {}

WorkerOrWorkletGlobalScope::~WorkerOrWorkletGlobalScope() {}

void WorkerOrWorkletGlobalScope::AddDeprecationMessage(
    UseCounter::Feature feature) {
  DCHECK_NE(UseCounter::kOBSOLETE_PageDestruction, feature);
  DCHECK_GT(UseCounter::kNumberOfFeatures, feature);

  // For each deprecated feature, send console message at most once
  // per worker lifecycle.
  if (deprecation_warning_bits_.QuickGet(feature))
    return;
  deprecation_warning_bits_.QuickSet(feature);
  DCHECK(!Deprecation::DeprecationMessage(feature).IsEmpty());
  AddConsoleMessage(
      ConsoleMessage::Create(kDeprecationMessageSource, kWarningMessageLevel,
                             Deprecation::DeprecationMessage(feature)));
}

void WorkerOrWorkletGlobalScope::PostTask(
    TaskType,
    const WebTraceLocation& location,
    std::unique_ptr<ExecutionContextTask> task,
    const String& task_name_for_instrumentation) {
  if (!GetThread())
    return;

  bool is_instrumented = !task_name_for_instrumentation.IsEmpty();
  if (is_instrumented) {
    probe::AsyncTaskScheduled(this, "Worker task", task.get());
  }

  GetThread()->PostTask(
      location, CrossThreadBind(&WorkerOrWorkletGlobalScope::RunTask,
                                WrapCrossThreadWeakPersistent(this),
                                WTF::Passed(std::move(task)), is_instrumented));
}

void WorkerOrWorkletGlobalScope::RunTask(
    std::unique_ptr<ExecutionContextTask> task,
    bool is_instrumented) {
  DCHECK(GetThread()->IsCurrentThread());
  probe::AsyncTask async_task(this, task.get(), nullptr, is_instrumented);
  task->PerformTask(this);
}

}  // namespace blink
