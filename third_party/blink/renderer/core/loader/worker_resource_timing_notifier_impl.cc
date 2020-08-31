// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/worker_resource_timing_notifier_impl.h"

#include <memory>
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/cross_thread_resource_timing_info_copier.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

Performance* GetPerformance(ExecutionContext& execution_context) {
  DCHECK(execution_context.IsContextThread());
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context))
    return DOMWindowPerformance::performance(*window);
  if (auto* global_scope = DynamicTo<WorkerGlobalScope>(execution_context))
    return WorkerGlobalScopePerformance::performance(*global_scope);
  NOTREACHED() << "Unexpected execution context, it should be either Window "
                  "or WorkerGlobalScope";
  return nullptr;
}

}  // namespace

// static
WorkerResourceTimingNotifierImpl*
WorkerResourceTimingNotifierImpl::CreateForInsideResourceFetcher(
    ExecutionContext& execution_context) {
  auto* notifier = MakeGarbageCollected<WorkerResourceTimingNotifierImpl>(
      execution_context.GetTaskRunner(TaskType::kPerformanceTimeline));
  notifier->inside_execution_context_ = &execution_context;
  return notifier;
}

// static
WorkerResourceTimingNotifierImpl*
WorkerResourceTimingNotifierImpl::CreateForOutsideResourceFetcher(
    ExecutionContext& execution_context) {
  auto* notifier = MakeGarbageCollected<WorkerResourceTimingNotifierImpl>(
      execution_context.GetTaskRunner(TaskType::kPerformanceTimeline));
  notifier->outside_execution_context_ = &execution_context;
  return notifier;
}

WorkerResourceTimingNotifierImpl::WorkerResourceTimingNotifierImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_);
}

void WorkerResourceTimingNotifierImpl::AddResourceTiming(
    mojom::blink::ResourceTimingInfoPtr info,
    const AtomicString& initiator_type,
    mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
        worker_timing_receiver) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    DCHECK(inside_execution_context_);
    if (inside_execution_context_->IsContextDestroyed())
      return;
    DCHECK(inside_execution_context_->IsContextThread());
    GetPerformance(*inside_execution_context_)
        ->AddResourceTiming(std::move(info), initiator_type,
                            std::move(worker_timing_receiver));
  } else {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &WorkerResourceTimingNotifierImpl::AddCrossThreadResourceTiming,
            WrapCrossThreadWeakPersistent(this), std::move(info),
            initiator_type.GetString(), std::move(worker_timing_receiver)));
  }
}

void WorkerResourceTimingNotifierImpl::AddCrossThreadResourceTiming(
    mojom::blink::ResourceTimingInfoPtr info,
    const String& initiator_type,
    mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
        worker_timing_receiver) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!outside_execution_context_ ||
      outside_execution_context_->IsContextDestroyed())
    return;
  DCHECK(outside_execution_context_->IsContextThread());
  GetPerformance(*outside_execution_context_)
      ->AddResourceTiming(std::move(info), AtomicString(initiator_type),
                          std::move(worker_timing_receiver));
}

void WorkerResourceTimingNotifierImpl::Trace(Visitor* visitor) {
  visitor->Trace(inside_execution_context_);
  WorkerResourceTimingNotifier::Trace(visitor);
}

}  // namespace blink
