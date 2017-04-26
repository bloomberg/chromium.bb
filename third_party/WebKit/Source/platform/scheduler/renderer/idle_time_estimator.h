// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_IDLE_TIME_ESTIMATOR_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_IDLE_TIME_ESTIMATOR_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/tick_clock.h"
#include "cc/base/rolling_time_delta_history.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/base/task_queue.h"

namespace blink {
namespace scheduler {

// Estimates how much idle time there is available.  Ignores nested tasks.
class PLATFORM_EXPORT IdleTimeEstimator
    : public base::MessageLoop::TaskObserver {
 public:
  IdleTimeEstimator(const scoped_refptr<TaskQueue>& compositor_task_runner,
                    base::TickClock* time_source,
                    int sample_count,
                    double estimation_percentile);

  ~IdleTimeEstimator() override;

  // Expected Idle time is defined as: |compositor_frame_interval| minus
  // expected compositor task duration.
  base::TimeDelta GetExpectedIdleDuration(
      base::TimeDelta compositor_frame_interval) const;

  void DidCommitFrameToCompositor();

  void Clear();

  // TaskObserver implementation:
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

 private:
  scoped_refptr<TaskQueue> compositor_task_queue_;
  cc::RollingTimeDeltaHistory per_frame_compositor_task_runtime_;
  base::TickClock* time_source_;  // NOT OWNED
  double estimation_percentile_;

  base::TimeTicks task_start_time_;
  base::TimeTicks prev_commit_time_;
  base::TimeDelta cumulative_compositor_runtime_;
  int nesting_level_;
  bool did_commit_;

  DISALLOW_COPY_AND_ASSIGN(IdleTimeEstimator);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_IDLE_TIME_ESTIMATOR_H_
