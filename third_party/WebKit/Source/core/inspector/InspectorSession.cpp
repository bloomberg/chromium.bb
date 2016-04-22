// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorSession.h"

#include "core/InstrumentingAgents.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "platform/inspector_protocol/Backend.h"
#include "platform/inspector_protocol/Parser.h"
#include "platform/inspector_protocol/TypeBuilder.h"

namespace blink {

InspectorSession::InspectorSession(Client* client, int sessionId, bool autoFlush)
    : m_client(client)
    , m_sessionId(sessionId)
    , m_autoFlush(autoFlush)
    , m_attached(false)
    , m_instrumentingAgents(new InstrumentingAgents())
    , m_inspectorFrontend(adoptPtr(new protocol::Frontend(this)))
    , m_inspectorBackendDispatcher(protocol::Dispatcher::create(this))
{
}

void InspectorSession::append(InspectorAgent* agent)
{
    m_agents.append(agent);
}

void InspectorSession::attach(const String* savedState)
{
    ASSERT(!m_attached);
    m_attached = true;
    InspectorInstrumentation::frontendCreated();
    bool restore = savedState;

    if (restore) {
        OwnPtr<protocol::Value> state = protocol::parseJSON(*savedState);
        if (state)
            m_state = protocol::DictionaryValue::cast(state.release());
        if (!m_state)
            m_state = protocol::DictionaryValue::create();
    } else {
        m_state = protocol::DictionaryValue::create();
    }

    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->init(m_instrumentingAgents.get(), m_inspectorFrontend.get(), m_inspectorBackendDispatcher.get(), m_state.get());

    if (restore) {
        for (size_t i = 0; i < m_agents.size(); i++)
            m_agents[i]->restore();
    }
}

void InspectorSession::detach()
{
    ASSERT(m_attached);
    m_attached = false;

    m_inspectorBackendDispatcher->clearFrontend();
    m_inspectorBackendDispatcher.clear();
    for (size_t i = m_agents.size(); i > 0; i--)
        m_agents[i - 1]->dispose();
    m_inspectorFrontend.clear();
    m_agents.clear();
    InspectorInstrumentation::frontendDeleted();
}

void InspectorSession::dispatchProtocolMessage(const String& message)
{
    ASSERT(m_attached);
    m_inspectorBackendDispatcher->dispatch(m_sessionId, message);
}

void InspectorSession::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->didCommitLoadForLocalFrame(frame);
}

void InspectorSession::sendProtocolResponse(int sessionId, int callId, PassOwnPtr<protocol::DictionaryValue> message)
{
    if (!m_attached)
        return;
    flush();
    String stateToSend = m_state->toJSONString();
    if (stateToSend == m_lastSentState)
        stateToSend = String();
    else
        m_lastSentState = stateToSend;
    m_client->sendProtocolMessage(m_sessionId, callId, message->toJSONString(), stateToSend);
}

void InspectorSession::sendProtocolNotification(PassOwnPtr<protocol::DictionaryValue> message)
{
    if (!m_attached)
        return;
    if (m_autoFlush)
        m_client->sendProtocolMessage(m_sessionId, 0, message->toJSONString(), String());
    else
        m_notificationQueue.append(message);
}

void InspectorSession::flush()
{
    flushPendingProtocolNotifications();
}

void InspectorSession::flushPendingProtocolNotifications()
{
    if (m_attached) {
        for (size_t i = 0; i < m_agents.size(); i++)
            m_agents[i]->flushPendingProtocolNotifications();
        for (size_t i = 0; i < m_notificationQueue.size(); ++i)
            m_client->sendProtocolMessage(m_sessionId, 0, m_notificationQueue[i]->toJSONString(), String());
    }
    m_notificationQueue.clear();
}

DEFINE_TRACE(InspectorSession)
{
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_agents);
}

} // namespace blink

