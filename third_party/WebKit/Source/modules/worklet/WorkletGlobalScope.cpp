// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/worklet/WorkletGlobalScope.h"

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"

namespace blink {

// static
PassRefPtrWillBeRawPtr<WorkletGlobalScope> WorkletGlobalScope::create(const KURL& url, const String& userAgent, v8::Isolate* isolate)
{
    RefPtrWillBeRawPtr<WorkletGlobalScope> workletGlobalScope = adoptRefWillBeNoop(new WorkletGlobalScope(url, userAgent, isolate));
    workletGlobalScope->script()->initializeContextIfNeeded();
    return workletGlobalScope.release();
}

WorkletGlobalScope::WorkletGlobalScope(const KURL& url, const String& userAgent, v8::Isolate* isolate)
    : m_script(WorkerOrWorkletScriptController::create(this, isolate))
{
}

WorkletGlobalScope::~WorkletGlobalScope()
{
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
    m_script->disableEval(errorMessage);
}

bool WorkletGlobalScope::isSecureContext(String& errorMessage, const SecureContextCheck privilegeContextCheck) const
{
    // Until there are APIs that are available in worklets and that
    // require a privileged context test that checks ancestors, just do
    // a simple check here.
    return securityOrigin()->isPotentiallyTrustworthy(errorMessage);
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
    visitor->trace(m_script);
    ExecutionContext::trace(visitor);
    SecurityContext::trace(visitor);
}

} // namespace blink
