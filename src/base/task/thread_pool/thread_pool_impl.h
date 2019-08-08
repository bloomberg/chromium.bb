// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_THREAD_POOL_IMPL_H_
#define BASE_TASK_THREAD_POOL_THREAD_POOL_IMPL_H_

#include <memory>
#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/delayed_task_manager.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/scheduler_single_thread_task_runner_manager.h"
#include "base/task/thread_pool/scheduler_task_runner_delegate.h"
#include "base/task/thread_pool/scheduler_worker_pool_impl.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/updateable_sequenced_task_runner.h"
#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
#include "base/task/thread_pool/task_tracker_posix.h"
#endif

#if defined(OS_WIN)
#include "base/task/thread_pool/platform_native_worker_pool_win.h"
#include "base/win/com_init_check_hook.h"
#endif

#if defined(OS_MACOSX)
#include "base/task/thread_pool/platform_native_worker_pool_mac.h"
#endif

namespace base {

class Thread;

namespace internal {

// Default ThreadPool implementation. This class is thread-safe.
class BASE_EXPORT ThreadPoolImpl : public ThreadPool,
                                   public SchedulerWorkerPool::Delegate,
                                   public SchedulerTaskRunnerDelegate {
 public:
  using TaskTrackerImpl =
#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
      TaskTrackerPosix;
#else
      TaskTracker;
#endif

  // Creates a ThreadPoolImpl with a production TaskTracker.
  //|histogram_label| is used to label histograms, it must not be empty.
  explicit ThreadPoolImpl(StringPiece histogram_label);

  // For testing only. Creates a ThreadPoolImpl with a custom TaskTracker.
  ThreadPoolImpl(StringPiece histogram_label,
                 std::unique_ptr<TaskTrackerImpl> task_tracker);

  ~ThreadPoolImpl() override;

  // ThreadPool:
  void Start(const ThreadPool::InitParams& init_params,
             SchedulerWorkerObserver* scheduler_worker_observer) override;
  int GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
      const TaskTraits& traits) const override;
  void Shutdown() override;
  void FlushForTesting() override;
  void FlushAsyncForTesting(OnceClosure flush_callback) override;
  void JoinForTesting() override;
  void SetCanRun(bool can_run) override;
  void SetCanRunBestEffort(bool can_run_best_effort) override;

  // TaskExecutor:
  bool PostDelayedTaskWithTraits(const Location& from_here,
                                 const TaskTraits& traits,
                                 OnceClosure task,
                                 TimeDelta delay) override;
  scoped_refptr<TaskRunner> CreateTaskRunnerWithTraits(
      const TaskTraits& traits) override;
  scoped_refptr<SequencedTaskRunner> CreateSequencedTaskRunnerWithTraits(
      const TaskTraits& traits) override;
  scoped_refptr<SingleThreadTaskRunner> CreateSingleThreadTaskRunnerWithTraits(
      const TaskTraits& traits,
      SingleThreadTaskRunnerThreadMode thread_mode) override;
#if defined(OS_WIN)
  scoped_refptr<SingleThreadTaskRunner> CreateCOMSTATaskRunnerWithTraits(
      const TaskTraits& traits,
      SingleThreadTaskRunnerThreadMode thread_mode) override;
#endif  // defined(OS_WIN)
  scoped_refptr<UpdateableSequencedTaskRunner>
  CreateUpdateableSequencedTaskRunnerWithTraitsForTesting(
      const TaskTraits& traits);

 private:
  // Invoked after |can_run_| or |can_run_best_effort_| is updated. Sets the
  // CanRunPolicy in TaskTracker and wakes up workers as appropriate.
  void UpdateCanRunPolicy();

  // Returns |traits|, with priority set to TaskPriority::USER_BLOCKING if
  // |all_tasks_user_blocking_| is set.
  TaskTraits SetUserBlockingPriorityIfNeeded(TaskTraits traits) const;

  void ReportHeartbeatMetrics() const;

  // Returns the thread pool responsible for foreground execution.
  const SchedulerWorkerPool* GetForegroundWorkerPool() const;
  SchedulerWorkerPool* GetForegroundWorkerPool();

  const SchedulerWorkerPool* GetWorkerPoolForTraits(
      const TaskTraits& traits) const;

  // SchedulerWorkerPool::Delegate:
  SchedulerWorkerPool* GetWorkerPoolForTraits(
      const TaskTraits& traits) override;

  // SchedulerTaskRunnerDelegate:
  bool PostTaskWithSequence(Task task,
                            scoped_refptr<Sequence> sequence) override;
  bool IsRunningPoolWithTraits(const TaskTraits& traits) const override;
  void UpdatePriority(scoped_refptr<Sequence> sequence,
                      TaskPriority priority) override;

  const std::unique_ptr<TaskTrackerImpl> task_tracker_;
  std::unique_ptr<Thread> service_thread_;
  DelayedTaskManager delayed_task_manager_;
  SchedulerSingleThreadTaskRunnerManager single_thread_task_runner_manager_;

  // Indicates that all tasks are handled as if they had been posted with
  // TaskPriority::USER_BLOCKING. Since this is set in Start(), it doesn't apply
  // to tasks posted before Start() or to tasks posted to TaskRunners created
  // before Start().
  //
  // TODO(fdoray): Remove after experiment. https://crbug.com/757022
  AtomicFlag all_tasks_user_blocking_;

  Optional<SchedulerWorkerPoolImpl> foreground_pool_;
  Optional<SchedulerWorkerPoolImpl> background_pool_;

  // Whether this TaskScheduler was started. Access controlled by
  // |sequence_checker_|.
  bool started_ = false;

  // Whether starting to run a Task with any/BEST_EFFORT priority is currently
  // allowed. Access controlled by |sequence_checker_|.
  bool can_run_ = true;
  bool can_run_best_effort_;

#if defined(OS_WIN)
  Optional<PlatformNativeWorkerPoolWin> native_foreground_pool_;
#elif defined(OS_MACOSX)
  Optional<PlatformNativeWorkerPoolMac> native_foreground_pool_;
#endif

#if DCHECK_IS_ON()
  // Set once JoinForTesting() has returned.
  AtomicFlag join_for_testing_returned_;
#endif

#if defined(OS_WIN) && defined(COM_INIT_CHECK_HOOK_ENABLED)
  // Provides COM initialization verification for supported builds.
  base::win::ComInitCheckHook com_init_check_hook_;
#endif

  // Asserts that operations occur in sequence with Start().
  SEQUENCE_CHECKER(sequence_checker_);

  TrackedRefFactory<SchedulerWorkerPool::Delegate> tracked_ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolImpl);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_THREAD_POOL_IMPL_H_
