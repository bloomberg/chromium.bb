// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ModuleProxy.h"

#include "bindings/core/v8/V8Binding.h"

namespace blink {

ModuleProxy& ModuleProxy::moduleProxy()
{
    DEFINE_STATIC_LOCAL(ModuleProxy, moduleProxy, ());
    return moduleProxy;
}

v8::Handle<v8::Object> ModuleProxy::wrapForEvent(Event* event, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    RELEASE_ASSERT(m_wrapForEvent);
    return (*m_wrapForEvent)(event, creationContext, isolate);
}

void ModuleProxy::registerWrapForEvent(v8::Handle<v8::Object> (*wrapForEvent)(Event*, v8::Handle<v8::Object>, v8::Isolate*))
{
    m_wrapForEvent = wrapForEvent;
}

v8::Handle<v8::Value> ModuleProxy::toV8ForEventTarget(EventTarget* eventTarget, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    RELEASE_ASSERT(m_toV8ForEventTarget);
    return (*m_toV8ForEventTarget)(eventTarget, creationContext, isolate);
}

void ModuleProxy::registerToV8ForEventTarget(v8::Handle<v8::Value> (*toV8ForEventTarget)(EventTarget*, v8::Handle<v8::Object>, v8::Isolate*))
{
    m_toV8ForEventTarget = toV8ForEventTarget;
}

void ModuleProxy::didLeaveScriptContextForRecursionScope(ExecutionContext& executionContext)
{
    RELEASE_ASSERT(m_didLeaveScriptContextForRecursionScope);
    (*m_didLeaveScriptContextForRecursionScope)(executionContext);
}

void ModuleProxy::registerDidLeaveScriptContextForRecursionScope(void (*didLeaveScriptContext)(ExecutionContext&))
{
    m_didLeaveScriptContextForRecursionScope = didLeaveScriptContext;
}

} // namespace blink
