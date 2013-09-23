/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
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
#include "core/dom/ScriptExecutionContext.h"

#include "core/dom/ContextLifecycleNotifier.h"
#include "core/events/ErrorEvent.h"
#include "core/events/EventTarget.h"
#include "core/dom/MessagePort.h"
#include "core/html/PublicURLManager.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/page/DOMTimer.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "wtf/MainThread.h"

namespace WebCore {

class ProcessMessagesSoonTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<ProcessMessagesSoonTask> create()
    {
        return adoptPtr(new ProcessMessagesSoonTask);
    }

    virtual void performTask(ScriptExecutionContext* context)
    {
        context->dispatchMessagePortEvents();
    }
};

class ScriptExecutionContext::PendingException {
    WTF_MAKE_NONCOPYABLE(PendingException);
public:
    PendingException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, PassRefPtr<ScriptCallStack> callStack)
        : m_errorMessage(errorMessage)
        , m_lineNumber(lineNumber)
        , m_columnNumber(columnNumber)
        , m_sourceURL(sourceURL)
        , m_callStack(callStack)
    {
    }
    String m_errorMessage;
    int m_lineNumber;
    int m_columnNumber;
    String m_sourceURL;
    RefPtr<ScriptCallStack> m_callStack;
};

void ScriptExecutionContext::AddConsoleMessageTask::performTask(ScriptExecutionContext* context)
{
    context->addConsoleMessage(m_source, m_level, m_message);
}

ScriptExecutionContext::ScriptExecutionContext()
    : m_circularSequentialID(0)
    , m_inDispatchErrorEvent(false)
    , m_activeDOMObjectsAreSuspended(false)
    , m_reasonForSuspendingActiveDOMObjects(static_cast<ActiveDOMObject::ReasonForSuspension>(-1))
    , m_activeDOMObjectsAreStopped(false)
{
}

ScriptExecutionContext::~ScriptExecutionContext()
{
    HashSet<MessagePort*>::iterator messagePortsEnd = m_messagePorts.end();
    for (HashSet<MessagePort*>::iterator iter = m_messagePorts.begin(); iter != messagePortsEnd; ++iter) {
        ASSERT((*iter)->scriptExecutionContext() == this);
        (*iter)->contextDestroyed();
    }
}

void ScriptExecutionContext::processMessagePortMessagesSoon()
{
    postTask(ProcessMessagesSoonTask::create());
}

void ScriptExecutionContext::dispatchMessagePortEvents()
{
    RefPtr<ScriptExecutionContext> protect(this);

    // Make a frozen copy.
    Vector<MessagePort*> ports;
    copyToVector(m_messagePorts, ports);

    unsigned portCount = ports.size();
    for (unsigned i = 0; i < portCount; ++i) {
        MessagePort* port = ports[i];
        // The port may be destroyed, and another one created at the same address, but this is safe, as the worst that can happen
        // as a result is that dispatchMessages() will be called needlessly.
        if (m_messagePorts.contains(port) && port->started())
            port->dispatchMessages();
    }
}

void ScriptExecutionContext::createdMessagePort(MessagePort* port)
{
    ASSERT(port);
    ASSERT((isDocument() && isMainThread())
        || (isWorkerGlobalScope() && toWorkerGlobalScope(this)->thread()->isCurrentThread()));

    m_messagePorts.add(port);
}

void ScriptExecutionContext::destroyedMessagePort(MessagePort* port)
{
    ASSERT(port);
    ASSERT((isDocument() && isMainThread())
        || (isWorkerGlobalScope() && toWorkerGlobalScope(this)->thread()->isCurrentThread()));

    m_messagePorts.remove(port);
}

bool ScriptExecutionContext::canSuspendActiveDOMObjects()
{
    return lifecycleNotifier()->canSuspendActiveDOMObjects();
}

bool ScriptExecutionContext::hasPendingActivity()
{
    if (lifecycleNotifier()->hasPendingActivity())
        return true;

    HashSet<MessagePort*>::const_iterator messagePortsEnd = m_messagePorts.end();
    for (HashSet<MessagePort*>::const_iterator iter = m_messagePorts.begin(); iter != messagePortsEnd; ++iter) {
        if ((*iter)->hasPendingActivity())
            return true;
    }

    return false;
}

void ScriptExecutionContext::suspendActiveDOMObjects(ActiveDOMObject::ReasonForSuspension why)
{
    lifecycleNotifier()->notifySuspendingActiveDOMObjects(why);
    m_activeDOMObjectsAreSuspended = true;
    m_reasonForSuspendingActiveDOMObjects = why;
}

void ScriptExecutionContext::resumeActiveDOMObjects()
{
    m_activeDOMObjectsAreSuspended = false;
    lifecycleNotifier()->notifyResumingActiveDOMObjects();
}

void ScriptExecutionContext::stopActiveDOMObjects()
{
    m_activeDOMObjectsAreStopped = true;
    lifecycleNotifier()->notifyStoppingActiveDOMObjects();
    // Also close MessagePorts. If they were ActiveDOMObjects (they could be) then they could be stopped instead.
    closeMessagePorts();
}

