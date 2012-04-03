/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/portability.h"

#include <time.h>
#include <vector>

// TODO(robertm): ConditionalVariable should be an abstract base class
#if NACL_WINDOWS
# include "native_client/src/shared/platform/win/condition_variable.h"
#elif NACL_LINUX || NACL_OSX
# include "native_client/src/shared/platform/posix/condition_variable.h"
#endif

#include "gtest/gtest.h"

namespace {

#define SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(tsp, cond) \
  for (int i =0; i < (tsp).InMilliseconds()/10; i++) \
    if (cond) \
      break; \
    else \
      Sleep(10);


TEST(ConditionVariableTest, StartupShutdownTest) {
  const NaCl::TimeDelta TEN_MS = NaCl::TimeDelta::FromMilliseconds(10);
  NaCl::Lock lock;

  // First try trivial startup/shutdown.
  NaCl::ConditionVariable* cv = new NaCl::ConditionVariable();
  delete cv;

  // Exercise with at least a few waits.
  cv = new NaCl::ConditionVariable();
  /* SCOPE */ {
    NaCl::AutoLock auto_lock(lock);
    cv->TimedWaitRel(lock, TEN_MS);  // Wait for 10 ms.
    cv->TimedWaitRel(lock, TEN_MS);  // Wait for 10 ms.
  }
  /* SCOPE */ {
    NaCl::AutoLock auto_lock(lock);
    cv->TimedWaitRel(lock, TEN_MS);  // Wait for 10 ms.
    cv->TimedWaitRel(lock, TEN_MS);  // Wait for 10 ms.
    cv->TimedWaitRel(lock, TEN_MS);  // Wait for 10 ms.
  }
  delete cv;
}


TEST(ConditionVariableTest, LockedExpressionTest) {
  int i = 0;
  NaCl::Lock lock;

  // Old LOCKED_EXPRESSION macro caused syntax errors here.
  // ... yes... compiler will optimize this example.
  // Syntax error is what I'm after precluding.
  if (0)
    LOCKED_EXPRESSION(lock, i = 1);
  else
    LOCKED_EXPRESSION(lock, i = 2);

  EXPECT_EQ(2, i);
}

TEST(ConditionVariableTest, TimeoutTest) {
  NaCl::Lock lock;
  NaCl::ConditionVariable cv;

  /* SCOPE */ {
    NaCl::AutoLock auto_lock(lock);
    NaCl::TimeTicks start = NaCl::TimeTicks::Now();
    const NaCl::TimeDelta WAIT_TIME = NaCl::TimeDelta::FromMilliseconds(300);
    // Allow for clocking rate granularity.
    const NaCl::TimeDelta FUDGE_TIME = NaCl::TimeDelta::FromMilliseconds(50);

    cv.TimedWaitRel(lock, WAIT_TIME + FUDGE_TIME);
    NaCl::TimeDelta duration = NaCl::TimeTicks::Now() - start;
    EXPECT_TRUE(duration >= WAIT_TIME);
  }
}


// TODO(gregoryd) - make the code below use cross-platform thread functions
#if NACL_WINDOWS
// Forward declare the worker_process task
DWORD WINAPI worker_process(void* p);

// Caller is responsible for synchronizing access to the following class.
// The cs_ member should be used for all synchronized access.
class WorkQueue {
 public:
  explicit WorkQueue(int thread_count)
    : thread_count_(thread_count),
      shutdown_task_count_(0),
      assignment_history_(thread_count),
      completion_history_(thread_count),
      handles_(new HANDLE[thread_count]) {
        EXPECT_GE(thread_count_, 1);
        ResetHistory();
        SetTaskCount(0);
        SetWorkTime(NaCl::TimeDelta::FromMilliseconds(30));
        thread_started_counter_ = 0;
        allow_help_requests_ = false;
        shutdown_ = false;
        performance_ = true;
      for (int i = 0; i < thread_count_; i++) {
        handles_[i] = CreateThread(NULL,  // security.
                                  0,     // <64K stack size.
                                  worker_process,  // Static function.
                                  reinterpret_cast<void*>(this),
                                  0,      // Create running process.
                                  NULL);  // OS version of thread id.
        EXPECT_TRUE(NULL != handles_[i]);
      }
    }

