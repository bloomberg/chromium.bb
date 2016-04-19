/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include "core/workers/InProcessWorkerMessagingProxy.h"

#include "bindings/core/v8/V8GCController.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/SecurityContext.h"
#include "core/events/ErrorEvent.h"
#include "core/events/MessageEvent.h"
#include "core/frame/Console.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/workers/InProcessWorkerBase.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThreadStartupData.h"

namespace blink {

namespace {

void processExceptionOnWorkerGlobalScope(int exceptionId, bool handled, ExecutionContext* scriptContext)
{
    WorkerGlobalScope* globalScope = toWorkerGlobalScope(scriptContext);
    globalScope->exceptionHandled(exceptionId, handled);
}

void processMessageOnWorkerGlobalScope(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels, InProcessWorkerObjectProxy* workerObjectProxy, ExecutionContext* scriptContext)
{
    WorkerGlobalScope* globalScope = toWorkerGlobalScope(scriptContext);
    MessagePortArray* ports = MessagePort::entanglePorts(*scriptContext, channels);
    globalScope->dispatchEvent(MessageEvent::create(ports, message));
    workerObjectProxy->confirmMessageFromWorkerObject(V8GCController::hasPendingActivity(globalScope->thread()->isolate(), scriptContext));
}

} // namespace

InProcessWorkerMessagingProxy::InProcessWorkerMessagingProxy(InProcessWorkerBase* workerObject, WorkerClients* workerClients)
    : m_executionContext(workerObject->getExecutionContext())
    , m_workerObjectProxy(InProcessWorkerObjectProxy::create(this))
    , m_workerObject(workerObject)
    , m_mayBeDestroyed(false)
    , m_unconfirmedMessageCount(0)
    , m_workerThreadHadPendingActivity(false)
    , m_askedToTerminate(false)
    , m_workerInspectorProxy(WorkerInspectorProxy::create())
    , m_workerClients(workerClients)
{
    ASSERT(m_workerObject);
    ASSERT((m_executionContext->isDocument() && isMainThread())
        || (m_executionContext->isWorkerGlobalScope() && toWorkerGlobalScope(m_executionContext.get())->thread()->isCurrentThread()));
}

InProcessWorkerMessagingProxy::~InProcessWorkerMessagingProxy()
{
    ASSERT(!m_workerObject);
    ASSERT((m_executionContext->isDocument() && isMainThread())
        || (m_executionContext->isWorkerGlobalScope() && toWorkerGlobalScope(m_executionContext.get())->thread()->isCurrentThread()));
    if (m_loaderProxy)
        m_loaderProxy->detachProvider(this);
}

void InProcessWorkerMessagingProxy::startWorkerGlobalScope(const KURL& scriptURL, const String& userAgent, const String& sourceCode)
{
    // FIXME: This need to be revisited when we support nested worker one day
    ASSERT(m_executionContext->isDocument());
    if (m_askedToTerminate) {
        // Worker.terminate() could be called from JS before the thread was created.
        return;
    }
    Document* document = toDocument(m_executionContext.get());
    SecurityOrigin* starterOrigin = document->getSecurityOrigin();

    ContentSecurityPolicy* csp = m_workerObject->contentSecurityPolicy() ? m_workerObject->contentSecurityPolicy() : document->contentSecurityPolicy();
    ASSERT(csp);

    WorkerThreadStartMode startMode = m_workerInspectorProxy->workerStartMode(document);
    OwnPtr<WorkerThreadStartupData> startupData = WorkerThreadStartupData::create(scriptURL, userAgent, sourceCode, nullptr, startMode, csp->headers(), starterOrigin, m_workerClients.release(), document->addressSpace());
    double originTime = document->loader() ? document->loader()->timing().referenceMonotonicTime() : monotonicallyIncreasingTime();

    m_loaderProxy = WorkerLoaderProxy::create(this);
    m_workerThread = createWorkerThread(originTime);
    m_workerThread->start(startupData.release());
    workerThreadCreated();
    m_workerInspectorProxy->workerThreadCreated(document, m_workerThread.get(), scriptURL);
}

void InProcessWorkerMessagingProxy::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels)
{
    if (!m_workerObject || m_askedToTerminate)
        return;

    MessagePortArray* ports = MessagePort::entanglePorts(*m_executionContext.get(), channels);
    m_workerObject->dispatchEvent(MessageEvent::create(ports, message));
}

void InProcessWorkerMessagingProxy::postMessageToWorkerGlobalScope(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels)
{
    if (m_askedToTerminate)
        return;

    OwnPtr<ExecutionContextTask> task = createCrossThreadTask(&processMessageOnWorkerGlobalScope, message, channels, AllowCrossThreadAccess(&workerObjectProxy()));
    if (m_workerThread) {
        ++m_unconfirmedMessageCount;
        m_workerThread->postTask(BLINK_FROM_HERE, task.release());
    } else {
        m_queuedEarlyTasks.append(task.release());
    }
}

bool InProcessWorkerMessagingProxy::postTaskToWorkerGlobalScope(PassOwnPtr<ExecutionContextTask> task)
{
    if (m_askedToTerminate)
        return false;

    ASSERT(m_workerThread);
    m_workerThread->postTask(BLINK_FROM_HERE, task);
    return true;
}

