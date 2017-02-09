// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/blame_context.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/BlameContext.h"
#include "public/platform/WebString.h"

namespace blink {
namespace scheduler {
namespace {

std::string PointerToId(void* pointer) {
  return base::StringPrintf(
      "0x%" PRIx64,
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)));
}

}  // namespace

WebFrameSchedulerImpl::ActiveConnectionHandleImpl::ActiveConnectionHandleImpl(
    WebFrameSchedulerImpl* frame_scheduler)
    : frame_scheduler_(frame_scheduler->AsWeakPtr()) {
  frame_scheduler->didOpenActiveConnection();
}

WebFrameSchedulerImpl::ActiveConnectionHandleImpl::
    ~ActiveConnectionHandleImpl() {
  if (frame_scheduler_)
    frame_scheduler_->didCloseActiveConnection();
}

WebFrameSchedulerImpl::WebFrameSchedulerImpl(
    RendererSchedulerImpl* renderer_scheduler,
    WebViewSchedulerImpl* parent_web_view_scheduler,
    base::trace_event::BlameContext* blame_context)
    : renderer_scheduler_(renderer_scheduler),
      parent_web_view_scheduler_(parent_web_view_scheduler),
      blame_context_(blame_context),
      frame_visible_(true),
      page_throttled_(true),
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
    RemoveTimerQueueFromBackgroundTimeBudgetPool();
    timer_task_queue_->UnregisterTaskQueue();
    timer_task_queue_->SetBlameContext(nullptr);
  }

  if (unthrottled_task_queue_) {
    unthrottled_task_queue_->UnregisterTaskQueue();
    unthrottled_task_queue_->SetBlameContext(nullptr);
  }

  if (parent_web_view_scheduler_) {
    parent_web_view_scheduler_->Unregister(this);

    if (active_connection_count_)
      parent_web_view_scheduler_->OnConnectionUpdated();
  }
}

void WebFrameSchedulerImpl::DetachFromWebViewScheduler() {
  RemoveTimerQueueFromBackgroundTimeBudgetPool();

  parent_web_view_scheduler_ = nullptr;
}

void WebFrameSchedulerImpl::RemoveTimerQueueFromBackgroundTimeBudgetPool() {
  if (!timer_task_queue_)
    return;

  if (!parent_web_view_scheduler_)
    return;

  TaskQueueThrottler::TimeBudgetPool* time_budget_pool =
      parent_web_view_scheduler_->BackgroundTimeBudgetPool();

  if (!time_budget_pool)
    return;

  time_budget_pool->RemoveQueue(renderer_scheduler_->tick_clock()->NowTicks(),
                                timer_task_queue_.get());
}

void WebFrameSchedulerImpl::setFrameVisible(bool frame_visible) {
  DCHECK(parent_web_view_scheduler_);
  if (frame_visible_ == frame_visible)
    return;
  bool was_throttled = ShouldThrottleTimers();
  frame_visible_ = frame_visible;
  UpdateTimerThrottling(was_throttled);
}

void WebFrameSchedulerImpl::setCrossOrigin(bool cross_origin) {
  DCHECK(parent_web_view_scheduler_);
  if (cross_origin_ == cross_origin)
    return;
  bool was_throttled = ShouldThrottleTimers();
  cross_origin_ = cross_origin;
  UpdateTimerThrottling(was_throttled);
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::loadingTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!loading_web_task_runner_) {
    loading_task_queue_ = renderer_scheduler_->NewLoadingTaskRunner(
        TaskQueue::QueueType::FRAME_LOADING);
    loading_task_queue_->SetBlameContext(blame_context_);
    loading_queue_enabled_voter_ =
        loading_task_queue_->CreateQueueEnabledVoter();
    loading_queue_enabled_voter_->SetQueueEnabled(!frame_suspended_);
    loading_web_task_runner_ = WebTaskRunnerImpl::create(loading_task_queue_);
  }
  return loading_web_task_runner_;
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::timerTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!timer_web_task_runner_) {
    timer_task_queue_ = renderer_scheduler_->NewTimerTaskRunner(
        TaskQueue::QueueType::FRAME_TIMER);
    timer_task_queue_->SetBlameContext(blame_context_);
    timer_queue_enabled_voter_ = timer_task_queue_->CreateQueueEnabledVoter();
    timer_queue_enabled_voter_->SetQueueEnabled(!frame_suspended_);

    TaskQueueThrottler::TimeBudgetPool* time_budget_pool =
        parent_web_view_scheduler_->BackgroundTimeBudgetPool();
    if (time_budget_pool) {
      time_budget_pool->AddQueue(renderer_scheduler_->tick_clock()->NowTicks(),
                                 timer_task_queue_.get());
    }

    if (ShouldThrottleTimers()) {
      renderer_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
          timer_task_queue_.get());
    }
    timer_web_task_runner_ = WebTaskRunnerImpl::create(timer_task_queue_);
  }
  return timer_web_task_runner_;
}

