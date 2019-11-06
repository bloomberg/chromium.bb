// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/task_queue.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/modules/scheduler/scheduler.h"
#include "third_party/blink/renderer/modules/scheduler/task.h"
#include "third_party/blink/renderer/modules/scheduler/task_queue_post_task_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

const AtomicString& ImmediatePriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, immediate_priority, ("immediate"));
  return immediate_priority;
}

const AtomicString& HighPriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, high_priority, ("high"));
  return high_priority;
}

const AtomicString& DefaultPriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, default_priority, ("default"));
  return default_priority;
}

const AtomicString& LowPriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, low_priority, ("low"));
  return low_priority;
}

const AtomicString& IdlePriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, idle_priority, ("idle"));
  return idle_priority;
}

}  // namespace

TaskQueue::TaskQueue(Document* document,
                     WebSchedulingPriority priority,
                     Scheduler* scheduler)
    : ContextLifecycleObserver(document),
      priority_(priority),
      web_scheduling_task_queue_(document->GetScheduler()
                                     ->ToFrameScheduler()
                                     ->CreateWebSchedulingTaskQueue(priority)),
      task_runner_(web_scheduling_task_queue_->GetTaskRunner()),
      scheduler_(scheduler) {
  DCHECK(!document->IsContextDestroyed());
}

void TaskQueue::Trace(Visitor* visitor) {
  visitor->Trace(scheduler_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void TaskQueue::ContextDestroyed(ExecutionContext* context) {
  web_scheduling_task_queue_.reset(nullptr);
  task_runner_.reset();
}

AtomicString TaskQueue::priority() const {
  return WebSchedulingPriorityToString(priority_);
}

Task* TaskQueue::postTask(V8Function* function,
                          TaskQueuePostTaskOptions* options,
                          const Vector<ScriptValue>& args) {
  // |task_runner_| will be nullptr when the context is destroyed, which
  // prevents us from scheduling tasks for detached documents.
  if (!task_runner_)
    return nullptr;

  // TODO(shaseley): We need to figure out the behavior we want for delay. For
  // now, we use behavior that is very similar to setTimeout: negative delays
  // are treated as 0, and we use the Blink scheduler's delayed task behavior.
  // We don't, however, adjust the timeout based on nested calls (yet) or clamp
  // the value to a minimal delay.
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(
      options->delay() > 0 ? options->delay() : 0);

  // For global task queues, we don't need to track the task objects separately;
  // tracking is handled by the |web_scheduling_task_queue_|.
  return MakeGarbageCollected<Task>(this, GetExecutionContext(), function, args,
                                    delay);
}

void TaskQueue::take(Task* task) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;
  task->MoveTo(this);
}

// static
AtomicString TaskQueue::WebSchedulingPriorityToString(
    WebSchedulingPriority priority) {
  switch (priority) {
    case WebSchedulingPriority::kImmediatePriority:
      return ImmediatePriorityKeyword();
    case WebSchedulingPriority::kHighPriority:
      return HighPriorityKeyword();
    case WebSchedulingPriority::kDefaultPriority:
      return DefaultPriorityKeyword();
    case WebSchedulingPriority::kLowPriority:
      return LowPriorityKeyword();
    case WebSchedulingPriority::kIdlePriority:
      return IdlePriorityKeyword();
  }

  NOTREACHED();
  return g_empty_atom;
}

// static
WebSchedulingPriority TaskQueue::WebSchedulingPriorityFromString(
    AtomicString priority) {
  if (priority == ImmediatePriorityKeyword())
    return WebSchedulingPriority::kImmediatePriority;
  if (priority == HighPriorityKeyword())
    return WebSchedulingPriority::kHighPriority;
  if (priority == DefaultPriorityKeyword())
    return WebSchedulingPriority::kDefaultPriority;
  if (priority == LowPriorityKeyword())
    return WebSchedulingPriority::kLowPriority;
  if (priority == IdlePriorityKeyword())
    return WebSchedulingPriority::kIdlePriority;

  NOTREACHED();
  return WebSchedulingPriority::kDefaultPriority;
}

}  // namespace blink
