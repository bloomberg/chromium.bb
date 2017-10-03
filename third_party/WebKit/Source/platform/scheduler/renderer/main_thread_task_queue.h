// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_TASK_QUEUE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_TASK_QUEUE_H_

#include "platform/scheduler/base/task_queue.h"

namespace blink {

class WebFrameScheduler;

namespace scheduler {

class RendererSchedulerImpl;

class PLATFORM_EXPORT MainThreadTaskQueue : public TaskQueue {
 public:
  enum class QueueType {
    // Keep MainThreadTaskQueue::NameForQueueType in sync.
    // This enum is used for a histogram and it should not be re-numbered.
    // TODO(altimin): Clean up obsolete names and use a new histogram when
    // the situation settles.
    CONTROL = 0,
    DEFAULT = 1,
    DEFAULT_LOADING = 2,
    // DEFAULT_TIMER is deprecated and should be replaced with appropriate
    // per-frame task queues.
    DEFAULT_TIMER = 3,
    UNTHROTTLED = 4,
    FRAME_LOADING = 5,
    // 6 : FRAME_THROTTLEABLE, replaced with FRAME_THROTTLEABLE.
    // 7 : FRAME_PAUSABLE, replaced with FRAME_PAUSABLE
    COMPOSITOR = 8,
    IDLE = 9,
    TEST = 10,
    FRAME_LOADING_CONTROL = 11,
    FRAME_THROTTLEABLE = 12,
    FRAME_DEFERRABLE = 13,
    FRAME_PAUSABLE = 14,
    FRAME_UNPAUSABLE = 15,

    COUNT = 16
  };

  // Returns name of the given queue type. Returned string has application
  // lifetime.
  static const char* NameForQueueType(QueueType queue_type);

  // High-level category used by RendererScheduler to make scheduling decisions.
  enum class QueueClass {
    NONE = 0,
    LOADING = 1,
    TIMER = 2,
    COMPOSITOR = 4,

    COUNT = 5
  };

  static QueueClass QueueClassForQueueType(QueueType type);

  struct QueueCreationParams {
    explicit QueueCreationParams(QueueType queue_type)
        : queue_type(queue_type),
          spec(NameForQueueType(queue_type)),
          can_be_blocked(false),
          can_be_throttled(false),
          can_be_paused(false),
          can_be_stopped(false),
          used_for_control_tasks(false) {}

    QueueCreationParams SetCanBeDeferred(bool value) {
      can_be_blocked = value;
      return *this;
    }

    QueueCreationParams SetCanBeThrottled(bool value) {
      can_be_throttled = value;
      return *this;
    }

    QueueCreationParams SetCanBePaused(bool value) {
      can_be_paused = value;
      return *this;
    }

    QueueCreationParams SetCanBeStopped(bool value) {
      can_be_stopped = value;
      return *this;
    }

    QueueCreationParams SetUsedForControlTasks(bool value) {
      used_for_control_tasks = value;
      return *this;
    }

    // Forwarded calls to |spec|.

    QueueCreationParams SetShouldMonitorQuiescence(bool should_monitor) {
      spec = spec.SetShouldMonitorQuiescence(should_monitor);
      return *this;
    }

    QueueCreationParams SetShouldNotifyObservers(bool run_observers) {
      spec = spec.SetShouldNotifyObservers(run_observers);
      return *this;
    }

    QueueCreationParams SetTimeDomain(TimeDomain* domain) {
      spec = spec.SetTimeDomain(domain);
      return *this;
    }

    QueueCreationParams SetShouldReportWhenExecutionBlocked(
        bool should_report) {
      spec = spec.SetShouldReportWhenExecutionBlocked(should_report);
      return *this;
    }

    QueueType queue_type;
    TaskQueue::Spec spec;
    WebFrameScheduler* frame_;
    bool can_be_blocked;
    bool can_be_throttled;
    bool can_be_paused;
    bool can_be_stopped;
    bool used_for_control_tasks;
  };

  ~MainThreadTaskQueue() override;

  QueueType queue_type() const { return queue_type_; }

  QueueClass queue_class() const { return queue_class_; }

  bool CanBeDeferred() const { return can_be_blocked_; }

  bool CanBeThrottled() const { return can_be_throttled_; }

  bool CanBePaused() const { return can_be_paused_; }

  bool CanBeStopped() const { return can_be_stopped_; }

  bool UsedForControlTasks() const { return used_for_control_tasks_; }

  void OnTaskStarted(const TaskQueue::Task& task, base::TimeTicks start);

  void OnTaskCompleted(const TaskQueue::Task& task,
                       base::TimeTicks start,
                       base::TimeTicks end);

  // Override base method to notify RendererScheduler about unregistered queue.
  void UnregisterTaskQueue() override;

  WebFrameScheduler* GetFrameScheduler() const;
  void SetFrameScheduler(WebFrameScheduler* frame);

 protected:
  MainThreadTaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl,
                      const QueueCreationParams& params,
                      RendererSchedulerImpl* renderer_scheduler);

 private:
  friend class TaskQueueManager;

  QueueType queue_type_;
  QueueClass queue_class_;
  const bool can_be_blocked_;
  const bool can_be_throttled_;
  const bool can_be_paused_;
  const bool can_be_stopped_;
  const bool used_for_control_tasks_;

  // Needed to notify renderer scheduler about completed tasks.
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED

  WebFrameScheduler* web_frame_scheduler_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(MainThreadTaskQueue);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_TASK_QUEUE_H_
