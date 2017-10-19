// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptedIdleTaskController.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/IdleRequestOptions.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/probe/CoreProbes.h"
#include "platform/Histogram.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/RefCounted.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

namespace internal {

class IdleRequestCallbackWrapper
    : public RefCounted<IdleRequestCallbackWrapper> {
 public:
  static RefPtr<IdleRequestCallbackWrapper> Create(
      ScriptedIdleTaskController::CallbackId id,
      ScriptedIdleTaskController* controller) {
    return WTF::AdoptRef(new IdleRequestCallbackWrapper(id, controller));
  }
  virtual ~IdleRequestCallbackWrapper() {}

  static void IdleTaskFired(RefPtr<IdleRequestCallbackWrapper> callback_wrapper,
                            double deadline_seconds) {
    // TODO(rmcilroy): Implement clamping of deadline in some form.
    if (ScriptedIdleTaskController* controller =
            callback_wrapper->Controller()) {
      // If we are going to yield immediately, reschedule the callback for
      // later.
      if (Platform::Current()
              ->CurrentThread()
              ->Scheduler()
              ->ShouldYieldForHighPriorityWork()) {
        controller->ScheduleCallback(std::move(callback_wrapper),
                                     /* timeout_millis */ 0);
        return;
      }
      controller->CallbackFired(callback_wrapper->Id(), deadline_seconds,
                                IdleDeadline::CallbackType::kCalledWhenIdle);
    }
    callback_wrapper->Cancel();
  }

  static void TimeoutFired(
      RefPtr<IdleRequestCallbackWrapper> callback_wrapper) {
    if (ScriptedIdleTaskController* controller =
            callback_wrapper->Controller()) {
      controller->CallbackFired(callback_wrapper->Id(),
                                MonotonicallyIncreasingTime(),
                                IdleDeadline::CallbackType::kCalledByTimeout);
    }
    callback_wrapper->Cancel();
  }

  void Cancel() { controller_ = nullptr; }

  ScriptedIdleTaskController::CallbackId Id() const { return id_; }
  ScriptedIdleTaskController* Controller() const { return controller_; }

 private:
  IdleRequestCallbackWrapper(ScriptedIdleTaskController::CallbackId id,
                             ScriptedIdleTaskController* controller)
      : id_(id), controller_(controller) {}

  ScriptedIdleTaskController::CallbackId id_;
  WeakPersistent<ScriptedIdleTaskController> controller_;
};

}  // namespace internal

ScriptedIdleTaskController::V8IdleTask::V8IdleTask(
    V8IdleRequestCallback* callback)
    : callback_(callback) {}

void ScriptedIdleTaskController::V8IdleTask::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_);
  ScriptedIdleTaskController::IdleTask::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(ScriptedIdleTaskController::V8IdleTask) {
  visitor->TraceWrappers(callback_);
  ScriptedIdleTaskController::IdleTask::TraceWrappers(visitor);
}

void ScriptedIdleTaskController::V8IdleTask::invoke(IdleDeadline* deadline) {
  callback_->call(nullptr, deadline);
}

ScriptedIdleTaskController::ScriptedIdleTaskController(
    ExecutionContext* context)
    : SuspendableObject(context),
      scheduler_(Platform::Current()->CurrentThread()->Scheduler()),
      next_callback_id_(0),
      suspended_(false) {
  SuspendIfNeeded();
}

ScriptedIdleTaskController::~ScriptedIdleTaskController() {}

