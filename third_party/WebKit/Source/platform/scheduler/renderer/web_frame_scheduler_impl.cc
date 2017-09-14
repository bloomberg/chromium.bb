// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

#include "base/trace_event/blame_context.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/trace_helper.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/budget_pool.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "public/platform/BlameContext.h"
#include "public/platform/WebString.h"

namespace blink {
namespace scheduler {

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
    base::trace_event::BlameContext* blame_context)
    : renderer_scheduler_(renderer_scheduler),
      parent_web_view_scheduler_(parent_web_view_scheduler),
      blame_context_(blame_context),
      frame_visible_(true),
      page_visible_(true),
      frame_paused_(false),
      cross_origin_(false),
      active_connection_count_(0),
      weak_factory_(this) {}

namespace {

void CleanUpQueue(MainThreadTaskQueue* queue) {
  if (!queue)
    return;
  queue->UnregisterTaskQueue();
  queue->SetFrameScheduler(nullptr);
  queue->SetBlameContext(nullptr);
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
  observer->OnThrottlingStateChanged(page_visible_
                                         ? ThrottlingState::kNotThrottled
                                         : ThrottlingState::kThrottled);
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
  bool was_throttled = ShouldThrottleTimers();
  frame_visible_ = frame_visible;
  UpdateThrottling(was_throttled);
}

void WebFrameSchedulerImpl::SetCrossOrigin(bool cross_origin) {
  DCHECK(parent_web_view_scheduler_);
  if (cross_origin_ == cross_origin)
    return;
  bool was_throttled = ShouldThrottleTimers();
  cross_origin_ = cross_origin;
  UpdateThrottling(was_throttled);
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::LoadingTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!loading_web_task_runner_) {
    loading_task_queue_ = renderer_scheduler_->NewLoadingTaskQueue(
        MainThreadTaskQueue::QueueType::FRAME_LOADING);
    loading_task_queue_->SetBlameContext(blame_context_);
    loading_task_queue_->SetFrameScheduler(this);
    loading_queue_enabled_voter_ =
        loading_task_queue_->CreateQueueEnabledVoter();
    loading_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
    loading_web_task_runner_ = WebTaskRunnerImpl::Create(loading_task_queue_);
  }
  return loading_web_task_runner_;
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::LoadingControlTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!loading_control_web_task_runner_) {
    loading_control_task_queue_ = renderer_scheduler_->NewLoadingTaskQueue(
        MainThreadTaskQueue::QueueType::FRAME_LOADING_CONTROL);
    loading_control_task_queue_->SetBlameContext(blame_context_);
    loading_control_task_queue_->SetFrameScheduler(this);
    loading_control_queue_enabled_voter_ =
        loading_control_task_queue_->CreateQueueEnabledVoter();
    loading_control_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
    loading_control_web_task_runner_ =
        WebTaskRunnerImpl::Create(loading_control_task_queue_);
  }
  return loading_control_web_task_runner_;
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::ThrottleableTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!throttleable_web_task_runner_) {
    throttleable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::FRAME_THROTTLEABLE)
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
    throttleable_web_task_runner_ =
        WebTaskRunnerImpl::Create(throttleable_task_queue_);
  }
  return throttleable_web_task_runner_;
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::DeferrableTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!deferrable_web_task_runner_) {
    deferrable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::FRAME_THROTTLEABLE)
            .SetShouldReportWhenExecutionBlocked(true)
            .SetCanBeDeferred(true)
            .SetCanBePaused(true));
    deferrable_task_queue_->SetBlameContext(blame_context_);
    deferrable_task_queue_->SetFrameScheduler(this);
    deferrable_web_task_runner_ =
        WebTaskRunnerImpl::Create(deferrable_task_queue_);
    deferrable_queue_enabled_voter_ =
        deferrable_task_queue_->CreateQueueEnabledVoter();
    deferrable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return deferrable_web_task_runner_;
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::PausableTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!pausable_web_task_runner_) {
    pausable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::FRAME_PAUSABLE)
            .SetShouldReportWhenExecutionBlocked(true)
            .SetCanBePaused(true));
    pausable_task_queue_->SetBlameContext(blame_context_);
    pausable_task_queue_->SetFrameScheduler(this);
    pausable_web_task_runner_ = WebTaskRunnerImpl::Create(pausable_task_queue_);
    pausable_queue_enabled_voter_ =
        pausable_task_queue_->CreateQueueEnabledVoter();
    pausable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return pausable_web_task_runner_;
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::UnpausableTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!unpausable_web_task_runner_) {
    unpausable_task_queue_ = renderer_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::FRAME_UNPAUSABLE));
    unpausable_task_queue_->SetBlameContext(blame_context_);
    unpausable_task_queue_->SetFrameScheduler(this);
    unpausable_web_task_runner_ =
        WebTaskRunnerImpl::Create(unpausable_task_queue_);
  }
  return unpausable_web_task_runner_;
}

