// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"

#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "modules/device_orientation/DeviceOrientationController.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include "platform/wtf/Assertions.h"

namespace blink {

using protocol::Response;

namespace DeviceOrientationInspectorAgentState {
static const char kAlpha[] = "alpha";
static const char kBeta[] = "beta";
static const char kGamma[] = "gamma";
static const char kOverrideEnabled[] = "overrideEnabled";
}

DeviceOrientationInspectorAgent::~DeviceOrientationInspectorAgent() {}

DeviceOrientationInspectorAgent::DeviceOrientationInspectorAgent(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

void DeviceOrientationInspectorAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

DeviceOrientationController* DeviceOrientationInspectorAgent::Controller() {
  Document* document = inspected_frames_->Root()->GetDocument();
  return document ? &DeviceOrientationController::From(*document) : nullptr;
}

Response DeviceOrientationInspectorAgent::setDeviceOrientationOverride(
    double alpha,
    double beta,
    double gamma) {
  state_->setBoolean(DeviceOrientationInspectorAgentState::kOverrideEnabled,
                     true);
  state_->setDouble(DeviceOrientationInspectorAgentState::kAlpha, alpha);
  state_->setDouble(DeviceOrientationInspectorAgentState::kBeta, beta);
  state_->setDouble(DeviceOrientationInspectorAgentState::kGamma, gamma);
  if (Controller()) {
    Controller()->SetOverride(
        DeviceOrientationData::Create(alpha, beta, gamma, false));
  }
  return Response::OK();
}

Response DeviceOrientationInspectorAgent::clearDeviceOrientationOverride() {
  state_->setBoolean(DeviceOrientationInspectorAgentState::kOverrideEnabled,
                     false);
  if (Controller())
    Controller()->ClearOverride();
  return Response::OK();
}

Response DeviceOrientationInspectorAgent::disable() {
  state_->setBoolean(DeviceOrientationInspectorAgentState::kOverrideEnabled,
                     false);
  if (Controller())
    Controller()->ClearOverride();
  return Response::OK();
}

void DeviceOrientationInspectorAgent::Restore() {
  if (!Controller())
    return;
  if (state_->booleanProperty(
          DeviceOrientationInspectorAgentState::kOverrideEnabled, false)) {
    double alpha = 0;
    state_->getDouble(DeviceOrientationInspectorAgentState::kAlpha, &alpha);
    double beta = 0;
    state_->getDouble(DeviceOrientationInspectorAgentState::kBeta, &beta);
    double gamma = 0;
    state_->getDouble(DeviceOrientationInspectorAgentState::kGamma, &gamma);
    Controller()->SetOverride(
        DeviceOrientationData::Create(alpha, beta, gamma, false));
  }
}

void DeviceOrientationInspectorAgent::DidCommitLoadForLocalFrame(
    LocalFrame* frame) {
  if (frame == inspected_frames_->Root()) {
    // New document in main frame - apply override there.
    // No need to cleanup previous one, as it's already gone.
    Restore();
  }
}

}  // namespace blink
