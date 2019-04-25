// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/worker_resource_timing_notifier_impl.h"

#include <memory>
#include "third_party/blink/public/platform/web_resource_timing_info.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/platform/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"

namespace blink {

namespace {

Performance* GetPerformance(ExecutionContext& execution_context) {
  DCHECK(execution_context.IsContextThread());
  if (execution_context.IsDocument()) {
    DCHECK(execution_context.ExecutingWindow());
    return DOMWindowPerformance::performance(
        *execution_context.ExecutingWindow());
  }
  if (execution_context.IsWorkerGlobalScope()) {
    return WorkerGlobalScopePerformance::performance(
        To<WorkerGlobalScope>(execution_context));
  }
  NOTREACHED() << "Unexpected execution context, it should be either Document "
                  "or WorkerGlobalScope";
  return nullptr;
}

}  // namespace

WorkerResourceTimingNotifierImpl::WorkerResourceTimingNotifierImpl() = default;

WorkerResourceTimingNotifierImpl::WorkerResourceTimingNotifierImpl(
    ExecutionContext& execution_context)
    : task_runner_(
          execution_context.GetTaskRunner(TaskType::kPerformanceTimeline)),
      execution_context_(execution_context) {
  DCHECK(task_runner_);
  DCHECK(execution_context_);
}

void WorkerResourceTimingNotifierImpl::AddResourceTiming(
    const WebResourceTimingInfo& info,
    const AtomicString& initiator_type) {
  DCHECK(task_runner_);
  if (task_runner_->RunsTasksInCurrentSequence()) {
    if (!execution_context_ || execution_context_->IsContextDestroyed())
      return;
    DCHECK(execution_context_->IsContextThread());
    GetPerformance(*execution_context_)
        ->AddResourceTiming(info, initiator_type);
  } else {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBind(
            &WorkerResourceTimingNotifierImpl::AddCrossThreadResourceTiming,
            WrapCrossThreadWeakPersistent(this), info,
            initiator_type.GetString()));
  }
}

void WorkerResourceTimingNotifierImpl::AddCrossThreadResourceTiming(
    const WebResourceTimingInfo& info,
    const String& initiator_type) {
  if (!execution_context_ || execution_context_->IsContextDestroyed())
    return;
  DCHECK(execution_context_->IsContextThread());
  GetPerformance(*execution_context_)
      ->AddResourceTiming(info, AtomicString(initiator_type));
}

}  // namespace blink
