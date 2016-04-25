// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorSession.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/InstrumentingAgents.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "platform/inspector_protocol/Backend.h"
#include "platform/inspector_protocol/Parser.h"
#include "platform/inspector_protocol/TypeBuilder.h"
#include "platform/v8_inspector/public/V8DebuggerAgent.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"
#include "platform/v8_inspector/public/V8ProfilerAgent.h"
#include "platform/v8_inspector/public/V8RuntimeAgent.h"

namespace blink {

InspectorSession::InspectorSession(Client* client, InspectedFrames* inspectedFrames, int sessionId, bool autoFlush)
    : m_client(client)
    , m_v8Session(nullptr)
    , m_sessionId(sessionId)
    , m_autoFlush(autoFlush)
    , m_attached(false)
    , m_inspectedFrames(inspectedFrames)
    , m_instrumentingAgents(new InstrumentingAgents())
    , m_inspectorFrontend(adoptPtr(new protocol::Frontend(this)))
    , m_inspectorBackendDispatcher(protocol::Dispatcher::create(this))
{
}

void InspectorSession::append(InspectorAgent* agent)
{
    m_agents.append(agent);
}

void InspectorSession::attach(V8InspectorSession* v8Session, const String* savedState)
{
    ASSERT(!m_attached);
    m_attached = true;
    m_v8Session = v8Session;
    m_v8Session->setClient(this);
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
    m_v8Session->setClient(nullptr);
    m_v8Session = nullptr;
    ASSERT(!isInstrumenting());
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
        m_notificationQueue.append(std::move(message));
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

void InspectorSession::scriptExecutionBlockedByCSP(const String& directiveText)
{
    ASSERT(isInstrumenting());
    OwnPtr<protocol::DictionaryValue> directive = protocol::DictionaryValue::create();
    directive->setString("directiveText", directiveText);
    m_v8Session->debuggerAgent()->breakProgramOnException(protocol::Debugger::Paused::ReasonEnum::CSPViolation, directive.release());
}

void InspectorSession::asyncTaskScheduled(const String& taskName, void* task)
{
    ASSERT(isInstrumenting());
    m_v8Session->debuggerAgent()->asyncTaskScheduled(taskName, task, false);
}

void InspectorSession::asyncTaskScheduled(const String& operationName, void* task, bool recurring)
{
    ASSERT(isInstrumenting());
    m_v8Session->debuggerAgent()->asyncTaskScheduled(operationName, task, recurring);
}

void InspectorSession::asyncTaskCanceled(void* task)
{
    ASSERT(isInstrumenting());
    m_v8Session->debuggerAgent()->asyncTaskCanceled(task);
}

void InspectorSession::allAsyncTasksCanceled()
{
    ASSERT(isInstrumenting());
    m_v8Session->debuggerAgent()->allAsyncTasksCanceled();
}

void InspectorSession::asyncTaskStarted(void* task)
{
    ASSERT(isInstrumenting());
    m_v8Session->debuggerAgent()->asyncTaskStarted(task);
}

void InspectorSession::asyncTaskFinished(void* task)
{
    ASSERT(isInstrumenting());
    m_v8Session->debuggerAgent()->asyncTaskFinished(task);
}

void InspectorSession::didStartProvisionalLoad(LocalFrame* frame)
{
    ASSERT(isInstrumenting());
    if (m_inspectedFrames && m_inspectedFrames->root() == frame) {
        ErrorString error;
        m_v8Session->debuggerAgent()->resume(&error);
    }
}

void InspectorSession::didClearDocumentOfWindowObject(LocalFrame* frame)
{
    ASSERT(isInstrumenting());
    frame->script().initializeMainWorld();
}

void InspectorSession::willProcessTask()
{
    ASSERT(isInstrumenting());
    m_v8Session->profilerAgent()->idleFinished();
}

void InspectorSession::didProcessTask()
{
    ASSERT(isInstrumenting());
    m_v8Session->profilerAgent()->idleStarted();
}

void InspectorSession::willEnterNestedRunLoop()
{
    ASSERT(isInstrumenting());
    m_v8Session->profilerAgent()->idleStarted();
}

void InspectorSession::didLeaveNestedRunLoop()
{
    ASSERT(isInstrumenting());
    m_v8Session->profilerAgent()->idleFinished();
}

void InspectorSession::startInstrumenting()
{
    ASSERT(!isInstrumenting());
    m_instrumentingAgents->setInspectorSession(this);
    forceContextsInAllFrames();
}

void InspectorSession::stopInstrumenting()
{
    ASSERT(isInstrumenting());
    m_instrumentingAgents->setInspectorSession(nullptr);
}

void InspectorSession::forceContextsInAllFrames()
{
    if (!m_inspectedFrames)
        return;
    if (!m_inspectedFrames->root()->loader().stateMachine()->committedFirstRealDocumentLoad())
        return;
    for (const LocalFrame* frame : *m_inspectedFrames)
        frame->script().initializeMainWorld();
}

#if ENABLE(ASSERT)
bool InspectorSession::isInstrumenting()
{
    return m_instrumentingAgents->inspectorSession() == this;
}
#endif

DEFINE_TRACE(InspectorSession)
{
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_agents);
}

} // namespace blink

