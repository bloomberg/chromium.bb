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

#include "core/inspector/InspectorWorkerAgent.h"

#include "core/dom/Document.h"
#include "core/inspector/InspectedFrames.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

using protocol::Response;

namespace WorkerAgentState {
static const char kAutoAttach[] = "autoAttach";
static const char kWaitForDebuggerOnStart[] = "waitForDebuggerOnStart";
static const char kAttachedWorkerIds[] = "attachedWorkerIds";
};

namespace {
// TODO(dgozman): support multiple sessions in protocol.
static const int kSessionId = 1;
}  // namespace

InspectorWorkerAgent::InspectorWorkerAgent(InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

InspectorWorkerAgent::~InspectorWorkerAgent() {}

void InspectorWorkerAgent::Restore() {
  if (!AutoAttachEnabled())
    return;
  instrumenting_agents_->addInspectorWorkerAgent(this);
  protocol::DictionaryValue* attached = AttachedWorkerIds();
  for (size_t i = 0; i < attached->size(); ++i)
    GetFrontend()->detachedFromTarget(attached->at(i).first);
  state_->remove(WorkerAgentState::kAttachedWorkerIds);
  ConnectToAllProxies();
}

Response InspectorWorkerAgent::disable() {
  if (AutoAttachEnabled()) {
    DisconnectFromAllProxies(false);
    instrumenting_agents_->removeInspectorWorkerAgent(this);
  }
  state_->setBoolean(WorkerAgentState::kAutoAttach, false);
  state_->setBoolean(WorkerAgentState::kWaitForDebuggerOnStart, false);
  state_->remove(WorkerAgentState::kAttachedWorkerIds);
  return Response::OK();
}

Response InspectorWorkerAgent::setAutoAttach(bool auto_attach,
                                             bool wait_for_debugger_on_start) {
  state_->setBoolean(WorkerAgentState::kWaitForDebuggerOnStart,
                     wait_for_debugger_on_start);

  if (auto_attach == AutoAttachEnabled())
    return Response::OK();
  state_->setBoolean(WorkerAgentState::kAutoAttach, auto_attach);
  if (auto_attach) {
    instrumenting_agents_->addInspectorWorkerAgent(this);
    ConnectToAllProxies();
  } else {
    DisconnectFromAllProxies(true);
    instrumenting_agents_->removeInspectorWorkerAgent(this);
  }
  return Response::OK();
}

Response InspectorWorkerAgent::setAttachToFrames(bool attach) {
  return Response::OK();
}

bool InspectorWorkerAgent::AutoAttachEnabled() {
  return state_->booleanProperty(WorkerAgentState::kAutoAttach, false);
}

Response InspectorWorkerAgent::sendMessageToTarget(const String& target_id,
                                                   const String& message) {
  WorkerInspectorProxy* proxy = connected_proxies_.at(target_id);
  if (!proxy)
    return Response::Error("Not attached to a target with given id");
  proxy->SendMessageToInspector(kSessionId, message);
  return Response::OK();
}

void InspectorWorkerAgent::SetTracingSessionId(
    const String& tracing_session_id) {
  tracing_session_id_ = tracing_session_id;
  if (tracing_session_id.IsEmpty())
    return;
  for (auto& id_proxy : connected_proxies_)
    id_proxy.value->WriteTimelineStartedEvent(tracing_session_id);
}

void InspectorWorkerAgent::ShouldWaitForDebuggerOnWorkerStart(bool* result) {
  if (AutoAttachEnabled() &&
      state_->booleanProperty(WorkerAgentState::kWaitForDebuggerOnStart, false))
    *result = true;
}

void InspectorWorkerAgent::DidStartWorker(WorkerInspectorProxy* proxy,
                                          bool waiting_for_debugger) {
  DCHECK(GetFrontend() && AutoAttachEnabled());
  ConnectToProxy(proxy, waiting_for_debugger);
  if (!tracing_session_id_.IsEmpty())
    proxy->WriteTimelineStartedEvent(tracing_session_id_);
}

void InspectorWorkerAgent::WorkerTerminated(WorkerInspectorProxy* proxy) {
  DCHECK(GetFrontend() && AutoAttachEnabled());
  if (connected_proxies_.find(proxy->InspectorId()) == connected_proxies_.end())
    return;
  AttachedWorkerIds()->remove(proxy->InspectorId());
  GetFrontend()->detachedFromTarget(proxy->InspectorId());
  proxy->DisconnectFromInspector(kSessionId, this);
  connected_proxies_.erase(proxy->InspectorId());
}

void InspectorWorkerAgent::ConnectToAllProxies() {
  for (WorkerInspectorProxy* proxy : WorkerInspectorProxy::AllProxies()) {
    // For now we assume this is document. TODO(kinuko): Fix this.
    DCHECK(proxy->GetExecutionContext()->IsDocument());
    Document* document = ToDocument(proxy->GetExecutionContext());
    if (document->GetFrame() &&
        inspected_frames_->Contains(document->GetFrame()))
      ConnectToProxy(proxy, false);
  }
}

void InspectorWorkerAgent::DisconnectFromAllProxies(bool report_to_frontend) {
  for (auto& id_proxy : connected_proxies_) {
    if (report_to_frontend) {
      AttachedWorkerIds()->remove(id_proxy.key);
      GetFrontend()->detachedFromTarget(id_proxy.key);
    }
    id_proxy.value->DisconnectFromInspector(kSessionId, this);
  }
  connected_proxies_.clear();
}

void InspectorWorkerAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  if (!AutoAttachEnabled() || frame != inspected_frames_->Root())
    return;

  // During navigation workers from old page may die after a while.
  // Usually, it's fine to report them terminated later, but some tests
  // expect strict set of workers, and we reuse renderer between tests.
  for (auto& id_proxy : connected_proxies_) {
    AttachedWorkerIds()->remove(id_proxy.key);
    GetFrontend()->detachedFromTarget(id_proxy.key);
    id_proxy.value->DisconnectFromInspector(kSessionId, this);
  }
  connected_proxies_.clear();
}

protocol::DictionaryValue* InspectorWorkerAgent::AttachedWorkerIds() {
  protocol::DictionaryValue* ids =
      state_->getObject(WorkerAgentState::kAttachedWorkerIds);
  if (!ids) {
    std::unique_ptr<protocol::DictionaryValue> new_ids =
        protocol::DictionaryValue::create();
    ids = new_ids.get();
    state_->setObject(WorkerAgentState::kAttachedWorkerIds, std::move(new_ids));
  }
  return ids;
}

void InspectorWorkerAgent::ConnectToProxy(WorkerInspectorProxy* proxy,
                                          bool waiting_for_debugger) {
  connected_proxies_.Set(proxy->InspectorId(), proxy);
  proxy->ConnectToInspector(kSessionId, this);
  DCHECK(GetFrontend());
  AttachedWorkerIds()->setBoolean(proxy->InspectorId(), true);
  GetFrontend()->attachedToTarget(protocol::Target::TargetInfo::create()
                                      .setTargetId(proxy->InspectorId())
                                      .setType("worker")
                                      .setTitle(proxy->Url())
                                      .setUrl(proxy->Url())
                                      .build(),
                                  waiting_for_debugger);
}

void InspectorWorkerAgent::DispatchMessageFromWorker(
    WorkerInspectorProxy* proxy,
    int session_id,
    const String& message) {
  DCHECK(session_id == kSessionId);
  GetFrontend()->receivedMessageFromTarget(proxy->InspectorId(), message);
}

DEFINE_TRACE(InspectorWorkerAgent) {
  visitor->Trace(connected_proxies_);
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
