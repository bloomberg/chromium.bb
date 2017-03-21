// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/task_queue_throttler.h"

#include <cstdint>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "platform/WebFrameScheduler.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/renderer/budget_pool.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/throttled_time_domain.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

namespace blink {
namespace scheduler {

namespace {

base::Optional<base::TimeTicks> NextTaskRunTime(LazyNow* lazy_now,
                                                TaskQueue* queue) {
  if (queue->HasPendingImmediateWork())
    return lazy_now->Now();
  return queue->GetNextScheduledWakeUp();
}

template <class T>
T Min(const base::Optional<T>& optional, const T& value) {
  if (!optional) {
    return value;
  }
  return std::min(optional.value(), value);
}

template <class T>
base::Optional<T> Min(const base::Optional<T>& a, const base::Optional<T>& b) {
  if (!b)
    return a;
  if (!a)
    return b;
  return std::min(a.value(), b.value());
}

template <class T>
T Max(const base::Optional<T>& optional, const T& value) {
  if (!optional)
    return value;
  return std::max(optional.value(), value);
}

template <class T>
base::Optional<T> Max(const base::Optional<T>& a, const base::Optional<T>& b) {
  if (!b)
    return a;
  if (!a)
    return b;
  return std::max(a.value(), b.value());
}

std::string PointerToId(void* pointer) {
  return base::StringPrintf(
      "0x%" PRIx64,
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)));
}

}  // namespace

TaskQueueThrottler::TaskQueueThrottler(
    RendererSchedulerImpl* renderer_scheduler,
    const char* tracing_category)
    : task_runner_(renderer_scheduler->ControlTaskRunner()),
      renderer_scheduler_(renderer_scheduler),
      tick_clock_(renderer_scheduler->tick_clock()),
      tracing_category_(tracing_category),
      time_domain_(new ThrottledTimeDomain(this, tracing_category)),
      allow_throttling_(true),
      weak_factory_(this) {
  pump_throttled_tasks_closure_.Reset(base::Bind(
      &TaskQueueThrottler::PumpThrottledTasks, weak_factory_.GetWeakPtr()));
  forward_immediate_work_callback_ =
      base::Bind(&TaskQueueThrottler::OnTimeDomainHasImmediateWork,
                 weak_factory_.GetWeakPtr());

  renderer_scheduler_->RegisterTimeDomain(time_domain_.get());
}

TaskQueueThrottler::~TaskQueueThrottler() {
  // It's possible for queues to be still throttled, so we need to tidy up
  // before unregistering the time domain.
  for (const TaskQueueMap::value_type& map_entry : queue_details_) {
    TaskQueue* task_queue = map_entry.first;
    if (IsThrottled(task_queue)) {
      task_queue->SetTimeDomain(renderer_scheduler_->real_time_domain());
      task_queue->RemoveFence();
    }
  }

  renderer_scheduler_->UnregisterTimeDomain(time_domain_.get());
}

void TaskQueueThrottler::IncreaseThrottleRefCount(TaskQueue* task_queue) {
  DCHECK_NE(task_queue, task_runner_.get());

  std::pair<TaskQueueMap::iterator, bool> insert_result =
      queue_details_.insert(std::make_pair(task_queue, Metadata()));
  insert_result.first->second.throttling_ref_count++;

  // If ref_count is 1, the task queue is newly throttled.
  if (insert_result.first->second.throttling_ref_count != 1)
    return;

  TRACE_EVENT1(tracing_category_, "TaskQueueThrottler_TaskQueueThrottled",
               "task_queue", task_queue);

  if (!allow_throttling_)
    return;

  task_queue->SetTimeDomain(time_domain_.get());
  // This blocks any tasks from |task_queue| until PumpThrottledTasks() to
  // enforce task alignment.
  task_queue->InsertFence(TaskQueue::InsertFencePosition::BEGINNING_OF_TIME);

  if (!task_queue->IsQueueEnabled())
    return;

  if (!task_queue->IsEmpty()) {
    if (task_queue->HasPendingImmediateWork()) {
      OnTimeDomainHasImmediateWork(task_queue);
    } else {
      OnTimeDomainHasDelayedWork(task_queue);
    }
  }
}

