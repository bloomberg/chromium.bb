// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_testing_agent.h"

#include "third_party/blink/renderer/core/inspector/inspected_frames.h"

namespace blink {

InspectorTestingAgent::InspectorTestingAgent(InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

InspectorTestingAgent::~InspectorTestingAgent() = default;

void InspectorTestingAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

protocol::Response InspectorTestingAgent::generateTestReport(
    const String& message,
    protocol::Maybe<String> group) {
  return protocol::Response::OK();
}

}  // namespace blink
