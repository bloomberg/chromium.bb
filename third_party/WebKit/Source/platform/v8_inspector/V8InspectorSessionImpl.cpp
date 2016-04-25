// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8InspectorSessionImpl.h"

#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/InjectedScriptHost.h"
#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/RemoteObjectId.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8HeapProfilerAgentImpl.h"
#include "platform/v8_inspector/V8ProfilerAgentImpl.h"
#include "platform/v8_inspector/V8RuntimeAgentImpl.h"
#include "platform/v8_inspector/public/V8ContextInfo.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"

namespace blink {

PassOwnPtr<V8InspectorSessionImpl> V8InspectorSessionImpl::create(V8DebuggerImpl* debugger, int contextGroupId)
{
    return adoptPtr(new V8InspectorSessionImpl(debugger, contextGroupId));
}

V8InspectorSessionImpl::V8InspectorSessionImpl(V8DebuggerImpl* debugger, int contextGroupId)
    : m_contextGroupId(contextGroupId)
    , m_debugger(debugger)
    , m_client(nullptr)
    , m_injectedScriptHost(InjectedScriptHost::create(debugger, this))
    , m_customObjectFormatterEnabled(false)
    , m_instrumentationCounter(0)
    , m_runtimeAgent(adoptPtr(new V8RuntimeAgentImpl(this)))
    , m_debuggerAgent(adoptPtr(new V8DebuggerAgentImpl(this)))
    , m_heapProfilerAgent(adoptPtr(new V8HeapProfilerAgentImpl(this)))
    , m_profilerAgent(adoptPtr(new V8ProfilerAgentImpl(this)))
    , m_clearConsoleCallback(nullptr)
{
}

V8InspectorSessionImpl::~V8InspectorSessionImpl()
{
    discardInjectedScripts();
    m_debugger->disconnect(this);
}

V8DebuggerAgent* V8InspectorSessionImpl::debuggerAgent()
{
    return m_debuggerAgent.get();
}

V8HeapProfilerAgent* V8InspectorSessionImpl::heapProfilerAgent()
{
    return m_heapProfilerAgent.get();
}

V8ProfilerAgent* V8InspectorSessionImpl::profilerAgent()
{
    return m_profilerAgent.get();
}

V8RuntimeAgent* V8InspectorSessionImpl::runtimeAgent()
{
    return m_runtimeAgent.get();
}

void V8InspectorSessionImpl::setClient(V8InspectorSessionClient* client)
{
    m_client = client;
}

void V8InspectorSessionImpl::reset()
{
    m_debuggerAgent->reset();
    m_runtimeAgent->reset();
    discardInjectedScripts();
}

void V8InspectorSessionImpl::discardInjectedScripts()
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

InjectedScript* V8InspectorSessionImpl::findInjectedScript(ErrorString* errorString, int contextId)
{
    if (!contextId) {
        *errorString = "Cannot find context with specified id";
        return nullptr;
    }

    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts || !contexts->contains(contextId)) {
        *errorString = "Cannot find context with specified id";
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

InjectedScript* V8InspectorSessionImpl::findInjectedScript(ErrorString* errorString, RemoteObjectIdBase* objectId)
{
    return objectId ? findInjectedScript(errorString, objectId->contextId()) : nullptr;
}

void V8InspectorSessionImpl::addInspectedObject(PassOwnPtr<V8RuntimeAgent::Inspectable> inspectable)
{
    m_injectedScriptHost->addInspectedObject(inspectable);
}

void V8InspectorSessionImpl::releaseObjectGroup(const String16& objectGroup)
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

void V8InspectorSessionImpl::setCustomObjectFormatterEnabled(bool enabled)
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

void V8InspectorSessionImpl::reportAllContexts(V8RuntimeAgentImpl* agent)
{
    const V8DebuggerImpl::ContextByIdMap* contexts = m_debugger->contextGroup(m_contextGroupId);
    if (!contexts)
        return;
    for (auto& idContext : *contexts)
        agent->reportExecutionContextCreated(idContext.second);
}

void V8InspectorSessionImpl::changeInstrumentationCounter(int delta)
{
    ASSERT(m_instrumentationCounter + delta >= 0);
    if (!m_instrumentationCounter && m_client)
        m_client->startInstrumenting();
    m_instrumentationCounter += delta;
    if (!m_instrumentationCounter && m_client)
        m_client->stopInstrumenting();
}

} // namespace blink
