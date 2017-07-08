// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_TASK_QUEUE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_TASK_QUEUE_H_

#include "platform/scheduler/base/task_queue.h"

namespace blink {
namespace scheduler {

class RendererSchedulerImpl;

class PLATFORM_EXPORT MainThreadTaskQueue : public TaskQueue {
 public:
  enum class QueueType {
    // Keep MainThreadTaskQueue::NameForQueueType in sync.
    // This enum is used for a histogram and it should not be re-numbered.
    CONTROL = 0,
    DEFAULT = 1,
    DEFAULT_LOADING = 2,
    DEFAULT_TIMER = 3,
    UNTHROTTLED = 4,
    FRAME_LOADING = 5,
    FRAME_TIMER = 6,
    FRAME_UNTHROTTLED = 7,
    COMPOSITOR = 8,
    IDLE = 9,
    TEST = 10,

    COUNT = 11
  };

  // Returns name of the given queue type. Returned string has application
  // lifetime.
  static const char* NameForQueueType(QueueType queue_type);

  // Create spec with correct name for given type.
  static TaskQueue::Spec CreateSpecForType(QueueType queue_type);

  ~MainThreadTaskQueue() override;

  QueueType queue_type() const { return queue_type_; }

  void OnTaskCompleted(base::TimeTicks start, base::TimeTicks end);

  // Override base method to notify RendererScheduler about unregistered queue.
  void UnregisterTaskQueue() override;

 private:
  MainThreadTaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl,
                      QueueType queue_type,
                      RendererSchedulerImpl* renderer_scheduler);

  friend class TaskQueueManager;

  QueueType queue_type_;

  // Needed to notify renderer scheduler about completed tasks.
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(MainThreadTaskQueue);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_TASK_QUEUE_H_
