// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_thread.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/inspector/protocol/DOM.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/double_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/scheduler/util/thread_cpu_throttler.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace EmulationAgentState {
static const char kScriptExecutionDisabled[] = "scriptExecutionDisabled";
static const char kScrollbarsHidden[] = "scrollbarsHidden";
static const char kDocumentCookieDisabled[] = "documentCookieDisabled";
static const char kTouchEventEmulationEnabled[] = "touchEventEmulationEnabled";
static const char kMaxTouchPoints[] = "maxTouchPoints";
static const char kEmulatedMedia[] = "emulatedMedia";
static const char kDefaultBackgroundColorOverrideRGBA[] =
    "defaultBackgroundColorOverrideRGBA";
static const char kNavigatorPlatform[] = "navigatorPlatform";
static const char kVirtualTimeBudget[] = "virtualTimeBudget";
static const char kVirtualTimeBudgetInitalOffset[] =
    "virtualTimeBudgetInitalOffset";
static const char kInitialVirtualTime[] = "initialVirtualTime";
static const char kVirtualTimeOffset[] = "virtualTimeOffset";
static const char kVirtualTimePolicy[] = "virtualTimePolicy";
static const char kVirtualTimeTaskStarvationCount[] =
    "virtualTimeTaskStarvationCount";
static const char kWaitForNavigation[] = "waitForNavigation";
static const char kUserAgentOverride[] = "userAgentOverride";
static const char kAcceptLanguageOverride[] = "acceptLanguage";
}  // namespace EmulationAgentState

InspectorEmulationAgent::InspectorEmulationAgent(
    WebLocalFrameImpl* web_local_frame_impl)
    : web_local_frame_(web_local_frame_impl) {}

InspectorEmulationAgent::~InspectorEmulationAgent() = default;

WebViewImpl* InspectorEmulationAgent::GetWebViewImpl() {
  return web_local_frame_ ? web_local_frame_->ViewImpl() : nullptr;
}

void InspectorEmulationAgent::Restore() {
  String user_agent;
  state_->getString(EmulationAgentState::kUserAgentOverride, &user_agent);
  String accept_language;
  state_->getString(EmulationAgentState::kAcceptLanguageOverride,
                    &accept_language);
  String navigator_platform;
  state_->getString(EmulationAgentState::kNavigatorPlatform,
                    &navigator_platform);
  setUserAgentOverride(user_agent, accept_language, navigator_platform);
  if (!web_local_frame_)
    return;

  // Following code only runs for pages.
  if (state_->booleanProperty(EmulationAgentState::kScriptExecutionDisabled,
                              false)) {
    GetWebViewImpl()->GetDevToolsEmulator()->SetScriptExecutionDisabled(true);
  }
  if (state_->booleanProperty(EmulationAgentState::kScrollbarsHidden, false))
    GetWebViewImpl()->GetDevToolsEmulator()->SetScrollbarsHidden(true);
  if (state_->booleanProperty(EmulationAgentState::kDocumentCookieDisabled,
                              false)) {
    GetWebViewImpl()->GetDevToolsEmulator()->SetDocumentCookieDisabled(true);
  }
  setTouchEmulationEnabled(
      state_->booleanProperty(EmulationAgentState::kTouchEventEmulationEnabled,
                              false),
      state_->integerProperty(EmulationAgentState::kMaxTouchPoints, 1));
  String emulated_media;
  state_->getString(EmulationAgentState::kEmulatedMedia, &emulated_media);
  setEmulatedMedia(emulated_media);
  auto* rgba_value =
      state_->get(EmulationAgentState::kDefaultBackgroundColorOverrideRGBA);
  if (rgba_value) {
    blink::protocol::ErrorSupport errors;
    auto rgba = protocol::DOM::RGBA::fromValue(rgba_value, &errors);
    if (!errors.hasErrors()) {
      setDefaultBackgroundColorOverride(
          Maybe<protocol::DOM::RGBA>(std::move(rgba)));
    }
  }

  String virtual_time_policy;
  if (state_->getString(EmulationAgentState::kVirtualTimePolicy,
                        &virtual_time_policy)) {
    // Tell the scheduler about the saved virtual time progress to ensure that
    // virtual time monotonically advances despite the cross origin navigation.
    // This should be done regardless of the virtual time mode.
    double offset = 0;
    state_->getDouble(EmulationAgentState::kVirtualTimeOffset, &offset);
    web_local_frame_->View()->Scheduler()->SetInitialVirtualTimeOffset(
        base::TimeDelta::FromMillisecondsD(offset));

    // Set initial time baselines for all modes.
    double initial_virtual_time = 0;
    bool has_initial_time = state_->getDouble(
        EmulationAgentState::kInitialVirtualTime, &initial_virtual_time);
    Maybe<double> initial_time = has_initial_time
                                     ? Maybe<double>()
                                     : Maybe<double>(initial_virtual_time);

    // Preserve wait for navigation in all modes.
    bool wait_for_navigation = false;
    state_->getBoolean(EmulationAgentState::kWaitForNavigation,
                       &wait_for_navigation);

    // Reinstate the stored policy.
    double virtual_time_ticks_base_ms;

    // For Pause, do not pass budget or starvation count.
    if (virtual_time_policy ==
        protocol::Emulation::VirtualTimePolicyEnum::Pause) {
      setVirtualTimePolicy(protocol::Emulation::VirtualTimePolicyEnum::Pause,
                           Maybe<double>(), Maybe<int>(), wait_for_navigation,
                           std::move(initial_time),
                           &virtual_time_ticks_base_ms);
      return;
    }

    // Calculate remaining budget for the advancing modes.
    double budget = 0;
    state_->getDouble(EmulationAgentState::kVirtualTimeBudget, &budget);
    double inital_offset = 0;
    state_->getDouble(EmulationAgentState::kVirtualTimeBudgetInitalOffset,
                      &inital_offset);
    double budget_remaining = budget + inital_offset - offset;
    DCHECK_GE(budget_remaining, 0);

    int starvation_count = 0;
    state_->getInteger(EmulationAgentState::kVirtualTimeTaskStarvationCount,
                       &starvation_count);

    setVirtualTimePolicy(virtual_time_policy, budget_remaining,
                         starvation_count, wait_for_navigation,
                         std::move(initial_time), &virtual_time_ticks_base_ms);
  }
}