void ScriptedIdleTaskController::Trace(blink::Visitor* visitor) {
  visitor->Trace(idle_tasks_);
  SuspendableObject::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(ScriptedIdleTaskController) {
  for (const auto& idle_task : idle_tasks_.Values()) {
    visitor->TraceWrappers(idle_task);
  }
}

int ScriptedIdleTaskController::NextCallbackId() {
  while (true) {
    ++next_callback_id_;

    if (!IsValidCallbackId(next_callback_id_))
      next_callback_id_ = 1;

    if (!idle_tasks_.Contains(next_callback_id_))
      return next_callback_id_;
  }
}

ScriptedIdleTaskController::CallbackId
ScriptedIdleTaskController::RegisterCallback(
    IdleTask* idle_task,
    const IdleRequestOptions& options) {
  CallbackId id = NextCallbackId();
  idle_tasks_.Set(id, idle_task);
  long long timeout_millis = options.timeout();

  probe::AsyncTaskScheduled(GetExecutionContext(), "requestIdleCallback",
                            idle_task);

  RefPtr<internal::IdleRequestCallbackWrapper> callback_wrapper =
      internal::IdleRequestCallbackWrapper::Create(id, this);
  ScheduleCallback(std::move(callback_wrapper), timeout_millis);
  TRACE_EVENT_INSTANT1("devtools.timeline", "RequestIdleCallback",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorIdleCallbackRequestEvent::Data(
                           GetExecutionContext(), id, timeout_millis));
  return id;
}

void ScriptedIdleTaskController::ScheduleCallback(
    RefPtr<internal::IdleRequestCallbackWrapper> callback_wrapper,
    long long timeout_millis) {
  scheduler_->PostIdleTask(
      BLINK_FROM_HERE,
      WTF::Bind(&internal::IdleRequestCallbackWrapper::IdleTaskFired,
                callback_wrapper));
  if (timeout_millis > 0) {
    TaskRunnerHelper::Get(TaskType::kIdleTask, GetExecutionContext())
        ->PostDelayedTask(
            BLINK_FROM_HERE,
            WTF::Bind(&internal::IdleRequestCallbackWrapper::TimeoutFired,
                      callback_wrapper),
            TimeDelta::FromMilliseconds(timeout_millis));
  }
}

void ScriptedIdleTaskController::CancelCallback(CallbackId id) {
  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "CancelIdleCallback", TRACE_EVENT_SCOPE_THREAD,
      "data",
      InspectorIdleCallbackCancelEvent::Data(GetExecutionContext(), id));
  if (!IsValidCallbackId(id))
    return;

  idle_tasks_.erase(id);
}

void ScriptedIdleTaskController::CallbackFired(
    CallbackId id,
    double deadline_seconds,
    IdleDeadline::CallbackType callback_type) {
  if (!idle_tasks_.Contains(id))
    return;

  if (suspended_) {
    if (callback_type == IdleDeadline::CallbackType::kCalledByTimeout) {
      // Queue for execution when we are resumed.
      pending_timeouts_.push_back(id);
    }
    // Just drop callbacks called while suspended, these will be reposted on the
    // idle task queue when we are resumed.
    return;
  }

  RunCallback(id, deadline_seconds, callback_type);
}

void ScriptedIdleTaskController::RunCallback(
    CallbackId id,
    double deadline_seconds,
    IdleDeadline::CallbackType callback_type) {
  DCHECK(!suspended_);
  IdleTask* idle_task = idle_tasks_.Take(id);
  if (!idle_task)
    return;

  double allotted_time_millis =
      std::max((deadline_seconds - MonotonicallyIncreasingTime()) * 1000, 0.0);

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, idle_callback_deadline_histogram,
      ("WebCore.ScriptedIdleTaskController.IdleCallbackDeadline", 0, 50, 50));
  idle_callback_deadline_histogram.Count(allotted_time_millis);

  probe::AsyncTask async_task(GetExecutionContext(), idle_task);
  probe::UserCallback probe(GetExecutionContext(), "requestIdleCallback",
                            AtomicString(), true);

  TRACE_EVENT1(
      "devtools.timeline", "FireIdleCallback", "data",
      InspectorIdleCallbackFireEvent::Data(
          GetExecutionContext(), id, allotted_time_millis,
          callback_type == IdleDeadline::CallbackType::kCalledByTimeout));
  idle_task->invoke(IdleDeadline::Create(deadline_seconds, callback_type));

  double overrun_millis =
      std::max((MonotonicallyIncreasingTime() - deadline_seconds) * 1000, 0.0);

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, idle_callback_overrun_histogram,
      ("WebCore.ScriptedIdleTaskController.IdleCallbackOverrun", 0, 10000, 50));
  idle_callback_overrun_histogram.Count(overrun_millis);
}

void ScriptedIdleTaskController::ContextDestroyed(ExecutionContext*) {
  idle_tasks_.clear();
}

void ScriptedIdleTaskController::Suspend() {
  suspended_ = true;
}

void ScriptedIdleTaskController::Resume() {
  DCHECK(suspended_);
  suspended_ = false;

  // Run any pending timeouts.
  Vector<CallbackId> pending_timeouts;
  pending_timeouts_.swap(pending_timeouts);
  for (auto& id : pending_timeouts)
    RunCallback(id, MonotonicallyIncreasingTime(),
                IdleDeadline::CallbackType::kCalledByTimeout);

  // Repost idle tasks for any remaining callbacks.
  for (auto& idle_task : idle_tasks_) {
    RefPtr<internal::IdleRequestCallbackWrapper> callback_wrapper =
        internal::IdleRequestCallbackWrapper::Create(idle_task.key, this);
    scheduler_->PostIdleTask(
        BLINK_FROM_HERE,
        WTF::Bind(&internal::IdleRequestCallbackWrapper::IdleTaskFired,
                  callback_wrapper));
  }
}

}  // namespace blink