void TaskQueueThrottler::DecreaseThrottleRefCount(TaskQueue* task_queue) {
  TaskQueueMap::iterator iter = queue_details_.find(task_queue);

  if (iter == queue_details_.end() ||
      --iter->second.throttling_ref_count != 0) {
    return;
  }

  TRACE_EVENT1(tracing_category_, "TaskQueueThrottler_TaskQueueUnthrottled",
               "task_queue", task_queue);

  MaybeDeleteQueueMetadata(iter);

  if (!allow_throttling_)
    return;

  task_queue->SetTimeDomain(renderer_scheduler_->real_time_domain());
  task_queue->RemoveFence();
}

bool TaskQueueThrottler::IsThrottled(TaskQueue* task_queue) const {
  if (!allow_throttling_)
    return false;

  auto find_it = queue_details_.find(task_queue);
  if (find_it == queue_details_.end())
    return false;
  return find_it->second.throttling_ref_count > 0;
}

void TaskQueueThrottler::UnregisterTaskQueue(TaskQueue* task_queue) {
  auto find_it = queue_details_.find(task_queue);
  if (find_it == queue_details_.end())
    return;

  LazyNow lazy_now(tick_clock_);
  std::unordered_set<BudgetPool*> budget_pools = find_it->second.budget_pools;
  for (BudgetPool* budget_pool : budget_pools) {
    budget_pool->RemoveQueue(lazy_now.Now(), task_queue);
  }

  // Iterator may have been deleted by BudgetPool::RemoveQueue, so don't
  // use it here.
  queue_details_.erase(task_queue);
}

void TaskQueueThrottler::OnTimeDomainHasImmediateWork(TaskQueue* queue) {
  // Forward to the main thread if called from another thread
  if (!task_runner_->RunsTasksOnCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(forward_immediate_work_callback_, queue));
    return;
  }
  TRACE_EVENT0(tracing_category_,
               "TaskQueueThrottler::OnTimeDomainHasImmediateWork");

  // We don't expect this to get called for disabled queues, but we can't DCHECK
  // because of the above thread hop.  Just bail out if the queue is disabled.
  if (!queue->IsQueueEnabled())
    return;

  base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeTicks next_allowed_run_time = GetNextAllowedRunTime(now, queue);
  MaybeSchedulePumpThrottledTasks(FROM_HERE, now, next_allowed_run_time);
}

void TaskQueueThrottler::OnTimeDomainHasDelayedWork(TaskQueue* queue) {
  TRACE_EVENT0(tracing_category_,
               "TaskQueueThrottler::OnTimeDomainHasDelayedWork");
  DCHECK(queue->IsQueueEnabled());
  base::TimeTicks now = tick_clock_->NowTicks();
  LazyNow lazy_now(now);

  base::Optional<base::TimeTicks> next_scheduled_delayed_task =
      NextTaskRunTime(&lazy_now, queue);
  DCHECK(next_scheduled_delayed_task);
  MaybeSchedulePumpThrottledTasks(FROM_HERE, now,
                                  next_scheduled_delayed_task.value());
}

void TaskQueueThrottler::PumpThrottledTasks() {
  TRACE_EVENT0(tracing_category_, "TaskQueueThrottler::PumpThrottledTasks");
  pending_pump_throttled_tasks_runtime_.reset();

  LazyNow lazy_now(tick_clock_);
  base::Optional<base::TimeTicks> next_scheduled_delayed_task;

  for (const TaskQueueMap::value_type& map_entry : queue_details_) {
    TaskQueue* task_queue = map_entry.first;
    if (task_queue->IsEmpty() || !IsThrottled(task_queue))
      continue;

    // Don't enable queues whose budget pool doesn't allow them to run now.
    base::TimeTicks next_allowed_run_time =
        GetNextAllowedRunTime(lazy_now.Now(), task_queue);
    base::Optional<base::TimeTicks> next_desired_run_time =
        NextTaskRunTime(&lazy_now, task_queue);

    if (next_desired_run_time &&
        next_allowed_run_time > next_desired_run_time.value()) {
      TRACE_EVENT1(
          tracing_category_,
          "TaskQueueThrottler::PumpThrottledTasks_ExpensiveTaskThrottled",
          "throttle_time_in_seconds",
          (next_allowed_run_time - next_desired_run_time.value()).InSecondsF());

      // Schedule a pump for queue which was disabled because of time budget.
      next_scheduled_delayed_task =
          Min(next_scheduled_delayed_task, next_allowed_run_time);

      continue;
    }

    next_scheduled_delayed_task =
        Min(next_scheduled_delayed_task, task_queue->GetNextScheduledWakeUp());

    if (next_allowed_run_time > lazy_now.Now())
      continue;

    // Remove previous fence and install a new one, allowing all tasks posted
    // on |task_queue| up until this point to run and block all further tasks.
    task_queue->InsertFence(TaskQueue::InsertFencePosition::NOW);
  }

  // Maybe schedule a call to TaskQueueThrottler::PumpThrottledTasks if there is
  // a pending delayed task or a throttled task ready to run.
  // NOTE: posting a non-delayed task in the future will result in
  // TaskQueueThrottler::OnTimeDomainHasImmediateWork being called.
  if (next_scheduled_delayed_task) {
    MaybeSchedulePumpThrottledTasks(FROM_HERE, lazy_now.Now(),
                                    *next_scheduled_delayed_task);
  }
}