Response InspectorEmulationAgent::disable() {
  if (enabled_)
    instrumenting_agents_->removeInspectorEmulationAgent(this);
  setScriptExecutionDisabled(false);
  setScrollbarsHidden(false);
  setDocumentCookieDisabled(false);
  setTouchEmulationEnabled(false, Maybe<int>());
  setEmulatedMedia(String());
  setCPUThrottlingRate(1);
  setDefaultBackgroundColorOverride(Maybe<protocol::DOM::RGBA>());
  if (virtual_time_setup_) {
    DCHECK(web_local_frame_);
    web_local_frame_->View()->Scheduler()->RemoveVirtualTimeObserver(this);
    virtual_time_setup_ = false;
  }
  setUserAgentOverride(String(), protocol::Maybe<String>(),
                       protocol::Maybe<String>());
  return Response::OK();
}

Response InspectorEmulationAgent::resetPageScaleFactor() {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  GetWebViewImpl()->ResetScaleStateImmediately();
  return response;
}

Response InspectorEmulationAgent::setPageScaleFactor(double page_scale_factor) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  GetWebViewImpl()->SetPageScaleFactor(static_cast<float>(page_scale_factor));
  return response;
}

Response InspectorEmulationAgent::setScriptExecutionDisabled(bool value) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  if (state_->booleanProperty(EmulationAgentState::kScriptExecutionDisabled,
                              false) == value) {
    return response;
  }
  state_->setBoolean(EmulationAgentState::kScriptExecutionDisabled, value);
  GetWebViewImpl()->GetDevToolsEmulator()->SetScriptExecutionDisabled(value);
  return response;
}

Response InspectorEmulationAgent::setScrollbarsHidden(bool hidden) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  if (state_->booleanProperty(EmulationAgentState::kScrollbarsHidden, false) ==
      hidden) {
    return response;
  }
  state_->setBoolean(EmulationAgentState::kScrollbarsHidden, hidden);
  GetWebViewImpl()->GetDevToolsEmulator()->SetScrollbarsHidden(hidden);
  return response;
}

