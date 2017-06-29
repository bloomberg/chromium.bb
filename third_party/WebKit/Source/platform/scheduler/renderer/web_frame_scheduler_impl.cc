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
      frame_suspended_(false),
      cross_origin_(false),
      active_connection_count_(0),
      weak_factory_(this) {}

WebFrameSchedulerImpl::~WebFrameSchedulerImpl() {
  weak_factory_.InvalidateWeakPtrs();

  if (loading_task_queue_) {
    loading_task_queue_->UnregisterTaskQueue();
    loading_task_queue_->SetBlameContext(nullptr);
  }

  if (timer_task_queue_) {
    RemoveTimerQueueFromBackgroundCPUTimeBudgetPool();
    timer_task_queue_->UnregisterTaskQueue();
    timer_task_queue_->SetBlameContext(nullptr);
  }

  if (unthrottled_task_queue_) {
    unthrottled_task_queue_->UnregisterTaskQueue();
    unthrottled_task_queue_->SetBlameContext(nullptr);
  }

  if (suspendable_task_queue_) {
    suspendable_task_queue_->UnregisterTaskQueue();
    suspendable_task_queue_->SetBlameContext(nullptr);
  }

  if (unthrottled_but_blockable_task_queue_) {
    unthrottled_but_blockable_task_queue_->UnregisterTaskQueue();
    unthrottled_but_blockable_task_queue_->SetBlameContext(nullptr);
  }

  if (parent_web_view_scheduler_) {
    parent_web_view_scheduler_->Unregister(this);

    if (active_connection_count_)
      parent_web_view_scheduler_->OnConnectionUpdated();
  }
}

void WebFrameSchedulerImpl::DetachFromWebViewScheduler() {
  RemoveTimerQueueFromBackgroundCPUTimeBudgetPool();

  parent_web_view_scheduler_ = nullptr;
}

void WebFrameSchedulerImpl::RemoveTimerQueueFromBackgroundCPUTimeBudgetPool() {
  if (!timer_task_queue_)
    return;

  if (!parent_web_view_scheduler_)
    return;

  CPUTimeBudgetPool* time_budget_pool =
      parent_web_view_scheduler_->BackgroundCPUTimeBudgetPool();

  if (!time_budget_pool)
    return;

  time_budget_pool->RemoveQueue(renderer_scheduler_->tick_clock()->NowTicks(),
                                timer_task_queue_.get());
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
  UpdateTimerThrottling(was_throttled);
}

void WebFrameSchedulerImpl::SetCrossOrigin(bool cross_origin) {
  DCHECK(parent_web_view_scheduler_);
  if (cross_origin_ == cross_origin)
    return;
  bool was_throttled = ShouldThrottleTimers();
  cross_origin_ = cross_origin;
  UpdateTimerThrottling(was_throttled);
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::LoadingTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!loading_web_task_runner_) {
    loading_task_queue_ = renderer_scheduler_->NewLoadingTaskQueue(
        TaskQueue::QueueType::FRAME_LOADING);
    loading_task_queue_->SetBlameContext(blame_context_);
    loading_queue_enabled_voter_ =
        loading_task_queue_->CreateQueueEnabledVoter();
    loading_queue_enabled_voter_->SetQueueEnabled(!frame_suspended_);
    loading_web_task_runner_ = WebTaskRunnerImpl::Create(loading_task_queue_);
  }
  return loading_web_task_runner_;
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::TimerTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!timer_web_task_runner_) {
    timer_task_queue_ = renderer_scheduler_->NewTimerTaskQueue(
        TaskQueue::QueueType::FRAME_TIMER);
    timer_task_queue_->SetBlameContext(blame_context_);
    timer_queue_enabled_voter_ = timer_task_queue_->CreateQueueEnabledVoter();
    timer_queue_enabled_voter_->SetQueueEnabled(!frame_suspended_);

    CPUTimeBudgetPool* time_budget_pool =
        parent_web_view_scheduler_->BackgroundCPUTimeBudgetPool();
    if (time_budget_pool) {
      time_budget_pool->AddQueue(renderer_scheduler_->tick_clock()->NowTicks(),
                                 timer_task_queue_.get());
    }

    if (ShouldThrottleTimers()) {
      renderer_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
          timer_task_queue_.get());
    }
    timer_web_task_runner_ = WebTaskRunnerImpl::Create(timer_task_queue_);
  }
  return timer_web_task_runner_;
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::SuspendableTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!suspendable_web_task_runner_) {
    // TODO(altimin): Split FRAME_UNTHROTTLED into FRAME_UNTHROTTLED and
    // FRAME_UNSUSPENDED.
    suspendable_task_queue_ = renderer_scheduler_->NewTimerTaskQueue(
        TaskQueue::QueueType::FRAME_UNTHROTTLED);
    suspendable_task_queue_->SetBlameContext(blame_context_);
    suspendable_web_task_runner_ =
        WebTaskRunnerImpl::Create(suspendable_task_queue_);
    suspendable_queue_enabled_voter_ =
        suspendable_task_queue_->CreateQueueEnabledVoter();
    suspendable_queue_enabled_voter_->SetQueueEnabled(!frame_suspended_);
  }
  return suspendable_web_task_runner_;
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::UnthrottledTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!unthrottled_web_task_runner_) {
    unthrottled_task_queue_ = renderer_scheduler_->NewUnthrottledTaskQueue(
        TaskQueue::QueueType::FRAME_UNTHROTTLED);
    unthrottled_task_queue_->SetBlameContext(blame_context_);
    unthrottled_web_task_runner_ =
        WebTaskRunnerImpl::Create(unthrottled_task_queue_);
  }
  return unthrottled_web_task_runner_;
}

