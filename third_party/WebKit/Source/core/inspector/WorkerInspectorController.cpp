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

#include "core/inspector/WorkerInspectorController.h"

#include "core/CoreProbeSink.h"
#include "core/inspector/InspectorLogAgent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/inspector/protocol/Protocol.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/WebThreadSupportingGC.h"

namespace blink {

WorkerInspectorController* WorkerInspectorController::Create(
    WorkerThread* thread) {
  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::From(thread->GetIsolate());
  return debugger ? new WorkerInspectorController(thread, debugger) : nullptr;
}

WorkerInspectorController::WorkerInspectorController(
    WorkerThread* thread,
    WorkerThreadDebugger* debugger)
    : debugger_(debugger),
      thread_(thread),
      instrumenting_agents_(new CoreProbeSink()) {
  instrumenting_agents_->addInspectorTraceEvents(new InspectorTraceEvents());
}

WorkerInspectorController::~WorkerInspectorController() {
  DCHECK(!thread_);
}

void WorkerInspectorController::ConnectFrontend() {
  if (session_)
    return;

  // sessionId will be overwritten by WebDevToolsAgent::sendProtocolNotification
  // call.
  session_ = new InspectorSession(this, instrumenting_agents_.Get(), 0,
                                  debugger_->GetV8Inspector(),
                                  debugger_->ContextGroupId(thread_), nullptr);
  session_->Append(
      new InspectorLogAgent(thread_->GetConsoleMessageStorage(), nullptr));
  thread_->GetWorkerBackingThread().BackingThread().AddTaskObserver(this);
}

void WorkerInspectorController::DisconnectFrontend() {
  if (!session_)
    return;
  session_->Dispose();
  session_.Clear();
  thread_->GetWorkerBackingThread().BackingThread().RemoveTaskObserver(this);
}

void WorkerInspectorController::DispatchMessageFromFrontend(
    const String& message) {
  if (!session_)
    return;
  String method;
  if (!protocol::DispatcherBase::getCommandName(message, &method))
    return;
  session_->DispatchProtocolMessage(method, message);
}

void WorkerInspectorController::Dispose() {
  DisconnectFrontend();
  thread_ = nullptr;
}

void WorkerInspectorController::FlushProtocolNotifications() {
  if (session_)
    session_->flushProtocolNotifications();
}

void WorkerInspectorController::SendProtocolMessage(int session_id,
                                                    int call_id,
                                                    const String& response,
                                                    const String& state) {
  // Worker messages are wrapped, no need to handle callId or state.
  thread_->GetWorkerReportingProxy().PostMessageToPageInspector(response);
}

void WorkerInspectorController::WillProcessTask() {}

void WorkerInspectorController::DidProcessTask() {
  if (session_)
    session_->flushProtocolNotifications();
}

DEFINE_TRACE(WorkerInspectorController) {
  visitor->Trace(instrumenting_agents_);
  visitor->Trace(session_);
}

}  // namespace blink
