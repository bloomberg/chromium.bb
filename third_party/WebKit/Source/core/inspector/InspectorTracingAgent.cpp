//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "config.h"

#include "core/inspector/InspectorTracingAgent.h"

#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorState.h"
#include "platform/TraceEvent.h"

namespace blink {

namespace TracingAgentState {
const char sessionId[] = "sessionId";
const char tracingStartedFromProtocol[] = "tracingStartedFromProtocol";
const char tracingStarted[] = "tracingStarted";
}

namespace {
const char devtoolsMetadataEventCategory[] = TRACE_DISABLED_BY_DEFAULT("devtools.timeline");
}

InspectorTracingAgent::InspectorTracingAgent(InspectorClient* client)
    : InspectorBaseAgent<InspectorTracingAgent>("Tracing")
    , m_layerTreeId(0)
    , m_client(client)
    , m_frontend(0)
{
}

void InspectorTracingAgent::restore()
{
    emitMetadataEvents();
}

void InspectorTracingAgent::start(ErrorString*, const String& categoryFilter, const String&, const double*)
{
    m_state->setBoolean(TracingAgentState::tracingStartedFromProtocol, true);
    innerStart(categoryFilter, false);
}

void InspectorTracingAgent::end(ErrorString* errorString)
{
    m_state->setBoolean(TracingAgentState::tracingStarted, false);
    m_consoleTimelines.clear();
    m_frontend->stopped();
}

void InspectorTracingAgent::innerStart(const String& categoryFilter, bool fromConsole)
{
    m_state->setString(TracingAgentState::sessionId, IdentifiersFactory::createIdentifier());
    m_state->setBoolean(TracingAgentState::tracingStarted, true);
    m_client->enableTracing(categoryFilter);
    emitMetadataEvents();
    m_frontend->started(fromConsole, sessionId());
}

String InspectorTracingAgent::sessionId()
{
    return m_state->getString(TracingAgentState::sessionId);
}

void InspectorTracingAgent::emitMetadataEvents()
{
    if (!m_state->getBoolean(TracingAgentState::tracingStarted))
        return;
    TRACE_EVENT_INSTANT1(devtoolsMetadataEventCategory, "TracingStartedInPage", "sessionId", sessionId().utf8());
    if (m_layerTreeId)
        setLayerTreeId(m_layerTreeId);
}

void InspectorTracingAgent::setLayerTreeId(int layerTreeId)
{
    m_layerTreeId = layerTreeId;
    TRACE_EVENT_INSTANT2(devtoolsMetadataEventCategory, "SetLayerTreeId", "sessionId", sessionId().utf8(), "layerTreeId", m_layerTreeId);
}

void InspectorTracingAgent::consoleTimeline(const String& title)
{
    m_consoleTimelines.append(title);
    if (m_state->getBoolean(TracingAgentState::tracingStarted))
        return;
    innerStart("-*,disabled-by-default-devtools.timeline", true);
}

void InspectorTracingAgent::consoleTimelineEnd(const String& title)
{
    size_t index = m_consoleTimelines.find(title);
    if (index == kNotFound)
        return;

    m_consoleTimelines.remove(index);
    if (!m_consoleTimelines.size()
        && m_state->getBoolean(TracingAgentState::tracingStarted)
        && !m_state->getBoolean(TracingAgentState::tracingStartedFromProtocol))
        m_frontend->stopped();
    m_state->setBoolean(TracingAgentState::tracingStarted, false);
}

void InspectorTracingAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->tracing();
}

}
