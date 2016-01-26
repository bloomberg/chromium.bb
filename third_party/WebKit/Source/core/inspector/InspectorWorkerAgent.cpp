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

#include "core/InspectorFrontend.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/PageConsoleAgent.h"
#include "platform/weborigin/KURL.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace WorkerAgentState {
static const char workerInspectionEnabled[] = "workerInspectionEnabled";
static const char autoconnectToWorkers[] = "autoconnectToWorkers";
};

PassOwnPtrWillBeRawPtr<InspectorWorkerAgent> InspectorWorkerAgent::create(PageConsoleAgent* consoleAgent)
{
    return adoptPtrWillBeNoop(new InspectorWorkerAgent(consoleAgent));
}

InspectorWorkerAgent::InspectorWorkerAgent(PageConsoleAgent* consoleAgent)
    : InspectorBaseAgent<InspectorWorkerAgent, InspectorFrontend::Worker>("Worker")
    , m_consoleAgent(consoleAgent)
{
}

InspectorWorkerAgent::~InspectorWorkerAgent()
{
#if !ENABLE(OILPAN)
    m_instrumentingAgents->setInspectorWorkerAgent(0);
#endif
}

void InspectorWorkerAgent::init()
{
    m_instrumentingAgents->setInspectorWorkerAgent(this);
}

void InspectorWorkerAgent::restore()
{
    if (m_state->booleanProperty(WorkerAgentState::workerInspectionEnabled, false))
        createWorkerAgentClientsForExistingWorkers();
}

void InspectorWorkerAgent::enable(ErrorString*)
{
    m_state->setBoolean(WorkerAgentState::workerInspectionEnabled, true);
    createWorkerAgentClientsForExistingWorkers();
}

void InspectorWorkerAgent::disable(ErrorString*)
{
    m_state->setBoolean(WorkerAgentState::workerInspectionEnabled, false);
    m_state->setBoolean(WorkerAgentState::autoconnectToWorkers, false);
    destroyWorkerAgentClients();
}

void InspectorWorkerAgent::connectToWorker(ErrorString* error, const String& workerId)
{
    WorkerAgentClient* client = m_idToClient.get(workerId);
    if (client)
        client->connectToWorker();
    else
        *error = "Worker is gone";
}

void InspectorWorkerAgent::disconnectFromWorker(ErrorString* error, const String& workerId)
{
    WorkerAgentClient* client = m_idToClient.get(workerId);
    if (client)
        client->dispose();
    else
        *error = "Worker is gone";
}

void InspectorWorkerAgent::sendMessageToWorker(ErrorString* error, const String& workerId, const String& message)
{
    WorkerAgentClient* client = m_idToClient.get(workerId);
    if (client)
        client->proxy()->sendMessageToInspector(message);
    else
        *error = "Worker is gone";
}

void InspectorWorkerAgent::setAutoconnectToWorkers(ErrorString*, bool value)
{
    m_state->setBoolean(WorkerAgentState::autoconnectToWorkers, value);
}

void InspectorWorkerAgent::setTracingSessionId(const String& sessionId)
{
    m_tracingSessionId = sessionId;
    if (sessionId.isEmpty())
        return;
    for (auto& info : m_workerInfos)
        info.key->writeTimelineStartedEvent(sessionId, info.value.id);
}

bool InspectorWorkerAgent::shouldPauseDedicatedWorkerOnStart()
{
    return m_state->booleanProperty(WorkerAgentState::autoconnectToWorkers, false);
}

void InspectorWorkerAgent::didStartWorker(WorkerInspectorProxy* workerInspectorProxy, const KURL& url)
{
    String id = "dedicated:" + IdentifiersFactory::createIdentifier();
    m_workerInfos.set(workerInspectorProxy, WorkerInfo(url.string(), id));
    if (frontend() && m_state->booleanProperty(WorkerAgentState::workerInspectionEnabled, false))
        createWorkerAgentClient(workerInspectorProxy, url.string(), id);
    if (!m_tracingSessionId.isEmpty())
        workerInspectorProxy->writeTimelineStartedEvent(m_tracingSessionId, id);
}

