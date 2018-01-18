// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

#include <memory>
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/blame_context.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/budget_pool.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "platform/scheduler/util/tracing_helper.h"
#include "public/platform/BlameContext.h"
#include "public/platform/WebString.h"

namespace blink {
namespace scheduler {

namespace {

const char* VisibilityStateToString(bool is_visible) {
  if (is_visible) {
    return "visible";
  } else {
    return "hidden";
  }
}

const char* PausedStateToString(bool is_paused) {
  if (is_paused) {
    return "paused";
  } else {
    return "running";
  }
}

const char* StoppedStateToString(bool is_stopped) {
  if (is_stopped) {
    return "stopped";
  } else {
    return "running";
  }
}

const char* CrossOriginStateToString(bool is_cross_origin) {
  if (is_cross_origin) {
    return "cross-origin";
  } else {
    return "same-origin";
  }
}

bool StopNonTimersInBackgroundEnabled() {
  return RuntimeEnabledFeatures::StopNonTimersInBackgroundEnabled();
}

}  // namespace

WebFrameSchedulerImpl::ActiveConnectionHandleImpl::ActiveConnectionHandleImpl(
    WebFrameSchedulerImpl* frame_scheduler)
    : frame_scheduler_(frame_scheduler->AsWeakPtr()) {
  frame_scheduler->DidOpenActiveConnection();
}

WebFrameSchedulerImpl::ActiveConnectionHandleImpl::
    ~ActiveConnectionHandleImpl() {
  if (frame_scheduler_)
    frame_scheduler_->DidCloseActiveConnection();
}

WebFrameSchedulerImpl::WebFrameSchedulerImpl(
    RendererSchedulerImpl* renderer_scheduler,
    WebViewSchedulerImpl* parent_web_view_scheduler,
    base::trace_event::BlameContext* blame_context,
    WebFrameScheduler::FrameType frame_type)
    : renderer_scheduler_(renderer_scheduler),
      parent_web_view_scheduler_(parent_web_view_scheduler),
      blame_context_(blame_context),
      throttling_state_(WebFrameScheduler::ThrottlingState::kNotThrottled),
      frame_visible_(
          true,
          "WebFrameScheduler.FrameVisible",
          this,
          &tracing_controller_,
          VisibilityStateToString),
      page_visible_(
          true,
          "WebFrameScheduler.PageVisible",
          this,
          &tracing_controller_,
          VisibilityStateToString),
      page_stopped_(
          false,
          "WebFrameScheduler.PageStopped",
          this,
          &tracing_controller_,
          StoppedStateToString),
      frame_paused_(
          false,
          "WebFrameScheduler.FramePaused",
          this,
          &tracing_controller_,
          PausedStateToString),
      cross_origin_(
          false,
          "WebFrameScheduler.Origin",
          this,
          &tracing_controller_,
          CrossOriginStateToString),
      frame_type_(frame_type),
      active_connection_count_(0),
      weak_factory_(this) {
  DCHECK_EQ(throttling_state_, CalculateThrottlingState());
}

namespace {

void CleanUpQueue(MainThreadTaskQueue* queue) {
  if (!queue)
    return;
  queue->DetachFromRendererScheduler();
  queue->SetFrameScheduler(nullptr);
  queue->SetBlameContext(nullptr);
  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
}

}  // namespace

WebFrameSchedulerImpl::~WebFrameSchedulerImpl() {
  weak_factory_.InvalidateWeakPtrs();

  RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool();

  CleanUpQueue(loading_task_queue_.get());
  CleanUpQueue(loading_control_task_queue_.get());
  CleanUpQueue(throttleable_task_queue_.get());
  CleanUpQueue(deferrable_task_queue_.get());
  CleanUpQueue(pausable_task_queue_.get());
  CleanUpQueue(unpausable_task_queue_.get());

  if (parent_web_view_scheduler_) {
    parent_web_view_scheduler_->Unregister(this);

    if (active_connection_count_)
      parent_web_view_scheduler_->OnConnectionUpdated();
  }
}

void WebFrameSchedulerImpl::DetachFromWebViewScheduler() {
  RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool();

  parent_web_view_scheduler_ = nullptr;
}

void WebFrameSchedulerImpl::
    RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool() {
  if (!throttleable_task_queue_)
    return;

  if (!parent_web_view_scheduler_)
    return;

  CPUTimeBudgetPool* time_budget_pool =
      parent_web_view_scheduler_->BackgroundCPUTimeBudgetPool();

  if (!time_budget_pool)
    return;

  time_budget_pool->RemoveQueue(renderer_scheduler_->tick_clock()->NowTicks(),
                                throttleable_task_queue_.get());
}

void WebFrameSchedulerImpl::AddThrottlingObserver(ObserverType type,
                                                  Observer* observer) {
  DCHECK_EQ(ObserverType::kLoader, type);
  DCHECK(observer);
  observer->OnThrottlingStateChanged(CalculateThrottlingState());
  loader_observers_.insert(observer);
}

void WebFrameSchedulerImpl::RemoveThrottlingObserver(ObserverType type,
                                                     Observer* observer) {
  DCHECK_EQ(ObserverType::kLoader, type);
  DCHECK(observer);
  const auto found = loader_observers_.find(observer);
  DCHECK(loader_observers_.end() != found);
  loader_observers_.erase(found);
}

void WebFrameSchedulerImpl::SetFrameVisible(bool frame_visible) {
  DCHECK(parent_web_view_scheduler_);
  if (frame_visible_ == frame_visible)
    return;
  UMA_HISTOGRAM_BOOLEAN("RendererScheduler.IPC.FrameVisibility", frame_visible);
  bool was_throttled = ShouldThrottleTimers();
  frame_visible_ = frame_visible;
  UpdateThrottling(was_throttled);
}

bool WebFrameSchedulerImpl::IsFrameVisible() const {
  return frame_visible_;
}

void WebFrameSchedulerImpl::SetCrossOrigin(bool cross_origin) {
  DCHECK(parent_web_view_scheduler_);
  if (cross_origin_ == cross_origin)
    return;
  bool was_throttled = ShouldThrottleTimers();
  cross_origin_ = cross_origin;
  UpdateThrottling(was_throttled);
}

bool WebFrameSchedulerImpl::IsCrossOrigin() const {
  return cross_origin_;
}

WebFrameScheduler::FrameType WebFrameSchedulerImpl::GetFrameType() const {
  return frame_type_;
}

scoped_refptr<blink::WebTaskRunner> WebFrameSchedulerImpl::GetTaskRunner(
    TaskType type) {
  // TODO(haraken): Optimize the mapping from TaskTypes to task runners.
  switch (type) {
    case TaskType::kJavascriptTimer:
      return WebTaskRunnerImpl::Create(ThrottleableTaskQueue(), type);
    case TaskType::kUnspecedLoading:
    case TaskType::kNetworking:
      return WebTaskRunnerImpl::Create(LoadingTaskQueue(), type);
    case TaskType::kNetworkingControl:
      return WebTaskRunnerImpl::Create(LoadingControlTaskQueue(), type);
    // Throttling following tasks may break existing web pages, so tentatively
    // these are unthrottled.
    // TODO(nhiroki): Throttle them again after we're convinced that it's safe
    // or provide a mechanism that web pages can opt-out it if throttling is not
    // desirable.
    case TaskType::kDatabaseAccess:
    case TaskType::kDOMManipulation:
    case TaskType::kHistoryTraversal:
    case TaskType::kEmbed:
    case TaskType::kCanvasBlobSerialization:
    case TaskType::kRemoteEvent:
    case TaskType::kWebSocket:
    case TaskType::kMicrotask:
    case TaskType::kUnshippedPortMessage:
    case TaskType::kFileReading:
    case TaskType::kPresentation:
    case TaskType::kSensor:
    case TaskType::kPerformanceTimeline:
    case TaskType::kWebGL:
    case TaskType::kIdleTask:
    case TaskType::kUnspecedTimer:
    case TaskType::kMiscPlatformAPI:
      // TODO(altimin): Move appropriate tasks to throttleable task queue.
      return WebTaskRunnerImpl::Create(DeferrableTaskQueue(), type);
    // PostedMessage can be used for navigation, so we shouldn't defer it
    // when expecting a user gesture.
    case TaskType::kPostedMessage:
    // UserInteraction tasks should be run even when expecting a user gesture.
    case TaskType::kUserInteraction:
    // Media events should not be deferred to ensure that media playback is
    // smooth.
    case TaskType::kMediaElementEvent:
    case TaskType::kInternalIndexedDB:
      return WebTaskRunnerImpl::Create(PausableTaskQueue(), type);
    case TaskType::kUnthrottled:
    case TaskType::kInternalTest:
    case TaskType::kInternalWebCrypto:
      return WebTaskRunnerImpl::Create(UnpausableTaskQueue(), type);
    case TaskType::kCount:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return nullptr;
}

scoped_refptr<TaskQueue> WebFrameSchedulerImpl::LoadingTaskQueue() {
  DCHECK(parent_web_view_scheduler_);
  if (!loading_task_queue_) {
    loading_task_queue_ = renderer_scheduler_->NewLoadingTaskQueue(
        MainThreadTaskQueue::QueueType::kFrameLoading);
    loading_task_queue_->SetBlameContext(blame_context_);
    loading_task_queue_->SetFrameScheduler(this);
    loading_queue_enabled_voter_ =
        loading_task_queue_->CreateQueueEnabledVoter();
    loading_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return loading_task_queue_;
}

scoped_refptr<TaskQueue> WebFrameSchedulerImpl::LoadingControlTaskQueue() {
  DCHECK(parent_web_view_scheduler_);
  if (!loading_control_task_queue_) {
    loading_control_task_queue_ = renderer_scheduler_->NewLoadingTaskQueue(
        MainThreadTaskQueue::QueueType::kFrameLoading_kControl);
    loading_control_task_queue_->SetBlameContext(blame_context_);
    loading_control_task_queue_->SetFrameScheduler(this);
    loading_control_queue_enabled_voter_ =
        loading_control_task_queue_->CreateQueueEnabledVoter();
    loading_control_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return loading_control_task_queue_;
}

scoped_refptr<TaskQueue> WebFrameSchedulerImpl::ThrottleableTaskQueue() {
  DCHECK(parent_web_view_scheduler_);
  if (!throttleable_task_queue_) {
    throttleable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameThrottleable)
            .SetShouldReportWhenExecutionBlocked(true)
            .SetCanBeThrottled(true)
            .SetCanBeStopped(true)
            .SetCanBeDeferred(true)
            .SetCanBePaused(true));
    throttleable_task_queue_->SetBlameContext(blame_context_);
    throttleable_task_queue_->SetFrameScheduler(this);
    throttleable_queue_enabled_voter_ =
        throttleable_task_queue_->CreateQueueEnabledVoter();
    throttleable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);

    CPUTimeBudgetPool* time_budget_pool =
        parent_web_view_scheduler_->BackgroundCPUTimeBudgetPool();
    if (time_budget_pool) {
      time_budget_pool->AddQueue(renderer_scheduler_->tick_clock()->NowTicks(),
                                 throttleable_task_queue_.get());
    }

    if (ShouldThrottleTimers()) {
      renderer_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
          throttleable_task_queue_.get());
    }
  }
  return throttleable_task_queue_;
}

scoped_refptr<TaskQueue> WebFrameSchedulerImpl::DeferrableTaskQueue() {
  DCHECK(parent_web_view_scheduler_);
  if (!deferrable_task_queue_) {
    deferrable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameThrottleable)
            .SetShouldReportWhenExecutionBlocked(true)
            .SetCanBeDeferred(true)
            .SetCanBeStopped(StopNonTimersInBackgroundEnabled())
            .SetCanBePaused(true));
    deferrable_task_queue_->SetBlameContext(blame_context_);
    deferrable_task_queue_->SetFrameScheduler(this);
    deferrable_queue_enabled_voter_ =
        deferrable_task_queue_->CreateQueueEnabledVoter();
    deferrable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return deferrable_task_queue_;
}

