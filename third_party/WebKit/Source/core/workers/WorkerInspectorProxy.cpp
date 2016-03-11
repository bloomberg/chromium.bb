// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerInspectorProxy.h"

#include "core/dom/CrossThreadTask.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/workers/WorkerThread.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

WorkerInspectorProxy::WorkerInspectorProxy()
    : m_workerThread(nullptr)
    , m_executionContext(nullptr)
    , m_pageInspector(nullptr)
{
}

PassOwnPtrWillBeRawPtr<WorkerInspectorProxy> WorkerInspectorProxy::create()
{
    return adoptPtrWillBeNoop(new WorkerInspectorProxy());
}

WorkerInspectorProxy::~WorkerInspectorProxy()
{
}

WorkerThreadStartMode WorkerInspectorProxy::workerStartMode(ExecutionContext* context)
{
    if (InspectorInstrumentation::shouldWaitForDebuggerOnWorkerStart(context))
        return PauseWorkerGlobalScopeOnStart;
    return DontPauseWorkerGlobalScopeOnStart;
}

void WorkerInspectorProxy::workerThreadCreated(ExecutionContext* context, WorkerThread* workerThread, const KURL& url)
{
    m_workerThread = workerThread;
    m_executionContext = context;
    m_url = url.getString();
    // We expect everyone starting worker thread to synchronously ask for workerStartMode right before.
    bool waitingForDebugger = InspectorInstrumentation::shouldWaitForDebuggerOnWorkerStart(context);
    InspectorInstrumentation::didStartWorker(context, this, waitingForDebugger);
}

void WorkerInspectorProxy::workerThreadTerminated()
{
    if (m_workerThread)
        InspectorInstrumentation::workerTerminated(m_executionContext, this);
    m_workerThread = nullptr;
    m_pageInspector = nullptr;
}

void WorkerInspectorProxy::dispatchMessageFromWorker(const String& message)
{
    if (m_pageInspector)
        m_pageInspector->dispatchMessageFromWorker(this, message);
}

void WorkerInspectorProxy::workerConsoleAgentEnabled()
{
    if (m_pageInspector)
        m_pageInspector->workerConsoleAgentEnabled(this);
}

static void connectToWorkerGlobalScopeInspectorTask(WorkerThread* workerThread)
{
    workerThread->workerGlobalScope()->workerInspectorController()->connectFrontend();
}

void WorkerInspectorProxy::connectToInspector(WorkerInspectorProxy::PageInspector* pageInspector)
{
    if (!m_workerThread)
        return;
    ASSERT(!m_pageInspector);
    m_pageInspector = pageInspector;
    m_workerThread->appendDebuggerTask(threadSafeBind(connectToWorkerGlobalScopeInspectorTask, AllowCrossThreadAccess(m_workerThread)));
}

static void disconnectFromWorkerGlobalScopeInspectorTask(WorkerThread* workerThread)
{
    workerThread->workerGlobalScope()->workerInspectorController()->disconnectFrontend();
}

void WorkerInspectorProxy::disconnectFromInspector(WorkerInspectorProxy::PageInspector* pageInspector)
{
    ASSERT(m_pageInspector == pageInspector);
    m_pageInspector = nullptr;
    if (m_workerThread)
        m_workerThread->appendDebuggerTask(threadSafeBind(disconnectFromWorkerGlobalScopeInspectorTask, AllowCrossThreadAccess(m_workerThread)));
}

static void dispatchOnInspectorBackendTask(const String& message, WorkerThread* workerThread)
{
    workerThread->workerGlobalScope()->workerInspectorController()->dispatchMessageFromFrontend(message);
}

void WorkerInspectorProxy::sendMessageToInspector(const String& message)
{
    if (m_workerThread)
        m_workerThread->appendDebuggerTask(threadSafeBind(dispatchOnInspectorBackendTask, message, AllowCrossThreadAccess(m_workerThread)));
}

void WorkerInspectorProxy::writeTimelineStartedEvent(const String& sessionId, const String& workerId)
{
    if (!m_workerThread)
        return;
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "TracingSessionIdForWorker", TRACE_EVENT_SCOPE_THREAD, "data", InspectorTracingSessionIdForWorkerEvent::data(sessionId, workerId, m_workerThread));
}

DEFINE_TRACE(WorkerInspectorProxy)
{
    visitor->trace(m_executionContext);
}

} // namespace blink
