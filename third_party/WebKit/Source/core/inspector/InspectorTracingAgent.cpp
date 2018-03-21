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
#include "core/loader/FrameLoader.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "public/platform/WebLayerTreeView.h"

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
                                             InspectedFrames* inspected_frames)
    : client_(client), inspected_frames_(inspected_frames) {}

InspectorTracingAgent::~InspectorTracingAgent() {}

void InspectorTracingAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorTracingAgent::Restore() {
  state_->getString(TracingAgentState::kSessionId, &session_id_);
  if (IsStarted()) {
    instrumenting_agents_->addInspectorTracingAgent(this);
    EmitMetadataEvents();
  }
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

void InspectorTracingAgent::DidStartWorker(WorkerInspectorProxy* proxy, bool) {
  // For now we assume this is document. TODO(kinuko): Fix this.
  DCHECK(proxy->GetExecutionContext()->IsDocument());
  LocalFrame* frame = ToDocument(proxy->GetExecutionContext())->GetFrame();
  if (proxy->GetWorkerThread() && frame && inspected_frames_->Contains(frame)) {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "TracingSessionIdForWorker", TRACE_EVENT_SCOPE_THREAD,
                         "data",
                         InspectorTracingSessionIdForWorkerEvent::Data(
                             frame, proxy->GetWorkerThread()));
  }
}

void InspectorTracingAgent::start(Maybe<String> categories,
                                  Maybe<String> options,
                                  Maybe<double> buffer_usage_reporting_interval,
                                  Maybe<String> transfer_mode,
                                  Maybe<String> transfer_compression,
                                  Maybe<protocol::Tracing::TraceConfig> config,
                                  std::unique_ptr<StartCallback> callback) {
  DCHECK(!IsStarted());
  if (config.isJust()) {
    callback->sendFailure(Response::Error(
        "Using trace config on renderer targets is not supported yet."));
    return;
  }

  instrumenting_agents_->addInspectorTracingAgent(this);
  session_id_ = IdentifiersFactory::CreateIdentifier();
  state_->setString(TracingAgentState::kSessionId, session_id_);

  // Tracing is already started by DevTools TracingHandler::Start for the
  // renderer target in the browser process. It will eventually start tracing
  // in the renderer process via IPC. But we still need a redundant enable here
  // for EmitMetadataEvents, at which point we are not sure if tracing
  // is already started in the renderer process.
  TraceEvent::EnableTracing(categories.fromMaybe(String()));

  EmitMetadataEvents();
  callback->sendSuccess();
}

void InspectorTracingAgent::end(std::unique_ptr<EndCallback> callback) {
  TraceEvent::DisableTracing();
  InnerDisable();
  callback->sendSuccess();
}

bool InspectorTracingAgent::IsStarted() const {
  return !session_id_.IsEmpty();
}

void InspectorTracingAgent::EmitMetadataEvents() {
  TRACE_EVENT_INSTANT1(kDevtoolsMetadataEventCategory, "TracingStartedInPage",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorTracingStartedInFrame::Data(
                           session_id_, inspected_frames_->Root()));
  for (WorkerInspectorProxy* proxy : WorkerInspectorProxy::AllProxies())
    DidStartWorker(proxy, false);
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
  session_id_ = String();
}

}  // namespace blink