void ScriptExecutionContext::suspendActiveDOMObjectIfNeeded(ActiveDOMObject* object)
{
    ASSERT(lifecycleNotifier()->contains(object));
    // Ensure all ActiveDOMObjects are suspended also newly created ones.
    if (m_activeDOMObjectsAreSuspended)
        object->suspend(m_reasonForSuspendingActiveDOMObjects);
}

void ScriptExecutionContext::closeMessagePorts() {
    HashSet<MessagePort*>::iterator messagePortsEnd = m_messagePorts.end();
    for (HashSet<MessagePort*>::iterator iter = m_messagePorts.begin(); iter != messagePortsEnd; ++iter) {
        ASSERT((*iter)->scriptExecutionContext() == this);
        (*iter)->close();
    }
}

bool ScriptExecutionContext::shouldSanitizeScriptError(const String& sourceURL, AccessControlStatus corsStatus)
{
    return !(securityOrigin()->canRequest(completeURL(sourceURL)) || corsStatus == SharableCrossOrigin);
}

void ScriptExecutionContext::reportException(PassRefPtr<ErrorEvent> event, PassRefPtr<ScriptCallStack> callStack, AccessControlStatus corsStatus)
{
    RefPtr<ErrorEvent> errorEvent = event;
    if (m_inDispatchErrorEvent) {
        if (!m_pendingExceptions)
            m_pendingExceptions = adoptPtr(new Vector<OwnPtr<PendingException> >());
        m_pendingExceptions->append(adoptPtr(new PendingException(errorEvent->messageForConsole(), errorEvent->lineno(), errorEvent->colno(), errorEvent->filename(), callStack)));
        return;
    }

    // First report the original exception and only then all the nested ones.
    if (!dispatchErrorEvent(errorEvent, corsStatus))
        logExceptionToConsole(errorEvent->messageForConsole(), errorEvent->filename(), errorEvent->lineno(), errorEvent->colno(), callStack);

    if (!m_pendingExceptions)
        return;

    for (size_t i = 0; i < m_pendingExceptions->size(); i++) {
        PendingException* e = m_pendingExceptions->at(i).get();
        logExceptionToConsole(e->m_errorMessage, e->m_sourceURL, e->m_lineNumber, e->m_columnNumber, e->m_callStack);
    }
    m_pendingExceptions.clear();
}

void ScriptExecutionContext::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber)
{
    addMessage(source, level, message, sourceURL, lineNumber, 0);
}

void ScriptExecutionContext::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, ScriptState* state)
{
    addMessage(source, level, message, String(), 0, state);
}

bool ScriptExecutionContext::dispatchErrorEvent(PassRefPtr<ErrorEvent> event, AccessControlStatus corsStatus)
{
    EventTarget* target = errorEventTarget();
    if (!target)
        return false;

    RefPtr<ErrorEvent> errorEvent = event;
    if (shouldSanitizeScriptError(errorEvent->filename(), corsStatus))
        errorEvent = ErrorEvent::createSanitizedError(errorEvent->world());

    ASSERT(!m_inDispatchErrorEvent);
    m_inDispatchErrorEvent = true;
    target->dispatchEvent(errorEvent);
    m_inDispatchErrorEvent = false;
    return errorEvent->defaultPrevented();
}

int ScriptExecutionContext::circularSequentialID()
{
    ++m_circularSequentialID;
    if (m_circularSequentialID <= 0)
        m_circularSequentialID = 1;
    return m_circularSequentialID;
}

int ScriptExecutionContext::installNewTimeout(PassOwnPtr<ScheduledAction> action, int timeout, bool singleShot)
{
    int timeoutID;
    while (true) {
        timeoutID = circularSequentialID();
        if (!m_timeouts.contains(timeoutID))
            break;
    }
    TimeoutMap::AddResult result = m_timeouts.add(timeoutID, DOMTimer::create(this, action, timeout, singleShot, timeoutID));
    ASSERT(result.isNewEntry);
    DOMTimer* timer = result.iterator->value.get();

    timer->suspendIfNeeded();

    return timer->timeoutID();
}

void ScriptExecutionContext::removeTimeoutByID(int timeoutID)
{
    if (timeoutID <= 0)
        return;
    m_timeouts.remove(timeoutID);
}

PublicURLManager& ScriptExecutionContext::publicURLManager()
{
    if (!m_publicURLManager)
        m_publicURLManager = PublicURLManager::create(this);
    return *m_publicURLManager;
}

void ScriptExecutionContext::didChangeTimerAlignmentInterval()
{
    for (TimeoutMap::iterator iter = m_timeouts.begin(); iter != m_timeouts.end(); ++iter)
        iter->value->didChangeAlignmentInterval();
}

double ScriptExecutionContext::timerAlignmentInterval() const
{
    return DOMTimer::visiblePageAlignmentInterval();
}

ContextLifecycleNotifier* ScriptExecutionContext::lifecycleNotifier()
{
    return static_cast<ContextLifecycleNotifier*>(LifecycleContext::lifecycleNotifier());
}

PassOwnPtr<LifecycleNotifier> ScriptExecutionContext::createLifecycleNotifier()
{
    return ContextLifecycleNotifier::create(this);
}

ScriptExecutionContext::Task::~Task()
{
}

void ScriptExecutionContext::setDatabaseContext(DatabaseContext* databaseContext)
{
    m_databaseContext = databaseContext;
}

} // namespace WebCore
