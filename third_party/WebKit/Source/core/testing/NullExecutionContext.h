// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NullExecutionContext_h
#define NullExecutionContext_h

#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/events/EventQueue.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/RefCounted.h"

namespace blink {

class NullExecutionContext FINAL : public RefCountedWillBeGarbageCollectedFinalized<NullExecutionContext>, public SecurityContext, public ExecutionContext {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NullExecutionContext);
public:
    NullExecutionContext();

    virtual void disableEval(const String&) OVERRIDE { }
    virtual String userAgent(const KURL&) const OVERRIDE { return String(); }

    virtual void postTask(PassOwnPtr<ExecutionContextTask>) OVERRIDE;

    virtual EventTarget* errorEventTarget() OVERRIDE { return 0; }
    virtual EventQueue* eventQueue() const OVERRIDE { return m_queue.get(); }

    virtual bool tasksNeedSuspension() OVERRIDE { return m_tasksNeedSuspension; }
    void setTasksNeedSuspension(bool flag) { m_tasksNeedSuspension = flag; }

    virtual void reportBlockedScriptExecutionToInspector(const String& directiveText) OVERRIDE { }
    virtual void didUpdateSecurityOrigin() OVERRIDE { }
    virtual SecurityContext& securityContext() OVERRIDE { return *this; }

    double timerAlignmentInterval() const;

    virtual void addConsoleMessage(PassRefPtrWillBeRawPtr<ConsoleMessage>) OVERRIDE { }
    virtual void logExceptionToConsole(const String& errorMessage, int scriptId, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack>) OVERRIDE { }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_queue);
        ExecutionContext::trace(visitor);
    }

#if !ENABLE(OILPAN)
    using RefCounted<NullExecutionContext>::ref;
    using RefCounted<NullExecutionContext>::deref;

    virtual void refExecutionContext() OVERRIDE { ref(); }
    virtual void derefExecutionContext() OVERRIDE { deref(); }
#endif

protected:
    virtual const KURL& virtualURL() const OVERRIDE { return m_dummyURL; }
    virtual KURL virtualCompleteURL(const String&) const OVERRIDE { return m_dummyURL; }

private:
    bool m_tasksNeedSuspension;
    OwnPtrWillBeMember<EventQueue> m_queue;

    KURL m_dummyURL;
};

} // namespace blink

#endif // NullExecutionContext_h
