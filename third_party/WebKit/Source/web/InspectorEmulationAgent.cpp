// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/InspectorEmulationAgent.h"

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/scheduler/CancellableTaskFactory.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebViewScheduler.h"
#include "web/DevToolsEmulator.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

namespace EmulationAgentState {
static const char scriptExecutionDisabled[] = "scriptExecutionDisabled";
static const char touchEventEmulationEnabled[] = "touchEventEmulationEnabled";
static const char emulatedMedia[] = "emulatedMedia";
static const char forcedViewportEnabled[] = "forcedViewportEnabled";
static const char forcedViewportX[] = "forcedViewportX";
static const char forcedViewportY[] = "forcedViewportY";
static const char forcedViewportScale[] = "forcedViewportScale";
}

InspectorEmulationAgent* InspectorEmulationAgent::create(
    WebLocalFrameImpl* webLocalFrameImpl,
    Client* client) {
  return new InspectorEmulationAgent(webLocalFrameImpl, client);
}

InspectorEmulationAgent::InspectorEmulationAgent(
    WebLocalFrameImpl* webLocalFrameImpl,
    Client* client)
    : m_webLocalFrameImpl(webLocalFrameImpl),
      m_client(client),
      m_virtualTimeBudgetExpiredTask(CancellableTaskFactory::create(
          this,
          &InspectorEmulationAgent::virtualTimeBudgetExpired)) {}

InspectorEmulationAgent::~InspectorEmulationAgent() {}

WebViewImpl* InspectorEmulationAgent::webViewImpl() {
  return m_webLocalFrameImpl->viewImpl();
}

void InspectorEmulationAgent::restore() {
  ErrorString error;
  setScriptExecutionDisabled(
      &error, m_state->booleanProperty(
                  EmulationAgentState::scriptExecutionDisabled, false));
  setTouchEmulationEnabled(
      &error, m_state->booleanProperty(
                  EmulationAgentState::touchEventEmulationEnabled, false),
      protocol::Maybe<String>());
  String emulatedMedia;
  m_state->getString(EmulationAgentState::emulatedMedia, &emulatedMedia);
  setEmulatedMedia(&error, emulatedMedia);
  if (m_state->booleanProperty(EmulationAgentState::forcedViewportEnabled,
                               false)) {
    forceViewport(
        &error,
        m_state->doubleProperty(EmulationAgentState::forcedViewportX, 0),
        m_state->doubleProperty(EmulationAgentState::forcedViewportY, 0),
        m_state->doubleProperty(EmulationAgentState::forcedViewportScale, 1));
  }
}

void InspectorEmulationAgent::disable(ErrorString*) {
  ErrorString error;
  setScriptExecutionDisabled(&error, false);
  setTouchEmulationEnabled(&error, false, protocol::Maybe<String>());
  setEmulatedMedia(&error, String());
  setCPUThrottlingRate(&error, 1);
  resetViewport(&error);
}

void InspectorEmulationAgent::forceViewport(ErrorString* error,
                                            double x,
                                            double y,
                                            double scale) {
  if (x < 0 || y < 0) {
    *error = "Coordinates must be non-negative";
    return;
  }

  if (scale <= 0) {
    *error = "Scale must be positive";
    return;
  }

  m_state->setBoolean(EmulationAgentState::forcedViewportEnabled, true);
  m_state->setDouble(EmulationAgentState::forcedViewportX, x);
  m_state->setDouble(EmulationAgentState::forcedViewportY, y);
  m_state->setDouble(EmulationAgentState::forcedViewportScale, scale);

  webViewImpl()->devToolsEmulator()->forceViewport(WebFloatPoint(x, y), scale);
}

void InspectorEmulationAgent::resetViewport(ErrorString*) {
  m_state->setBoolean(EmulationAgentState::forcedViewportEnabled, false);
  webViewImpl()->devToolsEmulator()->resetViewport();
}

void InspectorEmulationAgent::resetPageScaleFactor(ErrorString*) {
  webViewImpl()->resetScaleStateImmediately();
}

void InspectorEmulationAgent::setPageScaleFactor(ErrorString*,
                                                 double pageScaleFactor) {
  webViewImpl()->setPageScaleFactor(static_cast<float>(pageScaleFactor));
}

void InspectorEmulationAgent::setScriptExecutionDisabled(ErrorString*,
                                                         bool value) {
  m_state->setBoolean(EmulationAgentState::scriptExecutionDisabled, value);
  webViewImpl()->devToolsEmulator()->setScriptExecutionDisabled(value);
}

void InspectorEmulationAgent::setTouchEmulationEnabled(
    ErrorString*,
    bool enabled,
    const Maybe<String>& configuration) {
  m_state->setBoolean(EmulationAgentState::touchEventEmulationEnabled, enabled);
  webViewImpl()->devToolsEmulator()->setTouchEventEmulationEnabled(enabled);
}

void InspectorEmulationAgent::setEmulatedMedia(ErrorString*,
                                               const String& media) {
  m_state->setString(EmulationAgentState::emulatedMedia, media);
  webViewImpl()->page()->settings().setMediaTypeOverride(media);
}

void InspectorEmulationAgent::setCPUThrottlingRate(ErrorString*,
                                                   double throttlingRate) {
  m_client->setCPUThrottlingRate(throttlingRate);
}

void InspectorEmulationAgent::setVirtualTimePolicy(
    ErrorString*,
    const String& in_policy,
    const Maybe<int>& in_budget) {
  if (protocol::Emulation::VirtualTimePolicyEnum::Advance == in_policy) {
    m_webLocalFrameImpl->view()->scheduler()->setVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::ADVANCE);
  } else if (protocol::Emulation::VirtualTimePolicyEnum::Pause == in_policy) {
    m_webLocalFrameImpl->view()->scheduler()->setVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::PAUSE);
  } else if (protocol::Emulation::VirtualTimePolicyEnum::
                 PauseIfNetworkFetchesPending == in_policy) {
    m_webLocalFrameImpl->view()->scheduler()->setVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::DETERMINISTIC_LOADING);
  }
  m_webLocalFrameImpl->view()->scheduler()->enableVirtualTime();

  if (in_budget.isJust()) {
    WebTaskRunner* taskRunner =
        Platform::current()->currentThread()->getWebTaskRunner();
    long long delayMillis = static_cast<long long>(in_budget.fromJust());
    taskRunner->postDelayedTask(
        BLINK_FROM_HERE, m_virtualTimeBudgetExpiredTask->cancelAndCreate(),
        delayMillis);
  }
}

void InspectorEmulationAgent::virtualTimeBudgetExpired() {
  m_webLocalFrameImpl->view()->scheduler()->setVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::PAUSE);
  frontend()->virtualTimeBudgetExpired();
}

DEFINE_TRACE(InspectorEmulationAgent) {
  visitor->trace(m_webLocalFrameImpl);
  InspectorBaseAgent::trace(visitor);
}

}  // namespace blink