scoped_refptr<TaskQueue> WebFrameSchedulerImpl::PausableTaskQueue() {
  DCHECK(parent_web_view_scheduler_);
  if (!pausable_task_queue_) {
    pausable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFramePausable)
            .SetShouldReportWhenExecutionBlocked(true)
            .SetCanBeStopped(StopNonTimersInBackgroundEnabled())
            .SetCanBePaused(true));
    pausable_task_queue_->SetBlameContext(blame_context_);
    pausable_task_queue_->SetFrameScheduler(this);
    pausable_queue_enabled_voter_ =
        pausable_task_queue_->CreateQueueEnabledVoter();
    pausable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return pausable_task_queue_;
}

scoped_refptr<TaskQueue> WebFrameSchedulerImpl::UnpausableTaskQueue() {
  DCHECK(parent_web_view_scheduler_);
  if (!unpausable_task_queue_) {
    unpausable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameUnpausable));
    unpausable_task_queue_->SetBlameContext(blame_context_);
    unpausable_task_queue_->SetFrameScheduler(this);
  }
  return unpausable_task_queue_;
}

blink::WebViewScheduler* WebFrameSchedulerImpl::GetWebViewScheduler() const {
  return parent_web_view_scheduler_;
}

void WebFrameSchedulerImpl::DidStartProvisionalLoad(bool is_main_frame) {
  renderer_scheduler_->DidStartProvisionalLoad(is_main_frame);
}

