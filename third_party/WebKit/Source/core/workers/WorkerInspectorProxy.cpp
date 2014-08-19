// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/workers/WorkerInspectorProxy.h"

#include "core/dom/CrossThreadTask.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/workers/WorkerThread.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/KURL.h"

namespace blink {

WorkerInspectorProxy::WorkerInspectorProxy()
    : m_workerThread(0)
    , m_executionContext(0)
    , m_pageInspector(0)
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
    m_workerThread = 0;
    m_pageInspector = 0;
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
    m_workerThread->postDebuggerTask(createCrossThreadTask(connectToWorkerGlobalScopeInspectorTask, true));
}

static void disconnectFromWorkerGlobalScopeInspectorTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->disconnectFrontend();
}

void WorkerInspectorProxy::disconnectFromInspector()
{
    m_pageInspector = 0;
    if (!m_workerThread)
        return;
    m_workerThread->postDebuggerTask(createCrossThreadTask(disconnectFromWorkerGlobalScopeInspectorTask, true));
}

static void dispatchOnInspectorBackendTask(ExecutionContext* context, const String& message)
{
    toWorkerGlobalScope(context)->workerInspectorController()->dispatchMessageFromFrontend(message);
}

void WorkerInspectorProxy::sendMessageToInspector(const String& message)
{
    if (!m_workerThread)
        return;
    m_workerThread->postDebuggerTask(createCrossThreadTask(dispatchOnInspectorBackendTask, String(message)));
    m_workerThread->interruptAndDispatchInspectorCommands();
}

static void dispatchWriteTimelineStartedEvent(ExecutionContext* context, const String& sessionId)
{
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "TracingStartedInWorker", "sessionId", sessionId.utf8());
}

void WorkerInspectorProxy::writeTimelineStartedEvent(const String& sessionId)
{
    if (!m_workerThread)
        return;
    m_workerThread->postTask(createCrossThreadTask(dispatchWriteTimelineStartedEvent, String(sessionId)));
}

} // namespace blink
