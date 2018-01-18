// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_QUEUEING_TIME_ESTIMATOR_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_QUEUEING_TIME_ESTIMATOR_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/renderer/main_thread_task_queue.h"
#include "platform/scheduler/renderer/renderer_metrics_helper.h"
#include "third_party/WebKit/common/page/launching_process_state.h"

#include <array>
#include <vector>

namespace blink {
namespace scheduler {

// Records the expected queueing time for a high priority task occurring
// randomly during each interval of length equal to window's duration.
class PLATFORM_EXPORT QueueingTimeEstimator {
 public:
  class PLATFORM_EXPORT Client {
   public:
    virtual void OnQueueingTimeForWindowEstimated(base::TimeDelta queueing_time,
                                                  bool is_disjoint_window) = 0;
    virtual void OnReportFineGrainedExpectedQueueingTime(
        const char* split_description,
        base::TimeDelta queueing_time) = 0;
    Client() = default;
    virtual ~Client() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  class RunningAverage {
   public:
    explicit RunningAverage(int steps_per_window);
    int GetStepsPerWindow() const;
    void Add(base::TimeDelta bin_value);
    base::TimeDelta GetAverage() const;
    bool IndexIsZero() const;

   private:
    size_t index_;
    std::vector<base::TimeDelta> circular_buffer_;
    base::TimeDelta running_sum_;
  };

  class PLATFORM_EXPORT Calculator {
   public:
    explicit Calculator(int steps_per_window);
    static const char* GetReportingMessageFromQueueType(
        MainThreadTaskQueue::QueueType queue_type);
    static const char* GetReportingMessageFromFrameStatus(
        FrameStatus frame_status);

    void UpdateStatusFromTaskQueue(MainThreadTaskQueue* queue);
    void AddQueueingTime(base::TimeDelta queuing_time);
    void EndStep(Client* client);
    void ResetStep();

   private:
    // Variables to compute the total Expected Queueing Time.
    // |steps_per_window_| is the ratio of window duration to the sliding
    // window's step width. It is an integer since the window must be a integer
    // multiple of the step's width. This parameter is used for deciding the
    // sliding window's step width, and the number of bins of the circular
    // buffer.
    const int steps_per_window_;

    // |step_expected_queueing_time_| is the expected queuing time of a
    // smaller window of a step's width. By combining these step EQTs through a
    // running average, we can get window EQTs of a bigger window.
    //
    // ^ Instantaneous queuing time
    // |
    // |
    // |   |\                                           .
    // |   | \            |\             |\             .
    // |   |  \           | \       |\   | \            .
    // |   |   \    |\    |  \      | \  |  \           .
    // |   |    \   | \   |   \     |  \ |   \          .
    // ------------------------------------------------> Time
    //
    // |stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|
    //
    // |------windowEQT_1------|
    //         |------windowEQT_2------|
    //                 |------windowEQT_3------|
    //
    // In this case:
    // |steps_per_window_| = 3, because each window is the length of 3 steps.
    base::TimeDelta step_expected_queueing_time_;
    RunningAverage step_queueing_times_;

    // Variables to split Expected Queueing Time by task queue type.
    std::array<base::TimeDelta,
               static_cast<int>(MainThreadTaskQueue::QueueType::kCount)>
        eqt_by_queue_type_;
    MainThreadTaskQueue::QueueType current_queue_type_ =
        MainThreadTaskQueue::QueueType::kOther;

    // Variables to split Expected Queueing Time by frame type.
    std::array<base::TimeDelta, static_cast<int>(FrameStatus::kCount)>
        eqt_by_frame_status_;
    FrameStatus current_frame_status_ = FrameStatus::kNone;
  };

  class State {
   public:
    explicit State(int steps_per_window);
    void OnTopLevelTaskStarted(Client* client,
                               base::TimeTicks task_start_time,
                               MainThreadTaskQueue* queue);
    void OnTopLevelTaskCompleted(Client* client, base::TimeTicks task_end_time);
    void OnBeginNestedRunLoop();
    void OnRendererStateChanged(Client* client,
                                bool backgrounded,
                                base::TimeTicks transition_time);

    base::TimeDelta window_step_width;
    base::TimeTicks step_start_time;
    base::TimeTicks current_task_start_time;
    // |renderer_backgrounded| is the renderer's current status.
    bool renderer_backgrounded = kLaunchingProcessIsBackgrounded;
    bool processing_task = false;
    Calculator calculator_;

   private:
    void AdvanceTime(Client* client, base::TimeTicks current_time);
    bool TimePastStepEnd(base::TimeTicks task_end_time);
    bool in_nested_message_loop_ = false;
  };

  QueueingTimeEstimator(Client* client,
                        base::TimeDelta window_duration,
                        int steps_per_window);
  explicit QueueingTimeEstimator(const State& state);

  void OnTopLevelTaskStarted(base::TimeTicks task_start_time,
                             MainThreadTaskQueue* queue);
  void OnTopLevelTaskCompleted(base::TimeTicks task_end_time);
  void OnBeginNestedRunLoop();
  void OnRendererStateChanged(bool backgrounded,
                              base::TimeTicks transition_time);

  // Returns all state except for the current |client_|.
  const State& GetState() const { return state_; }

  base::TimeDelta EstimateQueueingTimeIncludingCurrentTask(
      base::TimeTicks now) const;

 private:
  Client* client_;  // NOT OWNED.
  State state_;

  DISALLOW_ASSIGN(QueueingTimeEstimator);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_QUEUEING_TIME_ESTIMATOR_H_
