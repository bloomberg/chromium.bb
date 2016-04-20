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

#include "core/inspector/InspectorRuntimeAgent.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/UserGestureIndicator.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8RuntimeAgent.h"
#include "wtf/Optional.h"

namespace blink {

namespace InspectorRuntimeAgentState {
static const char runtimeEnabled[] = "runtimeEnabled";
};

InspectorRuntimeAgent::InspectorRuntimeAgent(V8RuntimeAgent* agent, Client* client)
    : InspectorBaseAgent<InspectorRuntimeAgent, protocol::Frontend::Runtime>("Runtime")
    , m_enabled(false)
    , m_v8RuntimeAgent(agent)
    , m_client(client)
{
}

InspectorRuntimeAgent::~InspectorRuntimeAgent()
{
}

// InspectorBaseAgent overrides.
void InspectorRuntimeAgent::init(InstrumentingAgents* instrumentingAgents, protocol::Frontend* baseFrontend, protocol::Dispatcher* dispatcher, protocol::DictionaryValue* state)
{
    InspectorBaseAgent::init(instrumentingAgents, baseFrontend, dispatcher, state);
    m_v8RuntimeAgent->setInspectorState(m_state);
    m_v8RuntimeAgent->setFrontend(frontend());
}

void InspectorRuntimeAgent::dispose()
{
    m_v8RuntimeAgent->clearFrontend();
    InspectorBaseAgent::dispose();
}

void InspectorRuntimeAgent::restore()
{
    if (!m_state->booleanProperty(InspectorRuntimeAgentState::runtimeEnabled, false))
        return;
    m_v8RuntimeAgent->restore();
    ErrorString errorString;
    enable(&errorString);
}

void InspectorRuntimeAgent::evaluate(ErrorString* errorString,
    const String16& expression,
    const Maybe<String16>& objectGroup,
    const Maybe<bool>& includeCommandLineAPI,
    const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
    const Maybe<int>& executionContextId,
    const Maybe<bool>& returnByValue,
    const Maybe<bool>& generatePreview,
    const Maybe<bool>& userGesture,
    OwnPtr<protocol::Runtime::RemoteObject>* result,
    Maybe<bool>* wasThrown,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    Optional<UserGestureIndicator> userGestureIndicator;
    if (userGesture.fromMaybe(false))
        userGestureIndicator.emplace(DefinitelyProcessingNewUserGesture);
    m_v8RuntimeAgent->evaluate(errorString, expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, executionContextId, returnByValue, generatePreview, userGesture, result, wasThrown, exceptionDetails);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data", InspectorUpdateCountersEvent::data());
}

void InspectorRuntimeAgent::callFunctionOn(ErrorString* errorString,
    const String16& objectId,
    const String16& expression,
    const Maybe<protocol::Array<protocol::Runtime::CallArgument>>& optionalArguments,
    const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
    const Maybe<bool>& returnByValue,
    const Maybe<bool>& generatePreview,
    const Maybe<bool>& userGesture,
    OwnPtr<protocol::Runtime::RemoteObject>* result,
    Maybe<bool>* wasThrown)
{
    Optional<UserGestureIndicator> userGestureIndicator;
    if (userGesture.fromMaybe(false))
        userGestureIndicator.emplace(DefinitelyProcessingNewUserGesture);
    m_v8RuntimeAgent->callFunctionOn(errorString, objectId, expression, optionalArguments, doNotPauseOnExceptionsAndMuteConsole, returnByValue, generatePreview, userGesture, result, wasThrown);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data", InspectorUpdateCountersEvent::data());
}

void InspectorRuntimeAgent::getProperties(ErrorString* errorString,
    const String16& objectId,
    const Maybe<bool>& ownProperties,
    const Maybe<bool>& accessorPropertiesOnly,
    const Maybe<bool>& generatePreview,
    OwnPtr<protocol::Array<protocol::Runtime::PropertyDescriptor>>* result,
    Maybe<protocol::Array<protocol::Runtime::InternalPropertyDescriptor>>* internalProperties,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    m_v8RuntimeAgent->getProperties(errorString, objectId, ownProperties, accessorPropertiesOnly, generatePreview, result, internalProperties, exceptionDetails);
}

void InspectorRuntimeAgent::releaseObject(ErrorString* errorString, const String16& objectId)
{
    m_v8RuntimeAgent->releaseObject(errorString, objectId);
}

void InspectorRuntimeAgent::releaseObjectGroup(ErrorString* errorString, const String16& objectGroup)
{
    m_v8RuntimeAgent->releaseObjectGroup(errorString, objectGroup);
}

void InspectorRuntimeAgent::run(ErrorString* errorString)
{
    m_client->resumeStartup();
}

void InspectorRuntimeAgent::setCustomObjectFormatterEnabled(ErrorString* errorString, bool enabled)
{
    m_v8RuntimeAgent->setCustomObjectFormatterEnabled(errorString, enabled);
}

void InspectorRuntimeAgent::compileScript(ErrorString* errorString,
    const String16& inExpression,
    const String16& inSourceURL,
    bool inPersistScript,
    int inExecutionContextId,
    Maybe<protocol::Runtime::ScriptId>* optOutScriptId,
    Maybe<protocol::Runtime::ExceptionDetails>* optOutExceptionDetails)
{
    m_v8RuntimeAgent->compileScript(errorString, inExpression, inSourceURL, inPersistScript, inExecutionContextId, optOutScriptId, optOutExceptionDetails);
}

void InspectorRuntimeAgent::runScript(ErrorString* errorString,
    const String16& inScriptId,
    int inExecutionContextId,
    const Maybe<String16>& inObjectGroup,
    const Maybe<bool>& inDoNotPauseOnExceptionsAndMuteConsole,
    const Maybe<bool>& includeCommandLineAPI,
    OwnPtr<protocol::Runtime::RemoteObject>* outResult,
    Maybe<protocol::Runtime::ExceptionDetails>* optOutExceptionDetails)
{
    m_v8RuntimeAgent->runScript(errorString, inScriptId, inExecutionContextId, inObjectGroup, inDoNotPauseOnExceptionsAndMuteConsole, includeCommandLineAPI, outResult, optOutExceptionDetails);
}

void InspectorRuntimeAgent::enable(ErrorString* errorString)
{
    if (m_enabled)
        return;

    m_enabled = true;
    m_state->setBoolean(InspectorRuntimeAgentState::runtimeEnabled, true);
    m_v8RuntimeAgent->enable(errorString);
}

void InspectorRuntimeAgent::disable(ErrorString* errorString)
{
    if (!m_enabled)
        return;

    m_enabled = false;
    m_state->setBoolean(InspectorRuntimeAgentState::runtimeEnabled, false);
    m_v8RuntimeAgent->disable(errorString);
}

} // namespace blink
