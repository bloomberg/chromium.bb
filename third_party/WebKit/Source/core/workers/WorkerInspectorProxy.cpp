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
    : worker_thread_(nullptr),
      execution_context_(nullptr),
      page_inspector_(nullptr) {}

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
  page_inspector_ = nullptr;
  execution_context_ = nullptr;
}

void WorkerInspectorProxy::DispatchMessageFromWorker(const String& message) {
  if (page_inspector_)
    page_inspector_->DispatchMessageFromWorker(this, message);
}

void WorkerInspectorProxy::AddConsoleMessageFromWorker(
    MessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location) {
  execution_context_->AddConsoleMessage(ConsoleMessage::CreateFromWorker(
      level, message, std::move(location), inspector_id_));
}

static void ConnectToWorkerGlobalScopeInspectorTask(
    WorkerThread* worker_thread) {
  if (WorkerInspectorController* inspector =
          worker_thread->GetWorkerInspectorController())
    inspector->ConnectFrontend();
}

void WorkerInspectorProxy::ConnectToInspector(
    WorkerInspectorProxy::PageInspector* page_inspector) {
  if (!worker_thread_)
    return;
  DCHECK(!page_inspector_);
  page_inspector_ = page_inspector;
  worker_thread_->AppendDebuggerTask(
      CrossThreadBind(ConnectToWorkerGlobalScopeInspectorTask,
                      CrossThreadUnretained(worker_thread_)));
}

static void DisconnectFromWorkerGlobalScopeInspectorTask(
    WorkerThread* worker_thread) {
  if (WorkerInspectorController* inspector =
          worker_thread->GetWorkerInspectorController())
    inspector->DisconnectFrontend();
}

void WorkerInspectorProxy::DisconnectFromInspector(
    WorkerInspectorProxy::PageInspector* page_inspector) {
  DCHECK(page_inspector_ == page_inspector);
  page_inspector_ = nullptr;
  if (worker_thread_)
    worker_thread_->AppendDebuggerTask(
        CrossThreadBind(DisconnectFromWorkerGlobalScopeInspectorTask,
                        CrossThreadUnretained(worker_thread_)));
}

static void DispatchOnInspectorBackendTask(const String& message,
                                           WorkerThread* worker_thread) {
  if (WorkerInspectorController* inspector =
          worker_thread->GetWorkerInspectorController())
    inspector->DispatchMessageFromFrontend(message);
}

void WorkerInspectorProxy::SendMessageToInspector(const String& message) {
  if (worker_thread_)
    worker_thread_->AppendDebuggerTask(
        CrossThreadBind(DispatchOnInspectorBackendTask, message,
                        CrossThreadUnretained(worker_thread_)));
}

void WorkerInspectorProxy::WriteTimelineStartedEvent(const String& session_id) {
  if (!worker_thread_)
    return;
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "TracingSessionIdForWorker", TRACE_EVENT_SCOPE_THREAD,
                       "data",
                       InspectorTracingSessionIdForWorkerEvent::Data(
                           session_id, InspectorId(), worker_thread_));
}

DEFINE_TRACE(WorkerInspectorProxy) {
  visitor->Trace(execution_context_);
}

}  // namespace blink
