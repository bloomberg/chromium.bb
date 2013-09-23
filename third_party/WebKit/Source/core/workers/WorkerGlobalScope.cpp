/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
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
#include "core/workers/WorkerGlobalScope.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScheduledAction.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/ScriptValue.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ContextLifecycleNotifier.h"
#include "core/events/ErrorEvent.h"
#include "core/events/Event.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "core/html/DOMURL.h"
#include "core/inspector/InspectorConsoleInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/loader/WorkerThreadableLoader.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/DOMWindow.h"
#include "core/page/WorkerNavigator.h"
#include "core/platform/NotImplemented.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerLocation.h"
#include "core/workers/WorkerObjectProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThread.h"
#include "weborigin/KURL.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/RefPtr.h"
#include "wtf/UnusedParam.h"

namespace WebCore {

class CloseWorkerGlobalScopeTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<CloseWorkerGlobalScopeTask> create()
    {
        return adoptPtr(new CloseWorkerGlobalScopeTask);
    }

    virtual void performTask(ScriptExecutionContext *context)
    {
        WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
        // Notify parent that this context is closed. Parent is responsible for calling WorkerThread::stop().
        workerGlobalScope->thread()->workerReportingProxy().workerGlobalScopeClosed();
    }

    virtual bool isCleanupTask() const { return true; }
};

WorkerGlobalScope::WorkerGlobalScope(const KURL& url, const String& userAgent, WorkerThread* thread, double timeOrigin, PassOwnPtr<WorkerClients> workerClients)
    : m_url(url)
    , m_userAgent(userAgent)
    , m_script(adoptPtr(new WorkerScriptController(this)))
    , m_thread(thread)
    , m_workerInspectorController(adoptPtr(new WorkerInspectorController(this)))
    , m_closing(false)
    , m_eventQueue(WorkerEventQueue::create(this))
    , m_timeOrigin(timeOrigin)
    , m_workerClients(workerClients)
{
    ScriptWrappable::init(this);
    setSecurityOrigin(SecurityOrigin::create(url));
    m_workerClients->reattachThread();
}

WorkerGlobalScope::~WorkerGlobalScope()
{
    ASSERT(thread()->isCurrentThread());

    // Make sure we have no observers.
    notifyObserversOfStop();

    // Notify proxy that we are going away. This can free the WorkerThread object, so do not access it after this.
    thread()->workerReportingProxy().workerGlobalScopeDestroyed();
}

void WorkerGlobalScope::applyContentSecurityPolicyFromString(const String& policy, ContentSecurityPolicy::HeaderType contentSecurityPolicyType)
{
    setContentSecurityPolicy(ContentSecurityPolicy::create(this));
    contentSecurityPolicy()->didReceiveHeader(policy, contentSecurityPolicyType);
}

ScriptExecutionContext* WorkerGlobalScope::scriptExecutionContext() const
{
    return const_cast<WorkerGlobalScope*>(this);
}

const KURL& WorkerGlobalScope::virtualURL() const
{
    return m_url;
}

KURL WorkerGlobalScope::virtualCompleteURL(const String& url) const
{
    return completeURL(url);
}

KURL WorkerGlobalScope::completeURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the KURL constructor to have this behavior?
    if (url.isNull())
        return KURL();
    // Always use UTF-8 in Workers.
    return KURL(m_url, url);
}

String WorkerGlobalScope::userAgent(const KURL&) const
{
    return m_userAgent;
}

void WorkerGlobalScope::disableEval(const String& errorMessage)
{
    m_script->disableEval(errorMessage);
}

WorkerLocation* WorkerGlobalScope::location() const
{
    if (!m_location)
        m_location = WorkerLocation::create(m_url);
    return m_location.get();
}

void WorkerGlobalScope::close()
{
    if (m_closing)
        return;

    // Let current script run to completion but prevent future script evaluations.
    // After m_closing is set, all the tasks in the queue continue to be fetched but only
    // tasks with isCleanupTask()==true will be executed.
    m_closing = true;
    postTask(CloseWorkerGlobalScopeTask::create());
}

WorkerConsole* WorkerGlobalScope::console()
{
    if (!m_console)
        m_console = WorkerConsole::create(this);
    return m_console.get();
}

WorkerNavigator* WorkerGlobalScope::navigator() const
{
    if (!m_navigator)
        m_navigator = WorkerNavigator::create(m_userAgent);
    return m_navigator.get();
}

void WorkerGlobalScope::postTask(PassOwnPtr<Task> task)
{
    thread()->runLoop().postTask(task);
}

void WorkerGlobalScope::clearInspector()
{
    m_workerInspectorController.clear();
}

