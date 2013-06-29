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

#include "config.h"
#include "core/inspector/InspectorRuntimeAgent.h"

#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/platform/JSONValues.h"


#include "bindings/v8/ScriptDebugServer.h"

namespace WebCore {

static bool asBool(const bool* const b)
{
    return b ? *b : false;
}

InspectorRuntimeAgent::InspectorRuntimeAgent(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* state, InjectedScriptManager* injectedScriptManager, ScriptDebugServer* scriptDebugServer)
    : InspectorBaseAgent<InspectorRuntimeAgent>("Runtime", instrumentingAgents, state)
    , m_enabled(false)
    , m_injectedScriptManager(injectedScriptManager)
    , m_scriptDebugServer(scriptDebugServer)
{
}

InspectorRuntimeAgent::~InspectorRuntimeAgent()
{
}

static ScriptDebugServer::PauseOnExceptionsState setPauseOnExceptionsState(ScriptDebugServer* scriptDebugServer, ScriptDebugServer::PauseOnExceptionsState newState)
{
    ASSERT(scriptDebugServer);
    ScriptDebugServer::PauseOnExceptionsState presentState = scriptDebugServer->pauseOnExceptionsState();
    if (presentState != newState)
        scriptDebugServer->setPauseOnExceptionsState(newState);
    return presentState;
}

void InspectorRuntimeAgent::evaluate(ErrorString* errorString, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const doNotPauseOnExceptionsAndMuteConsole, const int* executionContextId, const bool* const returnByValue, const bool* generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    InjectedScript injectedScript = injectedScriptForEval(errorString, executionContextId);
    if (injectedScript.hasNoValue())
        return;
    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = ScriptDebugServer::DontPauseOnExceptions;
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        previousPauseOnExceptionsState = setPauseOnExceptionsState(m_scriptDebugServer, ScriptDebugServer::DontPauseOnExceptions);
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        muteConsole();

    injectedScript.evaluate(errorString, expression, objectGroup ? *objectGroup : "", asBool(includeCommandLineAPI), asBool(returnByValue), asBool(generatePreview), &result, wasThrown);

    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        unmuteConsole();
        setPauseOnExceptionsState(m_scriptDebugServer, previousPauseOnExceptionsState);
    }
}

void InspectorRuntimeAgent::callFunctionOn(ErrorString* errorString, const String& objectId, const String& expression, const RefPtr<JSONArray>* const optionalArguments, const bool* const doNotPauseOnExceptionsAndMuteConsole, const bool* const returnByValue, const bool* generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    String arguments;
    if (optionalArguments)
        arguments = (*optionalArguments)->toJSONString();

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = ScriptDebugServer::DontPauseOnExceptions;
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        previousPauseOnExceptionsState = setPauseOnExceptionsState(m_scriptDebugServer, ScriptDebugServer::DontPauseOnExceptions);
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        muteConsole();

    injectedScript.callFunctionOn(errorString, objectId, expression, arguments, asBool(returnByValue), asBool(generatePreview), &result, wasThrown);

    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        unmuteConsole();
        setPauseOnExceptionsState(m_scriptDebugServer, previousPauseOnExceptionsState);
    }
}

void InspectorRuntimeAgent::getProperties(ErrorString* errorString, const String& objectId, const bool* ownProperties, const bool* accessorPropertiesOnly, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::PropertyDescriptor> >& result, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::InternalPropertyDescriptor> >& internalProperties)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = setPauseOnExceptionsState(m_scriptDebugServer, ScriptDebugServer::DontPauseOnExceptions);
    muteConsole();

    bool accessorPropertiesOnlyValue = accessorPropertiesOnly && *accessorPropertiesOnly;
    injectedScript.getProperties(errorString, objectId, ownProperties && *ownProperties, accessorPropertiesOnlyValue, &result);

    if (!accessorPropertiesOnlyValue)
        injectedScript.getInternalProperties(errorString, objectId, &internalProperties);

    unmuteConsole();
    setPauseOnExceptionsState(m_scriptDebugServer, previousPauseOnExceptionsState);
}

void InspectorRuntimeAgent::releaseObject(ErrorString*, const String& objectId)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (!injectedScript.hasNoValue())
        injectedScript.releaseObject(objectId);
}

void InspectorRuntimeAgent::releaseObjectGroup(ErrorString*, const String& objectGroup)
{
    m_injectedScriptManager->releaseObjectGroup(objectGroup);
}

void InspectorRuntimeAgent::run(ErrorString*)
{
}

} // namespace WebCore

