/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/InspectorProfilerAgent.h"

#include "bindings/core/v8/ScriptCallStack.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/v8/public/V8ProfilerAgent.h"

namespace blink {

namespace ProfilerAgentState {
static const char profilerEnabled[] = "profilerEnabled";
}

PassOwnPtrWillBeRawPtr<InspectorProfilerAgent> InspectorProfilerAgent::create(V8Debugger* debugger, Client* client)
{
    return adoptPtrWillBeNoop(new InspectorProfilerAgent(debugger, client));
}

InspectorProfilerAgent::InspectorProfilerAgent(V8Debugger* debugger, Client* client)
    : InspectorBaseAgent<InspectorProfilerAgent, protocol::Frontend::Profiler>("Profiler")
    , m_client(client)
    , m_v8ProfilerAgent(V8ProfilerAgent::create(debugger))
{
}

InspectorProfilerAgent::~InspectorProfilerAgent()
{
}

// InspectorBaseAgent overrides.
void InspectorProfilerAgent::setState(PassRefPtr<JSONObject> state)
{
    InspectorBaseAgent::setState(state);
    m_v8ProfilerAgent->setInspectorState(m_state);
}

void InspectorProfilerAgent::setFrontend(protocol::Frontend* frontend)
{
    InspectorBaseAgent::setFrontend(frontend);
    m_v8ProfilerAgent->setFrontend(protocol::Frontend::Profiler::from(frontend));
}

void InspectorProfilerAgent::clearFrontend()
{
    m_v8ProfilerAgent->clearFrontend();
    InspectorBaseAgent::clearFrontend();
}

void InspectorProfilerAgent::restore()
{
    if (!m_state->booleanProperty(ProfilerAgentState::profilerEnabled, false))
        return;
    m_v8ProfilerAgent->restore();
    ErrorString errorString;
    enable(&errorString);
}

// Protocol implementation.
void InspectorProfilerAgent::consoleProfile(ExecutionContext* context, const String& title)
{
    UseCounter::count(context, UseCounter::DevToolsConsoleProfile);
    m_v8ProfilerAgent->consoleProfile(title);
}

void InspectorProfilerAgent::consoleProfileEnd(const String& title)
{
    m_v8ProfilerAgent->consoleProfileEnd(title);
}

void InspectorProfilerAgent::enable(ErrorString* errorString)
{
    m_v8ProfilerAgent->enable(errorString);
    m_state->setBoolean(ProfilerAgentState::profilerEnabled, true);
    m_instrumentingAgents->setInspectorProfilerAgent(this);
}

void InspectorProfilerAgent::disable(ErrorString* errorString)
{
    m_instrumentingAgents->setInspectorProfilerAgent(nullptr);
    m_state->setBoolean(ProfilerAgentState::profilerEnabled, false);
    m_v8ProfilerAgent->disable(errorString);
}

void InspectorProfilerAgent::setSamplingInterval(ErrorString* error, int interval)
{
    m_v8ProfilerAgent->setSamplingInterval(error, interval);
}

void InspectorProfilerAgent::start(ErrorString* error)
{
    m_v8ProfilerAgent->start(error);
    if (m_client && !*error)
        m_client->profilingStarted();
}

void InspectorProfilerAgent::stop(ErrorString* errorString, RefPtr<protocol::TypeBuilder::Profiler::CPUProfile>& profile)
{
    if (m_client)
        m_client->profilingStopped();
    m_v8ProfilerAgent->stop(errorString, profile);
}

void InspectorProfilerAgent::willProcessTask()
{
    m_v8ProfilerAgent->idleFinished();
}

void InspectorProfilerAgent::didProcessTask()
{
    m_v8ProfilerAgent->idleStarted();
}

void InspectorProfilerAgent::willEnterNestedRunLoop()
{
    m_v8ProfilerAgent->idleStarted();
}

void InspectorProfilerAgent::didLeaveNestedRunLoop()
{
    m_v8ProfilerAgent->idleFinished();
}

DEFINE_TRACE(InspectorProfilerAgent)
{
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink
