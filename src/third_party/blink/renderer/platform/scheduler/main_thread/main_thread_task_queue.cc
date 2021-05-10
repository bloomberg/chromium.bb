// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink {
namespace scheduler {

namespace internal {
using base::sequence_manager::internal::TaskQueueImpl;
}

// static
const char* MainThreadTaskQueue::NameForQueueType(
    MainThreadTaskQueue::QueueType queue_type) {
  switch (queue_type) {
    case MainThreadTaskQueue::QueueType::kControl:
      return "control_tq";
    case MainThreadTaskQueue::QueueType::kDefault:
      return "default_tq";
    case MainThreadTaskQueue::QueueType::kFrameLoading:
      return "frame_loading_tq";
    case MainThreadTaskQueue::QueueType::kFrameThrottleable:
      return "frame_throttleable_tq";
    case MainThreadTaskQueue::QueueType::kFrameDeferrable:
      return "frame_deferrable_tq";
    case MainThreadTaskQueue::QueueType::kFramePausable:
      return "frame_pausable_tq";
    case MainThreadTaskQueue::QueueType::kFrameUnpausable:
      return "frame_unpausable_tq";
    case MainThreadTaskQueue::QueueType::kCompositor:
      return "compositor_tq";
    case MainThreadTaskQueue::QueueType::kIdle:
      return "idle_tq";
    case MainThreadTaskQueue::QueueType::kTest:
      return "test_tq";
    case MainThreadTaskQueue::QueueType::kFrameLoadingControl:
      return "frame_loading_control_tq";
    case MainThreadTaskQueue::QueueType::kV8:
      return "v8_tq";
    case MainThreadTaskQueue::QueueType::kInput:
      return "input_tq";
    case MainThreadTaskQueue::QueueType::kDetached:
      return "detached_tq";
    case MainThreadTaskQueue::QueueType::kOther:
      return "other_tq";
    case MainThreadTaskQueue::QueueType::kWebScheduling:
      return "web_scheduling_tq";
    case MainThreadTaskQueue::QueueType::kNonWaking:
      return "non_waking_tq";
    case MainThreadTaskQueue::QueueType::kIPCTrackingForCachedPages:
      return "ipc_tracking_for_cached_pages_tq";
    case MainThreadTaskQueue::QueueType::kCount:
      NOTREACHED();
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

// static
bool MainThreadTaskQueue::IsPerFrameTaskQueue(
    MainThreadTaskQueue::QueueType queue_type) {
  switch (queue_type) {
    // TODO(altimin): Remove kDefault once there is no per-frame kDefault queue.
    case MainThreadTaskQueue::QueueType::kDefault:
    case MainThreadTaskQueue::QueueType::kFrameLoading:
    case MainThreadTaskQueue::QueueType::kFrameLoadingControl:
    case MainThreadTaskQueue::QueueType::kFrameThrottleable:
    case MainThreadTaskQueue::QueueType::kFrameDeferrable:
    case MainThreadTaskQueue::QueueType::kFramePausable:
    case MainThreadTaskQueue::QueueType::kFrameUnpausable:
    case MainThreadTaskQueue::QueueType::kIdle:
    case MainThreadTaskQueue::QueueType::kWebScheduling:
      return true;
    case MainThreadTaskQueue::QueueType::kControl:
    case MainThreadTaskQueue::QueueType::kCompositor:
    case MainThreadTaskQueue::QueueType::kTest:
    case MainThreadTaskQueue::QueueType::kV8:
    case MainThreadTaskQueue::QueueType::kInput:
    case MainThreadTaskQueue::QueueType::kDetached:
    case MainThreadTaskQueue::QueueType::kNonWaking:
    case MainThreadTaskQueue::QueueType::kOther:
    case MainThreadTaskQueue::QueueType::kIPCTrackingForCachedPages:
      return false;
    case MainThreadTaskQueue::QueueType::kCount:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

MainThreadTaskQueue::MainThreadTaskQueue(
    std::unique_ptr<base::sequence_manager::internal::TaskQueueImpl> impl,
    const TaskQueue::Spec& spec,
    const QueueCreationParams& params,
    MainThreadSchedulerImpl* main_thread_scheduler)
    : queue_type_(params.queue_type),
      queue_traits_(params.queue_traits),
      freeze_when_keep_active_(params.freeze_when_keep_active),
      web_scheduling_priority_(params.web_scheduling_priority),
      main_thread_scheduler_(main_thread_scheduler),
      agent_group_scheduler_(params.agent_group_scheduler),
      frame_scheduler_(params.frame_scheduler) {
  task_queue_ = base::MakeRefCounted<TaskQueue>(std::move(impl), spec);
  if (task_queue_->HasImpl() && spec.should_notify_observers) {
    // TaskQueueImpl may be null for tests.
    // TODO(scheduler-dev): Consider mapping directly to
    // MainThreadSchedulerImpl::OnTaskStarted/Completed. At the moment this
    // is not possible due to task queue being created inside
    // MainThreadScheduler's constructor.
    task_queue_->SetOnTaskStartedHandler(base::BindRepeating(
        &MainThreadTaskQueue::OnTaskStarted, base::Unretained(this)));
    task_queue_->SetOnTaskCompletedHandler(base::BindRepeating(
        &MainThreadTaskQueue::OnTaskCompleted, base::Unretained(this)));
  }
}

MainThreadTaskQueue::~MainThreadTaskQueue() = default;

void MainThreadTaskQueue::OnTaskStarted(
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (main_thread_scheduler_)
    main_thread_scheduler_->OnTaskStarted(this, task, task_timing);
}

void MainThreadTaskQueue::OnTaskCompleted(
    const base::sequence_manager::Task& task,
    TaskQueue::TaskTiming* task_timing,
    base::sequence_manager::LazyNow* lazy_now) {
  if (main_thread_scheduler_) {
    main_thread_scheduler_->OnTaskCompleted(weak_ptr_factory_.GetWeakPtr(),
                                            task, task_timing, lazy_now);
  }
}

void MainThreadTaskQueue::OnTaskRunTimeReported(
    TaskQueue::TaskTiming* task_timing) {
  main_thread_scheduler_->task_queue_throttler()->OnTaskRunTimeReported(
      task_queue_.get(), task_timing->start_time(), task_timing->end_time());
}

void MainThreadTaskQueue::DetachFromMainThreadScheduler() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Frame has already been detached.
  if (!main_thread_scheduler_)
    return;

  task_queue_->SetOnTaskStartedHandler(
      base::BindRepeating(&MainThreadSchedulerImpl::OnTaskStarted,
                          main_thread_scheduler_->GetWeakPtr(), nullptr));
  task_queue_->SetOnTaskCompletedHandler(
      base::BindRepeating(&MainThreadSchedulerImpl::OnTaskCompleted,
                          main_thread_scheduler_->GetWeakPtr(), nullptr));
  task_queue_->SetOnTaskPostedHandler(
      internal::TaskQueueImpl::OnTaskPostedHandler());

  ClearReferencesToSchedulers();
}

void MainThreadTaskQueue::SetOnIPCTaskPosted(
    base::RepeatingCallback<void(const base::sequence_manager::Task&)>
        on_ipc_task_posted_callback) {
  if (task_queue_->HasImpl()) {
    // We use the frame_scheduler_ to track metrics so as to ensure that metrics
    // are not tied to individual task queues.
    task_queue_->SetOnTaskPostedHandler(on_ipc_task_posted_callback);
  }
}

void MainThreadTaskQueue::DetachOnIPCTaskPostedWhileInBackForwardCache() {
  if (task_queue_->HasImpl()) {
    task_queue_->SetOnTaskPostedHandler(
        internal::TaskQueueImpl::OnTaskPostedHandler());
  }
}

void MainThreadTaskQueue::ShutdownTaskQueue() {
  ClearReferencesToSchedulers();
  task_queue_->ShutdownTaskQueue();
}

WebAgentGroupScheduler* MainThreadTaskQueue::GetAgentGroupScheduler() {
  DCHECK(task_queue_->task_runner()->BelongsToCurrentThread());

  if (agent_group_scheduler_) {
    DCHECK(!frame_scheduler_);
    return agent_group_scheduler_;
  }
  if (frame_scheduler_) {
    return frame_scheduler_->GetAgentGroupScheduler();
  }
  // If this MainThreadTaskQueue was created for MainThreadSchedulerImpl, this
  // queue will not be associated with AgentGroupScheduler or FrameScheduler.
  return nullptr;
}

void MainThreadTaskQueue::ClearReferencesToSchedulers() {
  if (main_thread_scheduler_) {
    main_thread_scheduler_->OnShutdownTaskQueue(this);

    if (main_thread_scheduler_->task_queue_throttler()) {
      main_thread_scheduler_->task_queue_throttler()->ShutdownTaskQueue(
          task_queue_.get());
    }
  }
  main_thread_scheduler_ = nullptr;
  agent_group_scheduler_ = nullptr;
  frame_scheduler_ = nullptr;
}

FrameSchedulerImpl* MainThreadTaskQueue::GetFrameScheduler() const {
  DCHECK(task_queue_->task_runner()->BelongsToCurrentThread());
  return frame_scheduler_;
}

void MainThreadTaskQueue::SetFrameSchedulerForTest(
    FrameSchedulerImpl* frame_scheduler) {
  frame_scheduler_ = frame_scheduler;
}

void MainThreadTaskQueue::SetNetRequestPriority(
    net::RequestPriority net_request_priority) {
  net_request_priority_ = net_request_priority;
}

base::Optional<net::RequestPriority> MainThreadTaskQueue::net_request_priority()
    const {
  return net_request_priority_;
}

void MainThreadTaskQueue::SetWebSchedulingPriority(
    WebSchedulingPriority priority) {
  if (web_scheduling_priority_ == priority)
    return;
  web_scheduling_priority_ = priority;
  frame_scheduler_->OnWebSchedulingTaskQueuePriorityChanged(this);
}

base::Optional<WebSchedulingPriority>
MainThreadTaskQueue::web_scheduling_priority() const {
  return web_scheduling_priority_;
}

bool MainThreadTaskQueue::IsThrottled() const {
  if (main_thread_scheduler_) {
    return main_thread_scheduler_->task_queue_throttler()->IsThrottled(
        task_queue_.get());
  } else {
    // When the frame detaches the task queue is removed from the throttler.
    return false;
  }
}

MainThreadTaskQueue::ThrottleHandle MainThreadTaskQueue::Throttle() {
  DCHECK(CanBeThrottled());
  return ThrottleHandle(
      task_queue_.get()->AsWeakPtr(),
      main_thread_scheduler_->task_queue_throttler()->AsWeakPtr());
}

void MainThreadTaskQueue::AddToBudgetPool(base::TimeTicks now,
                                          BudgetPool* pool) {
  pool->AddQueue(now, task_queue_.get());
}

void MainThreadTaskQueue::RemoveFromBudgetPool(base::TimeTicks now,
                                               BudgetPool* pool) {
  pool->RemoveQueue(now, task_queue_.get());
}

void MainThreadTaskQueue::SetImmediateWakeUpForTest() {
  if (main_thread_scheduler_) {
    main_thread_scheduler_->task_queue_throttler()->OnQueueNextWakeUpChanged(
        task_queue_.get(), base::TimeTicks());
  }
}

void MainThreadTaskQueue::WriteIntoTracedValue(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("type", queue_type_);
  dict.Add("traits", queue_traits_);
}

void MainThreadTaskQueue::QueueTraits::WriteIntoTracedValue(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("can_be_deferred", can_be_deferred);
  dict.Add("can_be_throttled", can_be_throttled);
  dict.Add("can_be_intensively_throttled", can_be_intensively_throttled);
  dict.Add("can_be_paused", can_be_paused);
  dict.Add("can_be_frozen", can_be_frozen);
  dict.Add("can_run_in_background", can_run_in_background);
  dict.Add("can_run_when_virtual_time_paused",
           can_run_when_virtual_time_paused);
  dict.Add("can_be_paused_for_android_webview",
           can_be_paused_for_android_webview);
  dict.Add("prioritisation_type", prioritisation_type);
}

}  // namespace scheduler
}  // namespace blink