void WebFrameSchedulerImpl::DidCommitProvisionalLoad(
    bool is_web_history_inert_commit,
    bool is_reload,
    bool is_main_frame) {
  renderer_scheduler_->DidCommitProvisionalLoad(is_web_history_inert_commit,
                                                is_reload, is_main_frame);
}

WebScopedVirtualTimePauser
WebFrameSchedulerImpl::CreateWebScopedVirtualTimePauser() {
  return WebScopedVirtualTimePauser(renderer_scheduler_);
}

void WebFrameSchedulerImpl::DidOpenActiveConnection() {
  ++active_connection_count_;
  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->OnConnectionUpdated();
}

void WebFrameSchedulerImpl::DidCloseActiveConnection() {
  DCHECK_GT(active_connection_count_, 0);
  --active_connection_count_;
  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->OnConnectionUpdated();
}

void WebFrameSchedulerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetBoolean("frame_visible", frame_visible_);
  state->SetBoolean("page_visible", page_visible_);
  state->SetBoolean("cross_origin", cross_origin_);
  state->SetString("frame_type",
                   frame_type_ == WebFrameScheduler::FrameType::kMainFrame
                       ? "MainFrame"
                       : "Subframe");
  if (loading_task_queue_) {
    state->SetString("loading_task_queue",
                     PointerToString(loading_task_queue_.get()));
  }
  if (loading_control_task_queue_) {
    state->SetString("loading_control_task_queue",
                     PointerToString(loading_control_task_queue_.get()));
  }
  if (throttleable_task_queue_)
    state->SetString("throttleable_task_queue",
                     PointerToString(throttleable_task_queue_.get()));
  if (deferrable_task_queue_) {
    state->SetString("deferrable_task_queue",
                     PointerToString(deferrable_task_queue_.get()));
  }
  if (pausable_task_queue_) {
    state->SetString("pausable_task_queue",
                     PointerToString(pausable_task_queue_.get()));
  }
  if (unpausable_task_queue_) {
    state->SetString("unpausable_task_queue",
                     PointerToString(unpausable_task_queue_.get()));
  }
  if (blame_context_) {
    state->BeginDictionary("blame_context");
    state->SetString(
        "id_ref",
        PointerToString(reinterpret_cast<void*>(blame_context_->id())));
    state->SetString("scope", blame_context_->scope());
    state->EndDictionary();
  }
}