RefPtr<blink::WebTaskRunner>
WebFrameSchedulerImpl::UnthrottledButBlockableTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!unthrottled_but_blockable_web_task_runner_) {
    unthrottled_but_blockable_task_queue_ =
        renderer_scheduler_->NewTimerTaskQueue(
            TaskQueue::QueueType::FRAME_UNTHROTTLED);
    unthrottled_but_blockable_task_queue_->SetBlameContext(blame_context_);
    unthrottled_but_blockable_web_task_runner_ =
        WebTaskRunnerImpl::Create(unthrottled_but_blockable_task_queue_);
  }
  return unthrottled_but_blockable_web_task_runner_;
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
  if (timer_task_queue_)
    state->SetString("timer_task_queue",
                     trace_helper::PointerToString(timer_task_queue_.get()));
  if (unthrottled_task_queue_) {
    state->SetString(
        "unthrottled_task_queue",
        trace_helper::PointerToString(unthrottled_task_queue_.get()));
  }
  if (suspendable_task_queue_) {
    state->SetString(
        "suspendable_task_queue",
        trace_helper::PointerToString(suspendable_task_queue_.get()));
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
  UpdateTimerThrottling(was_throttled);

  for (auto observer : loader_observers_) {
    observer->OnThrottlingStateChanged(page_visible_
                                           ? ThrottlingState::kNotThrottled
                                           : ThrottlingState::kThrottled);
  }
}

void WebFrameSchedulerImpl::SetSuspended(bool frame_suspended) {
  DCHECK(parent_web_view_scheduler_);
  if (frame_suspended_ == frame_suspended)
    return;

  frame_suspended_ = frame_suspended;
  if (loading_queue_enabled_voter_)
    loading_queue_enabled_voter_->SetQueueEnabled(!frame_suspended);
  if (timer_queue_enabled_voter_)
    timer_queue_enabled_voter_->SetQueueEnabled(!frame_suspended);
  if (suspendable_queue_enabled_voter_)
    suspendable_queue_enabled_voter_->SetQueueEnabled(!frame_suspended);
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

void WebFrameSchedulerImpl::UpdateTimerThrottling(bool was_throttled) {
  bool should_throttle = ShouldThrottleTimers();
  if (was_throttled == should_throttle || !timer_web_task_runner_)
    return;
  if (should_throttle) {
    renderer_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
        timer_task_queue_.get());
  } else {
    renderer_scheduler_->task_queue_throttler()->DecreaseThrottleRefCount(
        timer_task_queue_.get());
  }
}

base::WeakPtr<WebFrameSchedulerImpl> WebFrameSchedulerImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace scheduler
}  // namespace blink