RefPtr<blink::WebTaskRunner> WebFrameSchedulerImpl::unthrottledTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!unthrottled_web_task_runner_) {
    unthrottled_task_queue_ = renderer_scheduler_->NewUnthrottledTaskRunner(
        TaskQueue::QueueType::FRAME_UNTHROTTLED);
    unthrottled_task_queue_->SetBlameContext(blame_context_);
    unthrottled_web_task_runner_ =
        WebTaskRunnerImpl::create(unthrottled_task_queue_);
  }
  return unthrottled_web_task_runner_;
}

blink::WebViewScheduler* WebFrameSchedulerImpl::webViewScheduler() {
  return parent_web_view_scheduler_;
}

void WebFrameSchedulerImpl::didStartLoading(unsigned long identifier) {
  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->DidStartLoading(identifier);
}

void WebFrameSchedulerImpl::didStopLoading(unsigned long identifier) {
  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->DidStopLoading(identifier);
}

void WebFrameSchedulerImpl::didOpenActiveConnection() {
  ++active_connection_count_;
  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->OnConnectionUpdated();
}

void WebFrameSchedulerImpl::didCloseActiveConnection() {
  DCHECK_GT(active_connection_count_, 0);
  --active_connection_count_;
  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->OnConnectionUpdated();
}

void WebFrameSchedulerImpl::setDocumentParsingInBackground(
    bool background_parser_active) {
  if (background_parser_active)
    parent_web_view_scheduler_->IncrementBackgroundParserCount();
  else
    parent_web_view_scheduler_->DecrementBackgroundParserCount();
}

void WebFrameSchedulerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetBoolean("frame_visible", frame_visible_);
  state->SetBoolean("page_throttled", page_throttled_);
  state->SetBoolean("cross_origin", cross_origin_);
  if (loading_task_queue_) {
    state->SetString("loading_task_queue",
                     PointerToId(loading_task_queue_.get()));
  }
  if (timer_task_queue_)
    state->SetString("timer_task_queue", PointerToId(timer_task_queue_.get()));
  if (unthrottled_task_queue_) {
    state->SetString("unthrottled_task_queue",
                     PointerToId(unthrottled_task_queue_.get()));
  }
  if (blame_context_) {
    state->BeginDictionary("blame_context");
    state->SetString(
        "id_ref", PointerToId(reinterpret_cast<void*>(blame_context_->id())));
    state->SetString("scope", blame_context_->scope());
    state->EndDictionary();
  }
}

void WebFrameSchedulerImpl::setPageThrottled(bool page_throttled) {
  DCHECK(parent_web_view_scheduler_);
  if (page_throttled_ == page_throttled)
    return;
  bool was_throttled = ShouldThrottleTimers();
  page_throttled_ = page_throttled;
  UpdateTimerThrottling(was_throttled);
}

void WebFrameSchedulerImpl::setSuspended(bool frame_suspended) {
  DCHECK(parent_web_view_scheduler_);
  if (frame_suspended_ == frame_suspended)
    return;

  frame_suspended_ = frame_suspended;
  if (loading_queue_enabled_voter_)
    loading_queue_enabled_voter_->SetQueueEnabled(!frame_suspended);
  if (timer_queue_enabled_voter_)
    timer_queue_enabled_voter_->SetQueueEnabled(!frame_suspended);
}

void WebFrameSchedulerImpl::onFirstMeaningfulPaint() {
  renderer_scheduler_->OnFirstMeaningfulPaint();
}

std::unique_ptr<WebFrameScheduler::ActiveConnectionHandle>
WebFrameSchedulerImpl::onActiveConnectionCreated() {
  return base::MakeUnique<WebFrameSchedulerImpl::ActiveConnectionHandleImpl>(
      this);
}

bool WebFrameSchedulerImpl::ShouldThrottleTimers() const {
  if (page_throttled_)
    return true;
  return RuntimeEnabledFeatures::timerThrottlingForHiddenFramesEnabled() &&
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
