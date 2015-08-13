/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/inspector/InspectorDebuggerAgent.h"

namespace blink {

InspectorDebuggerAgent::InspectorDebuggerAgent(InjectedScriptManager* injectedScriptManager, V8Debugger* debugger)
    : V8DebuggerAgent(injectedScriptManager, debugger, this)
    , m_listener(nullptr)
{
}

InspectorDebuggerAgent::~InspectorDebuggerAgent()
{
#if !ENABLE(OILPAN)
    ASSERT(!m_instrumentingAgents->inspectorDebuggerAgent());
#endif
}

DEFINE_TRACE(InspectorDebuggerAgent)
{
#if ENABLE(OILPAN)
    visitor->trace(m_listener);
#endif
    V8DebuggerAgent::trace(visitor);
}

void InspectorDebuggerAgent::enable(ErrorString* errorString)
{
    V8DebuggerAgent::enable(errorString);
}

void InspectorDebuggerAgent::startListeningV8Debugger()
{
    m_instrumentingAgents->setInspectorDebuggerAgent(this);
}

void InspectorDebuggerAgent::stopListeningV8Debugger()
{
    m_instrumentingAgents->setInspectorDebuggerAgent(nullptr);
}

bool InspectorDebuggerAgent::canPauseOnPromiseEvent()
{
    return m_listener && m_listener->canPauseOnPromiseEvent();
}

void InspectorDebuggerAgent::didCreatePromise()
{
    if (m_listener)
        m_listener->didCreatePromise();
}

void InspectorDebuggerAgent::didResolvePromise()
{
    if (m_listener)
        m_listener->didResolvePromise();
}

void InspectorDebuggerAgent::didRejectPromise()
{
    if (m_listener)
        m_listener->didRejectPromise();
}

} // namespace blink