/* static */
base::TimeTicks TaskQueueThrottler::AlignedThrottledRunTime(
    base::TimeTicks unthrottled_runtime) {
  const base::TimeDelta one_second = base::TimeDelta::FromSeconds(1);
  return unthrottled_runtime + one_second -
         ((unthrottled_runtime - base::TimeTicks()) % one_second);
}

void TaskQueueThrottler::MaybeSchedulePumpThrottledTasks(
    const tracked_objects::Location& from_here,
    base::TimeTicks now,
    base::TimeTicks unaligned_runtime) {
  if (!allow_throttling_)
    return;

  base::TimeTicks runtime =
      AlignedThrottledRunTime(std::max(now, unaligned_runtime));
  DCHECK_LE(now, runtime);

  // If there is a pending call to PumpThrottledTasks and it's sooner than
  // |runtime| then return.
  if (pending_pump_throttled_tasks_runtime_ &&
      runtime >= pending_pump_throttled_tasks_runtime_.value()) {
    return;
  }

  pending_pump_throttled_tasks_runtime_ = runtime;

  pump_throttled_tasks_closure_.Cancel();

  base::TimeDelta delay = pending_pump_throttled_tasks_runtime_.value() - now;
  TRACE_EVENT1(tracing_category_,
               "TaskQueueThrottler::MaybeSchedulePumpThrottledTasks",
               "delay_till_next_pump_ms", delay.InMilliseconds());
  task_runner_->PostDelayedTask(
      from_here, pump_throttled_tasks_closure_.callback(), delay);
}

CPUTimeBudgetPool* TaskQueueThrottler::CreateCPUTimeBudgetPool(
    const char* name) {
  CPUTimeBudgetPool* time_budget_pool =
      new CPUTimeBudgetPool(name, this, tick_clock_->NowTicks());
  budget_pools_[time_budget_pool] = base::WrapUnique(time_budget_pool);
  return time_budget_pool;
}

void TaskQueueThrottler::OnTaskRunTimeReported(TaskQueue* task_queue,
                                               base::TimeTicks start_time,
                                               base::TimeTicks end_time) {
  if (!IsThrottled(task_queue))
    return;

  auto find_it = queue_details_.find(task_queue);
  if (find_it == queue_details_.end())
    return;

  for (BudgetPool* budget_pool : find_it->second.budget_pools) {
    budget_pool->RecordTaskRunTime(start_time, end_time);
    if (!budget_pool->HasEnoughBudgetToRun(end_time))
      budget_pool->BlockThrottledQueues(end_time);
  }
}

void TaskQueueThrottler::BlockQueue(base::TimeTicks now, TaskQueue* queue) {
  if (!IsThrottled(queue))
    return;

  queue->InsertFence(TaskQueue::InsertFencePosition::BEGINNING_OF_TIME);
  SchedulePumpQueue(FROM_HERE, now, queue);
}

void TaskQueueThrottler::AsValueInto(base::trace_event::TracedValue* state,
                                     base::TimeTicks now) const {
  if (pending_pump_throttled_tasks_runtime_) {
    state->SetDouble(
        "next_throttled_tasks_pump_in_seconds",
        (pending_pump_throttled_tasks_runtime_.value() - now).InSecondsF());
  }

  state->SetBoolean("allow_throttling", allow_throttling_);

  state->BeginDictionary("time_budget_pools");
  for (const auto& map_entry : budget_pools_) {
    BudgetPool* pool = map_entry.first;
    pool->AsValueInto(state, now);
  }
  state->EndDictionary();

  state->BeginDictionary("queue_details");
  for (const auto& map_entry : queue_details_) {
    state->BeginDictionaryWithCopiedName(PointerToId(map_entry.first));

    state->SetInteger("throttling_ref_count",
                      map_entry.second.throttling_ref_count);

    state->EndDictionary();
  }
  state->EndDictionary();
}

