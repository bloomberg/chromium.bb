// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerInspectorProxy.h"

#include "core/frame/FrameConsole.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

namespace {

static WorkerInspectorProxy::WorkerInspectorProxySet& InspectorProxies() {
  DEFINE_STATIC_LOCAL(WorkerInspectorProxy::WorkerInspectorProxySet, proxies,
                      ());
  return proxies;
}

}  // namespace

const WorkerInspectorProxy::WorkerInspectorProxySet&
WorkerInspectorProxy::AllProxies() {
  return InspectorProxies();
}

WorkerInspectorProxy::WorkerInspectorProxy()
    : worker_thread_(nullptr), execution_context_(nullptr) {}

WorkerInspectorProxy* WorkerInspectorProxy::Create() {
  return new WorkerInspectorProxy();
}

WorkerInspectorProxy::~WorkerInspectorProxy() {}

const String& WorkerInspectorProxy::InspectorId() {
  if (inspector_id_.IsEmpty())
    inspector_id_ = "dedicated:" + IdentifiersFactory::CreateIdentifier();
  return inspector_id_;
}

WorkerThreadStartMode WorkerInspectorProxy::WorkerStartMode(
    ExecutionContext* execution_context) {
  bool result = false;
  probe::shouldWaitForDebuggerOnWorkerStart(execution_context, &result);
  return result ? kPauseWorkerGlobalScopeOnStart
                : kDontPauseWorkerGlobalScopeOnStart;
}

void WorkerInspectorProxy::WorkerThreadCreated(
    ExecutionContext* execution_context,
    WorkerThread* worker_thread,
    const KURL& url) {
  worker_thread_ = worker_thread;
  execution_context_ = execution_context;
  url_ = url.GetString();
  InspectorProxies().insert(this);
  // We expect everyone starting worker thread to synchronously ask for
  // WorkerStartMode() right before.
  bool waiting_for_debugger = false;
  probe::shouldWaitForDebuggerOnWorkerStart(execution_context_,
                                            &waiting_for_debugger);
  probe::didStartWorker(execution_context_, this, waiting_for_debugger);
}

void WorkerInspectorProxy::WorkerThreadTerminated() {
  if (worker_thread_) {
    DCHECK(InspectorProxies().Contains(this));
    InspectorProxies().erase(this);
    probe::workerTerminated(execution_context_, this);
  }

  worker_thread_ = nullptr;
  page_inspectors_.clear();
  execution_context_ = nullptr;
}

void WorkerInspectorProxy::DispatchMessageFromWorker(int session_id,
                                                     const String& message) {
  auto it = page_inspectors_.find(session_id);
  if (it != page_inspectors_.end())
    it->value->DispatchMessageFromWorker(this, session_id, message);
}

void WorkerInspectorProxy::AddConsoleMessageFromWorker(
    MessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location) {
  execution_context_->AddConsoleMessage(ConsoleMessage::CreateFromWorker(
      level, message, std::move(location), inspector_id_));
}

static void ConnectToWorkerGlobalScopeInspectorTask(WorkerThread* worker_thread,
                                                    int session_id) {
  if (WorkerInspectorController* inspector =
          worker_thread->GetWorkerInspectorController()) {
    inspector->ConnectFrontend(session_id);
  }
}

void WorkerInspectorProxy::ConnectToInspector(
    int session_id,
    WorkerInspectorProxy::PageInspector* page_inspector) {
  if (!worker_thread_)
    return;
  DCHECK(page_inspectors_.find(session_id) == page_inspectors_.end());
  page_inspectors_.insert(session_id, page_inspector);
  worker_thread_->AppendDebuggerTask(
      CrossThreadBind(ConnectToWorkerGlobalScopeInspectorTask,
                      CrossThreadUnretained(worker_thread_), session_id));
}

static void DisconnectFromWorkerGlobalScopeInspectorTask(
    WorkerThread* worker_thread,
    int session_id) {
  if (WorkerInspectorController* inspector =
          worker_thread->GetWorkerInspectorController()) {
    inspector->DisconnectFrontend(session_id);
  }
}

void WorkerInspectorProxy::DisconnectFromInspector(
    int session_id,
    WorkerInspectorProxy::PageInspector* page_inspector) {
  DCHECK(page_inspectors_.at(session_id) == page_inspector);
  page_inspectors_.erase(session_id);
  if (worker_thread_) {
    worker_thread_->AppendDebuggerTask(
        CrossThreadBind(DisconnectFromWorkerGlobalScopeInspectorTask,
                        CrossThreadUnretained(worker_thread_), session_id));
  }
}

static void DispatchOnInspectorBackendTask(int session_id,
                                           const String& message,
                                           WorkerThread* worker_thread) {
  if (WorkerInspectorController* inspector =
          worker_thread->GetWorkerInspectorController()) {
    inspector->DispatchMessageFromFrontend(session_id, message);
  }
}

void WorkerInspectorProxy::SendMessageToInspector(int session_id,
                                                  const String& message) {
  if (worker_thread_) {
    worker_thread_->AppendDebuggerTask(
        CrossThreadBind(DispatchOnInspectorBackendTask, session_id, message,
                        CrossThreadUnretained(worker_thread_)));
  }
}

void WorkerInspectorProxy::WriteTimelineStartedEvent(
    const String& tracing_session_id) {
  if (!worker_thread_)
    return;
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "TracingSessionIdForWorker", TRACE_EVENT_SCOPE_THREAD,
                       "data",
                       InspectorTracingSessionIdForWorkerEvent::Data(
                           tracing_session_id, InspectorId(), worker_thread_));
}

DEFINE_TRACE(WorkerInspectorProxy) {
  visitor->Trace(execution_context_);
}

}  // namespace blink
