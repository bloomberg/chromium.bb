// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorEmulationAgent.h"

#include "core/exported/WebViewImpl.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/inspector/DevToolsEmulator.h"
#include "core/inspector/protocol/DOM.h"
#include "core/page/Page.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/graphics/Color.h"
#include "platform/scheduler/util/thread_cpu_throttler.h"
#include "platform/wtf/Time.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTouchEvent.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace EmulationAgentState {
static const char kScriptExecutionDisabled[] = "scriptExecutionDisabled";
static const char kTouchEventEmulationEnabled[] = "touchEventEmulationEnabled";
static const char kMaxTouchPoints[] = "maxTouchPoints";
static const char kEmulatedMedia[] = "emulatedMedia";
static const char kDefaultBackgroundColorOverrideRGBA[] =
    "defaultBackgroundColorOverrideRGBA";
static const char kNavigatorPlatform[] = "navigatorPlatform";
}

InspectorEmulationAgent::InspectorEmulationAgent(
    WebLocalFrameImpl* web_local_frame_impl)
    : web_local_frame_(web_local_frame_impl) {}

InspectorEmulationAgent::~InspectorEmulationAgent() = default;

WebViewImpl* InspectorEmulationAgent::GetWebViewImpl() {
  return web_local_frame_->ViewImpl();
}

void InspectorEmulationAgent::Restore() {
  setScriptExecutionDisabled(state_->booleanProperty(
      EmulationAgentState::kScriptExecutionDisabled, false));
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
  String navigator_platform;
  state_->getString(EmulationAgentState::kNavigatorPlatform,
                    &navigator_platform);
  setNavigatorOverrides(navigator_platform);
}

Response InspectorEmulationAgent::disable() {
  setScriptExecutionDisabled(false);
  setTouchEmulationEnabled(false, Maybe<int>());
  setEmulatedMedia(String());
  setCPUThrottlingRate(1);
  setDefaultBackgroundColorOverride(Maybe<protocol::DOM::RGBA>());
  if (virtual_time_setup_) {
    instrumenting_agents_->removeInspectorEmulationAgent(this);
    web_local_frame_->View()->Scheduler()->RemoveVirtualTimeObserver(this);
    virtual_time_setup_ = false;
  }
  setNavigatorOverrides(String());
  return Response::OK();
}

Response InspectorEmulationAgent::resetPageScaleFactor() {
  GetWebViewImpl()->ResetScaleStateImmediately();
  return Response::OK();
}

Response InspectorEmulationAgent::setPageScaleFactor(double page_scale_factor) {
  GetWebViewImpl()->SetPageScaleFactor(static_cast<float>(page_scale_factor));
  return Response::OK();
}

Response InspectorEmulationAgent::setScriptExecutionDisabled(bool value) {
  state_->setBoolean(EmulationAgentState::kScriptExecutionDisabled, value);
  GetWebViewImpl()->GetDevToolsEmulator()->SetScriptExecutionDisabled(value);
  return Response::OK();
}

Response InspectorEmulationAgent::setTouchEmulationEnabled(
    bool enabled,
    protocol::Maybe<int> max_touch_points) {
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
  return Response::OK();
}

Response InspectorEmulationAgent::setEmulatedMedia(const String& media) {
  state_->setString(EmulationAgentState::kEmulatedMedia, media);
  GetWebViewImpl()->GetPage()->GetSettings().SetMediaTypeOverride(media);
  return Response::OK();
}

Response InspectorEmulationAgent::setCPUThrottlingRate(double rate) {
  scheduler::ThreadCPUThrottler::GetInstance()->SetThrottlingRate(rate);
  return Response::OK();
}