void TaskQueueThrottler::AddQueueToBudgetPool(TaskQueue* queue,
                                              BudgetPool* budget_pool) {
  std::pair<TaskQueueMap::iterator, bool> insert_result =
      queue_details_.insert(std::make_pair(queue, Metadata()));

  Metadata& metadata = insert_result.first->second;

  DCHECK(metadata.budget_pools.find(budget_pool) ==
         metadata.budget_pools.end());

  metadata.budget_pools.insert(budget_pool);
}

void TaskQueueThrottler::RemoveQueueFromBudgetPool(TaskQueue* queue,
                                                   BudgetPool* budget_pool) {
  auto find_it = queue_details_.find(queue);
  DCHECK(find_it != queue_details_.end() &&
         find_it->second.budget_pools.find(budget_pool) !=
             find_it->second.budget_pools.end());

  find_it->second.budget_pools.erase(budget_pool);

  MaybeDeleteQueueMetadata(find_it);
}

void TaskQueueThrottler::UnregisterBudgetPool(BudgetPool* budget_pool) {
  budget_pools_.erase(budget_pool);
}

void TaskQueueThrottler::UnblockQueue(base::TimeTicks now, TaskQueue* queue) {
  SchedulePumpQueue(FROM_HERE, now, queue);
}

void TaskQueueThrottler::SchedulePumpQueue(
    const tracked_objects::Location& from_here,
    base::TimeTicks now,
    TaskQueue* queue) {
  if (!IsThrottled(queue))
    return;

  LazyNow lazy_now(now);
  base::Optional<base::TimeTicks> next_desired_run_time =
      NextTaskRunTime(&lazy_now, queue);
  if (!next_desired_run_time)
    return;

  base::Optional<base::TimeTicks> next_run_time =
      Max(next_desired_run_time, GetNextAllowedRunTime(now, queue));

  MaybeSchedulePumpThrottledTasks(from_here, now, next_run_time.value());
}

base::TimeTicks TaskQueueThrottler::GetNextAllowedRunTime(base::TimeTicks now,
                                                          TaskQueue* queue) {
  base::TimeTicks next_run_time = now;

  auto find_it = queue_details_.find(queue);
  if (find_it == queue_details_.end())
    return next_run_time;

  for (BudgetPool* budget_pool : find_it->second.budget_pools) {
    next_run_time =
        std::max(next_run_time, budget_pool->GetNextAllowedRunTime());
  }

  return next_run_time;
}

void TaskQueueThrottler::MaybeDeleteQueueMetadata(TaskQueueMap::iterator it) {
  if (it->second.throttling_ref_count == 0 && it->second.budget_pools.empty())
    queue_details_.erase(it);
}

void TaskQueueThrottler::DisableThrottling() {
  if (!allow_throttling_)
    return;

  allow_throttling_ = false;

  for (const auto& map_entry : queue_details_) {
    if (map_entry.second.throttling_ref_count == 0)
      continue;

    TaskQueue* queue = map_entry.first;

    queue->SetTimeDomain(renderer_scheduler_->GetActiveTimeDomain());

    queue->RemoveFence();
  }

  pump_throttled_tasks_closure_.Cancel();
  pending_pump_throttled_tasks_runtime_ = base::nullopt;

  TRACE_EVENT0(tracing_category_, "TaskQueueThrottler_DisableThrottling");
}

void TaskQueueThrottler::EnableThrottling() {
  if (allow_throttling_)
    return;

  allow_throttling_ = true;

  LazyNow lazy_now(tick_clock_);

  for (const auto& map_entry : queue_details_) {
    if (map_entry.second.throttling_ref_count == 0)
      continue;

    TaskQueue* queue = map_entry.first;

    // Throttling is enabled and task queue should be blocked immediately
    // to enforce task alignment.
    queue->InsertFence(TaskQueue::InsertFencePosition::BEGINNING_OF_TIME);
    queue->SetTimeDomain(time_domain_.get());
    SchedulePumpQueue(FROM_HERE, lazy_now.Now(), queue);
  }

  TRACE_EVENT0(tracing_category_, "TaskQueueThrottler_EnableThrottling");
}

}  // namespace scheduler
}  // namespace blink
