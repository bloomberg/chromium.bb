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
#include "core/dom/ErrorEvent.h"
#include "core/dom/MessageEvent.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/WorkerDebuggerAgent.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/DOMWindow.h"
#include "core/page/PageGroup.h"
#include "core/platform/NotImplemented.h"
#include "core/workers/DedicatedWorkerGlobalScope.h"
#include "core/workers/DedicatedWorkerThread.h"
#include "core/workers/Worker.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "wtf/MainThread.h"

namespace WebCore {

class MessageWorkerGlobalScopeTask : public ScriptExecutionContext::Task {
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

    virtual void performTask(ScriptExecutionContext* scriptContext)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(scriptContext->isWorkerGlobalScope());
        DedicatedWorkerGlobalScope* context = static_cast<DedicatedWorkerGlobalScope*>(scriptContext);
        OwnPtr<MessagePortArray> ports = MessagePort::entanglePorts(*scriptContext, m_channels.release());
        context->dispatchEvent(MessageEvent::create(ports.release(), m_message));
        context->thread()->workerObjectProxy().confirmMessageFromWorkerObject(context->hasPendingActivity());
    }

private:
    RefPtr<SerializedScriptValue> m_message;
    OwnPtr<MessagePortChannelArray> m_channels;
};

class MessageWorkerTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<MessageWorkerTask> create(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels, WorkerMessagingProxy* messagingProxy)
    {
        return adoptPtr(new MessageWorkerTask(message, channels, messagingProxy));
    }

private:
    MessageWorkerTask(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels, WorkerMessagingProxy* messagingProxy)
        : m_message(message)
        , m_channels(channels)
        , m_messagingProxy(messagingProxy)
    {
    }

    virtual void performTask(ScriptExecutionContext* scriptContext)
    {
        Worker* workerObject = m_messagingProxy->workerObject();
        if (!workerObject || m_messagingProxy->askedToTerminate())
            return;

        OwnPtr<MessagePortArray> ports = MessagePort::entanglePorts(*scriptContext, m_channels.release());
        workerObject->dispatchEvent(MessageEvent::create(ports.release(), m_message));
    }

private:
    RefPtr<SerializedScriptValue> m_message;
    OwnPtr<MessagePortChannelArray> m_channels;
    WorkerMessagingProxy* m_messagingProxy;
};

class WorkerExceptionTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<WorkerExceptionTask> create(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, WorkerMessagingProxy* messagingProxy)
    {
        return adoptPtr(new WorkerExceptionTask(errorMessage, lineNumber, columnNumber, sourceURL, messagingProxy));
    }

private:
    WorkerExceptionTask(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, WorkerMessagingProxy* messagingProxy)
        : m_errorMessage(errorMessage.isolatedCopy())
        , m_lineNumber(lineNumber)
        , m_columnNumber(columnNumber)
        , m_sourceURL(sourceURL.isolatedCopy())
        , m_messagingProxy(messagingProxy)
    {
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        Worker* workerObject = m_messagingProxy->workerObject();
        if (!workerObject)
            return;

        // We don't bother checking the askedToTerminate() flag here, because exceptions should *always* be reported even if the thread is terminated.
        // This is intentionally different than the behavior in MessageWorkerTask, because terminated workers no longer deliver messages (section 4.6 of the WebWorker spec), but they do report exceptions.

        RefPtr<ErrorEvent> event = ErrorEvent::create(m_errorMessage, m_sourceURL, m_lineNumber, m_columnNumber, 0);
        bool errorHandled = !workerObject->dispatchEvent(event);
        if (!errorHandled)
            context->reportException(event, 0, NotSharableCrossOrigin);
    }

    String m_errorMessage;
    int m_lineNumber;
    int m_columnNumber;
    String m_sourceURL;
    WorkerMessagingProxy* m_messagingProxy;
};

class WorkerGlobalScopeDestroyedTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<WorkerGlobalScopeDestroyedTask> create(WorkerMessagingProxy* messagingProxy)
    {
        return adoptPtr(new WorkerGlobalScopeDestroyedTask(messagingProxy));
    }