void WebFrameSchedulerImpl::SetPageVisible(bool page_visible) {
  DCHECK(parent_web_view_scheduler_);
  if (page_visible_ == page_visible)
    return;
  bool was_throttled = ShouldThrottleTimers();
  page_visible_ = page_visible;
  if (page_visible_)
    page_stopped_ = false;  // visible page must not be stopped.
  UpdateThrottling(was_throttled);
  UpdateThrottlingState();
}

bool WebFrameSchedulerImpl::IsPageVisible() const {
  return page_visible_;
}

void WebFrameSchedulerImpl::SetPaused(bool frame_paused) {
  DCHECK(parent_web_view_scheduler_);
  if (frame_paused_ == frame_paused)
    return;

  frame_paused_ = frame_paused;
  if (loading_queue_enabled_voter_)
    loading_queue_enabled_voter_->SetQueueEnabled(!frame_paused);
  if (loading_control_queue_enabled_voter_)
    loading_control_queue_enabled_voter_->SetQueueEnabled(!frame_paused);
  if (throttleable_queue_enabled_voter_)
    throttleable_queue_enabled_voter_->SetQueueEnabled(!frame_paused);
  if (deferrable_queue_enabled_voter_)
    deferrable_queue_enabled_voter_->SetQueueEnabled(!frame_paused);
  if (pausable_queue_enabled_voter_)
    pausable_queue_enabled_voter_->SetQueueEnabled(!frame_paused);
}