void InspectorWorkerAgent::workerTerminated(WorkerInspectorProxy* proxy)
{
    m_workerInfos.remove(proxy);
    for (WorkerClients::iterator it = m_idToClient.begin(); it != m_idToClient.end(); ++it) {
        if (proxy == it->value->proxy()) {
            frontend()->workerTerminated(it->key);
            it->value->dispose();
            m_idToClient.remove(it);
            return;
        }
    }
}

void InspectorWorkerAgent::createWorkerAgentClientsForExistingWorkers()
{
    for (auto& info : m_workerInfos)
        createWorkerAgentClient(info.key, info.value.url, info.value.id);
}

void InspectorWorkerAgent::destroyWorkerAgentClients()
{
    for (auto& client : m_idToClient)
        client.value->dispose();
    m_idToClient.clear();
}

void InspectorWorkerAgent::createWorkerAgentClient(WorkerInspectorProxy* workerInspectorProxy, const String& url, const String& id)
{
    OwnPtrWillBeRawPtr<WorkerAgentClient> client = WorkerAgentClient::create(frontend(), workerInspectorProxy, id, m_consoleAgent);
    WorkerAgentClient* rawClient = client.get();
    m_idToClient.set(id, client.release());

    ASSERT(frontend());
    bool autoconnectToWorkers = m_state->booleanProperty(WorkerAgentState::autoconnectToWorkers, false);
    if (autoconnectToWorkers)
        rawClient->connectToWorker();
    frontend()->workerCreated(id, url, autoconnectToWorkers);
}

DEFINE_TRACE(InspectorWorkerAgent)
{
#if ENABLE(OILPAN)
    visitor->trace(m_idToClient);
    visitor->trace(m_consoleAgent);
    visitor->trace(m_workerInfos);
#endif
    InspectorBaseAgent<InspectorWorkerAgent, InspectorFrontend::Worker>::trace(visitor);
}

PassOwnPtrWillBeRawPtr<InspectorWorkerAgent::WorkerAgentClient> InspectorWorkerAgent::WorkerAgentClient::create(InspectorFrontend::Worker* frontend, WorkerInspectorProxy* proxy, const String& id, PageConsoleAgent* consoleAgent)
{
    return adoptPtrWillBeNoop(new InspectorWorkerAgent::WorkerAgentClient(frontend, proxy, id, consoleAgent));
}

InspectorWorkerAgent::WorkerAgentClient::WorkerAgentClient(InspectorFrontend::Worker* frontend, WorkerInspectorProxy* proxy, const String& id, PageConsoleAgent* consoleAgent)
    : m_frontend(frontend)
    , m_proxy(proxy)
    , m_id(id)
    , m_connected(false)
    , m_consoleAgent(consoleAgent)
{
    ASSERT(!proxy->pageInspector());
}
InspectorWorkerAgent::WorkerAgentClient::~WorkerAgentClient()
{
    ASSERT(!m_frontend);
    ASSERT(!m_proxy);
    ASSERT(!m_consoleAgent);
}

void InspectorWorkerAgent::WorkerAgentClient::connectToWorker()
{
    if (m_connected)
        return;
    m_connected = true;
    m_proxy->connectToInspector(this);
}

void InspectorWorkerAgent::WorkerAgentClient::dispose()
{
    if (m_connected) {
        m_connected = false;
        m_proxy->disconnectFromInspector();
    }
    m_frontend = nullptr;
    m_proxy = nullptr;
    m_consoleAgent = nullptr;
}

void InspectorWorkerAgent::WorkerAgentClient::dispatchMessageFromWorker(const String& message)
{
    m_frontend->dispatchMessageFromWorker(m_id, message);
}
void InspectorWorkerAgent::WorkerAgentClient::workerConsoleAgentEnabled(WorkerGlobalScopeProxy* proxy)
{
    m_consoleAgent->workerConsoleAgentEnabled(proxy);
}

DEFINE_TRACE(InspectorWorkerAgent::WorkerAgentClient)
{
    visitor->trace(m_proxy);
    visitor->trace(m_consoleAgent);
    WorkerInspectorProxy::PageInspector::trace(visitor);
}

} // namespace blink