Response InspectorEmulationAgent::setDocumentCookieDisabled(bool disabled) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  if (state_->booleanProperty(EmulationAgentState::kDocumentCookieDisabled,
                              false) == disabled) {
    return response;
  }
  state_->setBoolean(EmulationAgentState::kDocumentCookieDisabled, disabled);
  GetWebViewImpl()->GetDevToolsEmulator()->SetDocumentCookieDisabled(disabled);
  return response;
}

Response InspectorEmulationAgent::setTouchEmulationEnabled(
    bool enabled,
    protocol::Maybe<int> max_touch_points) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  int max_points = max_touch_points.fromMaybe(1);
  if (max_points < 1 || max_points > WebTouchEvent::kTouchesLengthCap) {
    return Response::InvalidParams(
        "Touch points must be between 1 and " +
        String::Number(WebTouchEvent::kTouchesLengthCap));
  }
  state_->setBoolean(EmulationAgentState::kTouchEventEmulationEnabled, enabled);
  state_->setInteger(EmulationAgentState::kMaxTouchPoints, max_points);
  GetWebViewImpl()->GetDevToolsEmulator()->SetTouchEventEmulationEnabled(
      enabled, max_points);
  return response;
}

Response InspectorEmulationAgent::setEmulatedMedia(const String& media) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  state_->setString(EmulationAgentState::kEmulatedMedia, media);
  GetWebViewImpl()->GetPage()->GetSettings().SetMediaTypeOverride(media);
  return response;
}

Response InspectorEmulationAgent::setCPUThrottlingRate(double rate) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  scheduler::ThreadCPUThrottler::GetInstance()->SetThrottlingRate(rate);
  return response;
}

Response InspectorEmulationAgent::setVirtualTimePolicy(
    const String& policy,
    Maybe<double> virtual_time_budget_ms,
    protocol::Maybe<int> max_virtual_time_task_starvation_count,
    protocol::Maybe<bool> wait_for_navigation,
    protocol::Maybe<double> initial_virtual_time,
    double* virtual_time_ticks_base_ms) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  state_->setString(EmulationAgentState::kVirtualTimePolicy, policy);

  PendingVirtualTimePolicy new_policy;
  new_policy.policy = PageScheduler::VirtualTimePolicy::kPause;
  if (protocol::Emulation::VirtualTimePolicyEnum::Advance == policy) {
    new_policy.policy = PageScheduler::VirtualTimePolicy::kAdvance;
  } else if (protocol::Emulation::VirtualTimePolicyEnum::
                 PauseIfNetworkFetchesPending == policy) {
    new_policy.policy = PageScheduler::VirtualTimePolicy::kDeterministicLoading;
  }

  if (new_policy.policy == PageScheduler::VirtualTimePolicy::kPause &&
      virtual_time_budget_ms.isJust()) {
    LOG(ERROR) << "Can only specify virtual time budget for non-Pause policy";
    return Response::InvalidParams(
        "Can only specify budget for non-Pause policy");
  }
  if (new_policy.policy == PageScheduler::VirtualTimePolicy::kPause &&
      max_virtual_time_task_starvation_count.isJust()) {
    LOG(ERROR)
        << "Can only specify virtual time starvation for non-Pause policy";
    return Response::InvalidParams(
        "Can only specify starvation count for non-Pause policy");
  }

  if (virtual_time_budget_ms.isJust()) {
    new_policy.virtual_time_budget_ms = virtual_time_budget_ms.fromJust();
    state_->setDouble(EmulationAgentState::kVirtualTimeBudget,
                      *new_policy.virtual_time_budget_ms);
    // Record the current virtual time offset so Restore can compute how much
    // budget is left.
    state_->setDouble(
        EmulationAgentState::kVirtualTimeBudgetInitalOffset,
        state_->doubleProperty(EmulationAgentState::kVirtualTimeOffset, 0.0));
  } else {
    state_->remove(EmulationAgentState::kVirtualTimeBudget);
  }

  if (max_virtual_time_task_starvation_count.isJust()) {
    new_policy.max_virtual_time_task_starvation_count =
        max_virtual_time_task_starvation_count.fromJust();
    state_->setDouble(EmulationAgentState::kVirtualTimeTaskStarvationCount,
                      *new_policy.max_virtual_time_task_starvation_count);
  } else {
    state_->remove(EmulationAgentState::kVirtualTimeTaskStarvationCount);
  }

  InnerEnable();
  if (!virtual_time_setup_) {
    web_local_frame_->View()->Scheduler()->AddVirtualTimeObserver(this);
    virtual_time_setup_ = true;
  }

  // This needs to happen before we apply virtual time.
  if (initial_virtual_time.isJust()) {
    state_->setDouble(EmulationAgentState::kInitialVirtualTime,
                      initial_virtual_time.fromJust());
    web_local_frame_->View()->Scheduler()->SetInitialVirtualTime(
        base::Time::FromDoubleT(initial_virtual_time.fromJust()));
  }

  if (wait_for_navigation.fromMaybe(false)) {
    state_->setBoolean(EmulationAgentState::kWaitForNavigation, true);
    pending_virtual_time_policy_ = std::move(new_policy);
  } else {
    ApplyVirtualTimePolicy(new_policy);
  }

  if (virtual_time_base_ticks_.is_null()) {
    *virtual_time_ticks_base_ms = 0;
  } else {
    *virtual_time_ticks_base_ms =
        (virtual_time_base_ticks_ - WTF::TimeTicks()).InMillisecondsF();
  }

  return response;
}

