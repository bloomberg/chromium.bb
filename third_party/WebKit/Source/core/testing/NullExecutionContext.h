// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NullExecutionContext_h
#define NullExecutionContext_h

#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/events/EventQueue.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ScriptCallStack.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/RefCounted.h"

namespace blink {

class NullExecutionContext final : public RefCountedWillBeGarbageCollectedFinalized<NullExecutionContext>, public SecurityContext, public ExecutionContext {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NullExecutionContext);
public:
    NullExecutionContext();

    void disableEval(const String&) override { }
    String userAgent() const override { return String(); }

    void postTask(const WebTraceLocation&, PassOwnPtr<ExecutionContextTask>) override;

    EventTarget* errorEventTarget() override { return nullptr; }
    EventQueue* eventQueue() const override { return m_queue.get(); }

    bool tasksNeedSuspension() override { return m_tasksNeedSuspension; }
    void setTasksNeedSuspension(bool flag) { m_tasksNeedSuspension = flag; }

    void reportBlockedScriptExecutionToInspector(const String& directiveText) override { }
    void didUpdateSecurityOrigin() override { }
    SecurityContext& securityContext() override { return *this; }
    DOMTimerCoordinator* timers() override { return nullptr; }

    void addConsoleMessage(PassRefPtrWillBeRawPtr<ConsoleMessage>) override { }
    void logExceptionToConsole(const String& errorMessage, int scriptId, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack>) override { }

    bool isSecureContext(String& errorMessage, const SecureContextCheck = StandardSecureContextCheck) const override;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_queue);
        SecurityContext::trace(visitor);
        ExecutionContext::trace(visitor);
    }

#if !ENABLE(OILPAN)
    using RefCounted<NullExecutionContext>::ref;
    using RefCounted<NullExecutionContext>::deref;

    void refExecutionContext() override { ref(); }
    void derefExecutionContext() override { deref(); }
#endif

protected:
    const KURL& virtualURL() const override { return m_dummyURL; }
    KURL virtualCompleteURL(const String&) const override { return m_dummyURL; }

private:
    bool m_tasksNeedSuspension;
    OwnPtrWillBeMember<EventQueue> m_queue;

    KURL m_dummyURL;
};

} // namespace blink

#endif // NullExecutionContext_h