void InProcessWorkerMessagingProxy::postTaskToLoader(PassOwnPtr<ExecutionContextTask> task)
{
    // FIXME: In case of nested workers, this should go directly to the root Document context.
    ASSERT(m_executionContext->isDocument());
    m_executionContext->postTask(BLINK_FROM_HERE, task);
}

void InProcessWorkerMessagingProxy::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, int exceptionId)
{
    if (!m_workerObject)
        return;

    // We don't bother checking the askedToTerminate() flag here, because exceptions should *always* be reported even if the thread is terminated.
    // This is intentionally different than the behavior in MessageWorkerTask, because terminated workers no longer deliver messages (section 4.6 of the WebWorker spec), but they do report exceptions.

    ErrorEvent* event = ErrorEvent::create(errorMessage, sourceURL, lineNumber, columnNumber, nullptr);
    DispatchEventResult dispatchResult = m_workerObject->dispatchEvent(event);
    postTaskToWorkerGlobalScope(createCrossThreadTask(&processExceptionOnWorkerGlobalScope, exceptionId, dispatchResult != DispatchEventResult::NotCanceled));
}

void InProcessWorkerMessagingProxy::reportConsoleMessage(MessageSource source, MessageLevel level, const String& message, int lineNumber, const String& sourceURL)
{
    if (m_askedToTerminate)
        return;
    // FIXME: In case of nested workers, this should go directly to the root Document context.
    ASSERT(m_executionContext->isDocument());
    Document* document = toDocument(m_executionContext.get());
    LocalFrame* frame = document->frame();
    if (!frame)
        return;

    ConsoleMessage* consoleMessage = ConsoleMessage::create(source, level, message, sourceURL, lineNumber);
    consoleMessage->setWorkerInspectorProxy(m_workerInspectorProxy.get());
    frame->console().addMessage(consoleMessage);
}

void InProcessWorkerMessagingProxy::workerThreadCreated()
{
    ASSERT(!m_askedToTerminate);
    ASSERT(m_workerThread);

    ASSERT(!m_unconfirmedMessageCount);
    m_unconfirmedMessageCount = m_queuedEarlyTasks.size();
    m_workerThreadHadPendingActivity = true; // Worker initialization means a pending activity.

    for (auto& earlyTasks : m_queuedEarlyTasks)
        m_workerThread->postTask(BLINK_FROM_HERE, earlyTasks.release());
    m_queuedEarlyTasks.clear();
}

void InProcessWorkerMessagingProxy::workerObjectDestroyed()
{
    // workerObjectDestroyed() is called in InProcessWorkerBase's destructor.
    // Thus it should be guaranteed that a weak pointer m_workerObject has been cleared
    // before this method gets called.
    ASSERT(!m_workerObject);

    m_executionContext->postTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::workerObjectDestroyedInternal, this));
}

void InProcessWorkerMessagingProxy::workerObjectDestroyedInternal()
{
    m_mayBeDestroyed = true;
    if (m_workerThread)
        terminateWorkerGlobalScope();
    else
        workerThreadTerminated();
}

void InProcessWorkerMessagingProxy::workerThreadTerminated()
{
    // This method is always the last to be performed, so the proxy is not needed for communication
    // in either side any more. However, the Worker object may still exist, and it assumes that the proxy exists, too.
    m_askedToTerminate = true;
    m_workerThread = nullptr;
    terminateInternally();
    if (m_mayBeDestroyed)
        delete this;
}

void InProcessWorkerMessagingProxy::terminateWorkerGlobalScope()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;

    if (m_workerThread)
        m_workerThread->terminate();

    terminateInternally();
}

void InProcessWorkerMessagingProxy::postMessageToPageInspector(const String& message)
{
    if (m_workerInspectorProxy)
        m_workerInspectorProxy->dispatchMessageFromWorker(message);
}

void InProcessWorkerMessagingProxy::postWorkerConsoleAgentEnabled()
{
    if (m_workerInspectorProxy)
        m_workerInspectorProxy->workerConsoleAgentEnabled();
}

void InProcessWorkerMessagingProxy::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    if (!m_askedToTerminate) {
        ASSERT(m_unconfirmedMessageCount);
        --m_unconfirmedMessageCount;
    }
    reportPendingActivity(hasPendingActivity);
}

void InProcessWorkerMessagingProxy::reportPendingActivity(bool hasPendingActivity)
{
    m_workerThreadHadPendingActivity = hasPendingActivity;
}

bool InProcessWorkerMessagingProxy::hasPendingActivity() const
{
    return (m_unconfirmedMessageCount || m_workerThreadHadPendingActivity) && !m_askedToTerminate;
}

void InProcessWorkerMessagingProxy::terminateInternally()
{
    m_workerInspectorProxy->workerThreadTerminated();

    // FIXME: This need to be revisited when we support nested worker one day
    ASSERT(m_executionContext->isDocument());
    Document* document = toDocument(m_executionContext.get());
    LocalFrame* frame = document->frame();
    if (frame)
        frame->console().adoptWorkerMessagesAfterTermination(m_workerInspectorProxy.get());
}

} // namespace blink
