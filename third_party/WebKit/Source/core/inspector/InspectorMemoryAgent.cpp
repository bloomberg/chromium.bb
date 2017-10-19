/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/InspectorMemoryAgent.h"

#include "core/frame/LocalFrameClient.h"
#include "core/inspector/InspectedFrames.h"
#include "platform/InstanceCounters.h"

namespace blink {

using PrepareForLeakDetectionCallback =
    protocol::Memory::Backend::PrepareForLeakDetectionCallback;
using protocol::Response;

InspectorMemoryAgent::InspectorMemoryAgent(InspectedFrames* inspected_frames)
    : detector_(nullptr), callback_(nullptr), frames_(inspected_frames) {}

InspectorMemoryAgent::~InspectorMemoryAgent() {}

Response InspectorMemoryAgent::getDOMCounters(int* documents,
                                              int* nodes,
                                              int* js_event_listeners) {
  *documents =
      InstanceCounters::CounterValue(InstanceCounters::kDocumentCounter);
  *nodes = InstanceCounters::CounterValue(InstanceCounters::kNodeCounter);
  *js_event_listeners =
      InstanceCounters::CounterValue(InstanceCounters::kJSEventListenerCounter);
  return Response::OK();
}

void InspectorMemoryAgent::prepareForLeakDetection(
    std::unique_ptr<PrepareForLeakDetectionCallback> callback) {
  callback_ = std::move(callback);
  detector_.reset(new BlinkLeakDetector(this));
  detector_->PrepareForLeakDetection(frames_->Root()->Client()->GetWebFrame());
  detector_->CollectGarbage();
}

void InspectorMemoryAgent::OnLeakDetectionComplete() {
  DCHECK(callback_);
  callback_->sendSuccess();
  callback_.reset();
  detector_.reset();
}

void InspectorMemoryAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(frames_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
