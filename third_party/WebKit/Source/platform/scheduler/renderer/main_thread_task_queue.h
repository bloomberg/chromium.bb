// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_TASK_QUEUE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_TASK_QUEUE_H_

#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/base/task_queue_impl.h"

namespace blink {

class FrameScheduler;

namespace scheduler {

class RendererSchedulerImpl;

class PLATFORM_EXPORT MainThreadTaskQueue : public TaskQueue {
 public:
  enum class QueueType {
    // Keep MainThreadTaskQueue::NameForQueueType in sync.
    // This enum is used for a histogram and it should not be re-numbered.
    // TODO(altimin): Clean up obsolete names and use a new histogram when
    // the situation settles.
    kControl = 0,
    kDefault = 1,

    // 2 was used for default loading task runner but this was deprecated.

    // 3 was used for default timer task runner but this was deprecated.

    kUnthrottled = 4,
    kFrameLoading = 5,
    // 6 : kFrameThrottleable, replaced with FRAME_THROTTLEABLE.
    // 7 : kFramePausable, replaced with kFramePausable
    kCompositor = 8,
    kIdle = 9,
    kTest = 10,
    kFrameLoadingControl = 11,
    kFrameThrottleable = 12,
    kFrameDeferrable = 13,
    kFramePausable = 14,
    kFrameUnpausable = 15,
    kV8 = 16,
    kIPC = 17,
    kInput = 18,

    // Detached is used in histograms for tasks which are run after frame
    // is detached and task queue is gracefully shutdown.
    // TODO(altimin): Move to the top when histogram is renumbered.
    kDetached = 19,

    // Used to group multiple types when calculating Expected Queueing Time.
    kOther = 20,
    kCount = 21
  };

  // Returns name of the given queue type. Returned string has application
  // lifetime.
  static const char* NameForQueueType(QueueType queue_type);

  // High-level category used by RendererScheduler to make scheduling decisions.
  enum class QueueClass {
    kNone = 0,
    kLoading = 1,
    kTimer = 2,
    kCompositor = 4,

    kCount = 5,
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
          used_for_important_tasks(false) {}

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

    QueueCreationParams SetUsedForImportantTasks(bool value) {
      used_for_important_tasks = value;
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

    QueueType queue_type;
    TaskQueue::Spec spec;
    FrameScheduler* frame_;
    bool can_be_blocked;
    bool can_be_throttled;
    bool can_be_paused;
    bool can_be_stopped;
    bool used_for_important_tasks;
  };

  ~MainThreadTaskQueue() override;

  QueueType queue_type() const { return queue_type_; }

  QueueClass queue_class() const { return queue_class_; }

  bool CanBeDeferred() const { return can_be_blocked_; }

  bool CanBeThrottled() const { return can_be_throttled_; }

  bool CanBePaused() const { return can_be_paused_; }

  bool CanBeStopped() const { return can_be_stopped_; }

  bool UsedForImportantTasks() const { return used_for_important_tasks_; }

  void OnTaskStarted(const TaskQueue::Task& task, base::TimeTicks start);

  void OnTaskCompleted(const TaskQueue::Task& task,
                       base::TimeTicks start,
                       base::TimeTicks end,
                       base::Optional<base::TimeDelta> thread_time);

  void DetachFromRendererScheduler();

  // Override base method to notify RendererScheduler about shutdown queue.
  void ShutdownTaskQueue() override;

  FrameScheduler* GetFrameScheduler() const;
  void SetFrameScheduler(FrameScheduler* frame);

 protected:
  MainThreadTaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl,
                      const Spec& spec,
                      const QueueCreationParams& params,
                      RendererSchedulerImpl* renderer_scheduler);

 private:
  friend class TaskQueueManager;

  // Clear references to main thread scheduler and frame scheduler and dispatch
  // appropriate notifications. This is the common part of ShutdownTaskQueue and
  // DetachFromRendererScheduler.
  void ClearReferencesToSchedulers();

  QueueType queue_type_;
  QueueClass queue_class_;
  const bool can_be_blocked_;
  const bool can_be_throttled_;
  const bool can_be_paused_;
  const bool can_be_stopped_;
  const bool used_for_important_tasks_;

  // Needed to notify renderer scheduler about completed tasks.
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED

  FrameScheduler* frame_scheduler_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(MainThreadTaskQueue);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_TASK_QUEUE_H_
