// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_scheduler_post_task_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_task_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"

namespace blink {

const char DOMScheduler::kSupplementName[] = "DOMScheduler";

DOMScheduler* DOMScheduler::scheduler(ExecutionContext& context) {
  DOMScheduler* scheduler =
      Supplement<ExecutionContext>::From<DOMScheduler>(context);
  if (!scheduler) {
    scheduler = MakeGarbageCollected<DOMScheduler>(&context);
    Supplement<ExecutionContext>::ProvideTo(context, scheduler);
  }
  return scheduler;
}

DOMScheduler::DOMScheduler(ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      Supplement<ExecutionContext>(*context) {
  if (context->IsContextDestroyed())
    return;
  DCHECK(context->GetScheduler());
  CreateFixedPriorityTaskQueues(context);
}

void DOMScheduler::ContextDestroyed() {
  fixed_priority_task_queues_.clear();
  signal_to_task_queue_map_.clear();
}

void DOMScheduler::Trace(Visitor* visitor) const {
  visitor->Trace(fixed_priority_task_queues_);
  visitor->Trace(signal_to_task_queue_map_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  Supplement<ExecutionContext>::Trace(visitor);
}

ScriptPromise DOMScheduler::postTask(
    ScriptState* script_state,
    V8SchedulerPostTaskCallback* callback_function,
    SchedulerPostTaskOptions* options,
    ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    // The bindings layer implicitly converts thrown exceptions in
    // promise-returning functions to promise rejections.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current window is detached");
    return ScriptPromise();
  }
  if (options->hasSignal() && options->signal()->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "The task was aborted");
    return ScriptPromise();
  }

  // Always honor the priority and the task signal if given.
  DOMTaskQueue* task_queue;
  AbortSignal* signal = options->hasSignal() ? options->signal() : nullptr;
  if (!options->hasPriority() && signal && IsA<DOMTaskSignal>(signal)) {
    // If only a signal is given, and it is a TaskSignal rather than an
    // basic AbortSignal, use it.
    DOMTaskSignal* task_signal = To<DOMTaskSignal>(signal);

    // If we haven't seen this `TaskSignal` before, create a task queue for it.
    if (!signal_to_task_queue_map_.Contains(task_signal))
      CreateTaskQueueFor(task_signal);
    task_queue = signal_to_task_queue_map_.at(task_signal);
  } else {
    // Otherwise, use the appropriate task queue from
    // |fixed_priority_task_queues_|.
    WebSchedulingPriority priority =
        options->hasPriority() ? WebSchedulingPriorityFromString(AtomicString(
                                     IDLEnumAsString(options->priority())))
                               : kDefaultPriority;
    task_queue = fixed_priority_task_queues_[static_cast<int>(priority)];
  }

  DCHECK(task_queue);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  MakeGarbageCollected<DOMTask>(resolver, callback_function, signal, task_queue,
                                base::Milliseconds(options->delay()));
  return resolver->Promise();
}

void DOMScheduler::CreateFixedPriorityTaskQueues(ExecutionContext* context) {
  FrameOrWorkerScheduler* scheduler = context->GetScheduler();
  for (size_t i = 0; i < kWebSchedulingPriorityCount; i++) {
    auto priority = static_cast<WebSchedulingPriority>(i);
    std::unique_ptr<WebSchedulingTaskQueue> task_queue =
        scheduler->CreateWebSchedulingTaskQueue(priority);
    fixed_priority_task_queues_.push_back(
        MakeGarbageCollected<DOMTaskQueue>(std::move(task_queue), priority));
  }
}

void DOMScheduler::CreateTaskQueueFor(DOMTaskSignal* signal) {
  FrameOrWorkerScheduler* scheduler = GetExecutionContext()->GetScheduler();
  DCHECK(scheduler);
  WebSchedulingPriority priority =
      WebSchedulingPriorityFromString(signal->priority());
  std::unique_ptr<WebSchedulingTaskQueue> task_queue =
      scheduler->CreateWebSchedulingTaskQueue(priority);
  signal_to_task_queue_map_.insert(
      signal,
      MakeGarbageCollected<DOMTaskQueue>(std::move(task_queue), priority));
  signal->AddPriorityChangeAlgorithm(WTF::Bind(&DOMScheduler::OnPriorityChange,
                                               WrapWeakPersistent(this),
                                               WrapWeakPersistent(signal)));
}

void DOMScheduler::OnPriorityChange(DOMTaskSignal* signal) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;
  DCHECK(signal);
  DCHECK(signal_to_task_queue_map_.Contains(signal));
  DOMTaskQueue* task_queue = signal_to_task_queue_map_.at(signal);
  task_queue->SetPriority(WebSchedulingPriorityFromString(signal->priority()));
}

DOMScheduler::DOMTaskQueue::DOMTaskQueue(
    std::unique_ptr<WebSchedulingTaskQueue> task_queue,
    WebSchedulingPriority priority)
    : web_scheduling_task_queue_(std::move(task_queue)),
      task_runner_(web_scheduling_task_queue_->GetTaskRunner()),
      priority_(priority) {
  DCHECK(task_runner_);
}

void DOMScheduler::DOMTaskQueue::SetPriority(WebSchedulingPriority priority) {
  if (priority_ == priority)
    return;
  web_scheduling_task_queue_->SetPriority(priority);
  priority_ = priority;
}

DOMScheduler::DOMTaskQueue::~DOMTaskQueue() = default;

}  // namespace blink