private:
    WorkerGlobalScopeDestroyedTask(WorkerMessagingProxy* messagingProxy)
        : m_messagingProxy(messagingProxy)
    {
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_messagingProxy->workerGlobalScopeDestroyedInternal();
    }

    WorkerMessagingProxy* m_messagingProxy;
};

class WorkerTerminateTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<WorkerTerminateTask> create(WorkerMessagingProxy* messagingProxy)
    {
        return adoptPtr(new WorkerTerminateTask(messagingProxy));
    }

private:
    WorkerTerminateTask(WorkerMessagingProxy* messagingProxy)
        : m_messagingProxy(messagingProxy)
    {
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_messagingProxy->terminateWorkerGlobalScope();
    }

    WorkerMessagingProxy* m_messagingProxy;
};

class WorkerThreadActivityReportTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<WorkerThreadActivityReportTask> create(WorkerMessagingProxy* messagingProxy, bool confirmingMessage, bool hasPendingActivity)
    {
        return adoptPtr(new WorkerThreadActivityReportTask(messagingProxy, confirmingMessage, hasPendingActivity));
    }

private:
    WorkerThreadActivityReportTask(WorkerMessagingProxy* messagingProxy, bool confirmingMessage, bool hasPendingActivity)
        : m_messagingProxy(messagingProxy)
        , m_confirmingMessage(confirmingMessage)
        , m_hasPendingActivity(hasPendingActivity)
    {
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_messagingProxy->reportPendingActivityInternal(m_confirmingMessage, m_hasPendingActivity);
    }

    WorkerMessagingProxy* m_messagingProxy;
    bool m_confirmingMessage;
    bool m_hasPendingActivity;
};

class PostMessageToPageInspectorTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<PostMessageToPageInspectorTask> create(WorkerMessagingProxy* messagingProxy, const String& message)
    {
        return adoptPtr(new PostMessageToPageInspectorTask(messagingProxy, message));
    }

private:
    PostMessageToPageInspectorTask(WorkerMessagingProxy* messagingProxy, const String& message)
        : m_messagingProxy(messagingProxy)
        , m_message(message.isolatedCopy())
    {
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        if (WorkerGlobalScopeProxy::PageInspector* pageInspector = m_messagingProxy->m_pageInspector)
            pageInspector->dispatchMessageFromWorker(m_message);
    }

    WorkerMessagingProxy* m_messagingProxy;
    String m_message;
};

WorkerMessagingProxy::WorkerMessagingProxy(Worker* workerObject, PassOwnPtr<WorkerClients> workerClients)
    : m_scriptExecutionContext(workerObject->scriptExecutionContext())
    , m_workerObject(workerObject)
    , m_mayBeDestroyed(false)
    , m_unconfirmedMessageCount(0)
    , m_workerThreadHadPendingActivity(false)
    , m_askedToTerminate(false)
    , m_pageInspector(0)
    , m_workerClients(workerClients)
{
    ASSERT(m_workerObject);
    ASSERT((m_scriptExecutionContext->isDocument() && isMainThread())
        || (m_scriptExecutionContext->isWorkerGlobalScope() && toWorkerGlobalScope(m_scriptExecutionContext.get())->thread()->isCurrentThread()));
}

WorkerMessagingProxy::~WorkerMessagingProxy()
{
    ASSERT(!m_workerObject);
    ASSERT((m_scriptExecutionContext->isDocument() && isMainThread())
        || (m_scriptExecutionContext->isWorkerGlobalScope() && toWorkerGlobalScope(m_scriptExecutionContext.get())->thread()->isCurrentThread()));
}

