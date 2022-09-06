// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/probe/async_task_context.h"
#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {
class AbortSignal;
class ScriptState;
class V8SchedulerPostTaskCallback;

// DOMTask represents a task scheduled via the web scheduling API. It will
// keep itself alive until DOMTask::Invoke is called, which may be after the
// callback's v8 context is invalid, in which case, the task will not be run.
class DOMTask final : public GarbageCollected<DOMTask> {
 public:
  DOMTask(ScriptPromiseResolver*,
          V8SchedulerPostTaskCallback*,
          AbortSignal*,
          DOMScheduler::DOMTaskQueue*,
          base::TimeDelta delay);

  virtual void Trace(Visitor*) const;

 private:
  // Entry point for running this DOMTask's |callback_|.
  void Invoke();
  // Internal step of Invoke that handles invoking the callback, including
  // catching any errors and retrieving the result.
  void InvokeInternal(ScriptState*);
  void OnAbort();

  void RecordTaskStartMetrics();

  TaskHandle task_handle_;
  Member<V8SchedulerPostTaskCallback> callback_;
  Member<ScriptPromiseResolver> resolver_;
  probe::AsyncTaskContext async_task_context_;
  Member<AbortSignal> signal_;
  // Do not remove. For dynamic priority task queues, |task_queue_| ensures that
  // the associated WebSchedulingTaskQueue stays alive until after this task
  // runs, which is necessary to ensure throttling works correctly.
  Member<DOMScheduler::DOMTaskQueue> task_queue_;
  const base::TimeTicks queue_time_;
  const base::TimeDelta delay_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_
