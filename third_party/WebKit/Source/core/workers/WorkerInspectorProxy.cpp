// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/workers/WorkerInspectorProxy.h"

#include "core/dom/CrossThreadTask.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/workers/WorkerThread.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/KURL.h"

namespace blink {

WorkerInspectorProxy::WorkerInspectorProxy()
    : m_workerThread(nullptr)
    , m_executionContext(nullptr)
    , m_pageInspector(nullptr)
    , m_workerGlobalScopeProxy(nullptr)
{
}

PassOwnPtr<WorkerInspectorProxy> WorkerInspectorProxy::create()
{
    return adoptPtr(new WorkerInspectorProxy());
}

WorkerInspectorProxy::~WorkerInspectorProxy()
{
}

void WorkerInspectorProxy::workerThreadCreated(ExecutionContext* context, WorkerThread* workerThread, const KURL& url)
{
    m_workerThread = workerThread;
    m_executionContext = context;
    InspectorInstrumentation::didStartWorker(context, this, url);
}

void WorkerInspectorProxy::workerThreadTerminated()
{
    if (m_workerThread)
        InspectorInstrumentation::workerTerminated(m_executionContext, this);
    m_workerThread = nullptr;
    m_pageInspector = nullptr;
}

static void connectToWorkerGlobalScopeInspectorTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->connectFrontend();
}

void WorkerInspectorProxy::connectToInspector(WorkerInspectorProxy::PageInspector* pageInspector)
{
    if (!m_workerThread)
        return;
    ASSERT(!m_pageInspector);
    m_pageInspector = pageInspector;
    m_workerThread->postDebuggerTask(FROM_HERE, createCrossThreadTask(connectToWorkerGlobalScopeInspectorTask, true));
}

static void disconnectFromWorkerGlobalScopeInspectorTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->disconnectFrontend();
}

void WorkerInspectorProxy::disconnectFromInspector()
{
    m_pageInspector = nullptr;
    if (!m_workerThread)
        return;
    m_workerThread->postDebuggerTask(FROM_HERE, createCrossThreadTask(disconnectFromWorkerGlobalScopeInspectorTask, true));
}

static void dispatchOnInspectorBackendTask(ExecutionContext* context, const String& message)
{
    toWorkerGlobalScope(context)->workerInspectorController()->dispatchMessageFromFrontend(message);
}

void WorkerInspectorProxy::sendMessageToInspector(const String& message)
{
    if (!m_workerThread)
        return;
    m_workerThread->postDebuggerTask(FROM_HERE, createCrossThreadTask(dispatchOnInspectorBackendTask, String(message)));
    m_workerThread->interruptAndDispatchInspectorCommands();
}

void WorkerInspectorProxy::writeTimelineStartedEvent(const String& sessionId, const String& workerId)
{
    if (!m_workerThread)
        return;
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "TracingSessionIdForWorker", "data", InspectorTracingSessionIdForWorkerEvent::data(sessionId, workerId, m_workerThread));
}

} // namespace blink
