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

InspectorEmulationAgent* InspectorEmulationAgent::Create(
    WebLocalFrameImpl* web_local_frame_impl,
    Client* client) {
  return new InspectorEmulationAgent(web_local_frame_impl, client);
}

InspectorEmulationAgent::InspectorEmulationAgent(
    WebLocalFrameImpl* web_local_frame_impl,
    Client* client)
    : web_local_frame_(web_local_frame_impl),
      client_(client),
      virtual_time_observer_registered_(false) {}

InspectorEmulationAgent::~InspectorEmulationAgent() {}

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
  auto rgba_value =
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
  if (virtual_time_observer_registered_) {
    web_local_frame_->View()->Scheduler()->RemoveVirtualTimeObserver(this);
    virtual_time_observer_registered_ = false;
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

Response InspectorEmulationAgent::setCPUThrottlingRate(double throttling_rate) {
  client_->SetCPUThrottlingRate(throttling_rate);
  return Response::OK();
}

Response InspectorEmulationAgent::setVirtualTimePolicy(
    const String& policy,
    Maybe<int> budget,
    protocol::Maybe<int> max_virtual_time_task_starvation_count) {
  if (protocol::Emulation::VirtualTimePolicyEnum::Advance == policy) {
    web_local_frame_->View()->Scheduler()->SetVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::ADVANCE);
  } else if (protocol::Emulation::VirtualTimePolicyEnum::Pause == policy) {
    web_local_frame_->View()->Scheduler()->SetVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::PAUSE);
  } else if (protocol::Emulation::VirtualTimePolicyEnum::
                 PauseIfNetworkFetchesPending == policy) {
    web_local_frame_->View()->Scheduler()->SetVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::DETERMINISTIC_LOADING);
  }
  web_local_frame_->View()->Scheduler()->EnableVirtualTime();
  if (!virtual_time_observer_registered_) {
    web_local_frame_->View()->Scheduler()->AddVirtualTimeObserver(this);
    virtual_time_observer_registered_ = true;
  }

  if (budget.isJust()) {
    WTF::TimeDelta budget_amount =
        WTF::TimeDelta::FromMilliseconds(budget.fromJust());
    web_local_frame_->View()->Scheduler()->GrantVirtualTimeBudget(
        budget_amount,
        WTF::Bind(&InspectorEmulationAgent::VirtualTimeBudgetExpired,
                  WrapWeakPersistent(this)));
  }
  if (max_virtual_time_task_starvation_count.isJust()) {
    web_local_frame_->View()->Scheduler()->SetMaxVirtualTimeTaskStarvationCount(
        max_virtual_time_task_starvation_count.fromJust());
  }
  return Response::OK();
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
      WebViewScheduler::VirtualTimePolicy::PAUSE);
  GetFrontend()->virtualTimeBudgetExpired();
}

void InspectorEmulationAgent::OnVirtualTimeAdvanced(
    WTF::TimeDelta virtual_time_offset) {
  GetFrontend()->virtualTimeAdvanced(virtual_time_offset.InMilliseconds());
}

void InspectorEmulationAgent::OnVirtualTimePaused(
    WTF::TimeDelta virtual_time_offset) {
  GetFrontend()->virtualTimePaused(virtual_time_offset.InMilliseconds());
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

DEFINE_TRACE(InspectorEmulationAgent) {
  visitor->Trace(web_local_frame_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