void WorkerMessagingProxy::startWorkerGlobalScope(const KURL& scriptURL, const String& userAgent, const String& sourceCode, WorkerThreadStartMode startMode)
{
    // FIXME: This need to be revisited when we support nested worker one day
    ASSERT(m_scriptExecutionContext->isDocument());
    Document* document = toDocument(m_scriptExecutionContext.get());

    OwnPtr<WorkerThreadStartupData> startupData = WorkerThreadStartupData::create(scriptURL, userAgent, sourceCode, startMode, document->contentSecurityPolicy()->deprecatedHeader(), document->contentSecurityPolicy()->deprecatedHeaderType(), m_workerClients.release());
    double originTime = document->loader() ? document->loader()->timing()->referenceMonotonicTime() : monotonicallyIncreasingTime();

    RefPtr<DedicatedWorkerThread> thread = DedicatedWorkerThread::create(*this, *this, originTime, startupData.release());
    workerThreadCreated(thread);
    thread->start();
    InspectorInstrumentation::didStartWorkerGlobalScope(m_scriptExecutionContext.get(), this, scriptURL);
}

void WorkerMessagingProxy::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels)
{
    m_scriptExecutionContext->postTask(MessageWorkerTask::create(message, channels, this));
}

void WorkerMessagingProxy::postMessageToWorkerGlobalScope(PassRefPtr<SerializedScriptValue> message, PassOwnPtr<MessagePortChannelArray> channels)
{
    if (m_askedToTerminate)
        return;

    if (m_workerThread) {
        ++m_unconfirmedMessageCount;
        m_workerThread->runLoop().postTask(MessageWorkerGlobalScopeTask::create(message, channels));
    } else
        m_queuedEarlyTasks.append(MessageWorkerGlobalScopeTask::create(message, channels));
}

bool WorkerMessagingProxy::postTaskForModeToWorkerGlobalScope(PassOwnPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    if (m_askedToTerminate)
        return false;

    ASSERT(m_workerThread);
    m_workerThread->runLoop().postTaskForMode(task, mode);
    return true;
}

void WorkerMessagingProxy::postTaskToLoader(PassOwnPtr<ScriptExecutionContext::Task> task)
{
    // FIXME: In case of nested workers, this should go directly to the root Document context.
    ASSERT(m_scriptExecutionContext->isDocument());
    m_scriptExecutionContext->postTask(task);
}

void WorkerMessagingProxy::postExceptionToWorkerObject(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    m_scriptExecutionContext->postTask(WorkerExceptionTask::create(errorMessage, lineNumber, columnNumber, sourceURL, this));
}

static void postConsoleMessageTask(ScriptExecutionContext* context, WorkerMessagingProxy* messagingProxy, MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    if (messagingProxy->askedToTerminate())
        return;
    context->addConsoleMessage(source, level, message, sourceURL, lineNumber);
}

void WorkerMessagingProxy::postConsoleMessageToWorkerObject(MessageSource source, MessageLevel level, const String& message, int lineNumber, const String& sourceURL)
{
    m_scriptExecutionContext->postTask(createCallbackTask(&postConsoleMessageTask, AllowCrossThreadAccess(this), source, level, message, lineNumber, sourceURL));
}

void WorkerMessagingProxy::workerThreadCreated(PassRefPtr<DedicatedWorkerThread> workerThread)
{
    m_workerThread = workerThread;

    if (m_askedToTerminate) {
        // Worker.terminate() could be called from JS before the thread was created.
        m_workerThread->stop();
    } else {
        unsigned taskCount = m_queuedEarlyTasks.size();
        ASSERT(!m_unconfirmedMessageCount);
        m_unconfirmedMessageCount = taskCount;
        m_workerThreadHadPendingActivity = true; // Worker initialization means a pending activity.

        for (unsigned i = 0; i < taskCount; ++i)
            m_workerThread->runLoop().postTask(m_queuedEarlyTasks[i].release());
        m_queuedEarlyTasks.clear();
    }
}

void WorkerMessagingProxy::workerObjectDestroyed()
{
    m_workerObject = 0;
    m_scriptExecutionContext->postTask(createCallbackTask(&workerObjectDestroyedInternal, AllowCrossThreadAccess(this)));
}

void WorkerMessagingProxy::workerObjectDestroyedInternal(ScriptExecutionContext*, WorkerMessagingProxy* proxy)
{
    proxy->m_mayBeDestroyed = true;
    if (proxy->m_workerThread)
        proxy->terminateWorkerGlobalScope();
    else
        proxy->workerGlobalScopeDestroyedInternal();
}

