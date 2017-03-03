// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerOrWorkletGlobalScope.h"

#include "core/dom/ExecutionContextTask.h"
#include "core/frame/Deprecation.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "wtf/Functional.h"

namespace blink {

WorkerOrWorkletGlobalScope::WorkerOrWorkletGlobalScope()
    : m_deprecationWarningBits(UseCounter::NumberOfFeatures) {}

WorkerOrWorkletGlobalScope::~WorkerOrWorkletGlobalScope() {}

void WorkerOrWorkletGlobalScope::addDeprecationMessage(
    UseCounter::Feature feature) {
  DCHECK_NE(UseCounter::OBSOLETE_PageDestruction, feature);
  DCHECK_GT(UseCounter::NumberOfFeatures, feature);

  // For each deprecated feature, send console message at most once
  // per worker lifecycle.
  if (m_deprecationWarningBits.quickGet(feature))
    return;
  m_deprecationWarningBits.quickSet(feature);
  DCHECK(!Deprecation::deprecationMessage(feature).isEmpty());
  addConsoleMessage(
      ConsoleMessage::create(DeprecationMessageSource, WarningMessageLevel,
                             Deprecation::deprecationMessage(feature)));
}

void WorkerOrWorkletGlobalScope::postTask(
    TaskType,
    const WebTraceLocation& location,
    std::unique_ptr<ExecutionContextTask> task,
    const String& taskNameForInstrumentation) {
  if (!thread())
    return;

  bool isInstrumented = !taskNameForInstrumentation.isEmpty();
  if (isInstrumented) {
    probe::asyncTaskScheduled(this, "Worker task", task.get());
  }

  thread()->postTask(
      location, crossThreadBind(&WorkerOrWorkletGlobalScope::runTask,
                                wrapCrossThreadWeakPersistent(this),
                                WTF::passed(std::move(task)), isInstrumented));
}

void WorkerOrWorkletGlobalScope::runTask(
    std::unique_ptr<ExecutionContextTask> task,
    bool isInstrumented) {
  DCHECK(thread()->isCurrentThread());
  probe::AsyncTask asyncTask(this, task.get(), isInstrumented);
  task->performTask(this);
}

}  // namespace blink
