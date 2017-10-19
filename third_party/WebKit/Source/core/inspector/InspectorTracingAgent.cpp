//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "core/inspector/InspectorTracingAgent.h"

#include "core/frame/LocalFrame.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "core/loader/FrameLoader.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace TracingAgentState {
const char kSessionId[] = "sessionId";
}

namespace {
const char kDevtoolsMetadataEventCategory[] =
    TRACE_DISABLED_BY_DEFAULT("devtools.timeline");
}

InspectorTracingAgent::InspectorTracingAgent(Client* client,
                                             InspectorWorkerAgent* worker_agent,
                                             InspectedFrames* inspected_frames)
    : layer_tree_id_(0),
      client_(client),
      worker_agent_(worker_agent),
      inspected_frames_(inspected_frames) {}

void InspectorTracingAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(worker_agent_);
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorTracingAgent::Restore() {
  EmitMetadataEvents();
}

void InspectorTracingAgent::FrameStartedLoading(LocalFrame* frame,
                                                FrameLoadType type) {
  if (frame != inspected_frames_->Root() || !IsReloadLoadType(type))
    return;
  client_->ShowReloadingBlanket();
}

void InspectorTracingAgent::FrameStoppedLoading(LocalFrame* frame) {
  if (frame != inspected_frames_->Root())
    client_->HideReloadingBlanket();
}

void InspectorTracingAgent::start(Maybe<String> categories,
                                  Maybe<String> options,
                                  Maybe<double> buffer_usage_reporting_interval,
                                  Maybe<String> transfer_mode,
                                  Maybe<protocol::Tracing::TraceConfig> config,
                                  std::unique_ptr<StartCallback> callback) {
  DCHECK(!IsStarted());
  if (config.isJust()) {
    callback->sendFailure(Response::Error(
        "Using trace config on renderer targets is not supported yet."));
    return;
  }

  instrumenting_agents_->addInspectorTracingAgent(this);
  state_->setString(TracingAgentState::kSessionId,
                    IdentifiersFactory::CreateIdentifier());
  client_->EnableTracing(categories.fromMaybe(String()));
  EmitMetadataEvents();
  callback->sendSuccess();
}

void InspectorTracingAgent::end(std::unique_ptr<EndCallback> callback) {
  client_->DisableTracing();
  InnerDisable();
  callback->sendSuccess();
}

bool InspectorTracingAgent::IsStarted() const {
  return !SessionId().IsEmpty();
}

String InspectorTracingAgent::SessionId() const {
  String result;
  if (state_)
    state_->getString(TracingAgentState::kSessionId, &result);
  return result;
}

void InspectorTracingAgent::EmitMetadataEvents() {
  TRACE_EVENT_INSTANT1(kDevtoolsMetadataEventCategory, "TracingStartedInPage",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorTracingStartedInFrame::Data(
                           SessionId(), inspected_frames_->Root()));
  if (layer_tree_id_)
    SetLayerTreeId(layer_tree_id_);
  worker_agent_->SetTracingSessionId(SessionId());
}

void InspectorTracingAgent::SetLayerTreeId(int layer_tree_id) {
  layer_tree_id_ = layer_tree_id;
  TRACE_EVENT_INSTANT1(
      kDevtoolsMetadataEventCategory, "SetLayerTreeId",
      TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorSetLayerTreeId::Data(SessionId(), layer_tree_id_));
}

void InspectorTracingAgent::RootLayerCleared() {
  if (IsStarted())
    client_->HideReloadingBlanket();
}

Response InspectorTracingAgent::disable() {
  InnerDisable();
  return Response::OK();
}

void InspectorTracingAgent::InnerDisable() {
  client_->HideReloadingBlanket();
  instrumenting_agents_->removeInspectorTracingAgent(this);
  state_->remove(TracingAgentState::kSessionId);
  worker_agent_->SetTracingSessionId(String());
}

}  // namespace blink
