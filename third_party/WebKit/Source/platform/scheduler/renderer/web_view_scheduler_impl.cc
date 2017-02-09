// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_view_scheduler_impl.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/WebFrameScheduler.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

namespace blink {
namespace scheduler {

namespace {

const double kBackgroundBudgetAsCPUFraction = .01;
// Given that we already align timers to 1Hz, do not report throttling if
// it is under 3s.
constexpr base::TimeDelta kMinimalBackgroundThrottlingDurationToReport =
    base::TimeDelta::FromSeconds(3);
constexpr base::TimeDelta kDefaultMaxBackgroundBudgetLevel =
    base::TimeDelta::FromSeconds(3);
constexpr base::TimeDelta kDefaultMaxBackgroundThrottlingDelay =
    base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kDefaultInitialBackgroundBudget =
    base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kBackgroundThrottlingGracePeriod =
    base::TimeDelta::FromSeconds(10);

// Values coming from WebViewSchedulerSettings are interpreted as follows:
//   -1 is "not set". Scheduler should use a reasonable default.
//   0 is "none". base::nullopt will be used if value is optional.
//   other values are left without changes.

double GetBackgroundBudgetRecoveryRate(
    WebViewScheduler::WebViewSchedulerSettings* settings) {
  if (!settings)
    return kBackgroundBudgetAsCPUFraction;
  double settings_budget = settings->expensiveBackgroundThrottlingCPUBudget();
  if (settings_budget == -1.0)
    return kBackgroundBudgetAsCPUFraction;
  return settings_budget;
}

base::Optional<base::TimeDelta> GetMaxBudgetLevel(
    WebViewScheduler::WebViewSchedulerSettings* settings) {
  if (!settings)
    return base::nullopt;
  double max_budget_level = settings->expensiveBackgroundThrottlingMaxBudget();
  if (max_budget_level == -1.0)
    return kDefaultMaxBackgroundBudgetLevel;
  if (max_budget_level == 0.0)
    return base::nullopt;
  return base::TimeDelta::FromSecondsD(max_budget_level);
}

base::Optional<base::TimeDelta> GetMaxThrottlingDelay(
    WebViewScheduler::WebViewSchedulerSettings* settings) {
  if (!settings)
    return base::nullopt;
  double max_delay = settings->expensiveBackgroundThrottlingMaxDelay();
  if (max_delay == -1.0)
    return kDefaultMaxBackgroundThrottlingDelay;
  if (max_delay == 0.0)
    return base::nullopt;
  return base::TimeDelta::FromSecondsD(max_delay);
}

base::TimeDelta GetInitialBudget(
    WebViewSchedulerImpl::WebViewSchedulerSettings* settings) {
  if (!settings)
    return kDefaultInitialBackgroundBudget;
  double initial_budget =
      settings->expensiveBackgroundThrottlingInitialBudget();
  if (initial_budget == -1.0)
    return kDefaultMaxBackgroundBudgetLevel;
  return base::TimeDelta::FromSecondsD(initial_budget);
}

std::string PointerToId(void* pointer) {
  return base::StringPrintf(
      "0x%" PRIx64,
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)));
}

}  // namespace

WebViewSchedulerImpl::WebViewSchedulerImpl(
    WebScheduler::InterventionReporter* intervention_reporter,
    WebViewScheduler::WebViewSchedulerSettings* settings,
    RendererSchedulerImpl* renderer_scheduler,
    bool disable_background_timer_throttling)
    : intervention_reporter_(intervention_reporter),
      renderer_scheduler_(renderer_scheduler),
      virtual_time_policy_(VirtualTimePolicy::ADVANCE),
      background_parser_count_(0),
      page_visible_(true),
      should_throttle_frames_(false),
      disable_background_timer_throttling_(disable_background_timer_throttling),
      allow_virtual_time_to_advance_(true),
      have_seen_loading_task_(false),
      virtual_time_(false),
      is_audio_playing_(false),
      reported_background_throttling_since_navigation_(false),
      has_active_connection_(false),
      background_time_budget_pool_(nullptr),
      settings_(settings) {
  renderer_scheduler->AddWebViewScheduler(this);

  delayed_background_throttling_enabler_.Reset(
      base::Bind(&WebViewSchedulerImpl::EnableBackgroundThrottling,
                 base::Unretained(this)));
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

  UpdateBackgroundThrottlingState();
}

