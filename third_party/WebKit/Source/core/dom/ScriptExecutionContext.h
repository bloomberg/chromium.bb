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

#ifndef ScriptExecutionContext_h
#define ScriptExecutionContext_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/ErrorEvent.h"
#include "core/dom/SecurityContext.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/page/ConsoleTypes.h"
#include "core/page/DOMTimer.h"
#include "core/platform/LifecycleContext.h"
#include "core/platform/Supplementable.h"
#include "weborigin/KURL.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WTF {
class OrdinalNumber;
}

namespace WebCore {

class ContextLifecycleNotifier;
class DOMWindow;
class DatabaseContext;
class EventListener;
class EventQueue;
class EventTarget;
class MessagePort;
class PublicURLManager;
class ScriptCallStack;
class ScriptState;

class ScriptExecutionContext : public LifecycleContext, public SecurityContext, public Supplementable<ScriptExecutionContext> {
public:
    ScriptExecutionContext();
    virtual ~ScriptExecutionContext();

    virtual bool isDocument() const { return false; }
    virtual bool isWorkerGlobalScope() const { return false; }

    virtual bool isJSExecutionForbidden() const = 0;

    virtual DOMWindow* executingWindow() { return 0; }
    virtual void userEventWasHandled() { }

    const KURL& url() const { return virtualURL(); }
    KURL completeURL(const String& url) const { return virtualCompleteURL(url); }

    virtual String userAgent(const KURL&) const = 0;

    virtual void disableEval(const String& errorMessage) = 0;

    bool shouldSanitizeScriptError(const String& sourceURL, AccessControlStatus);
    void reportException(PassRefPtr<ErrorEvent>, PassRefPtr<ScriptCallStack>, AccessControlStatus);

    void addConsoleMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber);
    void addConsoleMessage(MessageSource, MessageLevel, const String& message, ScriptState* = 0);

    PublicURLManager& publicURLManager();

    // Active objects are not garbage collected even if inaccessible, e.g. because their activity may result in callbacks being invoked.
    bool canSuspendActiveDOMObjects();
    bool hasPendingActivity();

    // Active objects can be asked to suspend even if canSuspendActiveDOMObjects() returns 'false' -
    // step-by-step JS debugging is one example.
    virtual void suspendActiveDOMObjects(ActiveDOMObject::ReasonForSuspension);
    virtual void resumeActiveDOMObjects();
    virtual void stopActiveDOMObjects();

    bool activeDOMObjectsAreSuspended() const { return m_activeDOMObjectsAreSuspended; }
    bool activeDOMObjectsAreStopped() const { return m_activeDOMObjectsAreStopped; }

    // Called after the construction of an ActiveDOMObject to synchronize suspend state.
    void suspendActiveDOMObjectIfNeeded(ActiveDOMObject*);

    // MessagePort is conceptually a kind of ActiveDOMObject, but it needs to be tracked separately for message dispatch.
    void processMessagePortMessagesSoon();
    void dispatchMessagePortEvents();
    void createdMessagePort(MessagePort*);
    void destroyedMessagePort(MessagePort*);
    const HashSet<MessagePort*>& messagePorts() const { return m_messagePorts; }

    void ref() { refScriptExecutionContext(); }
    void deref() { derefScriptExecutionContext(); }

    class Task {
        WTF_MAKE_NONCOPYABLE(Task);
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Task() { }
        virtual ~Task();
        virtual void performTask(ScriptExecutionContext*) = 0;
        // Certain tasks get marked specially so that they aren't discarded, and are executed, when the context is shutting down its message queue.
        virtual bool isCleanupTask() const { return false; }
    };

    virtual void postTask(PassOwnPtr<Task>) = 0; // Executes the task on context's thread asynchronously.

    // Gets the next id in a circular sequence from 1 to 2^31-1.
    int circularSequentialID();

    void didChangeTimerAlignmentInterval();
    virtual double timerAlignmentInterval() const;

    virtual EventQueue* eventQueue() const = 0;

    void setDatabaseContext(DatabaseContext*);

protected:
    class AddConsoleMessageTask : public Task {
    public:
        static PassOwnPtr<AddConsoleMessageTask> create(MessageSource source, MessageLevel level, const String& message)
        {
            return adoptPtr(new AddConsoleMessageTask(source, level, message));
        }
        virtual void performTask(ScriptExecutionContext*);
    private:
        AddConsoleMessageTask(MessageSource source, MessageLevel level, const String& message)
            : m_source(source)
            , m_level(level)
            , m_message(message.isolatedCopy())
        {
        }
        MessageSource m_source;
        MessageLevel m_level;
        String m_message;
    };

    ContextLifecycleNotifier* lifecycleNotifier();

private:
    friend class DOMTimer; // For installNewTimeout() and removeTimeoutByID() below.

    virtual const KURL& virtualURL() const = 0;
    virtual KURL virtualCompleteURL(const String&) const = 0;

    virtual void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, ScriptState*) = 0;
    virtual EventTarget* errorEventTarget() = 0;
    virtual void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtr<ScriptCallStack>) = 0;
    bool dispatchErrorEvent(PassRefPtr<ErrorEvent>, AccessControlStatus);

    void closeMessagePorts();

    virtual void refScriptExecutionContext() = 0;
    virtual void derefScriptExecutionContext() = 0;
    virtual PassOwnPtr<LifecycleNotifier> createLifecycleNotifier() OVERRIDE;

    // Implementation details for DOMTimer. No other classes should call these functions.
    int installNewTimeout(PassOwnPtr<ScheduledAction>, int timeout, bool singleShot);
    void removeTimeoutByID(int timeoutID); // This makes underlying DOMTimer instance destructed.

    HashSet<MessagePort*> m_messagePorts;

    int m_circularSequentialID;
    typedef HashMap<int, OwnPtr<DOMTimer> > TimeoutMap;
    TimeoutMap m_timeouts;

    bool m_inDispatchErrorEvent;
    class PendingException;
    OwnPtr<Vector<OwnPtr<PendingException> > > m_pendingExceptions;

    bool m_activeDOMObjectsAreSuspended;
    ActiveDOMObject::ReasonForSuspension m_reasonForSuspendingActiveDOMObjects;
    bool m_activeDOMObjectsAreStopped;

    OwnPtr<PublicURLManager> m_publicURLManager;

    RefPtr<DatabaseContext> m_databaseContext;

    // The location of this member is important; to make sure contextDestroyed() notification on
    // ScriptExecutionContext's members (notably m_timeouts) is called before they are destructed,
    // m_lifecycleNotifer should be placed *after* such members.
    OwnPtr<ContextLifecycleNotifier> m_lifecycleNotifier;
};

} // namespace WebCore

#endif // ScriptExecutionContext_h
