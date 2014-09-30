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

#include "config.h"

#include "core/workers/WorkerMessagingProxy.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/events/ErrorEvent.h"
#include "core/events/MessageEvent.h"
#include "core/frame/Console.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/WorkerDebuggerAgent.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/workers/DedicatedWorkerGlobalScope.h"
#include "core/workers/DedicatedWorkerThread.h"
#include "core/workers/Worker.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerObjectProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/NotImplemented.h"
#include "platform/heap/Handle.h"
#include "wtf/Functional.h"
#include "wtf/MainThread.h"

namespace blink {

class MessageWorkerGlobalScopeTask : public ExecutionContextTask {
public:
    static PassOwnPtr<MessageWorkerGlobalScopeTask> create(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels)
    {
        return adoptPtr(new MessageWorkerGlobalScopeTask(message, channels));
    }

private:
    MessageWorkerGlobalScopeTask(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels)
        : m_message(message)
        , m_channels(channels)
    {
    }

    virtual void performTask(ExecutionContext* scriptContext)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(scriptContext->isWorkerGlobalScope());
        DedicatedWorkerGlobalScope* context = static_cast<DedicatedWorkerGlobalScope*>(scriptContext);
        OwnPtrWillBeRawPtr<MessagePortArray> ports = MessagePort::entanglePorts(*scriptContext, m_channels.release());
        context->dispatchEvent(MessageEvent::create(ports.release(), m_message));
        context->thread()->workerObjectProxy().confirmMessageFromWorkerObject(context->hasPendingActivity());
    }

private:
    RefPtr<SerializedScriptValue> m_message;
    OwnPtr<MessagePortChannelArray> m_channels;
};

WorkerMessagingProxy::WorkerMessagingProxy(Worker* workerObject, PassOwnPtrWillBeRawPtr<WorkerClients> workerClients)
    : m_executionContext(workerObject->executionContext())
    , m_workerObjectProxy(WorkerObjectProxy::create(m_executionContext.get(), this))
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

WorkerMessagingProxy::~WorkerMessagingProxy()
{
    ASSERT(!m_workerObject);
    ASSERT((m_executionContext->isDocument() && isMainThread())
        || (m_executionContext->isWorkerGlobalScope() && toWorkerGlobalScope(m_executionContext.get())->thread()->isCurrentThread()));
}

void WorkerMessagingProxy::startWorkerGlobalScope(const KURL& scriptURL, const String& userAgent, const String& sourceCode, WorkerThreadStartMode startMode)
{
    // FIXME: This need to be revisited when we support nested worker one day
    ASSERT(m_executionContext->isDocument());
    if (m_askedToTerminate) {
        // Worker.terminate() could be called from JS before the thread was created.
        return;
    }
    Document* document = toDocument(m_executionContext.get());
    SecurityOrigin* starterOrigin = document->securityOrigin();

    OwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData = WorkerThreadStartupData::create(scriptURL, userAgent, sourceCode, startMode, document->contentSecurityPolicy()->deprecatedHeader(), document->contentSecurityPolicy()->deprecatedHeaderType(), starterOrigin, m_workerClients.release());
    double originTime = document->loader() ? document->loader()->timing()->referenceMonotonicTime() : monotonicallyIncreasingTime();

    RefPtr<DedicatedWorkerThread> thread = DedicatedWorkerThread::create(*this, *m_workerObjectProxy.get(), originTime, startupData.release());
    thread->start();
    workerThreadCreated(thread);
    m_workerInspectorProxy->workerThreadCreated(m_executionContext.get(), m_workerThread.get(), scriptURL);
}

void WorkerMessagingProxy::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels)
{
    if (!m_workerObject || m_askedToTerminate)
        return;

    OwnPtrWillBeRawPtr<MessagePortArray> ports = MessagePort::entanglePorts(*m_executionContext.get(), channels);
    m_workerObject->dispatchEvent(MessageEvent::create(ports.release(), message));
}

void WorkerMessagingProxy::postMessageToWorkerGlobalScope(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels)
{
    if (m_askedToTerminate)
        return;

    if (m_workerThread) {
        ++m_unconfirmedMessageCount;
        m_workerThread->postTask(MessageWorkerGlobalScopeTask::create(message, channels));
    } else
        m_queuedEarlyTasks.append(MessageWorkerGlobalScopeTask::create(message, channels));
}

bool WorkerMessagingProxy::postTaskToWorkerGlobalScope(PassOwnPtr<ExecutionContextTask> task)
{
    if (m_askedToTerminate)
        return false;

    ASSERT(m_workerThread);
    m_workerThread->postTask(task);
    return true;
}