std::unique_ptr<WebFrameSchedulerImpl>
WebViewSchedulerImpl::createWebFrameSchedulerImpl(
    base::trace_event::BlameContext* blame_context) {
  MaybeInitializeBackgroundTimeBudgetPool();
  std::unique_ptr<WebFrameSchedulerImpl> frame_scheduler(
      new WebFrameSchedulerImpl(renderer_scheduler_, this, blame_context));
  frame_scheduler->setPageThrottled(should_throttle_frames_);
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

void WebViewSchedulerImpl::OnNavigation() {
  reported_background_throttling_since_navigation_ = false;
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

bool WebViewSchedulerImpl::hasActiveConnectionForTest() const {
  return has_active_connection_;
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

void WebViewSchedulerImpl::OnConnectionUpdated() {
  bool has_active_connection = false;
  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    has_active_connection |= frame_scheduler->has_active_connection();
  }

  if (has_active_connection_ != has_active_connection) {
    has_active_connection_ = has_active_connection;
    UpdateBackgroundThrottlingState();
  }
}

void WebViewSchedulerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetDouble("pending_loads", pending_loads_.size());
  state->SetString("virtual_time_policy",
                   VirtualTimePolicyToString(virtual_time_policy_));
  state->SetDouble("background_parser_count", background_parser_count_);
  state->SetBoolean("page_visible", page_visible_);
  state->SetBoolean("disable_background_timer_throttling",
                    disable_background_timer_throttling_);
  state->SetBoolean("allow_virtual_time_to_advance",
                    allow_virtual_time_to_advance_);
  state->SetBoolean("have_seen_loading_task", have_seen_loading_task_);
  state->SetBoolean("virtual_time", virtual_time_);
  state->SetBoolean("is_audio_playing", is_audio_playing_);
  state->SetBoolean("reported_background_throttling_since_navigation",
                    reported_background_throttling_since_navigation_);

  state->BeginDictionary("frame_schedulers");
  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    state->BeginDictionaryWithCopiedName(PointerToId(frame_scheduler));
    frame_scheduler->AsValueInto(state);
    state->EndDictionary();
  }
  state->EndDictionary();
}

TaskQueueThrottler::TimeBudgetPool*
WebViewSchedulerImpl::BackgroundTimeBudgetPool() {
  MaybeInitializeBackgroundTimeBudgetPool();
  return background_time_budget_pool_;
}

void WebViewSchedulerImpl::MaybeInitializeBackgroundTimeBudgetPool() {
  if (background_time_budget_pool_)
    return;

  if (!RuntimeEnabledFeatures::expensiveBackgroundTimerThrottlingEnabled())
    return;

  background_time_budget_pool_ =
      renderer_scheduler_->task_queue_throttler()->CreateTimeBudgetPool(
          "background", GetMaxBudgetLevel(settings_),
          GetMaxThrottlingDelay(settings_));

  UpdateBackgroundThrottlingState();

  LazyNow lazy_now(renderer_scheduler_->tick_clock());

  background_time_budget_pool_->SetTimeBudgetRecoveryRate(
      lazy_now.Now(), GetBackgroundBudgetRecoveryRate(settings_));

  background_time_budget_pool_->GrantAdditionalBudget(
      lazy_now.Now(), GetInitialBudget(settings_));
}

void WebViewSchedulerImpl::OnThrottlingReported(
    base::TimeDelta throttling_duration) {
  if (throttling_duration < kMinimalBackgroundThrottlingDurationToReport)
    return;

  if (reported_background_throttling_since_navigation_)
    return;
  reported_background_throttling_since_navigation_ = true;

  std::string message = base::StringPrintf(
      "Timer tasks have taken too much time while the page was in the "
      "background. "
      "As a result, they have been deferred for %.3f seconds. "
      "See https://www.chromestatus.com/feature/6172836527865856 "
      "for more details",
      throttling_duration.InSecondsF());

  intervention_reporter_->ReportIntervention(WebString::fromUTF8(message));
}

void WebViewSchedulerImpl::EnableBackgroundThrottling() {
  should_throttle_frames_ = true;
  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->setPageThrottled(true);
  }
  UpdateBackgroundBudgetPoolThrottlingState();
}

void WebViewSchedulerImpl::UpdateBackgroundThrottlingState() {
  delayed_background_throttling_enabler_.Cancel();

  if (page_visible_) {
    should_throttle_frames_ = false;
    for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
      frame_scheduler->setPageThrottled(false);
    }
    UpdateBackgroundBudgetPoolThrottlingState();
  } else {
    if (has_active_connection_) {
      // If connection is active, update state immediately to stop throttling.
      UpdateBackgroundBudgetPoolThrottlingState();
    } else {
      // TODO(altimin): Consider moving this logic into PumpThrottledTasks.
      renderer_scheduler_->ControlTaskRunner()->PostDelayedTask(
          FROM_HERE, delayed_background_throttling_enabler_.callback(),
          kBackgroundThrottlingGracePeriod);
    }
  }
}

void WebViewSchedulerImpl::UpdateBackgroundBudgetPoolThrottlingState() {
  if (!background_time_budget_pool_)
    return;

  LazyNow lazy_now(renderer_scheduler_->tick_clock());
  if (page_visible_ || has_active_connection_) {
    background_time_budget_pool_->DisableThrottling(&lazy_now);
  } else {
    background_time_budget_pool_->EnableThrottling(&lazy_now);
  }
}

// static
const char* WebViewSchedulerImpl::VirtualTimePolicyToString(
    VirtualTimePolicy virtual_time_policy) {
  switch (virtual_time_policy) {
    case VirtualTimePolicy::ADVANCE:
      return "ADVANCE";
    case VirtualTimePolicy::PAUSE:
      return "PAUSE";
    case VirtualTimePolicy::DETERMINISTIC_LOADING:
      return "DETERMINISTIC_LOADING";
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace scheduler
}  // namespace blink
