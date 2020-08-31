// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_CALCULATOR_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_CALCULATOR_H_

#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {
namespace responsiveness {

// This class receives execution latency on events and tasks, and uses that to
// estimate responsiveness.
//
// All members are UI-thread affine, with the exception of
// |janks_on_io_thread_| which is protected by |io_thread_lock_|.
class CONTENT_EXPORT Calculator {
 public:
  Calculator();
  virtual ~Calculator();

  // Must be called from the UI thread.
  // virtual for testing.
  // Assumes that |execution_finish_time| is the current time.
  // The implementation will gracefully handle successive calls with unordered
  // |queue_time|s.
  virtual void TaskOrEventFinishedOnUIThread(
      base::TimeTicks queue_time,
      base::TimeTicks execution_start_time,
      base::TimeTicks execution_finish_time);

  // Must be called from the IO thread.
  // virtual for testing.
  // The implementation will gracefully handle successive calls with unordered
  // |queue_time|s.
  virtual void TaskOrEventFinishedOnIOThread(
      base::TimeTicks queue_time,
      base::TimeTicks execution_start_time,
      base::TimeTicks execution_finish_time);

  // Change the Power state of the process. Must be called from the UI thread.
  void SetProcessSuspended(bool suspended);

  // Each janking task/event is fully defined by |start_time| and |end_time|.
  // Note that |duration| = |end_time| - |start_time|.
  struct Jank {
    Jank(base::TimeTicks start_time, base::TimeTicks end_time);

    base::TimeTicks start_time;
    base::TimeTicks end_time;
  };

  // Types of jank recorded by this Calculator. Public for testing.
  enum class JankType {
    kExecution,
    kQueueAndExecution,
  };

 protected:
  // Emits an UMA metric for responsiveness of a single measurement interval.
  // Exposed for testing.
  virtual void EmitResponsiveness(JankType jank_type,
                                  size_t janky_slices,
                                  bool was_process_suspended);

  // Exposed for testing.
  base::TimeTicks GetLastCalculationTime();

 private:
  using JankList = std::vector<Jank>;

  // If sufficient time has passed since the last calculation, then calculate
  // responsiveness again and update |last_calculation_time_|.
  //
  // We only trigger this from the UI thread since triggering it from the IO
  // thread would require us to grab the lock, which could cause contention. We
  // only need this to trigger every 30s or so, and we generally expect there to
  // be some activity on the UI thread if Chrome is actually in use.
  void CalculateResponsivenessIfNecessary(base::TimeTicks current_time);

  // Responsiveness is calculated by:
  //   1) Discretizing time into small intervals.
  //   2) In each interval, looking to see if there is a Janky event. If so, the
  //   interval is marked as |janky|.
  //   3) Computing the percentage of intervals that are janky.
  //
  // This method intentionally takes a std::vector<JankList>, as we may want to
  // extend it in the future to take JankLists from other threads/processes.
  void CalculateResponsiveness(
      JankType jank_type,
      std::vector<JankList> janks_from_multiple_threads,
      base::TimeTicks start_time,
      base::TimeTicks end_time);

  // Accessors for |execution_janks_on_ui_thread_| and
  // ||queue_and_execution_janks_on_ui_thread_|. Must be called from the UI
  // thread.
  JankList& GetExecutionJanksOnUIThread();
  JankList& GetQueueAndExecutionJanksOnUIThread();

#if defined(OS_ANDROID)
  // Callback invoked when the application state changes.
  void OnApplicationStateChanged(base::android::ApplicationState state);
#endif

  // This method:
  //   1) Removes all Janks with Jank.end_time < |end_time| from |janks|.
  //   2) Returns all Janks with Jank.start_time < |end_time|.
  JankList TakeJanksOlderThanTime(JankList* janks, base::TimeTicks end_time);

  // Used to generate a unique id when emitting Large Jank trace events
  int g_num_large_ui_janks_ = 0;
  int g_num_large_io_janks_ = 0;

  // Janks from tasks/events with a long execution time on the UI thread. Should
  // only be accessed via the accessor, which checks that the caller is on the
  // UI thread.
  JankList execution_janks_on_ui_thread_;

  // Janks from tasks/events with a long queueing + execution time on the UI
  // thread. Should only be accessed via the accessor, which checks that the
  // caller is on the UI thread.
  JankList queue_and_execution_janks_on_ui_thread_;

#if defined(OS_ANDROID)
  // Stores the current visibility state of the application. Accessed only on
  // the UI thread.
  bool is_application_visible_ = false;
#endif

  // Whether or not the process is suspended (Power management). Accessed only
  // on the UI thread.
  bool is_process_suspended_ = false;

  // Stores whether to process was suspended since last metric computation.
  // Accessed only on the UI thread.
  bool was_process_suspended_ = false;

  // We expect there to be low contention and this lock to cause minimal
  // overhead. If performance of this lock proves to be a problem, we can move
  // to a lock-free data structure.
  base::Lock io_thread_lock_;

  // Janks from tasks/events with a long execution time on the IO thread.
  JankList execution_janks_on_io_thread_ GUARDED_BY(io_thread_lock_);

  // Janks from tasks/events with a long queueing + execution time on the IO
  // thread.
  JankList queue_and_execution_janks_on_io_thread_ GUARDED_BY(io_thread_lock_);

  // The last time at which metrics were emitted. All janks older than this time
  // have been consumed. Newer janks are still in their JankLists waiting to be
  // consumed.
  base::TimeTicks last_calculation_time_;

  // This class keeps track of the time at which any activity occurred on the UI
  // thread. If a sufficiently long period of time passes without any activity,
  // then it's assumed that the process was suspended. In this case, we should
  // not emit any responsiveness metrics.
  //
  // Note that the process may be suspended while a task or event is being
  // executed, so a very long execution time should be treated similarly.
  base::TimeTicks most_recent_activity_time_;

#if defined(OS_ANDROID)
  // Listener for changes in application state, unregisters itself when
  // destroyed.
  const std::unique_ptr<base::android::ApplicationStatusListener>
      application_status_listener_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Calculator);
};

}  // namespace responsiveness
}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_CALCULATOR_H_