Response InspectorEmulationAgent::setVirtualTimePolicy(
    const String& policy,
    Maybe<double> virtual_time_budget_ms,
    protocol::Maybe<int> max_virtual_time_task_starvation_count,
    protocol::Maybe<bool> wait_for_navigation,
    double* virtual_time_base_ms) {
  PendingVirtualTimePolicy new_policy;
  new_policy.policy = WebViewScheduler::VirtualTimePolicy::kPause;
  if (protocol::Emulation::VirtualTimePolicyEnum::Advance == policy) {
    new_policy.policy = WebViewScheduler::VirtualTimePolicy::kAdvance;
  } else if (protocol::Emulation::VirtualTimePolicyEnum::
                 PauseIfNetworkFetchesPending == policy) {
    new_policy.policy =
        WebViewScheduler::VirtualTimePolicy::kDeterministicLoading;
  }

  if (virtual_time_budget_ms.isJust())
    new_policy.virtual_time_budget_ms = virtual_time_budget_ms.fromJust();

  if (max_virtual_time_task_starvation_count.isJust()) {
    new_policy.max_virtual_time_task_starvation_count =
        max_virtual_time_task_starvation_count.fromJust();
  }

  if (wait_for_navigation.fromMaybe(false)) {
    *virtual_time_base_ms = 0;
    pending_virtual_time_policy_ = std::move(new_policy);
    return Response::OK();
  }

  WTF::TimeDelta virtual_time_base_delta =
      ApplyVirtualTimePolicy(new_policy) - WTF::TimeTicks::UnixEpoch();
  *virtual_time_base_ms = virtual_time_base_delta.InMillisecondsF();

  return Response::OK();
}

WTF::TimeTicks InspectorEmulationAgent::ApplyVirtualTimePolicy(
    const PendingVirtualTimePolicy& new_policy) {
  web_local_frame_->View()->Scheduler()->SetVirtualTimePolicy(
      new_policy.policy);
  WTF::TimeTicks virtual_time_base_ticks(
      web_local_frame_->View()->Scheduler()->EnableVirtualTime());
  if (!virtual_time_setup_) {
    instrumenting_agents_->addInspectorEmulationAgent(this);
    web_local_frame_->View()->Scheduler()->AddVirtualTimeObserver(this);
    virtual_time_setup_ = true;
  }
  if (new_policy.virtual_time_budget_ms) {
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
  return virtual_time_base_ticks;
}

void InspectorEmulationAgent::FrameStartedLoading(LocalFrame*, FrameLoadType) {
  if (pending_virtual_time_policy_) {
    ApplyVirtualTimePolicy(*pending_virtual_time_policy_);
    pending_virtual_time_policy_ = WTF::nullopt;
  }
}

Response InspectorEmulationAgent::setNavigatorOverrides(
    const String& platform) {
  state_->setString(EmulationAgentState::kNavigatorPlatform, platform);
  GetWebViewImpl()->GetPage()->GetSettings().SetNavigatorPlatformOverride(
      platform);
  return Response::OK();
}

void InspectorEmulationAgent::VirtualTimeBudgetExpired() {
  web_local_frame_->View()->Scheduler()->SetVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::kPause);
  GetFrontend()->virtualTimeBudgetExpired();
}

void InspectorEmulationAgent::OnVirtualTimeAdvanced(
    WTF::TimeDelta virtual_time_offset) {
  GetFrontend()->virtualTimeAdvanced(virtual_time_offset.InMillisecondsF());
}

void InspectorEmulationAgent::OnVirtualTimePaused(
    WTF::TimeDelta virtual_time_offset) {
  GetFrontend()->virtualTimePaused(virtual_time_offset.InMillisecondsF());
}

Response InspectorEmulationAgent::setDefaultBackgroundColorOverride(
    Maybe<protocol::DOM::RGBA> color) {
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
  return Response::OK();
}

Response InspectorEmulationAgent::clearDeviceMetricsOverride() {
  // We don't have to do anything other than reply to the client, as the
  // emulation parameters should have already been cleared by the handling of
  // ViewMsg_DisableDeviceEmulation.
  return Response::OK();
}

void InspectorEmulationAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(web_local_frame_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
