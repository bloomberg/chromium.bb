// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/throttling_helper.h"

#include "base/logging.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/throttled_time_domain.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"
#include "public/platform/WebFrameScheduler.h"

namespace blink {
namespace scheduler {

ThrottlingHelper::ThrottlingHelper(RendererSchedulerImpl* renderer_scheduler,
                                   const char* tracing_category)
    : task_runner_(renderer_scheduler->ControlTaskRunner()),
      renderer_scheduler_(renderer_scheduler),
      tick_clock_(renderer_scheduler->tick_clock()),
      tracing_category_(tracing_category),
      time_domain_(new ThrottledTimeDomain(this, tracing_category)),
      virtual_time_(false),
      weak_factory_(this) {
  pump_throttled_tasks_closure_.Reset(base::Bind(
      &ThrottlingHelper::PumpThrottledTasks, weak_factory_.GetWeakPtr()));
  forward_immediate_work_closure_ =
      base::Bind(&ThrottlingHelper::OnTimeDomainHasImmediateWork,
                 weak_factory_.GetWeakPtr());

  renderer_scheduler_->RegisterTimeDomain(time_domain_.get());
}

ThrottlingHelper::~ThrottlingHelper() {
  // It's possible for queues to be still throttled, so we need to tidy up
  // before unregistering the time domain.
  for (const TaskQueueMap::value_type& map_entry : throttled_queues_) {
    TaskQueue* task_queue = map_entry.first;
    task_queue->SetTimeDomain(renderer_scheduler_->real_time_domain());
    task_queue->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
  }

  renderer_scheduler_->UnregisterTimeDomain(time_domain_.get());
}

void ThrottlingHelper::SetQueueEnabled(TaskQueue* task_queue, bool enabled) {
  TaskQueueMap::iterator find_it = throttled_queues_.find(task_queue);

  if (find_it == throttled_queues_.end()) {
    task_queue->SetQueueEnabled(enabled);
    return;
  }

  find_it->second.enabled = enabled;

  // We don't enable the queue here because it's throttled and there might be
  // tasks in it's work queue that would execute immediatly rather than after
  // PumpThrottledTasks runs.
  if (!enabled)
    task_queue->SetQueueEnabled(false);
}

void ThrottlingHelper::IncreaseThrottleRefCount(TaskQueue* task_queue) {
  DCHECK_NE(task_queue, task_runner_.get());

  if (virtual_time_)
    return;

  std::pair<TaskQueueMap::iterator, bool> insert_result =
      throttled_queues_.insert(std::make_pair(
          task_queue, Metadata(1, task_queue->IsQueueEnabled())));

  if (insert_result.second) {
    // The insert was succesful so we need to throttle the queue.
    task_queue->SetTimeDomain(time_domain_.get());
    task_queue->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);
    task_queue->SetQueueEnabled(false);

    if (!task_queue->IsEmpty()) {
      if (task_queue->HasPendingImmediateWork()) {
        OnTimeDomainHasImmediateWork();
      } else {
        OnTimeDomainHasDelayedWork();
      }
    }
  } else {
    // An entry already existed in the map so we need to increment the refcount.
    insert_result.first->second.throttling_ref_count++;
  }
}

void ThrottlingHelper::DecreaseThrottleRefCount(TaskQueue* task_queue) {
  if (virtual_time_)
    return;

  TaskQueueMap::iterator iter = throttled_queues_.find(task_queue);

  if (iter != throttled_queues_.end() &&
      --iter->second.throttling_ref_count == 0) {
    bool enabled = iter->second.enabled;
    // The refcount has become zero, we need to unthrottle the queue.
    throttled_queues_.erase(iter);

    task_queue->SetTimeDomain(renderer_scheduler_->real_time_domain());
    task_queue->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
    task_queue->SetQueueEnabled(enabled);
  }
}

void ThrottlingHelper::UnregisterTaskQueue(TaskQueue* task_queue) {
  throttled_queues_.erase(task_queue);
}

void ThrottlingHelper::OnTimeDomainHasImmediateWork() {
  // Forward to the main thread if called from another thread.
  if (!task_runner_->RunsTasksOnCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, forward_immediate_work_closure_);
    return;
  }
  TRACE_EVENT0(tracing_category_,
               "ThrottlingHelper::OnTimeDomainHasImmediateWork");
  base::TimeTicks now = tick_clock_->NowTicks();
  MaybeSchedulePumpThrottledTasksLocked(FROM_HERE, now, now);
}

