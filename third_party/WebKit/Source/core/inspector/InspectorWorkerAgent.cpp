/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/InspectorWorkerAgent.h"

#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/PageConsoleAgent.h"
#include "platform/weborigin/KURL.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace WorkerAgentState {
static const char workerInspectionEnabled[] = "workerInspectionEnabled";
static const char waitForDebuggerOnStart[] = "waitForDebuggerOnStart";
};

PassOwnPtrWillBeRawPtr<InspectorWorkerAgent> InspectorWorkerAgent::create(InspectedFrames* inspectedFrames, PageConsoleAgent* consoleAgent)
{
    return adoptPtrWillBeNoop(new InspectorWorkerAgent(inspectedFrames, consoleAgent));
}

InspectorWorkerAgent::InspectorWorkerAgent(InspectedFrames* inspectedFrames, PageConsoleAgent* consoleAgent)
    : InspectorBaseAgent<InspectorWorkerAgent, protocol::Frontend::Worker>("Worker")
    , m_inspectedFrames(inspectedFrames)
    , m_consoleAgent(consoleAgent)
{
}

InspectorWorkerAgent::~InspectorWorkerAgent()
{
#if !ENABLE(OILPAN)
    m_instrumentingAgents->setInspectorWorkerAgent(nullptr);
#endif
}

void InspectorWorkerAgent::init()
{
    m_instrumentingAgents->setInspectorWorkerAgent(this);
}

void InspectorWorkerAgent::restore()
{
    if (enabled())
        connectToAllProxies();
}

void InspectorWorkerAgent::enable(ErrorString*)
{
    if (enabled())
        return;
    m_state->setBoolean(WorkerAgentState::workerInspectionEnabled, true);
    connectToAllProxies();
}

void InspectorWorkerAgent::disable(ErrorString*)
{
    if (!enabled())
        return;
    m_state->setBoolean(WorkerAgentState::workerInspectionEnabled, false);
    m_state->setBoolean(WorkerAgentState::waitForDebuggerOnStart, false);
    for (auto& idProxy : m_idToProxy)
        idProxy.value->disconnectFromInspector(this);
}

bool InspectorWorkerAgent::enabled()
{
    return m_state->booleanProperty(WorkerAgentState::workerInspectionEnabled, false);
}

void InspectorWorkerAgent::sendMessageToWorker(ErrorString* error, const String& workerId, const String& message)
{
    if (!enabled()) {
        *error = "Worker inspection is not enabled";
        return;
    }

    WorkerInspectorProxy* proxy = m_idToProxy.get(workerId);
    if (proxy)
        proxy->sendMessageToInspector(message);
    else
        *error = "Worker is gone";
}

void InspectorWorkerAgent::setWaitForDebuggerOnStart(ErrorString*, bool value)
{
    m_state->setBoolean(WorkerAgentState::waitForDebuggerOnStart, value);
}

void InspectorWorkerAgent::setTracingSessionId(const String& sessionId)
{
    m_tracingSessionId = sessionId;
    if (sessionId.isEmpty())
        return;
    for (auto& idProxy : m_idToProxy)
        idProxy.value->writeTimelineStartedEvent(sessionId, idProxy.key);
}

bool InspectorWorkerAgent::shouldWaitForDebuggerOnWorkerStart()
{
    return enabled() && m_state->booleanProperty(WorkerAgentState::waitForDebuggerOnStart, false);
}

void InspectorWorkerAgent::didStartWorker(WorkerInspectorProxy* workerInspectorProxy, bool waitingForDebugger)
{
    String id = "dedicated:" + IdentifiersFactory::createIdentifier();
    m_idToProxy.set(id, workerInspectorProxy);
    m_proxyToId.set(workerInspectorProxy, id);

    if (frontend() && enabled())
        connectToProxy(workerInspectorProxy, id, waitingForDebugger);

    if (!m_tracingSessionId.isEmpty())
        workerInspectorProxy->writeTimelineStartedEvent(m_tracingSessionId, id);
}

void InspectorWorkerAgent::workerTerminated(WorkerInspectorProxy* proxy)
{
    if (m_proxyToId.find(proxy) == m_proxyToId.end())
        return;

    String id = m_proxyToId.get(proxy);
    if (enabled()) {
        frontend()->workerTerminated(id);
        proxy->disconnectFromInspector(this);
    }

    m_idToProxy.remove(id);
    m_proxyToId.remove(proxy);
}

void InspectorWorkerAgent::connectToAllProxies()
{
    for (auto& idProxy : m_idToProxy)
        connectToProxy(idProxy.value, idProxy.key, false);
}

void InspectorWorkerAgent::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    if (frame != m_inspectedFrames->root())
        return;

    // During navigation workers from old page may die after a while.
    // Usually, it's fine to report them terminated later, but some tests
    // expect strict set of workers, and we reuse renderer between tests.
    if (enabled()) {
        for (auto& idProxy : m_idToProxy) {
            frontend()->workerTerminated(idProxy.key);
            idProxy.value->disconnectFromInspector(this);
        }
    }
    m_idToProxy.clear();
    m_proxyToId.clear();
}

void InspectorWorkerAgent::connectToProxy(WorkerInspectorProxy* proxy, const String& id, bool waitingForDebugger)
{
    proxy->connectToInspector(this);
    ASSERT(frontend());
    frontend()->workerCreated(id, proxy->url(), waitingForDebugger);
}

void InspectorWorkerAgent::dispatchMessageFromWorker(WorkerInspectorProxy* proxy, const String& message)
{
    if (m_proxyToId.find(proxy) == m_proxyToId.end())
        return;
    frontend()->dispatchMessageFromWorker(m_proxyToId.get(proxy), message);
}

void InspectorWorkerAgent::workerConsoleAgentEnabled(WorkerInspectorProxy* proxy)
{
    m_consoleAgent->workerConsoleAgentEnabled(proxy);
}

DEFINE_TRACE(InspectorWorkerAgent)
{
#if ENABLE(OILPAN)
    visitor->trace(m_idToProxy);
    visitor->trace(m_proxyToId);
    visitor->trace(m_consoleAgent);
#endif
    visitor->trace(m_inspectedFrames);
    InspectorBaseAgent<InspectorWorkerAgent, protocol::Frontend::Worker>::trace(visitor);
}

} // namespace blink
