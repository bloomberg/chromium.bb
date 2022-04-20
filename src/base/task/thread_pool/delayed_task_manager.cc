// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/delayed_task_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool/task.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace internal {

DelayedTaskManager::DelayedTask::DelayedTask() = default;

DelayedTaskManager::DelayedTask::DelayedTask(
    Task task,
    PostTaskNowCallback callback,
    scoped_refptr<TaskRunner> task_runner)
    : task(std::move(task)),
      callback(std::move(callback)),
      task_runner(std::move(task_runner)) {}

DelayedTaskManager::DelayedTask::DelayedTask(
    DelayedTaskManager::DelayedTask&& other) = default;

DelayedTaskManager::DelayedTask::~DelayedTask() = default;

DelayedTaskManager::DelayedTask& DelayedTaskManager::DelayedTask::operator=(
    DelayedTaskManager::DelayedTask&& other) = default;

bool DelayedTaskManager::DelayedTask::operator>(
    const DelayedTask& other) const {
  return std::tie(task.delayed_run_time, task.sequence_num) >
         std::tie(other.task.delayed_run_time, other.task.sequence_num);
}

bool DelayedTaskManager::DelayedTask::IsScheduled() const {
  return scheduled_;
}
void DelayedTaskManager::DelayedTask::SetScheduled() {
  DCHECK(!scheduled_);
  scheduled_ = true;
}

DelayedTaskManager::DelayedTaskManager(const TickClock* tick_clock)
    : process_ripe_tasks_closure_(
          BindRepeating(&DelayedTaskManager::ProcessRipeTasks,
                        Unretained(this))),
      tick_clock_(tick_clock) {
  DCHECK(tick_clock_);
}

DelayedTaskManager::~DelayedTaskManager() = default;

void DelayedTaskManager::Start(
    scoped_refptr<SequencedTaskRunner> service_thread_task_runner) {
  DCHECK(service_thread_task_runner);

  TimeTicks process_ripe_tasks_time;
  {
    CheckedAutoLock auto_lock(queue_lock_);
    DCHECK(!service_thread_task_runner_);
    service_thread_task_runner_ = std::move(service_thread_task_runner);
    process_ripe_tasks_time = GetTimeToScheduleProcessRipeTasksLockRequired();
  }
  ScheduleProcessRipeTasksOnServiceThread(process_ripe_tasks_time);
}

void DelayedTaskManager::AddDelayedTask(
    Task task,
    PostTaskNowCallback post_task_now_callback,
    scoped_refptr<TaskRunner> task_runner) {
  DCHECK(task.task);
  DCHECK(!task.delayed_run_time.is_null());

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  TimeTicks process_ripe_tasks_time;
  {
    CheckedAutoLock auto_lock(queue_lock_);
    delayed_task_queue_.insert(DelayedTask(std::move(task),
                                           std::move(post_task_now_callback),
                                           std::move(task_runner)));
    // Not started yet.
    if (service_thread_task_runner_ == nullptr)
      return;
    process_ripe_tasks_time = GetTimeToScheduleProcessRipeTasksLockRequired();
  }
  ScheduleProcessRipeTasksOnServiceThread(process_ripe_tasks_time);
}

void DelayedTaskManager::ProcessRipeTasks() {
  std::vector<DelayedTask> ripe_delayed_tasks;
  TimeTicks process_ripe_tasks_time;

  {
    CheckedAutoLock auto_lock(queue_lock_);
    const TimeTicks now = tick_clock_->NowTicks();
    // A delayed task is ripe if it reached its delayed run time or if it is
    // canceled. If it is canceled, schedule its deletion on the correct
    // sequence now rather than in the future, to minimize CPU wake ups and save
    // power.
    while (!delayed_task_queue_.empty() &&
           (delayed_task_queue_.top().task.delayed_run_time <= now ||
            !delayed_task_queue_.top().task.task.MaybeValid())) {
      // The const_cast on top is okay since the DelayedTask is
      // transactionally being popped from |delayed_task_queue_| right after
      // and the move doesn't alter the sort order.
      ripe_delayed_tasks.push_back(
          std::move(const_cast<DelayedTask&>(delayed_task_queue_.top())));
      delayed_task_queue_.pop();
    }
    process_ripe_tasks_time = GetTimeToScheduleProcessRipeTasksLockRequired();
  }
  ScheduleProcessRipeTasksOnServiceThread(process_ripe_tasks_time);

  for (auto& delayed_task : ripe_delayed_tasks) {
    std::move(delayed_task.callback).Run(std::move(delayed_task.task));
  }
}

absl::optional<TimeTicks> DelayedTaskManager::NextScheduledRunTime() const {
  CheckedAutoLock auto_lock(queue_lock_);
  if (delayed_task_queue_.empty())
    return absl::nullopt;
  return delayed_task_queue_.top().task.delayed_run_time;
}

TimeTicks DelayedTaskManager::GetTimeToScheduleProcessRipeTasksLockRequired() {
  queue_lock_.AssertAcquired();
  if (delayed_task_queue_.empty())
    return TimeTicks::Max();
  // The const_cast on top is okay since |IsScheduled()| and |SetScheduled()|
  // don't alter the sort order.
  DelayedTask& ripest_delayed_task =
      const_cast<DelayedTask&>(delayed_task_queue_.top());
  if (ripest_delayed_task.IsScheduled())
    return TimeTicks::Max();
  ripest_delayed_task.SetScheduled();
  return ripest_delayed_task.task.delayed_run_time;
}

void DelayedTaskManager::ScheduleProcessRipeTasksOnServiceThread(
    TimeTicks next_delayed_task_run_time) {
  DCHECK(!next_delayed_task_run_time.is_null());
  if (next_delayed_task_run_time.is_max())
    return;
  service_thread_task_runner_->PostDelayedTaskAt(
      subtle::PostDelayedTaskPassKey(), FROM_HERE, process_ripe_tasks_closure_,
      next_delayed_task_run_time, subtle::DelayPolicy::kPrecise);
}

}  // namespace internal
}  // namespace base