blink::WebViewScheduler* WebFrameSchedulerImpl::GetWebViewScheduler() {
  return parent_web_view_scheduler_;
}

void WebFrameSchedulerImpl::WillNavigateBackForwardSoon() {
  parent_web_view_scheduler_->WillNavigateBackForwardSoon(this);
}

void WebFrameSchedulerImpl::DidStartProvisionalLoad(bool is_main_frame) {
  parent_web_view_scheduler_->DidBeginProvisionalLoad(this);
  renderer_scheduler_->DidStartProvisionalLoad(is_main_frame);
}

void WebFrameSchedulerImpl::DidFailProvisionalLoad() {
  parent_web_view_scheduler_->DidEndProvisionalLoad(this);
}

void WebFrameSchedulerImpl::DidCommitProvisionalLoad(
    bool is_web_history_inert_commit,
    bool is_reload,
    bool is_main_frame) {
  parent_web_view_scheduler_->DidEndProvisionalLoad(this);
  renderer_scheduler_->DidCommitProvisionalLoad(is_web_history_inert_commit,
                                                is_reload, is_main_frame);
}

void WebFrameSchedulerImpl::DidStartLoading(unsigned long identifier) {
  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->DidStartLoading(identifier);
}

void WebFrameSchedulerImpl::DidStopLoading(unsigned long identifier) {
  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->DidStopLoading(identifier);
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

void WebFrameSchedulerImpl::SetDocumentParsingInBackground(
    bool background_parser_active) {
  if (background_parser_active)
    parent_web_view_scheduler_->IncrementBackgroundParserCount();
  else
    parent_web_view_scheduler_->DecrementBackgroundParserCount();
}

void WebFrameSchedulerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetBoolean("frame_visible", frame_visible_);
  state->SetBoolean("page_visible", page_visible_);
  state->SetBoolean("cross_origin", cross_origin_);
  if (loading_task_queue_) {
    state->SetString("loading_task_queue",
                     trace_helper::PointerToString(loading_task_queue_.get()));
  }
  if (loading_control_task_queue_) {
    state->SetString(
        "loading_control_task_queue",
        trace_helper::PointerToString(loading_control_task_queue_.get()));
  }
  if (throttleable_task_queue_)
    state->SetString(
        "throttleable_task_queue",
        trace_helper::PointerToString(throttleable_task_queue_.get()));
  if (deferrable_task_queue_) {
    state->SetString(
        "deferrable_task_queue",
        trace_helper::PointerToString(deferrable_task_queue_.get()));
  }
  if (pausable_task_queue_) {
    state->SetString("pausable_task_queue",
                     trace_helper::PointerToString(pausable_task_queue_.get()));
  }
  if (unpausable_task_queue_) {
    state->SetString(
        "unpausable_task_queue",
        trace_helper::PointerToString(unpausable_task_queue_.get()));
  }
  if (blame_context_) {
    state->BeginDictionary("blame_context");
    state->SetString("id_ref",
                     trace_helper::PointerToString(
                         reinterpret_cast<void*>(blame_context_->id())));
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
  UpdateThrottling(was_throttled);

  for (auto observer : loader_observers_) {
    observer->OnThrottlingStateChanged(page_visible_
                                           ? ThrottlingState::kNotThrottled
                                           : ThrottlingState::kThrottled);
  }
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

void WebFrameSchedulerImpl::OnFirstMeaningfulPaint() {
  renderer_scheduler_->OnFirstMeaningfulPaint();
}

std::unique_ptr<WebFrameScheduler::ActiveConnectionHandle>
WebFrameSchedulerImpl::OnActiveConnectionCreated() {
  return base::MakeUnique<WebFrameSchedulerImpl::ActiveConnectionHandleImpl>(
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
  if (was_throttled == should_throttle || !throttleable_web_task_runner_)
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

}  // namespace scheduler
}  // namespace blink
