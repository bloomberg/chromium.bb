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

#include "core/inspector/InspectorDebuggerAgent.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/inspector/AsyncCallTracker.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/MuteConsoleScope.h"
#include "core/inspector/ScriptAsyncCallStack.h"
#include "core/inspector/v8/V8Debugger.h"
#include "platform/ScriptForbiddenScope.h"

namespace blink {

namespace DebuggerAgentState {
static const char debuggerEnabled[] = "debuggerEnabled";
}

InspectorDebuggerAgent::InspectorDebuggerAgent(InjectedScriptManager* injectedScriptManager, V8Debugger* debugger, int contextGroupId)
    : InspectorBaseAgent<InspectorDebuggerAgent, InspectorFrontend::Debugger>("Debugger")
    , m_v8DebuggerAgent(V8DebuggerAgent::create(injectedScriptManager, debugger, contextGroupId))
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
    visitor->trace(m_asyncCallTracker);
    InspectorBaseAgent<InspectorDebuggerAgent, InspectorFrontend::Debugger>::trace(visitor);
}

// Protocol implementation.
void InspectorDebuggerAgent::enable(ErrorString* errorString)
{
    m_v8DebuggerAgent->enable(errorString);
    m_instrumentingAgents->setInspectorDebuggerAgent(this);
    m_state->setBoolean(DebuggerAgentState::debuggerEnabled, true);
}

void InspectorDebuggerAgent::disable(ErrorString* errorString)
{
    setTrackingAsyncCalls(false);
    m_state->setBoolean(DebuggerAgentState::debuggerEnabled, false);
    m_instrumentingAgents->setInspectorDebuggerAgent(nullptr);
    m_v8DebuggerAgent->disable(errorString);
}

void InspectorDebuggerAgent::setBreakpointsActive(ErrorString* errorString, bool inActive)
{
    m_v8DebuggerAgent->setBreakpointsActive(errorString, inActive);
}

void InspectorDebuggerAgent::setSkipAllPauses(ErrorString* errorString, bool inSkipped)
{
    m_v8DebuggerAgent->setSkipAllPauses(errorString, inSkipped);
}

void InspectorDebuggerAgent::setBreakpointByUrl(ErrorString* errorString, int inLineNumber, const String* inUrl, const String* inUrlRegex, const int* inColumnNumber, const String* inCondition, TypeBuilder::Debugger::BreakpointId* outBreakpointId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location>>& outLocations)
{
    m_v8DebuggerAgent->setBreakpointByUrl(errorString, inLineNumber, inUrl, inUrlRegex, inColumnNumber, inCondition, outBreakpointId, outLocations);
}

void InspectorDebuggerAgent::setBreakpoint(ErrorString* errorString, const RefPtr<JSONObject>& inLocation, const String* inCondition, TypeBuilder::Debugger::BreakpointId* outBreakpointId, RefPtr<TypeBuilder::Debugger::Location>& outActualLocation)
{
    m_v8DebuggerAgent->setBreakpoint(errorString, inLocation, inCondition, outBreakpointId, outActualLocation);
}

void InspectorDebuggerAgent::removeBreakpoint(ErrorString* errorString, const String& inBreakpointId)
{
    m_v8DebuggerAgent->removeBreakpoint(errorString, inBreakpointId);
}

void InspectorDebuggerAgent::continueToLocation(ErrorString* errorString, const RefPtr<JSONObject>& inLocation, const bool* inInterstatementLocation)
{
    m_v8DebuggerAgent->continueToLocation(errorString, inLocation, inInterstatementLocation);
}

void InspectorDebuggerAgent::stepOver(ErrorString* errorString)
{
    m_v8DebuggerAgent->stepOver(errorString);
}

void InspectorDebuggerAgent::stepInto(ErrorString* errorString)
{
    m_v8DebuggerAgent->stepInto(errorString);
}

void InspectorDebuggerAgent::stepOut(ErrorString* errorString)
{
    m_v8DebuggerAgent->stepOut(errorString);
}

void InspectorDebuggerAgent::pause(ErrorString* errorString)
{
    m_v8DebuggerAgent->pause(errorString);
}

void InspectorDebuggerAgent::resume(ErrorString* errorString)
{
    m_v8DebuggerAgent->resume(errorString);
}

void InspectorDebuggerAgent::stepIntoAsync(ErrorString* errorString)
{
    m_v8DebuggerAgent->stepIntoAsync(errorString);
}

void InspectorDebuggerAgent::searchInContent(ErrorString* errorString, const String& inScriptId, const String& inQuery, const bool* inCaseSensitive, const bool* inIsRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::SearchMatch>>& outResult)
{
    m_v8DebuggerAgent->searchInContent(errorString, inScriptId, inQuery, inCaseSensitive, inIsRegex, outResult);
}

void InspectorDebuggerAgent::canSetScriptSource(ErrorString* errorString, bool* outResult)
{
    m_v8DebuggerAgent->canSetScriptSource(errorString, outResult);
}

