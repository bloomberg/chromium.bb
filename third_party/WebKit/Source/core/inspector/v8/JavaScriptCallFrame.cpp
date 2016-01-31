/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#include "core/inspector/v8/JavaScriptCallFrame.h"

#include "core/inspector/v8/V8DebuggerClient.h"
#include "core/inspector/v8/V8StringUtil.h"

#include <v8-debug.h>

namespace blink {

JavaScriptCallFrame::JavaScriptCallFrame(V8DebuggerClient* client, v8::Local<v8::Context> debuggerContext, v8::Local<v8::Object> callFrame)
    : m_client(client)
    , m_isolate(debuggerContext->GetIsolate())
    , m_debuggerContext(m_isolate, debuggerContext)
    , m_callFrame(m_isolate, callFrame)
{
}

JavaScriptCallFrame::~JavaScriptCallFrame()
{
}

JavaScriptCallFrame* JavaScriptCallFrame::caller()
{
    if (!m_caller) {
        v8::HandleScope handleScope(m_isolate);
        v8::Local<v8::Context> debuggerContext = v8::Local<v8::Context>::New(m_isolate, m_debuggerContext);
        v8::Context::Scope contextScope(debuggerContext);
        v8::Local<v8::Value> callerFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame)->Get(toV8StringInternalized(m_isolate, "caller"));
        if (callerFrame.IsEmpty() || !callerFrame->IsObject())
            return 0;
        m_caller = JavaScriptCallFrame::create(m_client, debuggerContext, v8::Local<v8::Object>::Cast(callerFrame));
    }
    return m_caller.get();
}

int JavaScriptCallFrame::callV8FunctionReturnInt(const char* name) const
{
    v8::HandleScope handleScope(m_isolate);
    v8::Context::Scope contextScope(v8::Local<v8::Context>::New(m_isolate, m_debuggerContext));
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, name)));
    v8::Local<v8::Value> result;
    if (!m_client->callInternalFunction(func, callFrame, 0, nullptr).ToLocal(&result) || !result->IsInt32())
        return 0;
    return result.As<v8::Int32>()->Value();
}

String JavaScriptCallFrame::callV8FunctionReturnString(const char* name) const
{
    v8::HandleScope handleScope(m_isolate);
    v8::Context::Scope contextScope(v8::Local<v8::Context>::New(m_isolate, m_debuggerContext));
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, name)));
    v8::Local<v8::Value> result;
    if (!m_client->callInternalFunction(func, callFrame, 0, nullptr).ToLocal(&result))
        return String();
    return toWTFStringWithTypeCheck(result);
}

int JavaScriptCallFrame::sourceID() const
{
    return callV8FunctionReturnInt("sourceID");
}

int JavaScriptCallFrame::line() const
{
    return callV8FunctionReturnInt("line");
}

int JavaScriptCallFrame::column() const
{
    return callV8FunctionReturnInt("column");
}

String JavaScriptCallFrame::scriptName() const
{
    return callV8FunctionReturnString("scriptName");
}

String JavaScriptCallFrame::functionName() const
{
    return callV8FunctionReturnString("functionName");
}

int JavaScriptCallFrame::functionLine() const
{
    return callV8FunctionReturnInt("functionLine");
}

int JavaScriptCallFrame::functionColumn() const
{
    return callV8FunctionReturnInt("functionColumn");
}

v8::Local<v8::Value> JavaScriptCallFrame::scopeChain() const
{
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, "scopeChain")));
    v8::Local<v8::Array> scopeChain = v8::Local<v8::Array>::Cast(m_client->callInternalFunction(func, callFrame, 0, nullptr).ToLocalChecked());
    v8::Local<v8::Array> result = v8::Array::New(m_isolate, scopeChain->Length());
    for (uint32_t i = 0; i < scopeChain->Length(); i++)
        result->Set(i, scopeChain->Get(i));
    return result;
}

int JavaScriptCallFrame::scopeType(int scopeIndex) const
{
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, "scopeType")));
    v8::Local<v8::Array> scopeType = v8::Local<v8::Array>::Cast(m_client->callInternalFunction(func, callFrame, 0, nullptr).ToLocalChecked());
    return scopeType->Get(scopeIndex)->Int32Value();
}

v8::Local<v8::String> JavaScriptCallFrame::scopeName(int scopeIndex) const
{
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, "scopeName")));
    v8::Local<v8::Array> scopeType = v8::Local<v8::Array>::Cast(m_client->callInternalFunction(func, callFrame, 0, nullptr).ToLocalChecked());
    return scopeType->Get(scopeIndex)->ToString();
}

v8::Local<v8::Value> JavaScriptCallFrame::thisObject() const
{
    return v8::Local<v8::Object>::New(m_isolate, m_callFrame)->Get(toV8StringInternalized(m_isolate, "thisObject"));
}