static void connectToWorkerGlobalScopeInspectorTask(ScriptExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->connectFrontend();
}

void WorkerMessagingProxy::connectToInspector(WorkerGlobalScopeProxy::PageInspector* pageInspector)
{
    if (m_askedToTerminate)
        return;
    ASSERT(!m_pageInspector);
    m_pageInspector = pageInspector;
    m_workerThread->runLoop().postTaskForMode(createCallbackTask(connectToWorkerGlobalScopeInspectorTask, true), WorkerDebuggerAgent::debuggerTaskMode);
}

static void disconnectFromWorkerGlobalScopeInspectorTask(ScriptExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->disconnectFrontend();
}

void WorkerMessagingProxy::disconnectFromInspector()
{
    m_pageInspector = 0;
    if (m_askedToTerminate)
        return;
    m_workerThread->runLoop().postTaskForMode(createCallbackTask(disconnectFromWorkerGlobalScopeInspectorTask, true), WorkerDebuggerAgent::debuggerTaskMode);
}

static void dispatchOnInspectorBackendTask(ScriptExecutionContext* context, const String& message)
{
    toWorkerGlobalScope(context)->workerInspectorController()->dispatchMessageFromFrontend(message);
}

void WorkerMessagingProxy::sendMessageToInspector(const String& message)
{
    if (m_askedToTerminate)
        return;
    m_workerThread->runLoop().postTaskForMode(createCallbackTask(dispatchOnInspectorBackendTask, String(message)), WorkerDebuggerAgent::debuggerTaskMode);
    WorkerDebuggerAgent::interruptAndDispatchInspectorCommands(m_workerThread.get());
}

void WorkerMessagingProxy::workerGlobalScopeDestroyed()
{
    m_scriptExecutionContext->postTask(WorkerGlobalScopeDestroyedTask::create(this));
    // Will execute workerGlobalScopeDestroyedInternal() on context's thread.
}

void WorkerMessagingProxy::workerGlobalScopeClosed()
{
    // Executes terminateWorkerGlobalScope() on parent context's thread.
    m_scriptExecutionContext->postTask(WorkerTerminateTask::create(this));
}

void WorkerMessagingProxy::workerGlobalScopeDestroyedInternal()
{
    // WorkerGlobalScopeDestroyedTask is always the last to be performed, so the proxy is not needed for communication
    // in either side any more. However, the Worker object may still exist, and it assumes that the proxy exists, too.
    m_askedToTerminate = true;
    m_workerThread = 0;

    InspectorInstrumentation::workerGlobalScopeTerminated(m_scriptExecutionContext.get(), this);

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

    InspectorInstrumentation::workerGlobalScopeTerminated(m_scriptExecutionContext.get(), this);
}

void WorkerMessagingProxy::postMessageToPageInspector(const String& message)
{
    m_scriptExecutionContext->postTask(PostMessageToPageInspectorTask::create(this, message));
}

void WorkerMessagingProxy::updateInspectorStateCookie(const String&)
{
    notImplemented();
}

void WorkerMessagingProxy::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask(WorkerThreadActivityReportTask::create(this, true, hasPendingActivity));
    // Will execute reportPendingActivityInternal() on context's thread.
}

void WorkerMessagingProxy::reportPendingActivity(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask(WorkerThreadActivityReportTask::create(this, false, hasPendingActivity));
    // Will execute reportPendingActivityInternal() on context's thread.
}

void WorkerMessagingProxy::reportPendingActivityInternal(bool confirmingMessage, bool hasPendingActivity)
{
    if (confirmingMessage && !m_askedToTerminate) {
        ASSERT(m_unconfirmedMessageCount);
        --m_unconfirmedMessageCount;
    }

    m_workerThreadHadPendingActivity = hasPendingActivity;
}

bool WorkerMessagingProxy::hasPendingActivity() const
{
    return (m_unconfirmedMessageCount || m_workerThreadHadPendingActivity) && !m_askedToTerminate;
}

} // namespace WebCore