void InspectorEmulationAgent::ApplyVirtualTimePolicy(
    const PendingVirtualTimePolicy& new_policy) {
  DCHECK(web_local_frame_);
  web_local_frame_->View()->Scheduler()->SetVirtualTimePolicy(
      new_policy.policy);
  virtual_time_base_ticks_ =
      web_local_frame_->View()->Scheduler()->EnableVirtualTime();
  if (new_policy.virtual_time_budget_ms) {
    TRACE_EVENT_ASYNC_BEGIN1("renderer.scheduler", "VirtualTimeBudget", this,
                             "budget", *new_policy.virtual_time_budget_ms);
    WTF::TimeDelta budget_amount =
        WTF::TimeDelta::FromMillisecondsD(*new_policy.virtual_time_budget_ms);
    web_local_frame_->View()->Scheduler()->GrantVirtualTimeBudget(
        budget_amount,
        WTF::Bind(&InspectorEmulationAgent::VirtualTimeBudgetExpired,
                  WrapWeakPersistent(this)));
  }
  if (new_policy.max_virtual_time_task_starvation_count) {
    web_local_frame_->View()->Scheduler()->SetMaxVirtualTimeTaskStarvationCount(
        *new_policy.max_virtual_time_task_starvation_count);
  }
}

void InspectorEmulationAgent::FrameStartedLoading(LocalFrame*) {
  if (pending_virtual_time_policy_) {
    state_->setBoolean(EmulationAgentState::kWaitForNavigation, false);
    ApplyVirtualTimePolicy(*pending_virtual_time_policy_);
    pending_virtual_time_policy_ = base::nullopt;
  }
}

void InspectorEmulationAgent::WillSendRequest(
    ExecutionContext* execution_context,
    unsigned long identifier,
    DocumentLoader* loader,
    ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const FetchInitiatorInfo& initiator_info,
    Resource::Type resource_type) {
  String accept_lang_override;
  state_->getString(EmulationAgentState::kAcceptLanguageOverride,
                    &accept_lang_override);
  if (!accept_lang_override.IsEmpty() &&
      request.HttpHeaderField("Accept-Language").IsEmpty()) {
    request.SetHTTPHeaderField(
        "Accept-Language",
        AtomicString(
            NetworkUtils::GenerateAcceptLanguageHeader(accept_lang_override)));
  }
}

Response InspectorEmulationAgent::setNavigatorOverrides(
    const String& platform) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  state_->setString(EmulationAgentState::kNavigatorPlatform, platform);
  GetWebViewImpl()->GetPage()->GetSettings().SetNavigatorPlatformOverride(
      platform);
  return response;
}

void InspectorEmulationAgent::VirtualTimeBudgetExpired() {
  TRACE_EVENT_ASYNC_END0("renderer.scheduler", "VirtualTimeBudget", this);
  DCHECK(web_local_frame_);
  web_local_frame_->View()->Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kPause);
  state_->setString(EmulationAgentState::kVirtualTimePolicy,
                    protocol::Emulation::VirtualTimePolicyEnum::Pause);
  GetFrontend()->virtualTimeBudgetExpired();
}

void InspectorEmulationAgent::OnVirtualTimeAdvanced(
    WTF::TimeDelta virtual_time_offset) {
  state_->setDouble(EmulationAgentState::kVirtualTimeOffset,
                    virtual_time_offset.InMillisecondsF());
  GetFrontend()->virtualTimeAdvanced(virtual_time_offset.InMillisecondsF());
}

