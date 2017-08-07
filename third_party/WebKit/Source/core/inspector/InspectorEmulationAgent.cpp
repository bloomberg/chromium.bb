// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorEmulationAgent.h"

#include "core/exported/WebViewBase.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/inspector/DevToolsEmulator.h"
#include "core/inspector/protocol/DOM.h"
#include "core/page/Page.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/graphics/Color.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"
#include "platform/wtf/Time.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebThread.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace EmulationAgentState {
static const char kScriptExecutionDisabled[] = "scriptExecutionDisabled";
static const char kTouchEventEmulationEnabled[] = "touchEventEmulationEnabled";
static const char kEmulatedMedia[] = "emulatedMedia";
static const char kDefaultBackgroundColorOverrideRGBA[] =
    "defaultBackgroundColorOverrideRGBA";
}

InspectorEmulationAgent* InspectorEmulationAgent::Create(
    WebLocalFrameImpl* web_local_frame_impl,
    Client* client) {
  return new InspectorEmulationAgent(web_local_frame_impl, client);
}

InspectorEmulationAgent::InspectorEmulationAgent(
    WebLocalFrameImpl* web_local_frame_impl,
    Client* client)
    : web_local_frame_(web_local_frame_impl), client_(client) {}

InspectorEmulationAgent::~InspectorEmulationAgent() {}

WebViewBase* InspectorEmulationAgent::GetWebViewBase() {
  return web_local_frame_->ViewImpl();
}

void InspectorEmulationAgent::Restore() {
  setScriptExecutionDisabled(state_->booleanProperty(
      EmulationAgentState::kScriptExecutionDisabled, false));
  setTouchEmulationEnabled(
      state_->booleanProperty(EmulationAgentState::kTouchEventEmulationEnabled,
                              false),
      protocol::Maybe<String>());
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
}

Response InspectorEmulationAgent::disable() {
  setScriptExecutionDisabled(false);
  setTouchEmulationEnabled(false, Maybe<String>());
  setEmulatedMedia(String());
  setCPUThrottlingRate(1);
  setDefaultBackgroundColorOverride(Maybe<protocol::DOM::RGBA>());
  return Response::OK();
}

Response InspectorEmulationAgent::resetPageScaleFactor() {
  GetWebViewBase()->ResetScaleStateImmediately();
  return Response::OK();
}

Response InspectorEmulationAgent::setPageScaleFactor(double page_scale_factor) {
  GetWebViewBase()->SetPageScaleFactor(static_cast<float>(page_scale_factor));
  return Response::OK();
}

Response InspectorEmulationAgent::setScriptExecutionDisabled(bool value) {
  state_->setBoolean(EmulationAgentState::kScriptExecutionDisabled, value);
  GetWebViewBase()->GetDevToolsEmulator()->SetScriptExecutionDisabled(value);
  return Response::OK();
}

Response InspectorEmulationAgent::setTouchEmulationEnabled(
    bool enabled,
    Maybe<String> configuration) {
  state_->setBoolean(EmulationAgentState::kTouchEventEmulationEnabled, enabled);
  GetWebViewBase()->GetDevToolsEmulator()->SetTouchEventEmulationEnabled(
      enabled);
  return Response::OK();
}

Response InspectorEmulationAgent::setEmulatedMedia(const String& media) {
  state_->setString(EmulationAgentState::kEmulatedMedia, media);
  GetWebViewBase()->GetPage()->GetSettings().SetMediaTypeOverride(media);
  return Response::OK();
}

Response InspectorEmulationAgent::setCPUThrottlingRate(double throttling_rate) {
  client_->SetCPUThrottlingRate(throttling_rate);
  return Response::OK();
}

Response InspectorEmulationAgent::setVirtualTimePolicy(const String& policy,
                                                       Maybe<int> budget) {
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

  if (budget.isJust()) {
    WTF::TimeDelta budget_amount =
        WTF::TimeDelta::FromMilliseconds(budget.fromJust());
    web_local_frame_->View()->Scheduler()->GrantVirtualTimeBudget(
        budget_amount,
        WTF::Bind(&InspectorEmulationAgent::VirtualTimeBudgetExpired,
                  WrapWeakPersistent(this)));
  }
  return Response::OK();
}

void InspectorEmulationAgent::VirtualTimeBudgetExpired() {
  web_local_frame_->View()->Scheduler()->SetVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::PAUSE);
  GetFrontend()->virtualTimeBudgetExpired();
}

Response InspectorEmulationAgent::setDefaultBackgroundColorOverride(
    Maybe<protocol::DOM::RGBA> color) {
  if (!color.isJust()) {
    // Clear the override and state.
    GetWebViewBase()->ClearBaseBackgroundColorOverride();
    state_->remove(EmulationAgentState::kDefaultBackgroundColorOverrideRGBA);
    return Response::OK();
  }

  blink::protocol::DOM::RGBA* rgba = color.fromJust();
  state_->setValue(EmulationAgentState::kDefaultBackgroundColorOverrideRGBA,
                   rgba->toValue());
  // Clamping of values is done by Color() constructor.
  int alpha = lroundf(255.0f * rgba->getA(1.0f));
  GetWebViewBase()->SetBaseBackgroundColorOverride(
      Color(rgba->getR(), rgba->getG(), rgba->getB(), alpha).Rgb());
  return Response::OK();
}

DEFINE_TRACE(InspectorEmulationAgent) {
  visitor->Trace(web_local_frame_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
