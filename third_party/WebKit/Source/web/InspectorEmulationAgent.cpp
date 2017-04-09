// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/InspectorEmulationAgent.h"

#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/inspector/protocol/DOM.h"
#include "core/page/Page.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/graphics/Color.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebViewScheduler.h"
#include "web/DevToolsEmulator.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace EmulationAgentState {
static const char kScriptExecutionDisabled[] = "scriptExecutionDisabled";
static const char kTouchEventEmulationEnabled[] = "touchEventEmulationEnabled";
static const char kEmulatedMedia[] = "emulatedMedia";
static const char kForcedViewportEnabled[] = "forcedViewportEnabled";
static const char kForcedViewportX[] = "forcedViewportX";
static const char kForcedViewportY[] = "forcedViewportY";
static const char kForcedViewportScale[] = "forcedViewportScale";
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
    : web_local_frame_impl_(web_local_frame_impl), client_(client) {}

InspectorEmulationAgent::~InspectorEmulationAgent() {}

WebViewImpl* InspectorEmulationAgent::GetWebViewImpl() {
  return web_local_frame_impl_->ViewImpl();
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
  if (state_->booleanProperty(EmulationAgentState::kForcedViewportEnabled,
                              false)) {
    forceViewport(

        state_->doubleProperty(EmulationAgentState::kForcedViewportX, 0),
        state_->doubleProperty(EmulationAgentState::kForcedViewportY, 0),
        state_->doubleProperty(EmulationAgentState::kForcedViewportScale, 1));
  }
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
  resetViewport();
  setDefaultBackgroundColorOverride(Maybe<protocol::DOM::RGBA>());
  return Response::OK();
}

Response InspectorEmulationAgent::forceViewport(double x,
                                                double y,
                                                double scale) {
  if (x < 0 || y < 0)
    return Response::Error("Coordinates must be non-negative");

  if (scale <= 0)
    return Response::Error("Scale must be positive");

  state_->setBoolean(EmulationAgentState::kForcedViewportEnabled, true);
  state_->setDouble(EmulationAgentState::kForcedViewportX, x);
  state_->setDouble(EmulationAgentState::kForcedViewportY, y);
  state_->setDouble(EmulationAgentState::kForcedViewportScale, scale);

  GetWebViewImpl()->GetDevToolsEmulator()->ForceViewport(WebFloatPoint(x, y),
                                                         scale);
  return Response::OK();
}

Response InspectorEmulationAgent::resetViewport() {
  state_->setBoolean(EmulationAgentState::kForcedViewportEnabled, false);
  GetWebViewImpl()->GetDevToolsEmulator()->ResetViewport();
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
    Maybe<String> configuration) {
  state_->setBoolean(EmulationAgentState::kTouchEventEmulationEnabled, enabled);
  GetWebViewImpl()->GetDevToolsEmulator()->SetTouchEventEmulationEnabled(
      enabled);
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

Response InspectorEmulationAgent::setVirtualTimePolicy(const String& policy,
                                                       Maybe<int> budget) {
  if (protocol::Emulation::VirtualTimePolicyEnum::Advance == policy) {
    web_local_frame_impl_->View()->Scheduler()->SetVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::ADVANCE);
  } else if (protocol::Emulation::VirtualTimePolicyEnum::Pause == policy) {
    web_local_frame_impl_->View()->Scheduler()->SetVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::PAUSE);
  } else if (protocol::Emulation::VirtualTimePolicyEnum::
                 PauseIfNetworkFetchesPending == policy) {
    web_local_frame_impl_->View()->Scheduler()->SetVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::DETERMINISTIC_LOADING);
  }
  web_local_frame_impl_->View()->Scheduler()->EnableVirtualTime();

  if (budget.isJust()) {
    RefPtr<WebTaskRunner> task_runner =
        Platform::Current()->CurrentThread()->GetWebTaskRunner();
    long long delay_millis = static_cast<long long>(budget.fromJust());
    virtual_time_budget_expired_task_handle_ =
        task_runner->PostDelayedCancellableTask(
            BLINK_FROM_HERE,
            WTF::Bind(&InspectorEmulationAgent::VirtualTimeBudgetExpired,
                      WrapWeakPersistent(this)),
            delay_millis);
  }
  return Response::OK();
}

void InspectorEmulationAgent::VirtualTimeBudgetExpired() {
  web_local_frame_impl_->View()->Scheduler()->SetVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::PAUSE);
  GetFrontend()->virtualTimeBudgetExpired();
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
  visitor->Trace(web_local_frame_impl_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