void InspectorDebuggerAgent::setScriptSource(ErrorString* errorString, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>& errorData, const String& inScriptId, const String& inScriptSource, const bool* inPreview, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& optOutCallFrames, TypeBuilder::OptOutput<bool>* optOutStackChanged, RefPtr<TypeBuilder::Debugger::StackTrace>& optOutAsyncStackTrace)
{
    m_v8DebuggerAgent->setScriptSource(errorString, errorData, inScriptId, inScriptSource, inPreview, optOutCallFrames, optOutStackChanged, optOutAsyncStackTrace);
}

void InspectorDebuggerAgent::restartFrame(ErrorString* errorString, const String& inCallFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& outCallFrames, RefPtr<TypeBuilder::Debugger::StackTrace>& optOutAsyncStackTrace)
{
    m_v8DebuggerAgent->restartFrame(errorString, inCallFrameId, outCallFrames, optOutAsyncStackTrace);
}

void InspectorDebuggerAgent::getScriptSource(ErrorString* errorString, const String& inScriptId, String* outScriptSource)
{
    m_v8DebuggerAgent->getScriptSource(errorString, inScriptId, outScriptSource);
}

void InspectorDebuggerAgent::getFunctionDetails(ErrorString* errorString, const String& inFunctionId, RefPtr<TypeBuilder::Debugger::FunctionDetails>& outDetails)
{
    m_v8DebuggerAgent->getFunctionDetails(errorString, inFunctionId, outDetails);
}

void InspectorDebuggerAgent::getGeneratorObjectDetails(ErrorString* errorString, const String& inObjectId, RefPtr<TypeBuilder::Debugger::GeneratorObjectDetails>& outDetails)
{
    m_v8DebuggerAgent->getGeneratorObjectDetails(errorString, inObjectId, outDetails);
}

void InspectorDebuggerAgent::getCollectionEntries(ErrorString* errorString, const String& inObjectId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CollectionEntry>>& outEntries)
{
    m_v8DebuggerAgent->getCollectionEntries(errorString, inObjectId, outEntries);
}

void InspectorDebuggerAgent::setPauseOnExceptions(ErrorString* errorString, const String& inState)
{
    m_v8DebuggerAgent->setPauseOnExceptions(errorString, inState);
}

void InspectorDebuggerAgent::evaluateOnCallFrame(ErrorString* errorString, const String& inCallFrameId, const String& inExpression, const String* inObjectGroup, const bool* inIncludeCommandLineAPI, const bool* inDoNotPauseOnExceptionsAndMuteConsole, const bool* inReturnByValue, const bool* inGeneratePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>& outResult, TypeBuilder::OptOutput<bool>* optOutWasThrown, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& optOutExceptionDetails)
{
    MuteConsoleScope<InspectorDebuggerAgent> muteScope;
    if (asBool(inDoNotPauseOnExceptionsAndMuteConsole))
        muteScope.enter(this);
    m_v8DebuggerAgent->evaluateOnCallFrame(errorString, inCallFrameId, inExpression, inObjectGroup, inIncludeCommandLineAPI, inDoNotPauseOnExceptionsAndMuteConsole, inReturnByValue, inGeneratePreview, outResult, optOutWasThrown, optOutExceptionDetails);
}

void InspectorDebuggerAgent::compileScript(ErrorString* errorString, const String& inExpression, const String& inSourceURL, bool inPersistScript, int inExecutionContextId, TypeBuilder::OptOutput<TypeBuilder::Debugger::ScriptId>* optOutScriptId, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& optOutExceptionDetails)
{
    m_v8DebuggerAgent->compileScript(errorString, inExpression, inSourceURL, inPersistScript, inExecutionContextId, optOutScriptId, optOutExceptionDetails);
}

void InspectorDebuggerAgent::runScript(ErrorString* errorString, const String& inScriptId, int inExecutionContextId, const String* inObjectGroup, const bool* inDoNotPauseOnExceptionsAndMuteConsole, RefPtr<TypeBuilder::Runtime::RemoteObject>& outResult, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& optOutExceptionDetails)
{
    MuteConsoleScope<InspectorDebuggerAgent> muteScope;
    if (asBool(inDoNotPauseOnExceptionsAndMuteConsole))
        muteScope.enter(this);
    m_v8DebuggerAgent->runScript(errorString, inScriptId, inExecutionContextId, inObjectGroup, inDoNotPauseOnExceptionsAndMuteConsole, outResult, optOutExceptionDetails);
}

void InspectorDebuggerAgent::setVariableValue(ErrorString* errorString, int inScopeNumber, const String& inVariableName, const RefPtr<JSONObject>& inNewValue, const String* inCallFrameId, const String* inFunctionObjectId)
{
    m_v8DebuggerAgent->setVariableValue(errorString, inScopeNumber, inVariableName, inNewValue, inCallFrameId, inFunctionObjectId);
}

void InspectorDebuggerAgent::getStepInPositions(ErrorString* errorString, const String& inCallFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location>>& optOutStepInPositions)
{
    m_v8DebuggerAgent->getStepInPositions(errorString, inCallFrameId, optOutStepInPositions);
}