  ~WorkQueue() {
    /* SCOPE */ {
      NaCl::AutoLock auto_lock(lock_);
      SetShutdown();
    }
    work_is_available_.Broadcast();  // Tell them all to terminate.

    DWORD result = WaitForMultipleObjects(
                      thread_count_,
                      &handles_[0],
                      true,  // Wait for all
                      10000);  // Ten seconds max.

    for (int i = 0; i < thread_count_; i++) {
      int ret_value = CloseHandle(handles_[i]);
      // CHECK(0 != ret_value);
      handles_[i] = NULL;
    }
    delete[] handles_;
  }

  // Worker threads only call the following six methods.
  // They should use the lock to get exclusive access.
  int GetThreadId() {
    // DCHECK(!EveryIdWasAllocated());
    return thread_started_counter_++;  // Give out Unique IDs.
  }

  bool EveryIdWasAllocated() {
    return thread_count_ == thread_started_counter_;
  }

  NaCl::TimeDelta GetAnAssignment(int thread_id) {
    // DCHECK(0 < task_count_);
    assignment_history_[thread_id]++;
    if (0 == --task_count_) {
      no_more_tasks_.Signal();
    };
    return worker_delay_;
  }

  void WorkIsCompleted(int thread_id) {
    completion_history_[thread_id]++;
  }

  int GetRemainingTaskCount() {return task_count_;}
  bool GetAllowHelp() {return allow_help_requests_;}
  bool shutdown() {return shutdown_;}
  void thread_shutting_down() {shutdown_task_count_++;}
  int shutdown_task_count() { return shutdown_task_count_; }

  // Both worker threads and controller use the following to synchronize.
  NaCl::Lock lock_;
  NaCl::Lock* lock() { return &lock_; }
  NaCl::ConditionVariable work_is_available_;  // To tell threads there is work.
  NaCl::ConditionVariable* work_is_available() {
    return &work_is_available_;
  }

  // Conditions to tell the controlling process (if it is interested)
  NaCl::ConditionVariable all_threads_have_ids_;  // All threads are running.
  NaCl::ConditionVariable* all_threads_have_ids() {
    return &all_threads_have_ids_;
  }
  NaCl::ConditionVariable no_more_tasks_;  // Task count is zero.
  NaCl::ConditionVariable* no_more_tasks() { return &no_more_tasks_; }

  //-------------------------------------------------------------------
  // The rest of the methods are for the controlling master
  // thread.
  void ResetHistory() {
    for (int i = 0; i < thread_count_; i++) {
      assignment_history_[i] = 0;
      completion_history_[i] = 0;
    }
  }

  int GetMinCompletionsByWorkerThread() {
    int min = completion_history_[0];
    for (int i = 0; i < thread_count_; i++)
      if (min > completion_history_[i]) min = completion_history_[i];
    return min;
  }

  int GetMaxCompletionsByWorkerThread() {
    int max = completion_history_[0];
    for (int i = 0; i < thread_count_; i++)
      if (max < completion_history_[i]) max = completion_history_[i];
    return max;
  }

  int GetNumThreadsTakingAssignments() {
    int count = 0;
    for (int i = 0; i < thread_count_; i++)
      if (assignment_history_[i])
        count++;
    return count;
  }

  int GetNumThreadsCompletingTasks() {
    int count = 0;
    for (int i = 0; i < thread_count_; i++)
      if (completion_history_[i])
        count++;
    return count;
  }

  int GetNumberOfCompletedTasks() {
    int total = 0;
    for (int i = 0; i < thread_count_; i++)
      total += completion_history_[i];
    return total;
  }

  void SetPerformance(bool performance) { performance_ = performance; }
  bool performance() { return performance_; }

  void SetWorkTime(NaCl::TimeDelta delay) { worker_delay_ = delay; }
  void SetTaskCount(int count) { task_count_ = count; }
  void SetAllowHelp(bool allow) { allow_help_requests_ = allow; }
  void SetShutdown() { shutdown_ = true; }

