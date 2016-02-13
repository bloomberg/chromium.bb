/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
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

#include "platform/v8_inspector/InjectedScriptHost.h"

#include "platform/JSONValues.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/public/V8Debugger.h"

#include "wtf/RefPtr.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

PassRefPtr<InjectedScriptHost> InjectedScriptHost::create(V8DebuggerImpl* debugger)
{
    return adoptRef(new InjectedScriptHost(debugger));
}

InjectedScriptHost::InjectedScriptHost(V8DebuggerImpl* debugger)
    : m_debugger(debugger)
    , m_debuggerAgent(nullptr)
    , m_inspectCallback(nullptr)
    , m_clearConsoleCallback(nullptr)
{
}

InjectedScriptHost::~InjectedScriptHost()
{
}

void InjectedScriptHost::setClearConsoleCallback(PassOwnPtr<V8RuntimeAgent::ClearConsoleCallback> callback)
{
    m_clearConsoleCallback = std::move(callback);
}

void InjectedScriptHost::setInspectObjectCallback(PassOwnPtr<V8RuntimeAgent::InspectCallback> callback)
{
    m_inspectCallback = std::move(callback);
}

void InjectedScriptHost::disconnect()
{
    m_debugger = nullptr;
    m_debuggerAgent = nullptr;
    m_inspectCallback = nullptr;
    m_clearConsoleCallback = nullptr;
    m_inspectedObjects.clear();
}

void InjectedScriptHost::inspectImpl(PassRefPtr<JSONValue> object, PassRefPtr<JSONValue> hints)
{
    if (m_inspectCallback) {
        RefPtr<protocol::TypeBuilder::Runtime::RemoteObject> remoteObject = protocol::TypeBuilder::Runtime::RemoteObject::runtimeCast(object);
        (*m_inspectCallback)(remoteObject, hints->asObject());
    }
}

void InjectedScriptHost::clearConsoleMessages()
{
    if (m_clearConsoleCallback)
        (*m_clearConsoleCallback)();
}

void InjectedScriptHost::addInspectedObject(PassOwnPtr<V8RuntimeAgent::Inspectable> object)
{
    m_inspectedObjects.prepend(object);
    while (m_inspectedObjects.size() > 5)
        m_inspectedObjects.removeLast();
}

void InjectedScriptHost::clearInspectedObjects()
{
    m_inspectedObjects.clear();
}

V8RuntimeAgent::Inspectable* InjectedScriptHost::inspectedObject(unsigned num)
{
    if (num >= m_inspectedObjects.size())
        return nullptr;
    return m_inspectedObjects[num].get();
}

void InjectedScriptHost::debugFunction(const String& scriptId, int lineNumber, int columnNumber)
{
    if (m_debuggerAgent)
        m_debuggerAgent->setBreakpointAt(scriptId, lineNumber, columnNumber, V8DebuggerAgentImpl::DebugCommandBreakpointSource);
}

void InjectedScriptHost::undebugFunction(const String& scriptId, int lineNumber, int columnNumber)
{
    if (m_debuggerAgent)
        m_debuggerAgent->removeBreakpointAt(scriptId, lineNumber, columnNumber, V8DebuggerAgentImpl::DebugCommandBreakpointSource);
}

void InjectedScriptHost::monitorFunction(const String& scriptId, int lineNumber, int columnNumber, const String& functionName)
{
    StringBuilder builder;
    builder.appendLiteral("console.log(\"function ");
    if (functionName.isEmpty())
        builder.appendLiteral("(anonymous function)");
    else
        builder.append(functionName);
    builder.appendLiteral(" called\" + (arguments.length > 0 ? \" with arguments: \" + Array.prototype.join.call(arguments, \", \") : \"\")) && false");
    if (m_debuggerAgent)
        m_debuggerAgent->setBreakpointAt(scriptId, lineNumber, columnNumber, V8DebuggerAgentImpl::MonitorCommandBreakpointSource, builder.toString());
}

void InjectedScriptHost::unmonitorFunction(const String& scriptId, int lineNumber, int columnNumber)
{
    if (m_debuggerAgent)
        m_debuggerAgent->removeBreakpointAt(scriptId, lineNumber, columnNumber, V8DebuggerAgentImpl::MonitorCommandBreakpointSource);
}

} // namespace blink