void InspectorEmulationAgent::OnVirtualTimePaused(
    WTF::TimeDelta virtual_time_offset) {
  state_->setDouble(EmulationAgentState::kVirtualTimeOffset,
                    virtual_time_offset.InMillisecondsF());
  GetFrontend()->virtualTimePaused(virtual_time_offset.InMillisecondsF());
}

Response InspectorEmulationAgent::setDefaultBackgroundColorOverride(
    Maybe<protocol::DOM::RGBA> color) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  if (!color.isJust()) {
    // Clear the override and state.
    GetWebViewImpl()->ClearBaseBackgroundColorOverride();
    state_->remove(EmulationAgentState::kDefaultBackgroundColorOverrideRGBA);
    return Response::OK();
  }

  blink::protocol::DOM::RGBA* rgba = color.fromJust();
  state_->setValue(EmulationAgentState::kDefaultBackgroundColorOverrideRGBA,
                   rgba->toValue());
  // Clamping of values is done by Color() constructor.
  int alpha = lroundf(255.0f * rgba->getA(1.0f));
  GetWebViewImpl()->SetBaseBackgroundColorOverride(
      Color(rgba->getR(), rgba->getG(), rgba->getB(), alpha).Rgb());
  return Response::OK();
}

Response InspectorEmulationAgent::setDeviceMetricsOverride(
    int width,
    int height,
    double device_scale_factor,
    bool mobile,
    Maybe<double> scale,
    Maybe<int> screen_width,
    Maybe<int> screen_height,
    Maybe<int> position_x,
    Maybe<int> position_y,
    Maybe<bool> dont_set_visible_size,
    Maybe<protocol::Emulation::ScreenOrientation>,
    Maybe<protocol::Page::Viewport>) {
  // We don't have to do anything other than reply to the client, as the
  // emulation parameters should have already been updated by the handling of
  // ViewMsg_EnableDeviceEmulation.
  return AssertPage();
}

Response InspectorEmulationAgent::clearDeviceMetricsOverride() {
  // We don't have to do anything other than reply to the client, as the
  // emulation parameters should have already been cleared by the handling of
  // ViewMsg_DisableDeviceEmulation.
  return AssertPage();
}

Response InspectorEmulationAgent::setUserAgentOverride(
    const String& user_agent,
    protocol::Maybe<String> accept_language,
    protocol::Maybe<String> platform) {
  if (!user_agent.IsEmpty() || accept_language.isJust() || platform.isJust())
    InnerEnable();
  state_->setString(EmulationAgentState::kUserAgentOverride, user_agent);
  state_->setString(EmulationAgentState::kAcceptLanguageOverride,
                    accept_language.fromMaybe(String()));
  state_->setString(EmulationAgentState::kNavigatorPlatform,
                    platform.fromMaybe(String()));
  if (web_local_frame_) {
    GetWebViewImpl()->GetPage()->GetSettings().SetNavigatorPlatformOverride(
        platform.fromMaybe(String()));
  }
  return Response::OK();
}

void InspectorEmulationAgent::ApplyAcceptLanguageOverride(String* accept_lang) {
  String accept_lang_override;
  state_->getString(EmulationAgentState::kAcceptLanguageOverride,
                    &accept_lang_override);
  if (!accept_lang_override.IsEmpty())
    *accept_lang = accept_lang_override;
}

void InspectorEmulationAgent::ApplyUserAgentOverride(String* user_agent) {
  String user_agent_override;
  state_->getString(EmulationAgentState::kUserAgentOverride,
                    &user_agent_override);
  if (!user_agent_override.IsEmpty())
    *user_agent = user_agent_override;
}

void InspectorEmulationAgent::InnerEnable() {
  if (enabled_)
    return;
  enabled_ = true;
  instrumenting_agents_->addInspectorEmulationAgent(this);
}

Response InspectorEmulationAgent::AssertPage() {
  if (!web_local_frame_) {
    LOG(ERROR) << "Can only enable virtual time for pages, not workers";
    return Response::InvalidParams(
        "Can only enable virtual time for pages, not workers");
  }
  return Response::OK();
}

void InspectorEmulationAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(web_local_frame_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
