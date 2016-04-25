/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#include "bindings/core/v8/ScriptCallStack.h"

#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ThreadDebugger.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TracedValue.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8StackTrace.h"

namespace blink {

PassRefPtr<ScriptCallStack> ScriptCallStack::create(v8::Isolate* isolate, v8::Local<v8::StackTrace> stackTrace, size_t maxStackSize)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    if (!data->threadDebugger())
        return nullptr;
    return adoptRef(new ScriptCallStack(data->threadDebugger()->debugger()->createStackTrace(stackTrace, maxStackSize)));
}

PassRefPtr<ScriptCallStack> ScriptCallStack::capture(size_t maxStackSize)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (!isolate->InContext())
        return nullptr;
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    if (!data->threadDebugger())
        return nullptr;
    ScriptForbiddenScope::AllowUserAgentScript allowScripting;
    OwnPtr<V8StackTrace> stack = data->threadDebugger()->debugger()->captureStackTrace(maxStackSize);
    return stack ? adoptRef(new ScriptCallStack(stack.release())) : nullptr;
}

PassRefPtr<ScriptCallStack> ScriptCallStack::captureForConsole()
{
    size_t stackSize = 1;
    if (InspectorInstrumentation::hasFrontends()) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        if (!isolate->InContext())
            return nullptr;
        if (InspectorInstrumentation::consoleAgentEnabled(currentExecutionContext(isolate)))
            stackSize = V8StackTrace::maxCallStackSizeToCapture;
    }
    return ScriptCallStack::capture(stackSize);
}

ScriptCallStack::ScriptCallStack(PassOwnPtr<V8StackTrace> stackTrace)
    : m_stackTrace(std::move(stackTrace))
{
}

ScriptCallStack::~ScriptCallStack()
{
}

bool ScriptCallStack::isEmpty() const
{
    return m_stackTrace->isEmpty();
}

String ScriptCallStack::topSourceURL() const
{
    return m_stackTrace->topSourceURL();
}

unsigned ScriptCallStack::topLineNumber() const
{
    return m_stackTrace->topLineNumber();
}

unsigned ScriptCallStack::topColumnNumber() const
{
    return m_stackTrace->topColumnNumber();
}

PassOwnPtr<protocol::Runtime::StackTrace> ScriptCallStack::buildInspectorObject() const
{
    return m_stackTrace->buildInspectorObject();
}

void ScriptCallStack::toTracedValue(TracedValue* value, const char* name) const
{
    if (m_stackTrace->isEmpty())
        return;
    value->beginArray(name);
    value->beginDictionary();
    value->setString("functionName", m_stackTrace->topFunctionName());
    value->setString("scriptId", m_stackTrace->topScriptId());
    value->setString("url", m_stackTrace->topSourceURL());
    value->setInteger("lineNumber", m_stackTrace->topLineNumber());
    value->setInteger("columnNumber", m_stackTrace->topColumnNumber());
    value->endDictionary();
    value->endArray();
}

String ScriptCallStack::toString() const
{
    return m_stackTrace->toString();
}

} // namespace blink
