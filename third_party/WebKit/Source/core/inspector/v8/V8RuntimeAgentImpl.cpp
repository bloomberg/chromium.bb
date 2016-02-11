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

#include "core/inspector/v8/V8RuntimeAgentImpl.h"

#include "core/inspector/v8/IgnoreExceptionsScope.h"
#include "core/inspector/v8/InjectedScript.h"
#include "core/inspector/v8/InjectedScriptHost.h"
#include "core/inspector/v8/InjectedScriptManager.h"
#include "core/inspector/v8/RemoteObjectId.h"
#include "core/inspector/v8/V8DebuggerImpl.h"
#include "core/inspector/v8/V8StackTraceImpl.h"
#include "core/inspector/v8/V8StringUtil.h"
#include "core/inspector/v8/public/V8DebuggerClient.h"
#include "platform/JSONValues.h"
#include "wtf/Optional.h"

using blink::TypeBuilder::Runtime::ExceptionDetails;
using blink::TypeBuilder::Runtime::ExecutionContextDescription;
using blink::TypeBuilder::Runtime::RemoteObject;
using blink::TypeBuilder::Runtime::ScriptId;

namespace blink {

namespace V8RuntimeAgentImplState {
static const char customObjectFormatterEnabled[] = "customObjectFormatterEnabled";
};

inline static bool asBool(const bool* const b)
{
    return b ? *b : false;
}

PassOwnPtr<V8RuntimeAgent> V8RuntimeAgent::create(V8Debugger* debugger)
{
    return adoptPtr(new V8RuntimeAgentImpl(static_cast<V8DebuggerImpl*>(debugger)));
}

V8RuntimeAgentImpl::V8RuntimeAgentImpl(V8DebuggerImpl* debugger)
    : m_state(nullptr)
    , m_frontend(nullptr)
    , m_injectedScriptManager(InjectedScriptManager::create(debugger))
    , m_debugger(debugger)
    , m_enabled(false)
{
}

V8RuntimeAgentImpl::~V8RuntimeAgentImpl()
{
}

void V8RuntimeAgentImpl::evaluate(ErrorString* errorString, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const doNotPauseOnExceptionsAndMuteConsole, const int* executionContextId, const bool* const returnByValue, const bool* generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<TypeBuilder::Runtime::ExceptionDetails>& exceptionDetails)
{
    if (!executionContextId) {
        *errorString = "Cannot find default execution context";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(*executionContextId);
    if (!injectedScript) {
        *errorString = "Cannot find execution context with given id";
        return;
    }
    Optional<IgnoreExceptionsScope> ignoreExceptionsScope;
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        ignoreExceptionsScope.emplace(m_debugger);
    injectedScript->evaluate(errorString, expression, objectGroup ? *objectGroup : "", asBool(includeCommandLineAPI), asBool(returnByValue), asBool(generatePreview), &result, wasThrown, &exceptionDetails);
}

void V8RuntimeAgentImpl::callFunctionOn(ErrorString* errorString, const String& objectId, const String& expression, const RefPtr<JSONArray>* const optionalArguments, const bool* const doNotPauseOnExceptionsAndMuteConsole, const bool* const returnByValue, const bool* generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(objectId);
    if (!remoteId) {
        *errorString = "Invalid object id";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (!injectedScript) {
        *errorString = "Inspected frame has gone";
        return;
    }
    String arguments;
    if (optionalArguments)
        arguments = (*optionalArguments)->toJSONString();

    Optional<IgnoreExceptionsScope> ignoreExceptionsScope;
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        ignoreExceptionsScope.emplace(m_debugger);
    injectedScript->callFunctionOn(errorString, objectId, expression, arguments, asBool(returnByValue), asBool(generatePreview), &result, wasThrown);
}

void V8RuntimeAgentImpl::getProperties(ErrorString* errorString, const String& objectId, const bool* ownProperties, const bool* accessorPropertiesOnly, const bool* generatePreview, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::PropertyDescriptor>>& result, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::InternalPropertyDescriptor>>& internalProperties, RefPtr<TypeBuilder::Runtime::ExceptionDetails>& exceptionDetails)
{
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(objectId);
    if (!remoteId) {
        *errorString = "Invalid object id";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (!injectedScript) {
        *errorString = "Inspected frame has gone";
        return;
    }

    IgnoreExceptionsScope ignoreExceptionsScope(m_debugger);

    injectedScript->getProperties(errorString, objectId, asBool(ownProperties), asBool(accessorPropertiesOnly), asBool(generatePreview), &result, &exceptionDetails);

    if (!exceptionDetails && !asBool(accessorPropertiesOnly))
        injectedScript->getInternalProperties(errorString, objectId, &internalProperties, &exceptionDetails);
}

void V8RuntimeAgentImpl::releaseObject(ErrorString* errorString, const String& objectId)
{
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(objectId);
    if (!remoteId) {
        *errorString = "Invalid object id";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (!injectedScript)
        return;
    bool pausingOnNextStatement = m_debugger->pausingOnNextStatement();
    if (pausingOnNextStatement)
        m_debugger->setPauseOnNextStatement(false);
    injectedScript->releaseObject(objectId);
    if (pausingOnNextStatement)
        m_debugger->setPauseOnNextStatement(true);
}

void V8RuntimeAgentImpl::releaseObjectGroup(ErrorString*, const String& objectGroup)
{
    bool pausingOnNextStatement = m_debugger->pausingOnNextStatement();
    if (pausingOnNextStatement)
        m_debugger->setPauseOnNextStatement(false);
    m_injectedScriptManager->releaseObjectGroup(objectGroup);
    if (pausingOnNextStatement)
        m_debugger->setPauseOnNextStatement(true);
}

void V8RuntimeAgentImpl::run(ErrorString* errorString)
{
    *errorString = "Not paused";
}

void V8RuntimeAgentImpl::setCustomObjectFormatterEnabled(ErrorString*, bool enabled)
{
    m_state->setBoolean(V8RuntimeAgentImplState::customObjectFormatterEnabled, enabled);
    m_injectedScriptManager->setCustomObjectFormatterEnabled(enabled);
}

void V8RuntimeAgentImpl::compileScript(ErrorString* errorString, const String& expression, const String& sourceURL, bool persistScript, int executionContextId, TypeBuilder::OptOutput<ScriptId>* scriptId, RefPtr<ExceptionDetails>& exceptionDetails)
{
    if (!m_enabled) {
        *errorString = "Runtime agent is not enabled";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(executionContextId);
    if (!injectedScript) {
        *errorString = "Inspected frame has gone";
        return;
    }

    v8::Isolate* isolate = injectedScript->isolate();
    v8::HandleScope handles(isolate);
    v8::Context::Scope scope(injectedScript->context());
    v8::TryCatch tryCatch(isolate);
    v8::Local<v8::Script> script = m_debugger->compileInternalScript(injectedScript->context(), toV8String(isolate, expression), sourceURL);
    if (script.IsEmpty()) {
        v8::Local<v8::Message> message = tryCatch.Message();
        if (!message.IsEmpty())
            exceptionDetails = createExceptionDetails(isolate, message);
        else
            *errorString = "Script compilation failed";
        return;
    }

    if (!persistScript)
        return;

    String scriptValueId = String::number(script->GetUnboundScript()->GetId());
    OwnPtr<v8::Global<v8::Script>> global = adoptPtr(new v8::Global<v8::Script>(isolate, script));
    m_compiledScripts.set(scriptValueId, global.release());
    *scriptId = scriptValueId;
}

void V8RuntimeAgentImpl::runScript(ErrorString* errorString, const ScriptId& scriptId, int executionContextId, const String* const objectGroup, const bool* const doNotPauseOnExceptionsAndMuteConsole, RefPtr<RemoteObject>& result, RefPtr<ExceptionDetails>& exceptionDetails)
{
    if (!m_enabled) {
        *errorString = "Runtime agent is not enabled";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(executionContextId);
    if (!injectedScript) {
        *errorString = "Inspected frame has gone";
        return;
    }

    Optional<IgnoreExceptionsScope> ignoreExceptionsScope;
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        ignoreExceptionsScope.emplace(m_debugger);

    if (!m_compiledScripts.contains(scriptId)) {
        *errorString = "Script execution failed";
        return;
    }

    v8::Isolate* isolate = injectedScript->isolate();
    v8::HandleScope handles(isolate);
    v8::Local<v8::Context> context = injectedScript->context();
    v8::Context::Scope scope(context);
    OwnPtr<v8::Global<v8::Script>> scriptWrapper = m_compiledScripts.take(scriptId);
    v8::Local<v8::Script> script = scriptWrapper->Get(isolate);

    if (script.IsEmpty()) {
        *errorString = "Script execution failed";
        return;
    }
    v8::TryCatch tryCatch(isolate);
    v8::Local<v8::Value> value;
    v8::MaybeLocal<v8::Value> maybeValue = m_debugger->client()->runCompiledScript(context, script);
    if (maybeValue.IsEmpty()) {
        value = tryCatch.Exception();
        v8::Local<v8::Message> message = tryCatch.Message();
        if (!message.IsEmpty())
            exceptionDetails = createExceptionDetails(isolate, message);
    } else {
        value = maybeValue.ToLocalChecked();
    }

    if (value.IsEmpty()) {
        *errorString = "Script execution failed";
        return;
    }

    result = injectedScript->wrapObject(value, objectGroup ? *objectGroup : "");
}

void V8RuntimeAgentImpl::setInspectorState(PassRefPtr<JSONObject> state)
{
    m_state = state;
}

void V8RuntimeAgentImpl::setFrontend(InspectorFrontend::Runtime* frontend)
{
    m_frontend = frontend;
}

void V8RuntimeAgentImpl::clearFrontend()
{
    ErrorString error;
    disable(&error);
    ASSERT(m_frontend);
    m_frontend = nullptr;
}

void V8RuntimeAgentImpl::restore()
{
    m_frontend->executionContextsCleared();
    String error;
    enable(&error);
    if (m_state->booleanProperty(V8RuntimeAgentImplState::customObjectFormatterEnabled, false))
        m_injectedScriptManager->setCustomObjectFormatterEnabled(true);
}

void V8RuntimeAgentImpl::enable(ErrorString* errorString)
{
    m_enabled = true;
}

void V8RuntimeAgentImpl::disable(ErrorString* errorString)
{
    m_enabled = false;
    m_compiledScripts.clear();
    clearInspectedObjects();
    m_injectedScriptManager->discardInjectedScripts();
}

int V8RuntimeAgentImpl::ensureDefaultContextAvailable(v8::Local<v8::Context> context)
{
    InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(context);
    return injectedScript ? injectedScript->contextId() : 0;
}

void V8RuntimeAgentImpl::setClearConsoleCallback(PassOwnPtr<V8RuntimeAgent::ClearConsoleCallback> callback)
{
    m_injectedScriptManager->injectedScriptHost()->setClearConsoleCallback(callback);
}

void V8RuntimeAgentImpl::setInspectObjectCallback(PassOwnPtr<V8RuntimeAgent::InspectCallback> callback)
{
    m_injectedScriptManager->injectedScriptHost()->setInspectObjectCallback(callback);
}

PassRefPtr<TypeBuilder::Runtime::RemoteObject> V8RuntimeAgentImpl::wrapObject(v8::Local<v8::Context> context, v8::Local<v8::Value> value, const String& groupName, bool generatePreview)
{
    InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(context);
    if (!injectedScript)
        return nullptr;
    return injectedScript->wrapObject(value, groupName, generatePreview);
}

PassRefPtr<TypeBuilder::Runtime::RemoteObject> V8RuntimeAgentImpl::wrapTable(v8::Local<v8::Context> context, v8::Local<v8::Value> table, v8::Local<v8::Value> columns)
{
    InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(context);
    if (!injectedScript)
        return nullptr;
    return injectedScript->wrapTable(table, columns);
}

void V8RuntimeAgentImpl::disposeObjectGroup(const String& groupName)
{
    m_injectedScriptManager->releaseObjectGroup(groupName);
}

v8::Local<v8::Value> V8RuntimeAgentImpl::findObject(const String& objectId, v8::Local<v8::Context>* context, String* groupName)
{
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(objectId);
    if (!remoteId)
        return v8::Local<v8::Value>();
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(remoteId->contextId());
    if (!injectedScript)
        return v8::Local<v8::Value>();
    if (context)
        *context = injectedScript->context();
    if (groupName)
        *groupName = injectedScript->objectGroupName(*remoteId);
    return injectedScript->findObject(*remoteId);
}

void V8RuntimeAgentImpl::addInspectedObject(PassOwnPtr<Inspectable> inspectable)
{
    m_injectedScriptManager->injectedScriptHost()->addInspectedObject(inspectable);
}

void V8RuntimeAgentImpl::clearInspectedObjects()
{
    m_injectedScriptManager->injectedScriptHost()->clearInspectedObjects();
}

void V8RuntimeAgentImpl::reportExecutionContextCreated(v8::Local<v8::Context> context, const String& type, const String& origin, const String& humanReadableName, const String& frameId)
{
    if (!m_enabled)
        return;
    InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(context);
    if (!injectedScript)
        return;
    RefPtr<ExecutionContextDescription> description = ExecutionContextDescription::create()
        .setId(injectedScript->contextId())
        .setName(humanReadableName)
        .setOrigin(origin)
        .setFrameId(frameId);
    if (!type.isEmpty())
        description->setType(type);
    m_frontend->executionContextCreated(description.release());
}


void V8RuntimeAgentImpl::reportExecutionContextDestroyed(v8::Local<v8::Context> context)
{
    int contextId = m_injectedScriptManager->discardInjectedScriptFor(context);
    if (m_enabled)
        m_frontend->executionContextDestroyed(contextId);
}

PassRefPtr<TypeBuilder::Runtime::ExceptionDetails> V8RuntimeAgentImpl::createExceptionDetails(v8::Isolate* isolate, v8::Local<v8::Message> message)
{
    RefPtr<ExceptionDetails> exceptionDetails = ExceptionDetails::create().setText(toWTFStringWithTypeCheck(message->Get()));
    exceptionDetails->setLine(message->GetLineNumber());
    exceptionDetails->setColumn(message->GetStartColumn());
    v8::Local<v8::StackTrace> messageStackTrace = message->GetStackTrace();
    if (!messageStackTrace.IsEmpty() && messageStackTrace->GetFrameCount() > 0)
        exceptionDetails->setStack(m_debugger->createStackTrace(messageStackTrace, messageStackTrace->GetFrameCount())->buildInspectorObject());
    return exceptionDetails.release();
}

} // namespace blink