String JavaScriptCallFrame::stepInPositions() const
{
    return callV8FunctionReturnString("stepInPositions");
}

bool JavaScriptCallFrame::isAtReturn() const
{
    v8::HandleScope handleScope(m_isolate);
    v8::Context::Scope contextScope(v8::Local<v8::Context>::New(m_isolate, m_debuggerContext));
    v8::Local<v8::Value> result = v8::Local<v8::Object>::New(m_isolate, m_callFrame)->Get(toV8StringInternalized(m_isolate, "isAtReturn"));
    if (result.IsEmpty() || !result->IsBoolean())
        return false;
    return result->BooleanValue();
}

v8::Local<v8::Value> JavaScriptCallFrame::returnValue() const
{
    return v8::Local<v8::Object>::New(m_isolate, m_callFrame)->Get(toV8StringInternalized(m_isolate, "returnValue"));
}

v8::Local<v8::Value> JavaScriptCallFrame::evaluateWithExceptionDetails(v8::Local<v8::Value> expression, v8::Local<v8::Value> scopeExtension)
{
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> evalFunction = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, "evaluate")));
    v8::Local<v8::Value> argv[] = {
        expression,
        scopeExtension
    };
    v8::TryCatch tryCatch(m_isolate);
    v8::Local<v8::Object> wrappedResult = v8::Object::New(m_isolate);
    v8::Local<v8::Value> result;
    if (m_client->callInternalFunction(evalFunction, callFrame, WTF_ARRAY_LENGTH(argv), argv).ToLocal(&result)) {
        wrappedResult->Set(v8::String::NewFromUtf8(m_isolate, "result"), result);
        wrappedResult->Set(v8::String::NewFromUtf8(m_isolate, "exceptionDetails"), v8::Undefined(m_isolate));
    } else {
        wrappedResult->Set(v8::String::NewFromUtf8(m_isolate, "result"), tryCatch.Exception());
        wrappedResult->Set(v8::String::NewFromUtf8(m_isolate, "exceptionDetails"), createExceptionDetails(m_isolate, tryCatch.Message()));
    }
    return wrappedResult;
}

v8::MaybeLocal<v8::Value> JavaScriptCallFrame::restart()
{
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> restartFunction = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, "restart")));
    v8::Debug::SetLiveEditEnabled(m_isolate, true);
    v8::MaybeLocal<v8::Value> result = m_client->callInternalFunction(restartFunction, callFrame, 0, nullptr);
    v8::Debug::SetLiveEditEnabled(m_isolate, false);
    return result;
}

v8::MaybeLocal<v8::Value> JavaScriptCallFrame::setVariableValue(int scopeNumber, v8::Local<v8::Value> variableName, v8::Local<v8::Value> newValue)
{
    v8::Local<v8::Object> callFrame = v8::Local<v8::Object>::New(m_isolate, m_callFrame);
    v8::Local<v8::Function> setVariableValueFunction = v8::Local<v8::Function>::Cast(callFrame->Get(toV8StringInternalized(m_isolate, "setVariableValue")));
    v8::Local<v8::Value> argv[] = {
        v8::Local<v8::Value>(v8::Integer::New(m_isolate, scopeNumber)),
        variableName,
        newValue
    };
    return m_client->callInternalFunction(setVariableValueFunction, callFrame, WTF_ARRAY_LENGTH(argv), argv);
}

v8::Local<v8::Object> JavaScriptCallFrame::createExceptionDetails(v8::Isolate* isolate, v8::Local<v8::Message> message)
{
    v8::Local<v8::Object> exceptionDetails = v8::Object::New(isolate);
    exceptionDetails->Set(v8::String::NewFromUtf8(isolate, "text"), message->Get());
    exceptionDetails->Set(v8::String::NewFromUtf8(isolate, "url"), message->GetScriptOrigin().ResourceName());
    exceptionDetails->Set(v8::String::NewFromUtf8(isolate, "scriptId"), v8::Integer::New(isolate, message->GetScriptOrigin().ScriptID()->Value()));
    exceptionDetails->Set(v8::String::NewFromUtf8(isolate, "line"), v8::Integer::New(isolate, message->GetLineNumber()));
    exceptionDetails->Set(v8::String::NewFromUtf8(isolate, "column"), v8::Integer::New(isolate, message->GetStartColumn()));
    if (!message->GetStackTrace().IsEmpty())
        exceptionDetails->Set(v8::String::NewFromUtf8(isolate, "stackTrace"), message->GetStackTrace()->AsArray());
    else
        exceptionDetails->Set(v8::String::NewFromUtf8(isolate, "stackTrace"), v8::Undefined(isolate));
    return exceptionDetails;
}

} // namespace blink