void ThrottlingHelper::OnTimeDomainHasDelayedWork() {
  TRACE_EVENT0(tracing_category_,
               "ThrottlingHelper::OnTimeDomainHasDelayedWork");
  base::TimeTicks next_scheduled_delayed_task;
  bool has_delayed_task =
      time_domain_->NextScheduledRunTime(&next_scheduled_delayed_task);
  DCHECK(has_delayed_task);
  base::TimeTicks now = tick_clock_->NowTicks();
  MaybeSchedulePumpThrottledTasksLocked(FROM_HERE, now,
                                        next_scheduled_delayed_task);
}

void ThrottlingHelper::PumpThrottledTasks() {
  TRACE_EVENT0(tracing_category_, "ThrottlingHelper::PumpThrottledTasks");
  pending_pump_throttled_tasks_runtime_ = base::TimeTicks();

  LazyNow lazy_low(tick_clock_);
  for (const TaskQueueMap::value_type& map_entry : throttled_queues_) {
    TaskQueue* task_queue = map_entry.first;
    if (task_queue->IsEmpty())
      continue;

    task_queue->SetQueueEnabled(map_entry.second.enabled);
    task_queue->PumpQueue(&lazy_low, false);
  }
  // Make sure NextScheduledRunTime gives us an up-to date result.
  time_domain_->ClearExpiredWakeups();

  base::TimeTicks next_scheduled_delayed_task;
  // Maybe schedule a call to ThrottlingHelper::PumpThrottledTasks if there is
  // a pending delayed task. NOTE posting a non-delayed task in the future will
  // result in ThrottlingHelper::OnTimeDomainHasImmediateWork being called.
  if (time_domain_->NextScheduledRunTime(&next_scheduled_delayed_task)) {
    MaybeSchedulePumpThrottledTasksLocked(FROM_HERE, lazy_low.Now(),
                                          next_scheduled_delayed_task);
  }
}

/* static */
base::TimeTicks ThrottlingHelper::ThrottledRunTime(
    base::TimeTicks unthrottled_runtime) {
  const base::TimeDelta one_second = base::TimeDelta::FromSeconds(1);
  return unthrottled_runtime + one_second -
         ((unthrottled_runtime - base::TimeTicks()) % one_second);
}

void ThrottlingHelper::MaybeSchedulePumpThrottledTasksLocked(
    const tracked_objects::Location& from_here,
    base::TimeTicks now,
    base::TimeTicks unthrottled_runtime) {
  if (virtual_time_)
    return;

  base::TimeTicks throttled_runtime =
      ThrottledRunTime(std::max(now, unthrottled_runtime));
  // If there is a pending call to PumpThrottledTasks and it's sooner than
  // |unthrottled_runtime| then return.
  if (!pending_pump_throttled_tasks_runtime_.is_null() &&
      throttled_runtime >= pending_pump_throttled_tasks_runtime_) {
    return;
  }

  pending_pump_throttled_tasks_runtime_ = throttled_runtime;

  pump_throttled_tasks_closure_.Cancel();

  base::TimeDelta delay = pending_pump_throttled_tasks_runtime_ - now;
  TRACE_EVENT1(tracing_category_,
               "ThrottlingHelper::MaybeSchedulePumpThrottledTasksLocked",
               "delay_till_next_pump_ms", delay.InMilliseconds());
  task_runner_->PostDelayedTask(
      from_here, pump_throttled_tasks_closure_.callback(), delay);
}

void ThrottlingHelper::EnableVirtualTime() {
  virtual_time_ = true;

  pump_throttled_tasks_closure_.Cancel();

  while (!throttled_queues_.empty()) {
    TaskQueue* task_queue = throttled_queues_.begin()->first;
    bool enabled = throttled_queues_.begin()->second.enabled;

    throttled_queues_.erase(throttled_queues_.begin());

    task_queue->SetTimeDomain(renderer_scheduler_->GetVirtualTimeDomain());
    task_queue->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
    task_queue->SetQueueEnabled(enabled);
  }
}

}  // namespace scheduler
}  // namespace blink
