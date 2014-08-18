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

#include "config.h"

#include "core/inspector/InspectorWorkerAgent.h"

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/JSONParser.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "platform/JSONValues.h"
#include "platform/weborigin/KURL.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace WorkerAgentState {
static const char workerInspectionEnabled[] = "workerInspectionEnabled";
static const char autoconnectToWorkers[] = "autoconnectToWorkers";
};

class InspectorWorkerAgent::WorkerFrontendChannel FINAL : public WorkerInspectorProxy::PageInspector {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WorkerFrontendChannel(InspectorFrontend::Worker* frontend, WorkerInspectorProxy* proxy)
        : m_frontend(frontend)
        , m_proxy(proxy)
        , m_id(s_nextId++)
        , m_connected(false)
    {
    }
    virtual ~WorkerFrontendChannel()
    {
        disconnectFromWorker();
    }

    int id() const { return m_id; }
    WorkerInspectorProxy* proxy() const { return m_proxy; }

    void connectToWorker()
    {
        if (m_connected)
            return;
        m_connected = true;
        m_proxy->connectToInspector(this);
    }

    void disconnectFromWorker()
    {
        if (!m_connected)
            return;
        m_connected = false;
        m_proxy->disconnectFromInspector();
    }

private:
    // WorkerInspectorProxy::PageInspector implementation
    virtual void dispatchMessageFromWorker(const String& message) OVERRIDE
    {
        RefPtr<JSONValue> value = parseJSON(message);
        if (!value)
            return;
        RefPtr<JSONObject> messageObject = value->asObject();
        if (!messageObject)
            return;
        m_frontend->dispatchMessageFromWorker(m_id, messageObject);
    }

    InspectorFrontend::Worker* m_frontend;
    WorkerInspectorProxy* m_proxy;
    int m_id;
    bool m_connected;
    static int s_nextId;
};

int InspectorWorkerAgent::WorkerFrontendChannel::s_nextId = 1;

PassOwnPtrWillBeRawPtr<InspectorWorkerAgent> InspectorWorkerAgent::create()
{
    return adoptPtrWillBeNoop(new InspectorWorkerAgent());
}

InspectorWorkerAgent::InspectorWorkerAgent()
    : InspectorBaseAgent<InspectorWorkerAgent>("Worker")
    , m_frontend(0)
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

void InspectorWorkerAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->worker();
}

void InspectorWorkerAgent::restore()
{
    if (m_state->getBoolean(WorkerAgentState::workerInspectionEnabled))
        createWorkerFrontendChannelsForExistingWorkers();
}

void InspectorWorkerAgent::clearFrontend()
{
    m_state->setBoolean(WorkerAgentState::autoconnectToWorkers, false);
    disable(0);
    m_frontend = 0;
}

void InspectorWorkerAgent::enable(ErrorString*)
{
    m_state->setBoolean(WorkerAgentState::workerInspectionEnabled, true);
    if (!m_frontend)
        return;
    createWorkerFrontendChannelsForExistingWorkers();
}

void InspectorWorkerAgent::disable(ErrorString*)
{
    m_state->setBoolean(WorkerAgentState::workerInspectionEnabled, false);
    if (!m_frontend)
        return;
    destroyWorkerFrontendChannels();
}

void InspectorWorkerAgent::canInspectWorkers(ErrorString*, bool* result)
{
    *result = true;
}

void InspectorWorkerAgent::connectToWorker(ErrorString* error, int workerId)
{
    WorkerFrontendChannel* channel = m_idToChannel.get(workerId);
    if (channel)
        channel->connectToWorker();
    else
        *error = "Worker is gone";
}

void InspectorWorkerAgent::disconnectFromWorker(ErrorString* error, int workerId)
{
    WorkerFrontendChannel* channel = m_idToChannel.get(workerId);
    if (channel)
        channel->disconnectFromWorker();
    else
        *error = "Worker is gone";
}

void InspectorWorkerAgent::sendMessageToWorker(ErrorString* error, int workerId, const RefPtr<JSONObject>& message)
{
    WorkerFrontendChannel* channel = m_idToChannel.get(workerId);
    if (channel)
        channel->proxy()->sendMessageToInspector(message->toJSONString());
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
    for (WorkerIds::iterator it = m_workerIds.begin(); it != m_workerIds.end(); ++it)
        it->key->writeTimelineStartedEvent(sessionId);
}

bool InspectorWorkerAgent::shouldPauseDedicatedWorkerOnStart()
{
    return m_state->getBoolean(WorkerAgentState::autoconnectToWorkers);
}

void InspectorWorkerAgent::didStartWorker(WorkerInspectorProxy* workerInspectorProxy, const KURL& url)
{
    m_workerIds.set(workerInspectorProxy, url.string());
    if (m_frontend && m_state->getBoolean(WorkerAgentState::workerInspectionEnabled))
        createWorkerFrontendChannel(workerInspectorProxy, url.string());
    if (!m_tracingSessionId.isEmpty())
        workerInspectorProxy->writeTimelineStartedEvent(m_tracingSessionId);
}

void InspectorWorkerAgent::workerTerminated(WorkerInspectorProxy* proxy)
{
    m_workerIds.remove(proxy);
    for (WorkerChannels::iterator it = m_idToChannel.begin(); it != m_idToChannel.end(); ++it) {
        if (proxy == it->value->proxy()) {
            m_frontend->workerTerminated(it->key);
            delete it->value;
            m_idToChannel.remove(it);
            return;
        }
    }
}

void InspectorWorkerAgent::createWorkerFrontendChannelsForExistingWorkers()
{
    for (WorkerIds::iterator it = m_workerIds.begin(); it != m_workerIds.end(); ++it)
        createWorkerFrontendChannel(it->key, it->value);
}

void InspectorWorkerAgent::destroyWorkerFrontendChannels()
{
    for (WorkerChannels::iterator it = m_idToChannel.begin(); it != m_idToChannel.end(); ++it) {
        it->value->disconnectFromWorker();
        delete it->value;
    }
    m_idToChannel.clear();
}

void InspectorWorkerAgent::createWorkerFrontendChannel(WorkerInspectorProxy* workerInspectorProxy, const String& url)
{
    WorkerFrontendChannel* channel = new WorkerFrontendChannel(m_frontend, workerInspectorProxy);
    m_idToChannel.set(channel->id(), channel);

    ASSERT(m_frontend);
    bool autoconnectToWorkers = m_state->getBoolean(WorkerAgentState::autoconnectToWorkers);
    if (autoconnectToWorkers)
        channel->connectToWorker();
    m_frontend->workerCreated(channel->id(), url, autoconnectToWorkers);
}

} // namespace blink