void InspectorDebuggerAgent::getBacktrace(ErrorString* errorString, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& outCallFrames, RefPtr<TypeBuilder::Debugger::StackTrace>& optOutAsyncStackTrace)
{
    m_v8DebuggerAgent->getBacktrace(errorString, outCallFrames, optOutAsyncStackTrace);
}

void InspectorDebuggerAgent::skipStackFrames(ErrorString* errorString, const String* inScript, const bool* inSkipContentScripts)
{
    m_v8DebuggerAgent->skipStackFrames(errorString, inScript, inSkipContentScripts);
}

void InspectorDebuggerAgent::setAsyncCallStackDepth(ErrorString* errorString, int inMaxDepth)
{
    m_v8DebuggerAgent->setAsyncCallStackDepth(errorString, inMaxDepth);
    setTrackingAsyncCalls(m_v8DebuggerAgent->trackingAsyncCalls());
}

void InspectorDebuggerAgent::enablePromiseTracker(ErrorString* errorString, const bool* inCaptureStacks)
{
    m_v8DebuggerAgent->enablePromiseTracker(errorString, inCaptureStacks);
}

void InspectorDebuggerAgent::disablePromiseTracker(ErrorString* errorString)
{
    m_v8DebuggerAgent->disablePromiseTracker(errorString);
}

void InspectorDebuggerAgent::getPromiseById(ErrorString* errorString, int inPromiseId, const String* inObjectGroup, RefPtr<TypeBuilder::Runtime::RemoteObject>& outPromise)
{
    m_v8DebuggerAgent->getPromiseById(errorString, inPromiseId, inObjectGroup, outPromise);
}

void InspectorDebuggerAgent::flushAsyncOperationEvents(ErrorString* errorString)
{
    m_v8DebuggerAgent->flushAsyncOperationEvents(errorString);
}

void InspectorDebuggerAgent::setAsyncOperationBreakpoint(ErrorString* errorString, int inOperationId)
{
    m_v8DebuggerAgent->setAsyncOperationBreakpoint(errorString, inOperationId);
}

void InspectorDebuggerAgent::removeAsyncOperationBreakpoint(ErrorString* errorString, int inOperationId)
{
    m_v8DebuggerAgent->removeAsyncOperationBreakpoint(errorString, inOperationId);
}

bool InspectorDebuggerAgent::isPaused()
{
    return m_v8DebuggerAgent->isPaused();
}

PassRefPtrWillBeRawPtr<ScriptAsyncCallStack> InspectorDebuggerAgent::currentAsyncStackTraceForConsole()
{
    ScriptForbiddenScope::AllowUserAgentScript allowScripting;
    return m_v8DebuggerAgent->currentAsyncStackTraceForConsole();
}

void InspectorDebuggerAgent::scriptExecutionBlockedByCSP(const String& directiveText)
{
    if (m_v8DebuggerAgent->debugger().pauseOnExceptionsState() == V8Debugger::DontPauseOnExceptions)
        return;
    RefPtr<JSONObject> directive = JSONObject::create();
    directive->setString("directiveText", directiveText);
    m_v8DebuggerAgent->breakProgram(InspectorFrontend::Debugger::Reason::CSPViolation, directive.release());
}

void InspectorDebuggerAgent::willExecuteScript(int scriptId)
{
    m_v8DebuggerAgent->willExecuteScript(scriptId);
}

void InspectorDebuggerAgent::didExecuteScript()
{
    m_v8DebuggerAgent->didExecuteScript();
}

// InspectorBaseAgent overrides.
void InspectorDebuggerAgent::init()
{
    m_v8DebuggerAgent->setInspectorState(m_state);
    m_asyncCallTracker = adoptPtrWillBeNoop(new AsyncCallTracker(m_v8DebuggerAgent.get(), m_instrumentingAgents.get()));
}

void InspectorDebuggerAgent::setFrontend(InspectorFrontend* frontend)
{
    InspectorBaseAgent::setFrontend(frontend);
    m_v8DebuggerAgent->setFrontend(InspectorFrontend::Debugger::from(frontend));
}

void InspectorDebuggerAgent::clearFrontend()
{
    m_v8DebuggerAgent->clearFrontend();
    InspectorBaseAgent::clearFrontend();
}

void InspectorDebuggerAgent::restore()
{
    if (!m_state->getBoolean(DebuggerAgentState::debuggerEnabled))
        return;
    m_v8DebuggerAgent->restore();
    ErrorString errorString;
    enable(&errorString);
    setTrackingAsyncCalls(m_v8DebuggerAgent->trackingAsyncCalls());
}

void InspectorDebuggerAgent::setTrackingAsyncCalls(bool tracking)
{
    m_asyncCallTracker->asyncCallTrackingStateChanged(tracking);
    if (!tracking)
        m_asyncCallTracker->resetAsyncOperations();
}

} // namespace blink