void WorkerGlobalScope::importScripts(const Vector<String>& urls, ExceptionState& es)
{
    ASSERT(contentSecurityPolicy());
    Vector<String>::const_iterator urlsEnd = urls.end();
    Vector<KURL> completedURLs;
    for (Vector<String>::const_iterator it = urls.begin(); it != urlsEnd; ++it) {
        const KURL& url = scriptExecutionContext()->completeURL(*it);
        if (!url.isValid()) {
            es.throwDOMException(SyntaxError, "Failed to execute 'importScripts': the URL '" + *it + "' is invalid.");
            return;
        }
        completedURLs.append(url);
    }
    Vector<KURL>::const_iterator end = completedURLs.end();

    for (Vector<KURL>::const_iterator it = completedURLs.begin(); it != end; ++it) {
        RefPtr<WorkerScriptLoader> scriptLoader(WorkerScriptLoader::create());
        scriptLoader->setTargetType(ResourceRequest::TargetIsScript);
        scriptLoader->loadSynchronously(scriptExecutionContext(), *it, AllowCrossOriginRequests);

        // If the fetching attempt failed, throw a NetworkError exception and abort all these steps.
        if (scriptLoader->failed()) {
            es.throwDOMException(NetworkError, "Failed to execute 'importScripts': the script at '" + it->elidedString() + "' failed to load.");
            return;
        }

        InspectorInstrumentation::scriptImported(scriptExecutionContext(), scriptLoader->identifier(), scriptLoader->script());

        RefPtr<ErrorEvent> errorEvent;
        m_script->evaluate(ScriptSourceCode(scriptLoader->script(), scriptLoader->responseURL()), &errorEvent);
        if (errorEvent) {
            m_script->rethrowExceptionFromImportedScript(errorEvent.release());
            return;
        }
    }
}

EventTarget* WorkerGlobalScope::errorEventTarget()
{
    return this;
}

void WorkerGlobalScope::logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtr<ScriptCallStack>)
{
    thread()->workerReportingProxy().postExceptionToWorkerObject(errorMessage, lineNumber, columnNumber, sourceURL);
}

void WorkerGlobalScope::addMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, ScriptState* state)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask::create(source, level, message));
        return;
    }
    thread()->workerReportingProxy().postConsoleMessageToWorkerObject(source, level, message, lineNumber, sourceURL);
    addMessageToWorkerConsole(source, level, message, sourceURL, lineNumber, 0, state);
}

void WorkerGlobalScope::addMessageToWorkerConsole(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, PassRefPtr<ScriptCallStack> callStack, ScriptState* state)
{
    ASSERT(isContextThread());
    if (callStack)
        InspectorInstrumentation::addMessageToConsole(this, source, LogMessageType, level, message, callStack);
    else
        InspectorInstrumentation::addMessageToConsole(this, source, LogMessageType, level, message, sourceURL, lineNumber, 0, state);
}

bool WorkerGlobalScope::isContextThread() const
{
    return thread()->isCurrentThread();
}

bool WorkerGlobalScope::isJSExecutionForbidden() const
{
    return m_script->isExecutionForbidden();
}

EventTargetData* WorkerGlobalScope::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* WorkerGlobalScope::ensureEventTargetData()
{
    return &m_eventTargetData;
}

WorkerGlobalScope::Observer::Observer(WorkerGlobalScope* context)
    : m_context(context)
{
    ASSERT(m_context && m_context->isContextThread());
    m_context->registerObserver(this);
}

WorkerGlobalScope::Observer::~Observer()
{
    if (!m_context)
        return;
    ASSERT(m_context->isContextThread());
    m_context->unregisterObserver(this);
}

void WorkerGlobalScope::Observer::stopObserving()
{
    if (!m_context)
        return;
    ASSERT(m_context->isContextThread());
    m_context->unregisterObserver(this);
    m_context = 0;
}

void WorkerGlobalScope::registerObserver(Observer* observer)
{
    ASSERT(observer);
    m_workerObservers.add(observer);
}

void WorkerGlobalScope::unregisterObserver(Observer* observer)
{
    ASSERT(observer);
    m_workerObservers.remove(observer);
}

void WorkerGlobalScope::notifyObserversOfStop()
{
    HashSet<Observer*>::iterator iter = m_workerObservers.begin();
    while (iter != m_workerObservers.end()) {
        WorkerGlobalScope::Observer* observer = *iter;
        observer->stopObserving();
        observer->notifyStop();
        iter = m_workerObservers.begin();
    }
}

bool WorkerGlobalScope::idleNotification()
{
    return script()->idleNotification();
}

WorkerEventQueue* WorkerGlobalScope::eventQueue() const
{
    return m_eventQueue.get();
}

} // namespace WebCore
