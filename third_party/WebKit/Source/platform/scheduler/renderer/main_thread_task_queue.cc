// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/main_thread_task_queue.h"

#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"

namespace blink {
namespace scheduler {

// static
const char* MainThreadTaskQueue::NameForQueueType(
    MainThreadTaskQueue::QueueType queue_type) {
  switch (queue_type) {
    case MainThreadTaskQueue::QueueType::kControl:
      return "ControlTQ";
    case MainThreadTaskQueue::QueueType::kDefault:
      return "DefaultTQ";
    case MainThreadTaskQueue::QueueType::kDefaultLoading:
      return "DefaultLoadingTQ";
    case MainThreadTaskQueue::QueueType::kDefaultTimer:
      return "DefaultTimerTQ";
    case MainThreadTaskQueue::QueueType::kUnthrottled:
      return "UnthrottledTQ";
    case MainThreadTaskQueue::QueueType::kFrameLoading:
      return "FrameLoadingTQ";
    case MainThreadTaskQueue::QueueType::kFrameThrottleable:
      return "FrameThrottleableTQ";
    case MainThreadTaskQueue::QueueType::kFrameDeferrable:
      return "FrameDeferrableTQ";
    case MainThreadTaskQueue::QueueType::kFramePausable:
      return "FramePausableTQ";
    case MainThreadTaskQueue::QueueType::kFrameUnpausable:
      return "FrameUnpausableTQ";
    case MainThreadTaskQueue::QueueType::kCompositor:
      return "CompositorTQ";
    case MainThreadTaskQueue::QueueType::kIdle:
      return "IdleTQ";
    case MainThreadTaskQueue::QueueType::kTest:
      return "TestTQ";
    case MainThreadTaskQueue::QueueType::kFrameLoading_kControl:
      return "FrameLoadingControlTQ";
    case MainThreadTaskQueue::QueueType::kV8:
      return "V8TQ";
    case MainThreadTaskQueue::QueueType::kIPC:
      return "IPCTQ";
    case MainThreadTaskQueue::QueueType::kOther:
      return "OtherTQ";
    case MainThreadTaskQueue::QueueType::kCount:
      NOTREACHED();
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

MainThreadTaskQueue::QueueClass MainThreadTaskQueue::QueueClassForQueueType(
    QueueType type) {
  switch (type) {
    case QueueType::kControl:
    case QueueType::kDefault:
    case QueueType::kIdle:
    case QueueType::kTest:
    case QueueType::kV8:
    case QueueType::kIPC:
      return QueueClass::kNone;
    case QueueType::kDefaultLoading:
    case QueueType::kFrameLoading:
    case QueueType::kFrameLoading_kControl:
      return QueueClass::kLoading;
    case QueueType::kDefaultTimer:
    case QueueType::kUnthrottled:
    case QueueType::kFrameThrottleable:
    case QueueType::kFrameDeferrable:
    case QueueType::kFramePausable:
    case QueueType::kFrameUnpausable:
      return QueueClass::kTimer;
    case QueueType::kCompositor:
      return QueueClass::kCompositor;
    case QueueType::kOther:
    case QueueType::kCount:
      DCHECK(false);
      return QueueClass::kCount;
  }
  NOTREACHED();
  return QueueClass::kNone;
}

MainThreadTaskQueue::MainThreadTaskQueue(
    std::unique_ptr<internal::TaskQueueImpl> impl,
    const TaskQueue::Spec& spec,
    const QueueCreationParams& params,
    RendererSchedulerImpl* renderer_scheduler)
    : TaskQueue(std::move(impl), spec),
      queue_type_(params.queue_type),
      queue_class_(QueueClassForQueueType(params.queue_type)),
      can_be_blocked_(params.can_be_blocked),
      can_be_throttled_(params.can_be_throttled),
      can_be_paused_(params.can_be_paused),
      can_be_stopped_(params.can_be_stopped),
      used_for_control_tasks_(params.used_for_control_tasks),
      renderer_scheduler_(renderer_scheduler),
      web_frame_scheduler_(nullptr) {
  if (GetTaskQueueImpl()) {
    // TaskQueueImpl may be null for tests.
    GetTaskQueueImpl()->SetOnTaskStartedHandler(base::Bind(
        &MainThreadTaskQueue::OnTaskStarted, base::Unretained(this)));
    GetTaskQueueImpl()->SetOnTaskCompletedHandler(base::Bind(
        &MainThreadTaskQueue::OnTaskCompleted, base::Unretained(this)));
  }
}

MainThreadTaskQueue::~MainThreadTaskQueue() {}

void MainThreadTaskQueue::OnTaskStarted(const TaskQueue::Task& task,
                                        base::TimeTicks start) {
  if (renderer_scheduler_)
    renderer_scheduler_->OnTaskStarted(this, task, start);
}

void MainThreadTaskQueue::OnTaskCompleted(const TaskQueue::Task& task,
                                          base::TimeTicks start,
                                          base::TimeTicks end) {
  if (renderer_scheduler_)
    renderer_scheduler_->OnTaskCompleted(this, task, start, end);
}

void MainThreadTaskQueue::ShutdownTaskQueue() {
  if (renderer_scheduler_) {
    // RendererScheduler can be null in tests.
    renderer_scheduler_->OnShutdownTaskQueue(this);
  }
  TaskQueue::ShutdownTaskQueue();
}

WebFrameScheduler* MainThreadTaskQueue::GetFrameScheduler() const {
  return web_frame_scheduler_;
}

void MainThreadTaskQueue::SetFrameScheduler(WebFrameScheduler* frame) {
  web_frame_scheduler_ = frame;
}

}  // namespace scheduler
}  // namespace blink
