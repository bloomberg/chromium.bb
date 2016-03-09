// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/worklet/WorkletGlobalScope.h"

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/frame/FrameConsole.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "modules/worklet/WorkletConsole.h"

namespace blink {

// static
PassRefPtrWillBeRawPtr<WorkletGlobalScope> WorkletGlobalScope::create(LocalFrame* frame, const KURL& url, const String& userAgent, PassRefPtr<SecurityOrigin> securityOrigin, v8::Isolate* isolate)
{
    RefPtrWillBeRawPtr<WorkletGlobalScope> workletGlobalScope = adoptRefWillBeNoop(new WorkletGlobalScope(frame, url, userAgent, securityOrigin, isolate));
    workletGlobalScope->scriptController()->initializeContextIfNeeded();
    return workletGlobalScope.release();
}

WorkletGlobalScope::WorkletGlobalScope(LocalFrame* frame, const KURL& url, const String& userAgent, PassRefPtr<SecurityOrigin> securityOrigin, v8::Isolate* isolate)
    : LocalFrameLifecycleObserver(frame)
    , m_url(url)
    , m_userAgent(userAgent)
    , m_scriptController(WorkerOrWorkletScriptController::create(this, isolate))
{
    setSecurityOrigin(securityOrigin);
}

WorkletGlobalScope::~WorkletGlobalScope()
{
}

WorkletConsole* WorkletGlobalScope::console()
{
    if (!m_console)
        m_console = WorkletConsole::create(this);
    return m_console.get();
}

v8::Local<v8::Object> WorkletGlobalScope::wrap(v8::Isolate*, v8::Local<v8::Object> creationContext)
{
    // WorkletGlobalScope must never be wrapped with wrap method. The global
    // object of ECMAScript environment is used as the wrapper.
    RELEASE_ASSERT_NOT_REACHED();
    return v8::Local<v8::Object>();
}

v8::Local<v8::Object> WorkletGlobalScope::associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Local<v8::Object> wrapper)
{
    RELEASE_ASSERT_NOT_REACHED(); // Same as wrap method.
    return v8::Local<v8::Object>();
}

void WorkletGlobalScope::disableEval(const String& errorMessage)
{
    m_scriptController->disableEval(errorMessage);
}

bool WorkletGlobalScope::isSecureContext(String& errorMessage, const SecureContextCheck privilegeContextCheck) const
{
    // Until there are APIs that are available in worklets and that
    // require a privileged context test that checks ancestors, just do
    // a simple check here.
    if (getSecurityOrigin()->isPotentiallyTrustworthy())
        return true;
    errorMessage = getSecurityOrigin()->isPotentiallyTrustworthyErrorMessage();
    return false;
}

void WorkletGlobalScope::reportBlockedScriptExecutionToInspector(const String& directiveText)
{
    InspectorInstrumentation::scriptExecutionBlockedByCSP(this, directiveText);
}

void WorkletGlobalScope::addConsoleMessage(PassRefPtrWillBeRawPtr<ConsoleMessage> prpConsoleMessage)
{
    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = prpConsoleMessage;
    frame()->console().addMessage(consoleMessage.release());
}

void WorkletGlobalScope::logExceptionToConsole(const String& errorMessage, int scriptId, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtr<ScriptCallStack> callStack)
{
    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, errorMessage, sourceURL, lineNumber, columnNumber);
    consoleMessage->setScriptId(scriptId);
    consoleMessage->setCallStack(callStack);
    addConsoleMessage(consoleMessage.release());
}

KURL WorkletGlobalScope::virtualCompleteURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // TODO(ikilpatrick): Should we change the KURL constructor to have this
    // behavior?
    if (url.isNull())
        return KURL();
    // Always use UTF-8 in Worklets.
    return KURL(m_url, url);
}

DEFINE_TRACE(WorkletGlobalScope)
{
    visitor->trace(m_scriptController);
    visitor->trace(m_console);
    ExecutionContext::trace(visitor);
    SecurityContext::trace(visitor);
    LocalFrameLifecycleObserver::trace(visitor);
}

} // namespace blink
