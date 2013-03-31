// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PLUGIN_THREAD_TASK_RUNNER_H_
#define REMOTING_BASE_PLUGIN_THREAD_TASK_RUNNER_H_

#include <set>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/pending_task.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"

namespace remoting {

// SingleThreadTaskRunner for plugin main threads.
class PluginThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    virtual bool RunOnPluginThread(
        base::TimeDelta delay, void(function)(void*), void* data) = 0;
  };

  // Caller keeps ownership of delegate.
  PluginThreadTaskRunner(Delegate* delegate);

  // Detaches the PluginThreadTaskRunner from the underlying Delegate and
  // processes posted tasks until Quit() is called. This is used during plugin
  // shutdown, when the plugin environment has stopped accepting new tasks to
  // run, to process cleanup tasks posted to the plugin thread.
  // This method must be called on the plugin thread.
  void DetachAndRunShutdownLoop();

  // Makes DetachAndRunShutdownLoop() stop processing tasks and return control
  // to the caller. Calling Quit() before DetachAndRunShutdownLoop() causes
  // the latter to exit immediately when called, without processing any delayed
  // shutdown tasks. This method can be called from any thread.
  void Quit();

  // base::SingleThreadTaskRunner interface.
  virtual bool PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;

 protected:
  virtual ~PluginThreadTaskRunner();

 private:
  // Methods that can be called from any thread.

  // Schedules RunTasks to be called on the plugin thread.
  void PostRunTasks();

  // Methods that are always called on the plugin thread.

  // Schedules RunDelayedTasks() to be called on the plugin thread. |when|
  // specifies the time when RunDelayedTasks() should be called.
  void PostDelayedRunTasks(base::TimeTicks when);

  // Processes the incoming task queue: runs all non delayed tasks and posts all
  // delayed tasks to |delayed_queue_|.
  void ProcessIncomingTasks();

  // Called in response to PostDelayedRunTasks().
  void RunDelayedTasks(base::TimeTicks when);

  // Runs all tasks that are due.
  void RunDueTasks(base::TimeTicks now);

  // Called in response to PostRunTasks().
  void RunTasks();

  static void TaskSpringboard(void* data);

  const base::PlatformThreadId plugin_thread_id_;

  // Used by the shutdown loop to block the thread until there is a task ready
  // to run.
  base::WaitableEvent event_;

  base::Lock lock_;

  // The members below are protected by |lock_|.

  // Pointer to the delegate that implements scheduling tasks via the plugin
  // API.
  Delegate* delegate_;

  // Contains all posted tasks that haven't been sorted yet.
  base::TaskQueue incoming_queue_;

  // The next sequence number to use for delayed tasks.
  int next_sequence_num_;

  // True if Quit() has been called.
  bool quit_received_;

  // The members below are accessed only on the plugin thread.

  // Contains delayed tasks, sorted by their 'delayed_run_time' property.
  base::DelayedTaskQueue delayed_queue_;

  // The list of timestamps when scheduled timers are expected to fire.
  std::set<base::TimeTicks> scheduled_timers_;

  // True if the shutdown task loop was been stopped.
  bool stopped_;

  DISALLOW_COPY_AND_ASSIGN(PluginThreadTaskRunner);
};

}  // namespace remoting

#endif  // REMOTING_BASE_PLUGIN_THREAD_TASK_RUNNER_H_
