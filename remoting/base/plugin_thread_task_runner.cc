// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/plugin_thread_task_runner.h"

#include "base/bind.h"

namespace {

base::TimeDelta CalcTimeDelta(base::TimeTicks when) {
  return std::max(when - base::TimeTicks::Now(), base::TimeDelta());
}

}  // namespace

namespace remoting {

PluginThreadTaskRunner::Delegate::~Delegate() {
}

PluginThreadTaskRunner::PluginThreadTaskRunner(Delegate* delegate)
    : plugin_thread_id_(base::PlatformThread::CurrentId()),
      event_(false, false),
      delegate_(delegate),
      next_sequence_num_(0),
      quit_received_(false),
      stopped_(false) {
}

PluginThreadTaskRunner::~PluginThreadTaskRunner() {
  DCHECK(delegate_ == NULL);
  DCHECK(stopped_);
}

void PluginThreadTaskRunner::DetachAndRunShutdownLoop() {
  DCHECK(BelongsToCurrentThread());

  // Detach from the plugin thread and redirect all tasks posted after this
  // point to the shutdown task loop.
  {
    base::AutoLock auto_lock(lock_);

    DCHECK(delegate_ != NULL);
    DCHECK(!stopped_);

    delegate_ = NULL;
    stopped_ = quit_received_;
  }

  // When DetachAndRunShutdownLoop() is called from NPP_Destroy() all scheduled
  // timers are cancelled. It is OK to clear |scheduled_timers_| even if
  // the timers weren't actually cancelled (i.e. DetachAndRunShutdownLoop() is
  // called before NPP_Destroy()).
  scheduled_timers_.clear();

  // Run all tasks that are due.
  ProcessIncomingTasks();
  RunDueTasks(base::TimeTicks::Now());

  while (!stopped_) {
    if (delayed_queue_.empty()) {
      event_.Wait();
    } else {
      event_.TimedWait(CalcTimeDelta(delayed_queue_.top().delayed_run_time));
    }

    // Run all tasks that are due.
    ProcessIncomingTasks();
    RunDueTasks(base::TimeTicks::Now());

    base::AutoLock auto_lock(lock_);
    stopped_ = quit_received_;
  }
}

void PluginThreadTaskRunner::Quit() {
  base::AutoLock auto_lock(lock_);

  if (!quit_received_) {
    quit_received_ = true;
    event_.Signal();
  }
}

bool PluginThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {

  // Wrap the task into |base::PendingTask|.
  base::TimeTicks delayed_run_time;
  if (delay > base::TimeDelta()) {
    delayed_run_time = base::TimeTicks::Now() + delay;
  } else {
    DCHECK_EQ(delay.InMilliseconds(), 0) << "delay should not be negative";
  }

  base::PendingTask pending_task(from_here, task, delayed_run_time, false);

  // Push the task to the incoming queue.
  base::AutoLock locked(lock_);

  // Initialize the sequence number. The sequence number provides FIFO ordering
  // for tasks with the same |delayed_run_time|.
  pending_task.sequence_num = next_sequence_num_++;

  // Post an asynchronous call on the plugin thread to process the task.
  if (incoming_queue_.empty()) {
    PostRunTasks();
  }

  incoming_queue_.push(pending_task);
  pending_task.task.Reset();

  // No tasks should be posted after Quit() has been called.
  DCHECK(!quit_received_);
  return true;
}

bool PluginThreadTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // All tasks running on this task loop are non-nestable.
  return PostDelayedTask(from_here, task, delay);
}

bool PluginThreadTaskRunner::RunsTasksOnCurrentThread() const {
  // In pepper plugins ideally we should use pp::Core::IsMainThread,
  // but it is problematic because we would need to keep reference to
  // Core somewhere, e.g. make the delegate ref-counted.
  return base::PlatformThread::CurrentId() == plugin_thread_id_;
}

void PluginThreadTaskRunner::PostRunTasks() {
  // Post tasks to the plugin thread when it is availabe or spin the shutdown
  // task loop.
  if (delegate_ != NULL) {
    base::Closure closure = base::Bind(&PluginThreadTaskRunner::RunTasks, this);
    delegate_->RunOnPluginThread(
        base::TimeDelta(),
        &PluginThreadTaskRunner::TaskSpringboard,
        new base::Closure(closure));
  } else {
    event_.Signal();
  }
}

void PluginThreadTaskRunner::PostDelayedRunTasks(base::TimeTicks when) {
  DCHECK(BelongsToCurrentThread());

  // |delegate_| is updated from the plugin thread only, so it is safe to access
  // it here without taking the lock.
  if (delegate_ != NULL) {
    // Schedule RunDelayedTasks() to be called at |when| if it hasn't been
    // scheduled already.
    if (scheduled_timers_.insert(when).second) {
      base::TimeDelta delay = CalcTimeDelta(when);
      base::Closure closure =
          base::Bind(&PluginThreadTaskRunner::RunDelayedTasks, this, when);
      delegate_->RunOnPluginThread(
          delay,
          &PluginThreadTaskRunner::TaskSpringboard,
          new base::Closure(closure));
    }
  } else {
    // Spin the shutdown loop if the task runner has already been detached.
    // The shutdown loop will pick the tasks to run itself.
    event_.Signal();
  }
}

void PluginThreadTaskRunner::ProcessIncomingTasks() {
  DCHECK(BelongsToCurrentThread());

  // Grab all unsorted tasks accomulated so far.
  base::TaskQueue work_queue;
  {
    base::AutoLock locked(lock_);
    incoming_queue_.Swap(&work_queue);
  }

  while (!work_queue.empty()) {
    base::PendingTask pending_task = work_queue.front();
    work_queue.pop();

    if (pending_task.delayed_run_time.is_null()) {
      pending_task.task.Run();
    } else {
      delayed_queue_.push(pending_task);
    }
  }
}

void PluginThreadTaskRunner::RunDelayedTasks(base::TimeTicks when) {
  DCHECK(BelongsToCurrentThread());

  scheduled_timers_.erase(when);

  // |stopped_| is updated by the plugin thread only, so it is safe to access
  // it here without taking the lock.
  if (!stopped_) {
    ProcessIncomingTasks();
    RunDueTasks(base::TimeTicks::Now());
  }
}

void PluginThreadTaskRunner::RunDueTasks(base::TimeTicks now) {
  DCHECK(BelongsToCurrentThread());

  // Run all due tasks.
  while (!delayed_queue_.empty() &&
         delayed_queue_.top().delayed_run_time <= now) {
    delayed_queue_.top().task.Run();
    delayed_queue_.pop();
  }

  // Post a delayed asynchronous call to the plugin thread to process tasks from
  // the delayed queue.
  if (!delayed_queue_.empty()) {
    base::TimeTicks when = delayed_queue_.top().delayed_run_time;
    if (scheduled_timers_.empty() || when < *scheduled_timers_.begin()) {
      PostDelayedRunTasks(when);
    }
  }
}

void PluginThreadTaskRunner::RunTasks() {
  DCHECK(BelongsToCurrentThread());

  // |stopped_| is updated by the plugin thread only, so it is safe to access
  // it here without taking the lock.
  if (!stopped_) {
    ProcessIncomingTasks();
    RunDueTasks(base::TimeTicks::Now());
  }
}

// static
void PluginThreadTaskRunner::TaskSpringboard(void* data) {
  base::Closure* task = reinterpret_cast<base::Closure*>(data);
  task->Run();
  delete task;
}

}  // namespace remoting
