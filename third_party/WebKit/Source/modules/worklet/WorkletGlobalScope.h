// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletGlobalScope_h
#define WorkletGlobalScope_h

#include "bindings/core/v8/ScriptCallStack.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/SecurityContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "platform/heap/Handle.h"

namespace blink {

class EventQueue;
class WorkerOrWorkletScriptController;

class WorkletGlobalScope : public RefCountedWillBeGarbageCollectedFinalized<WorkletGlobalScope>, public SecurityContext, public WorkerOrWorkletGlobalScope, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(WorkletGlobalScope);
public:
#if !ENABLE(OILPAN)
    using RefCounted<WorkletGlobalScope>::ref;
    using RefCounted<WorkletGlobalScope>::deref;
#endif

    // The url and userAgent arguments are inherited from the parent
    // ExecutionContext for Worklets.
    static PassRefPtrWillBeRawPtr<WorkletGlobalScope> create(const KURL&, const String& userAgent, v8::Isolate*);
    ~WorkletGlobalScope() override;

    bool isWorkletGlobalScope() const final { return true; }


    // WorkerOrWorkletGlobalScope
    ScriptWrappable* scriptWrappable() const final { return const_cast<WorkletGlobalScope*>(this); }
    WorkerOrWorkletScriptController* script() final { return m_script.get(); }

    // ScriptWrappable
    v8::Local<v8::Object> wrap(v8::Isolate*, v8::Local<v8::Object> creationContext) final;
    v8::Local<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Local<v8::Object> wrapper) final;

    // ExecutionContext
    void disableEval(const String& errorMessage) final;
    String userAgent() const final { return m_userAgent; }
    SecurityContext& securityContext() final { return *this; }
    EventQueue* eventQueue() const final { ASSERT_NOT_REACHED(); return nullptr; } // WorkletGlobalScopes don't have an event queue.
    bool isSecureContext(String& errorMessage, const SecureContextCheck = StandardSecureContextCheck) const final;

    using SecurityContext::securityOrigin;
    using SecurityContext::contentSecurityPolicy;

    DOMTimerCoordinator* timers() final { ASSERT_NOT_REACHED(); return nullptr; } // WorkletGlobalScopes don't have timers.
    void postTask(const WebTraceLocation&, PassOwnPtr<ExecutionContextTask>) override
    {
        // TODO(ikilpatrick): implement.
        ASSERT_NOT_REACHED();
    }

    // TODO(ikilpatrick): implement when we implement devtools support.
    void reportBlockedScriptExecutionToInspector(const String& directiveText) final { }
    void addConsoleMessage(PassRefPtrWillBeRawPtr<ConsoleMessage>) final { }
    void logExceptionToConsole(const String& errorMessage, int scriptId, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtr<ScriptCallStack>) final { }

    DECLARE_VIRTUAL_TRACE();

private:
#if !ENABLE(OILPAN)
    void refExecutionContext() final { ref(); }
    void derefExecutionContext() final { deref(); }
#endif

    WorkletGlobalScope(const KURL&, const String& userAgent, v8::Isolate*);

    const KURL& virtualURL() const final { return m_url; }
    KURL virtualCompleteURL(const String&) const final;

    EventTarget* errorEventTarget() final { return nullptr; }
    void didUpdateSecurityOrigin() final { }

    KURL m_url;
    String m_userAgent;
    OwnPtrWillBeMember<WorkerOrWorkletScriptController> m_script;
};

DEFINE_TYPE_CASTS(WorkletGlobalScope, ExecutionContext, context, context->isWorkletGlobalScope(), context.isWorkletGlobalScope());

} // namespace blink

#endif // WorkletGlobalScope_h
