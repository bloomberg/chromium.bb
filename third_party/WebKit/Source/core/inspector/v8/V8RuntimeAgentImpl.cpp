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
#include "core/inspector/v8/InjectedScriptManager.h"
#include "core/inspector/v8/RemoteObjectId.h"
#include "core/inspector/v8/V8Debugger.h"
#include "core/inspector/v8/V8DebuggerImpl.h"
#include "platform/JSONValues.h"
#include "wtf/Optional.h"

using blink::TypeBuilder::Runtime::ExecutionContextDescription;

namespace blink {

namespace V8RuntimeAgentImplState {
static const char customObjectFormatterEnabled[] = "customObjectFormatterEnabled";
};

PassOwnPtr<V8RuntimeAgent> V8RuntimeAgent::create(InjectedScriptManager* injectedScriptManager, V8Debugger* debugger)
{
    return adoptPtr(new V8RuntimeAgentImpl(injectedScriptManager, static_cast<V8DebuggerImpl*>(debugger)));
}

V8RuntimeAgentImpl::V8RuntimeAgentImpl(InjectedScriptManager* injectedScriptManager, V8DebuggerImpl* debugger)
    : m_state(nullptr)
    , m_frontend(nullptr)
    , m_injectedScriptManager(injectedScriptManager)
    , m_debugger(debugger)
    , m_enabled(false)
{
}

V8RuntimeAgentImpl::~V8RuntimeAgentImpl()
{
}

void V8RuntimeAgentImpl::evaluate(ErrorString* errorString, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const doNotPauseOnExceptionsAndMuteConsole, const int* executionContextId, const bool* const returnByValue, const bool* generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& exceptionDetails)
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

void V8RuntimeAgentImpl::getProperties(ErrorString* errorString, const String& objectId, const bool* ownProperties, const bool* accessorPropertiesOnly, const bool* generatePreview, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::PropertyDescriptor>>& result, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::InternalPropertyDescriptor>>& internalProperties, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& exceptionDetails)
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

void V8RuntimeAgentImpl::isRunRequired(ErrorString*, bool* out_result)
{
    *out_result = false;
}

void V8RuntimeAgentImpl::setCustomObjectFormatterEnabled(ErrorString*, bool enabled)
{
    m_state->setBoolean(V8RuntimeAgentImplState::customObjectFormatterEnabled, enabled);
    injectedScriptManager()->setCustomObjectFormatterEnabled(enabled);
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
        injectedScriptManager()->setCustomObjectFormatterEnabled(true);
}

void V8RuntimeAgentImpl::enable(ErrorString* errorString)
{
    m_enabled = true;
}

void V8RuntimeAgentImpl::disable(ErrorString* errorString)
{
    m_enabled = false;
}

void V8RuntimeAgentImpl::reportExecutionContextCreated(v8::Local<v8::Context> context, const String& type, const String& origin, const String& humanReadableName, const String& frameId)
{
    if (!m_enabled)
        return;
    InjectedScript* injectedScript = injectedScriptManager()->injectedScriptFor(context);
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
    int contextId = injectedScriptManager()->discardInjectedScriptFor(context);
    if (m_enabled)
        m_frontend->executionContextDestroyed(contextId);
}

} // namespace blink
