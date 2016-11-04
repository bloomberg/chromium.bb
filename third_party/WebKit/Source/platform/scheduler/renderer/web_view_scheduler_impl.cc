// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_view_scheduler_impl.h"

#include "base/logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"
#include "public/platform/WebFrameScheduler.h"

namespace blink {
namespace scheduler {

namespace {

const double kBackgroundBudgetAsCPUFraction = .01;

}  // namespace

WebViewSchedulerImpl::WebViewSchedulerImpl(
    WebScheduler::InterventionReporter* intervention_reporter,
    RendererSchedulerImpl* renderer_scheduler,
    bool disable_background_timer_throttling)
    : intervention_reporter_(intervention_reporter),
      renderer_scheduler_(renderer_scheduler),
      virtual_time_policy_(VirtualTimePolicy::ADVANCE),
      background_parser_count_(0),
      page_visible_(true),
      disable_background_timer_throttling_(disable_background_timer_throttling),
      allow_virtual_time_to_advance_(true),
      have_seen_loading_task_(false),
      virtual_time_(false),
      is_audio_playing_(false),
      background_time_budget_pool_(nullptr) {
  renderer_scheduler->AddWebViewScheduler(this);

  if (RuntimeEnabledFeatures::expensiveBackgroundTimerThrottlingEnabled()) {
    background_time_budget_pool_ =
        renderer_scheduler_->task_queue_throttler()->CreateTimeBudgetPool(
            "background");

    LazyNow lazy_now(renderer_scheduler_->tick_clock());

    // Disable throttling because page is visible by default.
    background_time_budget_pool_->DisableThrottling(&lazy_now);

    background_time_budget_pool_->SetTimeBudget(lazy_now.Now(),
                                                kBackgroundBudgetAsCPUFraction);
  }
}

WebViewSchedulerImpl::~WebViewSchedulerImpl() {
  // TODO(alexclarke): Find out why we can't rely on the web view outliving the
  // frame.
  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->DetachFromWebViewScheduler();
  }
  renderer_scheduler_->RemoveWebViewScheduler(this);

  if (background_time_budget_pool_)
    background_time_budget_pool_->Close();
}

void WebViewSchedulerImpl::setPageVisible(bool page_visible) {
  if (disable_background_timer_throttling_ || page_visible_ == page_visible)
    return;

  page_visible_ = page_visible;

  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->setPageVisible(page_visible_);
  }

  if (background_time_budget_pool_) {
    LazyNow lazy_now(renderer_scheduler_->tick_clock());
    if (page_visible_) {
      background_time_budget_pool_->DisableThrottling(&lazy_now);
    } else {
      background_time_budget_pool_->EnableThrottling(&lazy_now);
    }
  }
}

std::unique_ptr<WebFrameSchedulerImpl>
WebViewSchedulerImpl::createWebFrameSchedulerImpl(
    base::trace_event::BlameContext* blame_context) {
  std::unique_ptr<WebFrameSchedulerImpl> frame_scheduler(
      new WebFrameSchedulerImpl(renderer_scheduler_, this, blame_context));
  frame_scheduler->setPageVisible(page_visible_);
  frame_schedulers_.insert(frame_scheduler.get());
  return frame_scheduler;
}

std::unique_ptr<blink::WebFrameScheduler>
WebViewSchedulerImpl::createFrameScheduler(blink::BlameContext* blame_context) {
  return createWebFrameSchedulerImpl(blame_context);
}

void WebViewSchedulerImpl::Unregister(WebFrameSchedulerImpl* frame_scheduler) {
  DCHECK(frame_schedulers_.find(frame_scheduler) != frame_schedulers_.end());
  frame_schedulers_.erase(frame_scheduler);
}

void WebViewSchedulerImpl::ReportIntervention(const std::string& message) {
  intervention_reporter_->ReportIntervention(WebString::fromUTF8(message));
}

void WebViewSchedulerImpl::enableVirtualTime() {
  if (virtual_time_)
    return;

  virtual_time_ = true;
  renderer_scheduler_->GetVirtualTimeDomain()->SetCanAdvanceVirtualTime(
      allow_virtual_time_to_advance_);

  renderer_scheduler_->EnableVirtualTime();
}

void WebViewSchedulerImpl::setAllowVirtualTimeToAdvance(
    bool allow_virtual_time_to_advance) {
  allow_virtual_time_to_advance_ = allow_virtual_time_to_advance;

  if (!virtual_time_)
    return;

  renderer_scheduler_->GetVirtualTimeDomain()->SetCanAdvanceVirtualTime(
      allow_virtual_time_to_advance);
}

bool WebViewSchedulerImpl::virtualTimeAllowedToAdvance() const {
  return allow_virtual_time_to_advance_;
}

void WebViewSchedulerImpl::DidStartLoading(unsigned long identifier) {
  pending_loads_.insert(identifier);
  have_seen_loading_task_ = true;
  ApplyVirtualTimePolicy();
}

void WebViewSchedulerImpl::DidStopLoading(unsigned long identifier) {
  pending_loads_.erase(identifier);
  ApplyVirtualTimePolicy();
}

void WebViewSchedulerImpl::IncrementBackgroundParserCount() {
  background_parser_count_++;
  ApplyVirtualTimePolicy();
}

void WebViewSchedulerImpl::DecrementBackgroundParserCount() {
  background_parser_count_--;
  DCHECK_GE(background_parser_count_, 0);
  ApplyVirtualTimePolicy();
}

void WebViewSchedulerImpl::setVirtualTimePolicy(VirtualTimePolicy policy) {
  virtual_time_policy_ = policy;

  switch (virtual_time_policy_) {
    case VirtualTimePolicy::ADVANCE:
      setAllowVirtualTimeToAdvance(true);
      break;

    case VirtualTimePolicy::PAUSE:
      setAllowVirtualTimeToAdvance(false);
      break;

    case VirtualTimePolicy::DETERMINISTIC_LOADING:
      ApplyVirtualTimePolicy();
      break;
  }
}

void WebViewSchedulerImpl::audioStateChanged(bool is_audio_playing) {
  is_audio_playing_ = is_audio_playing;
  renderer_scheduler_->OnAudioStateChanged();
}

void WebViewSchedulerImpl::ApplyVirtualTimePolicy() {
  if (virtual_time_policy_ != VirtualTimePolicy::DETERMINISTIC_LOADING) {
    return;
  }

  // We pause virtual time until we've seen a loading task posted, because
  // otherwise we could advance virtual time arbitarially far before the
  // first load arrives.
  setAllowVirtualTimeToAdvance(pending_loads_.size() == 0 &&
                               background_parser_count_ == 0 &&
                               have_seen_loading_task_);
}

bool WebViewSchedulerImpl::IsAudioPlaying() const {
  return is_audio_playing_;
}

}  // namespace scheduler
}  // namespace blink
