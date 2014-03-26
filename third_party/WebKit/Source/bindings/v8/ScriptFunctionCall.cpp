/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/ScriptFunctionCall.h"

#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptScope.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8ScriptRunner.h"

#include <v8.h>

namespace WebCore {

void ScriptCallArgumentHandler::appendArgument(const ScriptObject& argument)
{
    if (argument.scriptState() != m_scriptState) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_arguments.append(argument);
}

void ScriptCallArgumentHandler::appendArgument(const ScriptValue& argument)
{
    m_arguments.append(argument);
}

void ScriptCallArgumentHandler::appendArgument(const String& argument)
{
    v8::Isolate* isolate = m_scriptState->isolate();
    ScriptScope scope(m_scriptState);
    m_arguments.append(ScriptValue(v8String(isolate, argument), isolate));
}

void ScriptCallArgumentHandler::appendArgument(const char* argument)
{
    v8::Isolate* isolate = m_scriptState->isolate();
    ScriptScope scope(m_scriptState);
    m_arguments.append(ScriptValue(v8String(isolate, argument), isolate));
}

void ScriptCallArgumentHandler::appendArgument(long argument)
{
    v8::Isolate* isolate = m_scriptState->isolate();
    ScriptScope scope(m_scriptState);
    m_arguments.append(ScriptValue(v8::Number::New(isolate, argument), isolate));
}

void ScriptCallArgumentHandler::appendArgument(long long argument)
{
    v8::Isolate* isolate = m_scriptState->isolate();
    ScriptScope scope(m_scriptState);
    m_arguments.append(ScriptValue(v8::Number::New(isolate, argument), isolate));
}

void ScriptCallArgumentHandler::appendArgument(unsigned argument)
{
    v8::Isolate* isolate = m_scriptState->isolate();
    ScriptScope scope(m_scriptState);
    m_arguments.append(ScriptValue(v8::Number::New(isolate, argument), isolate));
}

void ScriptCallArgumentHandler::appendArgument(unsigned long argument)
{
    v8::Isolate* isolate = m_scriptState->isolate();
    ScriptScope scope(m_scriptState);
    m_arguments.append(ScriptValue(v8::Number::New(isolate, argument), isolate));
}

void ScriptCallArgumentHandler::appendArgument(int argument)
{
    v8::Isolate* isolate = m_scriptState->isolate();
    ScriptScope scope(m_scriptState);
    m_arguments.append(ScriptValue(v8::Number::New(isolate, argument), isolate));
}

void ScriptCallArgumentHandler::appendArgument(bool argument)
{
    v8::Isolate* isolate = m_scriptState->isolate();
    m_arguments.append(ScriptValue(v8Boolean(argument, isolate), isolate));
}

void ScriptCallArgumentHandler::appendArgument(const Vector<ScriptValue>& argument)
{
    v8::Isolate* isolate = m_scriptState->isolate();
    ScriptScope scope(m_scriptState);
    v8::Handle<v8::Array> result = v8::Array::New(isolate, argument.size());
    for (size_t i = 0; i < argument.size(); ++i)
        result->Set(v8::Integer::New(isolate, i), argument[i].v8Value());
    m_arguments.append(ScriptValue(result, isolate));
}

ScriptFunctionCall::ScriptFunctionCall(const ScriptObject& thisObject, const String& name)
    : ScriptCallArgumentHandler(thisObject.scriptState())
    , m_thisObject(thisObject)
    , m_name(name)
{
}

ScriptValue ScriptFunctionCall::call(bool& hadException, bool reportExceptions)
{
    ScriptScope scope(m_scriptState, reportExceptions);

    v8::Handle<v8::Object> thisObject = m_thisObject.v8Object();
    v8::Local<v8::Value> value = thisObject->Get(v8String(m_scriptState->isolate(), m_name));
    if (!scope.success()) {
        hadException = true;
        return ScriptValue();
    }

    ASSERT(value->IsFunction());

    v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(value);
    OwnPtr<v8::Handle<v8::Value>[]> info = adoptArrayPtr(new v8::Handle<v8::Value>[m_arguments.size()]);
    for (size_t i = 0; i < m_arguments.size(); ++i) {
        info[i] = m_arguments[i].v8Value();
        ASSERT(!info[i].IsEmpty());
    }

    v8::Local<v8::Value> result = V8ScriptRunner::callFunction(function, currentExecutionContext(m_scriptState->isolate()), thisObject, m_arguments.size(), info.get(), m_scriptState->isolate());
    if (!scope.success()) {
        hadException = true;
        return ScriptValue();
    }

    return ScriptValue(result, m_scriptState->isolate());
}

ScriptValue ScriptFunctionCall::call()
{
    bool hadException = false;
    return call(hadException);
}

ScriptObject ScriptFunctionCall::construct(bool& hadException, bool reportExceptions)
{
    ScriptScope scope(m_scriptState, reportExceptions);

    v8::Handle<v8::Object> thisObject = m_thisObject.v8Object();
    v8::Local<v8::Value> value = thisObject->Get(v8String(m_scriptState->isolate(), m_name));
    if (!scope.success()) {
        hadException = true;
        return ScriptObject();
    }

    ASSERT(value->IsFunction());

    v8::Local<v8::Function> constructor = v8::Local<v8::Function>::Cast(value);
    OwnPtr<v8::Handle<v8::Value>[]> info = adoptArrayPtr(new v8::Handle<v8::Value>[m_arguments.size()]);
    for (size_t i = 0; i < m_arguments.size(); ++i)
        info[i] = m_arguments[i].v8Value();

    v8::Local<v8::Object> result = V8ObjectConstructor::newInstance(constructor, m_arguments.size(), info.get());
    if (!scope.success()) {
        hadException = true;
        return ScriptObject();
    }

    return ScriptObject(m_scriptState, result);
}

ScriptCallback::ScriptCallback(ScriptState* state, const ScriptValue& function)
    : ScriptCallArgumentHandler(state)
    , m_scriptState(state)
    , m_function(function)
{
}

ScriptValue ScriptCallback::call()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    ASSERT(isolate->InContext());
    ASSERT(m_function.v8Value()->IsFunction());

    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);
    v8::Handle<v8::Object> object = isolate->GetCurrentContext()->Global();
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(m_function.v8Value());

    OwnPtr<v8::Handle<v8::Value>[]> info = adoptArrayPtr(new v8::Handle<v8::Value>[m_arguments.size()]);
    for (size_t i = 0; i < m_arguments.size(); ++i)
        info[i] = m_arguments[i].v8Value();

    v8::Handle<v8::Value> result = ScriptController::callFunction(m_scriptState->executionContext(), function, object, m_arguments.size(), info.get(), m_scriptState->isolate());
    return ScriptValue(result, m_scriptState->isolate());
}

} // namespace WebCore