void WorkerMessagingProxy::postTaskToLoader(PassOwnPtr<ExecutionContextTask> task)
{
    // FIXME: In case of nested workers, this should go directly to the root Document context.
    ASSERT(m_executionContext->isDocument());
    m_executionContext->postTask(task);
}

void WorkerMessagingProxy::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    if (!m_workerObject)
        return;

    // We don't bother checking the askedToTerminate() flag here, because exceptions should *always* be reported even if the thread is terminated.
    // This is intentionally different than the behavior in MessageWorkerTask, because terminated workers no longer deliver messages (section 4.6 of the WebWorker spec), but they do report exceptions.

    RefPtrWillBeRawPtr<ErrorEvent> event = ErrorEvent::create(errorMessage, sourceURL, lineNumber, columnNumber, 0);
    bool errorHandled = !m_workerObject->dispatchEvent(event);
    if (!errorHandled)
        m_executionContext->reportException(event, 0, nullptr, NotSharableCrossOrigin);
}

void WorkerMessagingProxy::reportConsoleMessage(MessageSource source, MessageLevel level, const String& message, int lineNumber, const String& sourceURL)
{
    if (m_askedToTerminate)
        return;
    // FIXME: In case of nested workers, this should go directly to the root Document context.
    ASSERT(m_executionContext->isDocument());
    Document* document = toDocument(m_executionContext.get());
    LocalFrame* frame = document->frame();
    if (!frame)
        return;

    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(source, level, message, sourceURL, lineNumber);
    consoleMessage->setWorkerGlobalScopeProxy(this);
    frame->console().addMessage(consoleMessage.release());
}

void WorkerMessagingProxy::workerThreadCreated(PassRefPtr<DedicatedWorkerThread> workerThread)
{
    ASSERT(!m_askedToTerminate);
    m_workerThread = workerThread;

    unsigned taskCount = m_queuedEarlyTasks.size();
    ASSERT(!m_unconfirmedMessageCount);
    m_unconfirmedMessageCount = taskCount;
    m_workerThreadHadPendingActivity = true; // Worker initialization means a pending activity.

    for (unsigned i = 0; i < taskCount; ++i)
        m_workerThread->postTask(m_queuedEarlyTasks[i].release());
    m_queuedEarlyTasks.clear();
}

void WorkerMessagingProxy::workerObjectDestroyed()
{
    m_workerObject = 0;
    m_executionContext->postTask(createCrossThreadTask(&workerObjectDestroyedInternal, AllowCrossThreadAccess(this)));
}

void WorkerMessagingProxy::workerObjectDestroyedInternal(ExecutionContext*, WorkerMessagingProxy* proxy)
{
    proxy->m_mayBeDestroyed = true;
    if (proxy->m_workerThread)
        proxy->terminateWorkerGlobalScope();
    else
        proxy->workerThreadTerminated();
}

void WorkerMessagingProxy::workerThreadTerminated()
{
    // This method is always the last to be performed, so the proxy is not needed for communication
    // in either side any more. However, the Worker object may still exist, and it assumes that the proxy exists, too.
    m_askedToTerminate = true;
    m_workerThread = nullptr;
    terminateInternally();
    if (m_mayBeDestroyed)
        delete this;
}

void WorkerMessagingProxy::terminateWorkerGlobalScope()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;

    if (m_workerThread)
        m_workerThread->stop();

    terminateInternally();
}

void WorkerMessagingProxy::postMessageToPageInspector(const String& message)
{
    if (!m_workerInspectorProxy)
        return;
    WorkerInspectorProxy::PageInspector* pageInspector = m_workerInspectorProxy->pageInspector();
    if (pageInspector)
        pageInspector->dispatchMessageFromWorker(message);
}

WorkerInspectorProxy* WorkerMessagingProxy::workerInspectorProxy()
{
    return m_workerInspectorProxy.get();
}

void WorkerMessagingProxy::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    if (!m_askedToTerminate) {
        ASSERT(m_unconfirmedMessageCount);
        --m_unconfirmedMessageCount;
    }
    reportPendingActivity(hasPendingActivity);
}

void WorkerMessagingProxy::reportPendingActivity(bool hasPendingActivity)
{
    m_workerThreadHadPendingActivity = hasPendingActivity;
}

bool WorkerMessagingProxy::hasPendingActivity() const
{
    return (m_unconfirmedMessageCount || m_workerThreadHadPendingActivity) && !m_askedToTerminate;
}

void WorkerMessagingProxy::terminateInternally()
{
    m_workerInspectorProxy->workerThreadTerminated();

    // FIXME: This need to be revisited when we support nested worker one day
    ASSERT(m_executionContext->isDocument());
    Document* document = toDocument(m_executionContext.get());
    LocalFrame* frame = document->frame();
    if (frame)
        frame->console().adoptWorkerMessagesAfterTermination(this);
}

} // namespace blink
