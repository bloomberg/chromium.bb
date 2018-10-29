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

#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_log_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_session.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/protocol/Protocol.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/layout_test_support.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/web_thread_supporting_gc.h"

namespace blink {

// static
WorkerInspectorController* WorkerInspectorController::Create(
    WorkerThread* thread,
    scoped_refptr<InspectorTaskRunner> inspector_task_runner,
    mojom::blink::DevToolsAgentRequest agent_request,
    mojom::blink::DevToolsAgentHostPtrInfo host_ptr_info) {
  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::From(thread->GetIsolate());
  return debugger ? new WorkerInspectorController(
                        thread, debugger, std::move(inspector_task_runner),
                        std::move(agent_request), std::move(host_ptr_info))
                  : nullptr;
}

WorkerInspectorController::WorkerInspectorController(
    WorkerThread* thread,
    WorkerThreadDebugger* debugger,
    scoped_refptr<InspectorTaskRunner> inspector_task_runner,
    mojom::blink::DevToolsAgentRequest agent_request,
    mojom::blink::DevToolsAgentHostPtrInfo host_ptr_info)
    : debugger_(debugger), thread_(thread), probe_sink_(new CoreProbeSink()) {
  probe_sink_->addInspectorTraceEvents(new InspectorTraceEvents());
  if (auto* scope = DynamicTo<WorkerGlobalScope>(thread->GlobalScope())) {
    worker_devtools_token_ = thread->GetDevToolsWorkerToken();
    parent_devtools_token_ = scope->GetParentDevToolsToken();
    url_ = scope->Url();
    worker_thread_id_ = thread->GetPlatformThreadId();
  }
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      Platform::Current()->GetIOTaskRunner();
  if (!parent_devtools_token_.is_empty() && io_task_runner) {
    // There may be no io task runner in unit tests.
    agent_ = new DevToolsAgent(this, std::move(inspector_task_runner),
                               std::move(io_task_runner));
    agent_->BindRequest(std::move(host_ptr_info), std::move(agent_request),
                        thread->GetTaskRunner(TaskType::kInternalInspector));
  }
  TraceEvent::AddEnabledStateObserver(this);
  EmitTraceEvent();
}

WorkerInspectorController::~WorkerInspectorController() {
  DCHECK(!thread_);
  TraceEvent::RemoveEnabledStateObserver(this);
}

InspectorSession* WorkerInspectorController::AttachSession(
    InspectorSession::Client* session_client,
    mojom::blink::DevToolsSessionStatePtr reattach_session_state) {
  if (!sessions_.size())
    thread_->GetWorkerBackingThread().BackingThread().AddTaskObserver(this);

  bool should_reattach = !reattach_session_state.is_null();

  InspectedFrames* inspected_frames = new InspectedFrames(nullptr);
  InspectorSession* inspector_session = new InspectorSession(
      session_client, probe_sink_.Get(), inspected_frames, 0,
      debugger_->GetV8Inspector(), debugger_->ContextGroupId(thread_),
      std::move(reattach_session_state));
  inspector_session->Append(
      new InspectorLogAgent(thread_->GetConsoleMessageStorage(), nullptr,
                            inspector_session->V8Session()));
  if (auto* scope = DynamicTo<WorkerGlobalScope>(thread_->GlobalScope())) {
    DCHECK(scope->EnsureFetcher());
    inspector_session->Append(new InspectorNetworkAgent(
        inspected_frames, scope, inspector_session->V8Session()));
    inspector_session->Append(new InspectorEmulationAgent(nullptr));
  }

  if (should_reattach)
    inspector_session->Restore();
  sessions_.insert(inspector_session);
  return inspector_session;
}

void WorkerInspectorController::DetachSession(InspectorSession* session) {
  sessions_.erase(session);
  if (!sessions_.size())
    thread_->GetWorkerBackingThread().BackingThread().RemoveTaskObserver(this);
}

void WorkerInspectorController::InspectElement(const WebPoint&) {
  NOTREACHED();
}

void WorkerInspectorController::DebuggerTaskStarted() {
  thread_->DebuggerTaskStarted();
}

void WorkerInspectorController::DebuggerTaskFinished() {
  thread_->DebuggerTaskFinished();
}

void WorkerInspectorController::Dispose() {
  if (agent_)
    agent_->Dispose();
  thread_ = nullptr;
}

void WorkerInspectorController::FlushProtocolNotifications() {
  if (agent_)
    agent_->FlushProtocolNotifications();
}

void WorkerInspectorController::WillProcessTask(
    const base::PendingTask& pending_task) {}

void WorkerInspectorController::DidProcessTask(
    const base::PendingTask& pending_task) {
  FlushProtocolNotifications();
}

void WorkerInspectorController::OnTraceLogEnabled() {
  EmitTraceEvent();
}

void WorkerInspectorController::OnTraceLogDisabled() {}

void WorkerInspectorController::EmitTraceEvent() {
  if (worker_devtools_token_.is_empty())
    return;
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "TracingSessionIdForWorker", TRACE_EVENT_SCOPE_THREAD,
                       "data",
                       InspectorTracingSessionIdForWorkerEvent::Data(
                           worker_devtools_token_, parent_devtools_token_, url_,
                           worker_thread_id_));
}

void WorkerInspectorController::Trace(blink::Visitor* visitor) {
  visitor->Trace(agent_);
  visitor->Trace(probe_sink_);
  visitor->Trace(sessions_);
}

}  // namespace blink
