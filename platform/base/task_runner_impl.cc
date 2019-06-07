// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/base/task_runner_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

TaskRunnerImpl::TaskRunnerImpl(platform::ClockNowFunctionPtr now_function,
                               TaskWaiter* event_waiter,
                               Clock::duration waiter_timeout)
    : now_function_(now_function),
      is_running_(false),
      task_waiter_(event_waiter),
      waiter_timeout_(waiter_timeout) {}

TaskRunnerImpl::~TaskRunnerImpl() = default;

void TaskRunnerImpl::PostPackagedTask(Task task) {
  std::lock_guard<std::mutex> lock(task_mutex_);
  tasks_.push_back(std::move(task));
  if (task_waiter_) {
    task_waiter_->OnTaskPosted();
  } else {
    run_loop_wakeup_.notify_one();
  }
}

void TaskRunnerImpl::PostPackagedTaskWithDelay(Task task,
                                               Clock::duration delay) {
  std::lock_guard<std::mutex> lock(task_mutex_);
  delayed_tasks_.emplace(
      std::make_pair(now_function_() + delay, std::move(task)));
  if (task_waiter_) {
    task_waiter_->OnTaskPosted();
  } else {
    run_loop_wakeup_.notify_one();
  }
}

void TaskRunnerImpl::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  RunTasksUntilStopped();
}

void TaskRunnerImpl::RequestStopSoon() {
  const bool was_running = is_running_.exchange(false);

  if (was_running) {
    OSP_DVLOG << "Requesting stop...";
    if (task_waiter_) {
      task_waiter_->OnTaskPosted();
    } else {
      std::lock_guard<std::mutex> lock(task_mutex_);
      run_loop_wakeup_.notify_one();
    }
  }
}

void TaskRunnerImpl::RunUntilIdleForTesting() {
  ScheduleDelayedTasks();
  RunCurrentTasksForTesting();
}

void TaskRunnerImpl::RunCurrentTasksForTesting() {
  {
    // Unlike in the RunCurrentTasksBlocking method, here we just immediately
    // take the lock and drain the tasks_ queue. This allows tests to avoid
    // having to do any multithreading to interact with the queue.
    std::unique_lock<std::mutex> lock(task_mutex_);
    OSP_DCHECK(running_tasks_.empty());
    running_tasks_.swap(tasks_);
  }

  for (Task& task : running_tasks_) {
    // Move the task to the stack so that its bound state is freed immediately
    // after being run.
    std::move(task)();
  }
  running_tasks_.clear();
}

void TaskRunnerImpl::RunCurrentTasksBlocking() {
  {
    // Wait for the lock. If there are no current tasks, we will wait until
    // a delayed task is ready or a task gets added to the queue.
    auto lock = WaitForWorkAndAcquireLock();
    if (!lock) {
      return;
    }

    OSP_DCHECK(running_tasks_.empty());
    running_tasks_.swap(tasks_);
  }

  OSP_DVLOG << "Running " << running_tasks_.size() << " tasks...";
  for (Task& task : running_tasks_) {
    // Move the task to the stack so that its bound state is freed immediately
    // after being run.
    std::move(task)();
  }
  running_tasks_.clear();
}

void TaskRunnerImpl::RunTasksUntilStopped() {
  while (is_running_) {
    ScheduleDelayedTasks();
    RunCurrentTasksBlocking();
  }
}

void TaskRunnerImpl::ScheduleDelayedTasks() {
  std::lock_guard<std::mutex> lock(task_mutex_);

  // Getting the time can be expensive on some platforms, so only get it once.
  const auto current_time = now_function_();
  const auto end_of_range = delayed_tasks_.upper_bound(current_time);
  for (auto it = delayed_tasks_.begin(); it != end_of_range; ++it) {
    tasks_.push_back(std::move(it->second));
  }
  delayed_tasks_.erase(delayed_tasks_.begin(), end_of_range);
}

bool TaskRunnerImpl::ShouldWakeUpRunLoop() {
  if (!is_running_) {
    return true;
  }

  if (!tasks_.empty()) {
    return true;
  }

  return !delayed_tasks_.empty() &&
         (delayed_tasks_.begin()->first <= now_function_());
}

std::unique_lock<std::mutex> TaskRunnerImpl::WaitForWorkAndAcquireLock() {
  // These checks are redundant, as they are a subset of predicates in the
  // below wait predicate. However, this is more readable and a slight
  // optimization, as we don't need to take a lock if we aren't running.
  if (!is_running_) {
    OSP_DVLOG << "TaskRunnerImpl not running. Returning empty lock.";
    return {};
  }

  std::unique_lock<std::mutex> lock(task_mutex_);
  if (!tasks_.empty()) {
    OSP_DVLOG << "TaskRunnerImpl lock acquired.";
    return lock;
  }

  if (task_waiter_) {
    do {
      Clock::duration timeout = waiter_timeout_;
      if (!delayed_tasks_.empty()) {
        Clock::duration next_task_delta =
            delayed_tasks_.begin()->first - now_function_();
        if (next_task_delta < timeout) {
          timeout = next_task_delta;
        }
      }
      lock.unlock();
      task_waiter_->WaitForTaskToBePosted(timeout);
      lock.lock();
    } while (!ShouldWakeUpRunLoop());
  } else {
    // Pass a wait predicate to avoid lost or spurious wakeups.
    if (!delayed_tasks_.empty()) {
      // We don't have any work to do currently, but have some in the
      // pipe.
      const auto wait_predicate = [this] { return ShouldWakeUpRunLoop(); };
      OSP_DVLOG << "TaskRunner waiting for lock until delayed task ready...";
      run_loop_wakeup_.wait_for(lock,
                                delayed_tasks_.begin()->first - now_function_(),
                                wait_predicate);
    } else {
      // We don't have any work queued.
      const auto wait_predicate = [this] {
        return !delayed_tasks_.empty() || ShouldWakeUpRunLoop();
      };
      OSP_DVLOG << "TaskRunnerImpl waiting for lock...";
      run_loop_wakeup_.wait(lock, wait_predicate);
    }
  }

  OSP_DVLOG << "TaskRunnerImpl lock acquired.";
  return lock;
}
}  // namespace platform
}  // namespace openscreen
