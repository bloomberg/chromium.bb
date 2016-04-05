// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8InspectorConnectionImpl.h"

#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/InjectedScriptHost.h"
#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/RemoteObjectId.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8RuntimeAgentImpl.h"
#include "platform/v8_inspector/public/V8ContextInfo.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"

namespace blink {

PassOwnPtr<V8InspectorConnectionImpl> V8InspectorConnectionImpl::create(V8DebuggerImpl* debugger, int contextGroupId)
{
    return adoptPtr(new V8InspectorConnectionImpl(debugger, contextGroupId));
}

V8InspectorConnectionImpl::V8InspectorConnectionImpl(V8DebuggerImpl* debugger, int contextGroupId)
    : m_contextGroupId(contextGroupId)
    , m_debugger(debugger)
    , m_injectedScriptHost(InjectedScriptHost::create(debugger, this))
    , m_customObjectFormatterEnabled(false)
    , m_debuggerAgent(nullptr)
    , m_inspectCallback(nullptr)
    , m_clearConsoleCallback(nullptr)
{
}

V8InspectorConnectionImpl::~V8InspectorConnectionImpl()
{
    resetInjectedScripts();
    ASSERT(!m_debuggerAgent);
}

void V8InspectorConnectionImpl::resetInjectedScripts()
{
    m_injectedScriptHost->clearInspectedObjects();
    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts)
        return;

    protocol::Vector<int> keys;
    for (auto& idContext : *contexts)
        keys.append(idContext.first);
    for (auto& key : keys) {
        contexts = m_debugger->contextGroup(m_contextGroupId);
        if (contexts && contexts->contains(key))
            contexts->get(key)->discardInjectedScript(); // This may destroy some contexts.
    }
}

InjectedScript* V8InspectorConnectionImpl::findInjectedScript(ErrorString* errorString, int contextId)
{
    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts || !contexts->contains(contextId)) {
        *errorString = "Inspected frame has gone";
        return nullptr;
    }

    InspectedContext* context = contexts->get(contextId);
    if (!context->getInjectedScript()) {
        context->createInjectedScript(m_injectedScriptHost.get());
        if (!context->getInjectedScript()) {
            *errorString = "Cannot access specified execution context";
            return nullptr;
        }
        if (m_customObjectFormatterEnabled)
            context->getInjectedScript()->setCustomObjectFormatterEnabled(true);
    }
    return context->getInjectedScript();
}

InjectedScript* V8InspectorConnectionImpl::findInjectedScript(ErrorString* errorString, RemoteObjectIdBase* objectId)
{
    return objectId ? findInjectedScript(errorString, objectId->contextId()) : nullptr;
}

void V8InspectorConnectionImpl::addInspectedObject(PassOwnPtr<V8RuntimeAgent::Inspectable> inspectable)
{
    m_injectedScriptHost->addInspectedObject(inspectable);
}

void V8InspectorConnectionImpl::releaseObjectGroup(const String16& objectGroup)
{
    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts)
        return;

    protocol::Vector<int> keys;
    for (auto& idContext : *contexts)
        keys.append(idContext.first);
    for (auto& key : keys) {
        contexts = m_debugger->contextGroup(m_contextGroupId);
        if (contexts && contexts->contains(key)) {
            InjectedScript* injectedScript = contexts->get(key)->getInjectedScript();
            if (injectedScript)
                injectedScript->releaseObjectGroup(objectGroup); // This may destroy some contexts.
        }
    }
}

void V8InspectorConnectionImpl::setCustomObjectFormatterEnabled(bool enabled)
{
    m_customObjectFormatterEnabled = enabled;
    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts)
        return;
    for (auto& idContext : *contexts) {
        InjectedScript* injectedScript = idContext.second->getInjectedScript();
        if (injectedScript)
            injectedScript->setCustomObjectFormatterEnabled(enabled);
    }
}

void V8InspectorConnectionImpl::reportAllContexts(V8RuntimeAgentImpl* agent)
{
    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts)
        return;
    for (auto& idContext : *contexts)
        agent->reportExecutionContextCreated(idContext.second);
}

} // namespace blink