void WebFrameSchedulerImpl::SetPageStopped(bool stopped) {
  if (stopped == page_stopped_)
    return;
  DCHECK(!page_visible_);
  page_stopped_ = stopped;
  UpdateThrottlingState();
}

void WebFrameSchedulerImpl::UpdateThrottlingState() {
  WebFrameScheduler::ThrottlingState throttling_state =
      CalculateThrottlingState();
  if (throttling_state == throttling_state_)
    return;
  throttling_state_ = throttling_state;
  for (auto observer : loader_observers_)
    observer->OnThrottlingStateChanged(throttling_state_);
}

WebFrameScheduler::ThrottlingState
WebFrameSchedulerImpl::CalculateThrottlingState() const {
  if (RuntimeEnabledFeatures::StopLoadingInBackgroundEnabled() &&
      page_stopped_) {
    DCHECK(!page_visible_);
    return WebFrameScheduler::ThrottlingState::kStopped;
  }
  if (!page_visible_)
    return WebFrameScheduler::ThrottlingState::kThrottled;
  return WebFrameScheduler::ThrottlingState::kNotThrottled;
}

void WebFrameSchedulerImpl::OnFirstMeaningfulPaint() {
  renderer_scheduler_->OnFirstMeaningfulPaint();
}

std::unique_ptr<WebFrameScheduler::ActiveConnectionHandle>
WebFrameSchedulerImpl::OnActiveConnectionCreated() {
  return std::make_unique<WebFrameSchedulerImpl::ActiveConnectionHandleImpl>(
      this);
}

bool WebFrameSchedulerImpl::ShouldThrottleTimers() const {
  if (!page_visible_)
    return true;
  return RuntimeEnabledFeatures::TimerThrottlingForHiddenFramesEnabled() &&
         !frame_visible_ && cross_origin_;
}

void WebFrameSchedulerImpl::UpdateThrottling(bool was_throttled) {
  bool should_throttle = ShouldThrottleTimers();
  if (was_throttled == should_throttle || !throttleable_task_queue_)
    return;
  if (should_throttle) {
    renderer_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
        throttleable_task_queue_.get());
  } else {
    renderer_scheduler_->task_queue_throttler()->DecreaseThrottleRefCount(
        throttleable_task_queue_.get());
  }
}

base::WeakPtr<WebFrameSchedulerImpl> WebFrameSchedulerImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool WebFrameSchedulerImpl::IsExemptFromBudgetBasedThrottling() const {
  return has_active_connection();
}

}  // namespace scheduler
}  // namespace blink
