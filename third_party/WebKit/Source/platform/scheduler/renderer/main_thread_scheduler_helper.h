// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_SCHEDULER_HELPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_SCHEDULER_HELPER_H_

#include "platform/scheduler/child/scheduler_helper.h"

#include "platform/scheduler/renderer/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

class RendererSchedulerImpl;

class PLATFORM_EXPORT MainThreadSchedulerHelper : public SchedulerHelper {
 public:
  MainThreadSchedulerHelper(
      scoped_refptr<SchedulerTqmDelegate> task_queue_manager_delegate,
      RendererSchedulerImpl* renderer_scheduler);
  ~MainThreadSchedulerHelper() override;

  scoped_refptr<MainThreadTaskQueue> NewTaskQueue(
      MainThreadTaskQueue::QueueType type,
      const TaskQueue::Spec& spec);

  scoped_refptr<MainThreadTaskQueue> DefaultMainThreadTaskQueue();
  scoped_refptr<MainThreadTaskQueue> ControlMainThreadTaskQueue();

 protected:
  scoped_refptr<TaskQueue> DefaultTaskQueue() override;
  scoped_refptr<TaskQueue> ControlTaskQueue() override;

 private:
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED

  const scoped_refptr<MainThreadTaskQueue> default_task_queue_;
  const scoped_refptr<MainThreadTaskQueue> control_task_queue_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadSchedulerHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_MAIN_THREAD_SCHEDULER_HELPER_H_