 private:
  const int thread_count_;
  HANDLE *handles_;
  std::vector<int> assignment_history_;  // Number of assignment per worker.
  std::vector<int> completion_history_;  // Number of completions per worker.
  int thread_started_counter_;  // Used to issue unique id to workers.
  int shutdown_task_count_;  // Number of tasks told to shutdown
  int task_count_;  // Number of assignment tasks waiting to be processed.
  NaCl::TimeDelta worker_delay_;  // Time each task takes to complete.
  bool allow_help_requests_;  // Workers can signal more workers.
  bool shutdown_;  // Set when threads need to terminate.
  bool performance_;  // performance vs fairness in thread waking and waiting.
};

// The remaining tests involve several threads with a task to perform
// as directed in the preceding class WorkQueue.
// The task is to:
// a) make sure there are more tasks (there is a task counter).
//    a1) Wait on condition variable if there are no tasks currently.
// b) Call a function to see what should be done.
// c) Do some computation based on the number of milliseconds returned in (b).
// d) go back to (a).

// worker_process() implements the above task for all threads.
// It calls the controlling object to tell the creator about progress,
// and to ask about tasks.
DWORD WINAPI worker_process(void* p) {
  NaCl::Lock private_lock;  // Used to waste time on "our work".
  int thread_id;

  class WorkQueue* queue = reinterpret_cast<WorkQueue*>(p);
  /* SCOPE */ {
    NaCl::AutoLock auto_lock(*queue->lock());
    thread_id = queue->GetThreadId();
    if (queue->EveryIdWasAllocated())
      queue->all_threads_have_ids_.Signal();  // Tell creator we're ready.
  }

  while (1) {  // This is the main consumer loop.
    NaCl::TimeDelta work_time;
    bool could_use_help;
    /* SCOPE */ {
      NaCl::AutoLock auto_lock(*queue->lock());
      if (queue->performance()) {
        // Get best performance by not waiting until there is no work.
        while (0 == queue->GetRemainingTaskCount() && !queue->shutdown()) {
          queue->work_is_available_.Wait(*queue->lock());
        }
      } else {
        // Be ridiculously "fair" and try to let other threads go. :-/.
        do {
          queue->work_is_available_.Wait(*queue->lock());
        } while (0 == queue->GetRemainingTaskCount() && !queue->shutdown());
      }
      if (queue->shutdown()) {
        queue->thread_shutting_down();
        return 0;  // termination time.
      }
      work_time = queue->GetAnAssignment(thread_id);
      could_use_help = (queue->GetRemainingTaskCount() > 0) &&
          queue->GetAllowHelp();
    }  // Release lock

    // Do work (out of true critical region, consisting of waiting :-).
    if (could_use_help)
      queue->work_is_available_.Signal();  // Get help from other threads.

    if (work_time > NaCl::TimeDelta::FromMilliseconds(0)) {
      NaCl::AutoLock auto_lock(private_lock);
      NaCl::ConditionVariable private_cv;
      private_cv.TimedWaitRel(private_lock, work_time);
    }

    /* SCOPE */ {
      NaCl::AutoLock auto_lock(*queue->lock());
      queue->WorkIsCompleted(thread_id);
    }
  }
}

#if 0
/* TODO(gregoryd) - This test fails periodically, fix it */
TEST(ConditionVariableTest, MultiThreadConsumerTest) {
  const int     kThreadCount = 10;
  WorkQueue     queue(kThreadCount);  // Start the threads.

  NaCl::Lock private_lock;  // Used locally for master to wait.
  NaCl::AutoLock private_held_lock(private_lock);
  NaCl::ConditionVariable private_cv;

  const NaCl::TimeDelta ZERO_MS = NaCl::TimeDelta::FromMilliseconds(0);
  const NaCl::TimeDelta TEN_MS = NaCl::TimeDelta::FromMilliseconds(10);
  const NaCl::TimeDelta THIRTY_MS = NaCl::TimeDelta::FromMilliseconds(30);
  const NaCl::TimeDelta FORTY_FIVE_MS = NaCl::TimeDelta::FromMilliseconds(45);
  const NaCl::TimeDelta SIXTY_MS = NaCl::TimeDelta::FromMilliseconds(60);
  const NaCl::TimeDelta ONE_HUNDRED_MS = NaCl::TimeDelta::FromMilliseconds(100);

  /* SCOPE */ {
    NaCl::AutoLock auto_lock(*queue.lock());
    while (!queue.EveryIdWasAllocated())
      queue.all_threads_have_ids_.Wait(*queue.lock());
  }

  // Wait a bit more to allow threads to reach their wait state.
  private_cv.TimedWaitRel(private_lock, TEN_MS);

  /* SCOPE */ {
    // Since we have no tasks, all threads should be waiting by now.
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(0, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(0, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.GetRemainingTaskCount());
    EXPECT_EQ(0, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetNumberOfCompletedTasks());

    // Set up to make one worker do 3 30ms tasks.
    queue.ResetHistory();
    queue.SetTaskCount(3);
    queue.SetWorkTime(THIRTY_MS);
    queue.SetAllowHelp(false);
  }
  queue.work_is_available()->Signal();  // Start up one thread.
  // Wait to allow solo worker insufficient time to get done.
  // Should take about 90 ms.
  private_cv.TimedWaitRel(private_lock, FORTY_FIVE_MS);

  /* SCOPE */ {
    // Check that all work HASN'T completed yet.
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(1, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(1, queue.GetNumThreadsCompletingTasks());
    EXPECT_GT(2, queue.GetRemainingTaskCount());  // 2 should have started.
    EXPECT_GT(3, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(1, queue.GetNumberOfCompletedTasks());
  }
  // Wait to allow solo workers to get done.
  // Should take about 45ms more.
  private_cv.TimedWaitRel(private_lock, SIXTY_MS);

  /* SCOPE */ {
    // Check that all work was done by one thread id.
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(1, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(1, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.GetRemainingTaskCount());
    EXPECT_EQ(3, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(3, queue.GetNumberOfCompletedTasks());

    // Set up to make each task include getting help from another worker.
    queue.ResetHistory();
    queue.SetTaskCount(3);
    queue.SetWorkTime(THIRTY_MS);
    queue.SetAllowHelp(true);
  }
  queue.work_is_available()->Signal();  // But each worker can signal another.
  // Wait to allow the 3 workers to get done.
  // Should  take about 30 ms.
  private_cv.TimedWaitRel(private_lock, FORTY_FIVE_MS);
  /* SCOPE */ {
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(3, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(3, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.GetRemainingTaskCount());
    EXPECT_EQ(1, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(3, queue.GetNumberOfCompletedTasks());

    // Try to ask all workers to help, and only a few will do the work.
    queue.ResetHistory();
    queue.SetTaskCount(3);
    queue.SetWorkTime(THIRTY_MS);
    queue.SetAllowHelp(false);
  }
  queue.work_is_available()->Broadcast();  // Make them all try.
  // Wait to allow the 3 workers to get done.
  private_cv.TimedWaitRel(private_lock, FORTY_FIVE_MS);

  /* SCOPE */ {
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(3, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(3, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.GetRemainingTaskCount());
    EXPECT_EQ(1, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(3, queue.GetNumberOfCompletedTasks());

    // Set up to make each task get help from another worker.
    queue.ResetHistory();
    queue.SetTaskCount(3);
    queue.SetWorkTime(THIRTY_MS);
    queue.SetAllowHelp(true);  // Allow (unnecessary) help requests.
  }
  queue.work_is_available()->Broadcast();  // We already signal all threads.
  // Wait to allow the 3 workers to get done.
  private_cv.TimedWaitRel(private_lock, ONE_HUNDRED_MS);

  /* SCOPE */ {
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(3, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(3, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.GetRemainingTaskCount());
    EXPECT_EQ(1, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(3, queue.GetNumberOfCompletedTasks());

    // Set up to make each task get help from another worker.
    queue.ResetHistory();
    queue.SetTaskCount(20);
    queue.SetWorkTime(THIRTY_MS);
    queue.SetAllowHelp(true);
  }
  queue.work_is_available()->Signal();  // But each worker can signal another.
  // Wait to allow the 10 workers to get done.
  // Should take about 60 ms.
  private_cv.TimedWaitRel(private_lock, ONE_HUNDRED_MS);

  /* SCOPE */ {
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(10, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(10, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.GetRemainingTaskCount());
    EXPECT_EQ(2, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(2, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(20, queue.GetNumberOfCompletedTasks());

    // Same as last test, but with Broadcast().
    queue.ResetHistory();
    queue.SetTaskCount(20);  // 2 tasks per process.
    queue.SetWorkTime(THIRTY_MS);
    queue.SetAllowHelp(true);
  }
  queue.work_is_available()->Broadcast();
  // Wait to allow the 10 workers to get done.
  // Should take about 60 ms.
  private_cv.TimedWaitRel(private_lock, ONE_HUNDRED_MS);

  /* SCOPE */ {
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(10, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(10, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.GetRemainingTaskCount());
    EXPECT_EQ(2, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(2, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(20, queue.GetNumberOfCompletedTasks());

    queue.SetShutdown();
  }
  queue.work_is_available()->Broadcast();  // Force check for shutdown.

  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(NaCl::TimeDelta::FromMinutes(1),
                                  queue.shutdown_task_count() == kThreadCount);
  Sleep(10);  // Be sure they're all shutdown.
}

#endif /* disabled MultiThreadConsumerTest */

TEST(ConditionVariableTest, LargeFastTaskTest) {
  const int kThreadCount = 200;
  WorkQueue queue(kThreadCount);  // Start the threads.

  NaCl::Lock private_lock;  // Used locally for master to wait.
  NaCl::AutoLock private_held_lock(private_lock);
  NaCl::ConditionVariable private_cv;

  const NaCl::TimeDelta ZERO_MS = NaCl::TimeDelta::FromMilliseconds(0);
  const NaCl::TimeDelta TEN_MS = NaCl::TimeDelta::FromMilliseconds(10);
  const NaCl::TimeDelta THIRTY_MS = NaCl::TimeDelta::FromMilliseconds(30);
  const NaCl::TimeDelta FORTY_FIVE_MS = NaCl::TimeDelta::FromMilliseconds(45);
  const NaCl::TimeDelta SIXTY_MS = NaCl::TimeDelta::FromMilliseconds(60);
  const NaCl::TimeDelta ONE_HUNDRED_MS = NaCl::TimeDelta::FromMilliseconds(100);

  /* SCOPE */ {
    NaCl::AutoLock auto_lock(*queue.lock());
    while (!queue.EveryIdWasAllocated())
      queue.all_threads_have_ids_.Wait(*queue.lock());
  }

  // Wait a bit more to allow threads to reach their wait state.
  private_cv.TimedWaitRel(private_lock, THIRTY_MS);

  /* SCOPE */ {
    // Since we have no tasks, all threads should be waiting by now.
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(0, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(0, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.GetRemainingTaskCount());
    EXPECT_EQ(0, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetNumberOfCompletedTasks());

    // Set up to make all workers do (an average of) 20 tasks.
    queue.ResetHistory();
    queue.SetTaskCount(20 * kThreadCount);
    queue.SetWorkTime(FORTY_FIVE_MS);
    queue.SetAllowHelp(false);
  }
  queue.work_is_available()->Broadcast();  // Start up all threads.
  // Wait until we've handed out all tasks.
  /* SCOPE */ {
    NaCl::AutoLock auto_lock(*queue.lock());
    while (queue.GetRemainingTaskCount() != 0)
      queue.no_more_tasks()->Wait(*queue.lock());
  }

  // Wait till the last of the tasks complete.
  // Don't bother to use locks: We may not get info in time... but we'll see it
  // eventually.
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(NaCl::TimeDelta::FromMinutes(1),
                                    20 * kThreadCount ==
                                      queue.GetNumberOfCompletedTasks());

  /* SCOPE */ {
    // With Broadcast(), every thread should have participated.
    // but with racing.. they may not all have done equal numbers of tasks.
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(kThreadCount, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(kThreadCount, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.GetRemainingTaskCount());
    EXPECT_LE(20, queue.GetMaxCompletionsByWorkerThread());
    // EXPECT_LE(1, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(20 * kThreadCount, queue.GetNumberOfCompletedTasks());

    // Set up to make all workers do (an average of) 4 tasks.
    queue.ResetHistory();
    queue.SetTaskCount(kThreadCount * 4);
    queue.SetWorkTime(FORTY_FIVE_MS);
    queue.SetAllowHelp(true);  // Might outperform Broadcast().
  }
  queue.work_is_available()->Signal();  // Start up one thread.

  // Wait until we've handed out all tasks
  /* SCOPE */ {
    NaCl::AutoLock auto_lock(*queue.lock());
    while (queue.GetRemainingTaskCount() != 0)
      queue.no_more_tasks()->Wait(*queue.lock());
  }

  // Wait till the last of the tasks complete.
  // Don't bother to use locks: We may not get info in time... but we'll see it
  // eventually.
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(NaCl::TimeDelta::FromMinutes(1),
                                    4 * kThreadCount ==
                                      queue.GetNumberOfCompletedTasks());

  /* SCOPE */ {
    // With Signal(), every thread should have participated.
    // but with racing.. they may not all have done four tasks.
    NaCl::AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(kThreadCount, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(kThreadCount, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.GetRemainingTaskCount());
    EXPECT_LE(4, queue.GetMaxCompletionsByWorkerThread());
    // EXPECT_LE(1, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(4 * kThreadCount, queue.GetNumberOfCompletedTasks());

    queue.SetShutdown();
  }
  queue.work_is_available()->Broadcast();  // Force check for shutdown.

  // Wait for shutdows to complete.
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(NaCl::TimeDelta::FromMinutes(1),
                                    queue.shutdown_task_count() ==
                                      kThreadCount);
  Sleep(10);  // Be sure they're all shutdown.
}

#endif  // NACL_WINDOWS

}  // namespace
