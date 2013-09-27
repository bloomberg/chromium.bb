/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
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
#include "core/inspector/JavaScriptCallFrame.h"

#include "InspectorFrontend.h"
#include "bindings/v8/ScriptDebugServer.h"
#include "bindings/v8/ScriptObject.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "core/fetch/Resource.h"
#include "core/inspector/ContentSearchUtils.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/ScriptArguments.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/platform/JSONValues.h"
#include "core/platform/text/RegularExpression.h"
#include "wtf/text/WTFString.h"

using WebCore::TypeBuilder::Array;
using WebCore::TypeBuilder::Debugger::FunctionDetails;
using WebCore::TypeBuilder::Debugger::ScriptId;
using WebCore::TypeBuilder::Runtime::RemoteObject;

namespace WebCore {

namespace DebuggerAgentState {
static const char debuggerEnabled[] = "debuggerEnabled";
static const char javaScriptBreakpoints[] = "javaScriptBreakopints";
static const char pauseOnExceptionsState[] = "pauseOnExceptionsState";

// Breakpoint properties.
static const char url[] = "url";
static const char isRegex[] = "isRegex";
static const char lineNumber[] = "lineNumber";
static const char columnNumber[] = "columnNumber";
static const char condition[] = "condition";
static const char isAnti[] = "isAnti";
static const char skipStackPattern[] = "skipStackPattern";
static const char skipAllPauses[] = "skipAllPauses";
static const char skipAllPausesExpiresOnReload[] = "skipAllPausesExpiresOnReload";

};

static const int numberOfStepsBeforeStepOut = 10;

const char* InspectorDebuggerAgent::backtraceObjectGroup = "backtrace";

static String breakpointIdSuffix(InspectorDebuggerAgent::BreakpointSource source)
{
    switch (source) {
    case InspectorDebuggerAgent::UserBreakpointSource:
        break;
    case InspectorDebuggerAgent::DebugCommandBreakpointSource:
        return ":debug";
    case InspectorDebuggerAgent::MonitorCommandBreakpointSource:
        return ":monitor";
    }
    return String();
}

static String generateBreakpointId(const String& scriptId, int lineNumber, int columnNumber, InspectorDebuggerAgent::BreakpointSource source)
{
    return scriptId + ':' + String::number(lineNumber) + ':' + String::number(columnNumber) + breakpointIdSuffix(source);
}

InspectorDebuggerAgent::InspectorDebuggerAgent(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* inspectorState, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorDebuggerAgent>("Debugger", instrumentingAgents, inspectorState)
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
    , m_pausedScriptState(0)
    , m_javaScriptPauseScheduled(false)
    , m_listener(0)
    , m_skipStepInCount(numberOfStepsBeforeStepOut)
    , m_skipAllPauses(false)
{
    // FIXME: make breakReason optional so that there was no need to init it with "other".
    clearBreakDetails();
    m_state->setLong(DebuggerAgentState::pauseOnExceptionsState, ScriptDebugServer::DontPauseOnExceptions);
}

InspectorDebuggerAgent::~InspectorDebuggerAgent()
{
    ASSERT(!m_instrumentingAgents->inspectorDebuggerAgent());
}

void InspectorDebuggerAgent::enable()
{
    m_instrumentingAgents->setInspectorDebuggerAgent(this);

    // FIXME(WK44513): breakpoints activated flag should be synchronized between all front-ends
    scriptDebugServer().setBreakpointsActivated(true);
    startListeningScriptDebugServer();

    if (m_listener)
        m_listener->debuggerWasEnabled();
}

void InspectorDebuggerAgent::disable()
{
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, JSONObject::create());
    m_state->setLong(DebuggerAgentState::pauseOnExceptionsState, ScriptDebugServer::DontPauseOnExceptions);
    m_state->setString(DebuggerAgentState::skipStackPattern, "");
    m_instrumentingAgents->setInspectorDebuggerAgent(0);

    stopListeningScriptDebugServer();
    scriptDebugServer().clearBreakpoints();
    scriptDebugServer().clearCompiledScripts();
    clear();

    if (m_listener)
        m_listener->debuggerWasDisabled();

    m_skipAllPauses = false;
}

bool InspectorDebuggerAgent::enabled()
{
    return m_state->getBoolean(DebuggerAgentState::debuggerEnabled);
}

void InspectorDebuggerAgent::enable(ErrorString*)
{
    if (enabled())
        return;

    enable();
    m_state->setBoolean(DebuggerAgentState::debuggerEnabled, true);

    ASSERT(m_frontend);
}

void InspectorDebuggerAgent::disable(ErrorString*)
{
    if (!enabled())
        return;

    disable();
    m_state->setBoolean(DebuggerAgentState::debuggerEnabled, false);
}

static PassOwnPtr<RegularExpression> compileSkipCallFramePattern(String patternText)
{
    if (patternText.isEmpty())
        return nullptr;
    OwnPtr<RegularExpression> result = adoptPtr(new RegularExpression(patternText, TextCaseSensitive));
    if (!result->isValid())
        result.clear();
    return result.release();
}

void InspectorDebuggerAgent::restore()
{
    if (enabled()) {
        m_frontend->globalObjectCleared();
        enable();
        long pauseState = m_state->getLong(DebuggerAgentState::pauseOnExceptionsState);
        String error;
        setPauseOnExceptionsImpl(&error, pauseState);
        m_cachedSkipStackRegExp = compileSkipCallFramePattern(m_state->getString(DebuggerAgentState::skipStackPattern));
        m_skipAllPauses = m_state->getBoolean(DebuggerAgentState::skipAllPauses);
        if (m_skipAllPauses && m_state->getBoolean(DebuggerAgentState::skipAllPausesExpiresOnReload)) {
            m_skipAllPauses = false;
            m_state->setBoolean(DebuggerAgentState::skipAllPauses, false);
        }
    }
}

void InspectorDebuggerAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->debugger();
}

void InspectorDebuggerAgent::clearFrontend()
{
    m_frontend = 0;

    if (!enabled())
        return;

    disable();

    // FIXME: due to m_state->mute() hack in InspectorController, debuggerEnabled is actually set to false only
    // in InspectorState, but not in cookie. That's why after navigation debuggerEnabled will be true,
    // but after front-end re-open it will still be false.
    m_state->setBoolean(DebuggerAgentState::debuggerEnabled, false);
}

void InspectorDebuggerAgent::setBreakpointsActive(ErrorString*, bool active)
{
    scriptDebugServer().setBreakpointsActivated(active);
}

void InspectorDebuggerAgent::setSkipAllPauses(ErrorString*, bool skipped, const bool* untilReload)
{
    m_skipAllPauses = skipped;
    bool untilReloadValue = untilReload && *untilReload;
    m_state->setBoolean(DebuggerAgentState::skipAllPauses, m_skipAllPauses);
    m_state->setBoolean(DebuggerAgentState::skipAllPausesExpiresOnReload, untilReloadValue);
}

void InspectorDebuggerAgent::pageDidCommitLoad()
{
    if (m_state->getBoolean(DebuggerAgentState::skipAllPausesExpiresOnReload)) {
        m_skipAllPauses = false;
        m_state->setBoolean(DebuggerAgentState::skipAllPauses, m_skipAllPauses);
    }
}

bool InspectorDebuggerAgent::isPaused()
{
    return scriptDebugServer().isPaused();
}

bool InspectorDebuggerAgent::runningNestedMessageLoop()
{
    return scriptDebugServer().runningNestedMessageLoop();
}

void InspectorDebuggerAgent::addMessageToConsole(MessageSource source, MessageType type)
{
    if (source == ConsoleAPIMessageSource && type == AssertMessageType && scriptDebugServer().pauseOnExceptionsState() != ScriptDebugServer::DontPauseOnExceptions)
        breakProgram(InspectorFrontend::Debugger::Reason::Assert, 0);
}

void InspectorDebuggerAgent::addMessageToConsole(MessageSource source, MessageType type, MessageLevel, const String&, PassRefPtr<ScriptCallStack>, unsigned long)
{
    addMessageToConsole(source, type);
}

void InspectorDebuggerAgent::addMessageToConsole(MessageSource source, MessageType type, MessageLevel, const String&, ScriptState*, PassRefPtr<ScriptArguments>, unsigned long)
{
    addMessageToConsole(source, type);
}

String InspectorDebuggerAgent::preprocessEventListener(Frame* frame, const String& source, const String& url, const String& functionName)
{
    return scriptDebugServer().preprocessEventListener(frame, source, url, functionName);
}

PassOwnPtr<ScriptSourceCode> InspectorDebuggerAgent::preprocess(Frame* frame, const ScriptSourceCode& sourceCode)
{
    return scriptDebugServer().preprocess(frame, sourceCode);
}

static PassRefPtr<JSONObject> buildObjectForBreakpointCookie(const String& url, int lineNumber, int columnNumber, const String& condition, bool isRegex, bool isAnti)
{
    RefPtr<JSONObject> breakpointObject = JSONObject::create();
    breakpointObject->setString(DebuggerAgentState::url, url);
    breakpointObject->setNumber(DebuggerAgentState::lineNumber, lineNumber);
    breakpointObject->setNumber(DebuggerAgentState::columnNumber, columnNumber);
    breakpointObject->setString(DebuggerAgentState::condition, condition);
    breakpointObject->setBoolean(DebuggerAgentState::isRegex, isRegex);
    breakpointObject->setBoolean(DebuggerAgentState::isAnti, isAnti);
    return breakpointObject;
}

static bool matches(const String& url, const String& pattern, bool isRegex)
{
    if (isRegex) {
        RegularExpression regex(pattern, TextCaseSensitive);
        return regex.match(url) != -1;
    }
    return url == pattern;
}

void InspectorDebuggerAgent::setBreakpointByUrl(ErrorString* errorString, int lineNumber, const String* const optionalURL, const String* const optionalURLRegex, const int* const optionalColumnNumber, const String* const optionalCondition, const bool* isAntiBreakpoint, TypeBuilder::Debugger::BreakpointId* outBreakpointId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location> >& locations)
{
    locations = Array<TypeBuilder::Debugger::Location>::create();
    if (!optionalURL == !optionalURLRegex) {
        *errorString = "Either url or urlRegex must be specified.";
        return;
    }

    bool isAntiBreakpointValue = isAntiBreakpoint && *isAntiBreakpoint;

    String url = optionalURL ? *optionalURL : *optionalURLRegex;
    int columnNumber;
    if (optionalColumnNumber) {
        columnNumber = *optionalColumnNumber;
        if (columnNumber < 0) {
            *errorString = "Incorrect column number";
            return;
        }
    } else {
        columnNumber = isAntiBreakpointValue ? -1 : 0;
    }
    String condition = optionalCondition ? *optionalCondition : "";
    bool isRegex = optionalURLRegex;

    String breakpointId = (isRegex ? "/" + url + "/" : url) + ':' + String::number(lineNumber) + ':' + String::number(columnNumber);
    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    if (breakpointsCookie->find(breakpointId) != breakpointsCookie->end()) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }

    breakpointsCookie->setObject(breakpointId, buildObjectForBreakpointCookie(url, lineNumber, columnNumber, condition, isRegex, isAntiBreakpointValue));
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);

    if (!isAntiBreakpointValue) {
        ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
        for (ScriptsMap::iterator it = m_scripts.begin(); it != m_scripts.end(); ++it) {
            if (!matches(it->value.url, url, isRegex))
                continue;
            RefPtr<TypeBuilder::Debugger::Location> location = resolveBreakpoint(breakpointId, it->key, breakpoint, UserBreakpointSource);
            if (location)
                locations->addItem(location);
        }
    }
    *outBreakpointId = breakpointId;
}

static bool parseLocation(ErrorString* errorString, PassRefPtr<JSONObject> location, String* scriptId, int* lineNumber, int* columnNumber)
{
    if (!location->getString("scriptId", scriptId) || !location->getNumber("lineNumber", lineNumber)) {
        // FIXME: replace with input validation.
        *errorString = "scriptId and lineNumber are required.";
        return false;
    }
    *columnNumber = 0;
    location->getNumber("columnNumber", columnNumber);
    return true;
}

void InspectorDebuggerAgent::setBreakpoint(ErrorString* errorString, const RefPtr<JSONObject>& location, const String* const optionalCondition, TypeBuilder::Debugger::BreakpointId* outBreakpointId, RefPtr<TypeBuilder::Debugger::Location>& actualLocation)
{
    String scriptId;
    int lineNumber;
    int columnNumber;

    if (!parseLocation(errorString, location, &scriptId, &lineNumber, &columnNumber))
        return;

    String condition = optionalCondition ? *optionalCondition : emptyString();

    String breakpointId = generateBreakpointId(scriptId, lineNumber, columnNumber, UserBreakpointSource);
    if (m_breakpointIdToDebugServerBreakpointIds.find(breakpointId) != m_breakpointIdToDebugServerBreakpointIds.end()) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }
    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    actualLocation = resolveBreakpoint(breakpointId, scriptId, breakpoint, UserBreakpointSource);
    if (actualLocation)
        *outBreakpointId = breakpointId;
    else
        *errorString = "Could not resolve breakpoint";
}

void InspectorDebuggerAgent::removeBreakpoint(ErrorString*, const String& breakpointId)
{
    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    JSONObject::iterator it = breakpointsCookie->find(breakpointId);
    bool isAntibreakpoint = false;
    if (it != breakpointsCookie->end()) {
        RefPtr<JSONObject> breakpointObject = it->value->asObject();
        breakpointObject->getBoolean(DebuggerAgentState::isAnti, &isAntibreakpoint);
        breakpointsCookie->remove(breakpointId);
        m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);
    }

    if (!isAntibreakpoint)
        removeBreakpoint(breakpointId);
}

void InspectorDebuggerAgent::removeBreakpoint(const String& breakpointId)
{
    BreakpointIdToDebugServerBreakpointIdsMap::iterator debugServerBreakpointIdsIterator = m_breakpointIdToDebugServerBreakpointIds.find(breakpointId);
    if (debugServerBreakpointIdsIterator == m_breakpointIdToDebugServerBreakpointIds.end())
        return;
    for (size_t i = 0; i < debugServerBreakpointIdsIterator->value.size(); ++i) {
        const String& debugServerBreakpointId = debugServerBreakpointIdsIterator->value[i];
        scriptDebugServer().removeBreakpoint(debugServerBreakpointId);
        m_serverBreakpoints.remove(debugServerBreakpointId);
    }
    m_breakpointIdToDebugServerBreakpointIds.remove(debugServerBreakpointIdsIterator);
}

void InspectorDebuggerAgent::continueToLocation(ErrorString* errorString, const RefPtr<JSONObject>& location, const bool* interstateLocationOpt)
{
    bool interstateLocation = interstateLocationOpt ? *interstateLocationOpt : false;
    if (!m_continueToLocationBreakpointId.isEmpty()) {
        scriptDebugServer().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }

    String scriptId;
    int lineNumber;
    int columnNumber;

    if (!parseLocation(errorString, location, &scriptId, &lineNumber, &columnNumber))
        return;

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, "");
    m_continueToLocationBreakpointId = scriptDebugServer().setBreakpoint(scriptId, breakpoint, &lineNumber, &columnNumber, interstateLocation);
    resume(errorString);
}

void InspectorDebuggerAgent::getStepInPositions(ErrorString* errorString, const String& callFrameId, RefPtr<Array<TypeBuilder::Debugger::Location> >& positions)
{
    if (!isPaused() || m_currentCallStack.isNull()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(callFrameId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    injectedScript.getStepInPositions(errorString, m_currentCallStack, callFrameId, positions);
}

void InspectorDebuggerAgent::getBacktrace(ErrorString* errorString, RefPtr<Array<TypeBuilder::Debugger::CallFrame> >& callFrames)
{
    if (!assertPaused(errorString))
        return;
    scriptDebugServer().updateCallStack(&m_currentCallStack);
    callFrames = currentCallFrames();
}

String InspectorDebuggerAgent::scriptURL(JavaScriptCallFrame* frame)
{
    String scriptIdString = String::number(frame->sourceID());
    ScriptsMap::iterator it = m_scripts.find(scriptIdString);
    if (it == m_scripts.end())
        return String();
    return it->value.url;
}

ScriptDebugListener::SkipPauseRequest InspectorDebuggerAgent::shouldSkipExceptionPause(RefPtr<JavaScriptCallFrame>& topFrame)
{
    if (m_skipAllPauses)
        return ScriptDebugListener::Continue;

    String topFrameScriptUrl = scriptURL(topFrame.get());
    if (m_cachedSkipStackRegExp && !topFrameScriptUrl.isEmpty() && m_cachedSkipStackRegExp->match(topFrameScriptUrl) != -1)
        return ScriptDebugListener::Continue;

    // Prepare top frame parameters;
    int topFrameLineNumber = topFrame->line();
    int topFrameColumnNumber = topFrame->column();

    // Match against breakpoints.
    if (topFrameScriptUrl.isEmpty())
        return ScriptDebugListener::NoSkip;

    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    for (JSONObject::iterator it = breakpointsCookie->begin(); it != breakpointsCookie->end(); ++it) {
        RefPtr<JSONObject> breakpointObject = it->value->asObject();
        bool isAntibreakpoint;
        breakpointObject->getBoolean(DebuggerAgentState::isAnti, &isAntibreakpoint);
        if (!isAntibreakpoint)
            continue;

        int breakLineNumber;
        breakpointObject->getNumber(DebuggerAgentState::lineNumber, &breakLineNumber);
        int breakColumnNumber;
        breakpointObject->getNumber(DebuggerAgentState::columnNumber, &breakColumnNumber);

        if (breakLineNumber != topFrameLineNumber)
            continue;

        if (breakColumnNumber != -1 && breakColumnNumber != topFrameColumnNumber)
            continue;

        bool isRegex;
        breakpointObject->getBoolean(DebuggerAgentState::isRegex, &isRegex);
        String url;
        breakpointObject->getString(DebuggerAgentState::url, &url);
        if (!matches(topFrameScriptUrl, url, isRegex))
            continue;

        return ScriptDebugListener::Continue;
    }

    return ScriptDebugListener::NoSkip;
}

ScriptDebugListener::SkipPauseRequest InspectorDebuggerAgent::shouldSkipBreakpointPause(RefPtr<JavaScriptCallFrame>& topFrame)
{
    if (m_skipAllPauses)
        return ScriptDebugListener::Continue;
    return ScriptDebugListener::NoSkip;
}

ScriptDebugListener::SkipPauseRequest InspectorDebuggerAgent::shouldSkipStepPause(RefPtr<JavaScriptCallFrame>& topFrame)
{
    if (m_skipAllPauses)
        return ScriptDebugListener::Continue;

    if (m_cachedSkipStackRegExp) {
        String scriptUrl = scriptURL(topFrame.get());
        if (!scriptUrl.isEmpty() && m_cachedSkipStackRegExp->match(scriptUrl) != -1) {
            if (m_skipStepInCount > 0) {
                --m_skipStepInCount;
                return ScriptDebugListener::StepInto;
            }
            return ScriptDebugListener::StepOut;
        }
    }
    return ScriptDebugListener::NoSkip;
}

PassRefPtr<TypeBuilder::Debugger::Location> InspectorDebuggerAgent::resolveBreakpoint(const String& breakpointId, const String& scriptId, const ScriptBreakpoint& breakpoint, BreakpointSource source)
{
    ScriptsMap::iterator scriptIterator = m_scripts.find(scriptId);
    if (scriptIterator == m_scripts.end())
        return 0;
    Script& script = scriptIterator->value;
    if (breakpoint.lineNumber < script.startLine || script.endLine < breakpoint.lineNumber)
        return 0;

    int actualLineNumber;
    int actualColumnNumber;
    String debugServerBreakpointId = scriptDebugServer().setBreakpoint(scriptId, breakpoint, &actualLineNumber, &actualColumnNumber, false);
    if (debugServerBreakpointId.isEmpty())
        return 0;

    m_serverBreakpoints.set(debugServerBreakpointId, std::make_pair(breakpointId, source));

    BreakpointIdToDebugServerBreakpointIdsMap::iterator debugServerBreakpointIdsIterator = m_breakpointIdToDebugServerBreakpointIds.find(breakpointId);
    if (debugServerBreakpointIdsIterator == m_breakpointIdToDebugServerBreakpointIds.end())
        debugServerBreakpointIdsIterator = m_breakpointIdToDebugServerBreakpointIds.set(breakpointId, Vector<String>()).iterator;
    debugServerBreakpointIdsIterator->value.append(debugServerBreakpointId);

    RefPtr<TypeBuilder::Debugger::Location> location = TypeBuilder::Debugger::Location::create()
        .setScriptId(scriptId)
        .setLineNumber(actualLineNumber);
    location->setColumnNumber(actualColumnNumber);
    return location;
}

static PassRefPtr<JSONObject> scriptToInspectorObject(ScriptObject scriptObject)
{
    if (scriptObject.hasNoValue())
        return 0;
    RefPtr<JSONValue> value = scriptObject.toJSONValue(scriptObject.scriptState());
    if (!value)
        return 0;
    return value->asObject();
}

void InspectorDebuggerAgent::searchInContent(ErrorString* error, const String& scriptId, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<Array<WebCore::TypeBuilder::Page::SearchMatch> >& results)
{
    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;

    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it != m_scripts.end())
        results = ContentSearchUtils::searchInTextByLines(it->value.source, query, caseSensitive, isRegex);
    else
        *error = "No script for id: " + scriptId;
}

void InspectorDebuggerAgent::setScriptSource(ErrorString* error, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>& errorData, const String& scriptId, const String& newContent, const bool* const preview, RefPtr<Array<TypeBuilder::Debugger::CallFrame> >& newCallFrames, RefPtr<JSONObject>& result)
{
    bool previewOnly = preview && *preview;
    ScriptObject resultObject;
    if (!scriptDebugServer().setScriptSource(scriptId, newContent, previewOnly, error, errorData, &m_currentCallStack, &resultObject))
        return;
    newCallFrames = currentCallFrames();
    RefPtr<JSONObject> object = scriptToInspectorObject(resultObject);
    if (object)
        result = object;
}
void InspectorDebuggerAgent::restartFrame(ErrorString* errorString, const String& callFrameId, RefPtr<Array<TypeBuilder::Debugger::CallFrame> >& newCallFrames, RefPtr<JSONObject>& result)
{
    if (!isPaused() || m_currentCallStack.isNull()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(callFrameId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    injectedScript.restartFrame(errorString, m_currentCallStack, callFrameId, &result);
    scriptDebugServer().updateCallStack(&m_currentCallStack);
    newCallFrames = currentCallFrames();
}

void InspectorDebuggerAgent::getScriptSource(ErrorString* error, const String& scriptId, String* scriptSource)
{
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it != m_scripts.end())
        *scriptSource = it->value.source;
    else
        *error = "No script for id: " + scriptId;
}

void InspectorDebuggerAgent::getFunctionDetails(ErrorString* errorString, const String& functionId, RefPtr<TypeBuilder::Debugger::FunctionDetails>& details)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(functionId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Function object id is obsolete";
        return;
    }
    injectedScript.getFunctionDetails(errorString, functionId, &details);
}

void InspectorDebuggerAgent::schedulePauseOnNextStatement(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data)
{
    if (m_javaScriptPauseScheduled)
        return;
    m_breakReason = breakReason;
    m_breakAuxData = data;
    scriptDebugServer().setPauseOnNextStatement(true);
}

void InspectorDebuggerAgent::cancelPauseOnNextStatement()
{
    if (m_javaScriptPauseScheduled)
        return;
    clearBreakDetails();
    scriptDebugServer().setPauseOnNextStatement(false);
}

void InspectorDebuggerAgent::didFireTimer()
{
    cancelPauseOnNextStatement();
}

void InspectorDebuggerAgent::didHandleEvent()
{
    cancelPauseOnNextStatement();
}

void InspectorDebuggerAgent::pause(ErrorString*)
{
    if (m_javaScriptPauseScheduled)
        return;
    clearBreakDetails();
    scriptDebugServer().setPauseOnNextStatement(true);
    m_javaScriptPauseScheduled = true;
}

void InspectorDebuggerAgent::resume(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().continueProgram();
}

ScriptValue InspectorDebuggerAgent::resolveCallFrame(ErrorString* errorString, const String* callFrameId)
{
    if (!callFrameId)
        return ScriptValue();
    if (!isPaused() || m_currentCallStack.isNull()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return ScriptValue();
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(*callFrameId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return ScriptValue();
    }
    return injectedScript.findCallFrameById(errorString, m_currentCallStack, *callFrameId);
}

void InspectorDebuggerAgent::stepOver(ErrorString* errorString, const String* callFrameId)
{
    if (!assertPaused(errorString))
        return;
    ScriptValue frame = resolveCallFrame(errorString, callFrameId);
    if (!errorString->isEmpty())
        return;
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().stepOverStatement(frame);
}

void InspectorDebuggerAgent::stepInto(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().stepIntoStatement();
    if (m_listener)
        m_listener->stepInto();
}

void InspectorDebuggerAgent::stepOut(ErrorString* errorString, const String* callFrameId)
{
    if (!assertPaused(errorString))
        return;
    ScriptValue frame = resolveCallFrame(errorString, callFrameId);
    if (!errorString->isEmpty())
        return;
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().stepOutOfFunction(frame);
}

void InspectorDebuggerAgent::setPauseOnExceptions(ErrorString* errorString, const String& stringPauseState)
{
    ScriptDebugServer::PauseOnExceptionsState pauseState;
    if (stringPauseState == "none")
        pauseState = ScriptDebugServer::DontPauseOnExceptions;
    else if (stringPauseState == "all")
        pauseState = ScriptDebugServer::PauseOnAllExceptions;
    else if (stringPauseState == "uncaught")
        pauseState = ScriptDebugServer::PauseOnUncaughtExceptions;
    else {
        *errorString = "Unknown pause on exceptions mode: " + stringPauseState;
        return;
    }
    setPauseOnExceptionsImpl(errorString, pauseState);
}

void InspectorDebuggerAgent::setPauseOnExceptionsImpl(ErrorString* errorString, int pauseState)
{
    scriptDebugServer().setPauseOnExceptionsState(static_cast<ScriptDebugServer::PauseOnExceptionsState>(pauseState));
    if (scriptDebugServer().pauseOnExceptionsState() != pauseState)
        *errorString = "Internal error. Could not change pause on exceptions state";
    else
        m_state->setLong(DebuggerAgentState::pauseOnExceptionsState, pauseState);
}

void InspectorDebuggerAgent::evaluateOnCallFrame(ErrorString* errorString, const String& callFrameId, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const doNotPauseOnExceptionsAndMuteConsole, const bool* const returnByValue, const bool* generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    if (!isPaused() || m_currentCallStack.isNull()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(callFrameId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = scriptDebugServer().pauseOnExceptionsState();
    if (doNotPauseOnExceptionsAndMuteConsole ? *doNotPauseOnExceptionsAndMuteConsole : false) {
        if (previousPauseOnExceptionsState != ScriptDebugServer::DontPauseOnExceptions)
            scriptDebugServer().setPauseOnExceptionsState(ScriptDebugServer::DontPauseOnExceptions);
        muteConsole();
    }

    injectedScript.evaluateOnCallFrame(errorString, m_currentCallStack, callFrameId, expression, objectGroup ? *objectGroup : "", includeCommandLineAPI ? *includeCommandLineAPI : false, returnByValue ? *returnByValue : false, generatePreview ? *generatePreview : false, &result, wasThrown);

    if (doNotPauseOnExceptionsAndMuteConsole ? *doNotPauseOnExceptionsAndMuteConsole : false) {
        unmuteConsole();
        if (scriptDebugServer().pauseOnExceptionsState() != previousPauseOnExceptionsState)
            scriptDebugServer().setPauseOnExceptionsState(previousPauseOnExceptionsState);
    }
}

void InspectorDebuggerAgent::compileScript(ErrorString* errorString, const String& expression, const String& sourceURL, TypeBuilder::OptOutput<ScriptId>* scriptId, TypeBuilder::OptOutput<String>* syntaxErrorMessage)
{
    InjectedScript injectedScript = injectedScriptForEval(errorString, 0);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    String scriptIdValue;
    String exceptionMessage;
    scriptDebugServer().compileScript(injectedScript.scriptState(), expression, sourceURL, &scriptIdValue, &exceptionMessage);
    if (!scriptIdValue && !exceptionMessage) {
        *errorString = "Script compilation failed";
        return;
    }
    *syntaxErrorMessage = exceptionMessage;
    *scriptId = scriptIdValue;
}

void InspectorDebuggerAgent::runScript(ErrorString* errorString, const ScriptId& scriptId, const int* executionContextId, const String* const objectGroup, const bool* const doNotPauseOnExceptionsAndMuteConsole, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    InjectedScript injectedScript = injectedScriptForEval(errorString, executionContextId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = scriptDebugServer().pauseOnExceptionsState();
    if (doNotPauseOnExceptionsAndMuteConsole && *doNotPauseOnExceptionsAndMuteConsole) {
        if (previousPauseOnExceptionsState != ScriptDebugServer::DontPauseOnExceptions)
            scriptDebugServer().setPauseOnExceptionsState(ScriptDebugServer::DontPauseOnExceptions);
        muteConsole();
    }

    ScriptValue value;
    bool wasThrownValue;
    String exceptionMessage;
    scriptDebugServer().runScript(injectedScript.scriptState(), scriptId, &value, &wasThrownValue, &exceptionMessage);
    *wasThrown = wasThrownValue;
    if (value.hasNoValue()) {
        *errorString = "Script execution failed";
        return;
    }
    result = injectedScript.wrapObject(value, objectGroup ? *objectGroup : "");
    if (wasThrownValue)
        result->setDescription(exceptionMessage);

    if (doNotPauseOnExceptionsAndMuteConsole && *doNotPauseOnExceptionsAndMuteConsole) {
        unmuteConsole();
        if (scriptDebugServer().pauseOnExceptionsState() != previousPauseOnExceptionsState)
            scriptDebugServer().setPauseOnExceptionsState(previousPauseOnExceptionsState);
    }
}

void InspectorDebuggerAgent::setOverlayMessage(ErrorString*, const String*)
{
}

void InspectorDebuggerAgent::setVariableValue(ErrorString* errorString, int scopeNumber, const String& variableName, const RefPtr<JSONObject>& newValue, const String* callFrameId, const String* functionObjectId)
{
    InjectedScript injectedScript;
    if (callFrameId) {
        if (!isPaused() || m_currentCallStack.isNull()) {
            *errorString = "Attempt to access callframe when debugger is not on pause";
            return;
        }
        injectedScript = m_injectedScriptManager->injectedScriptForObjectId(*callFrameId);
        if (injectedScript.hasNoValue()) {
            *errorString = "Inspected frame has gone";
            return;
        }
    } else if (functionObjectId) {
        injectedScript = m_injectedScriptManager->injectedScriptForObjectId(*functionObjectId);
        if (injectedScript.hasNoValue()) {
            *errorString = "Function object id cannot be resolved";
            return;
        }
    } else {
        *errorString = "Either call frame or function object must be specified";
        return;
    }
    String newValueString = newValue->toJSONString();

    injectedScript.setVariableValue(errorString, m_currentCallStack, callFrameId, functionObjectId, scopeNumber, variableName, newValueString);
}

void InspectorDebuggerAgent::skipStackFrames(ErrorString* errorString, const String* pattern)
{
    OwnPtr<RegularExpression> compiled;
    String patternValue = pattern ? *pattern : "";
    if (!patternValue.isEmpty()) {
        compiled = compileSkipCallFramePattern(patternValue);
        if (!compiled) {
            *errorString = "Invalid regular expression";
            return;
        }
    }
    m_state->setString(DebuggerAgentState::skipStackPattern, patternValue);
    m_cachedSkipStackRegExp = compiled.release();
}

void InspectorDebuggerAgent::scriptExecutionBlockedByCSP(const String& directiveText)
{
    if (scriptDebugServer().pauseOnExceptionsState() != ScriptDebugServer::DontPauseOnExceptions) {
        RefPtr<JSONObject> directive = JSONObject::create();
        directive->setString("directiveText", directiveText);
        breakProgram(InspectorFrontend::Debugger::Reason::CSPViolation, directive.release());
    }
}

PassRefPtr<Array<TypeBuilder::Debugger::CallFrame> > InspectorDebuggerAgent::currentCallFrames()
{
    if (!m_pausedScriptState)
        return Array<TypeBuilder::Debugger::CallFrame>::create();
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(m_pausedScriptState);
    if (injectedScript.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return Array<TypeBuilder::Debugger::CallFrame>::create();
    }
    return injectedScript.wrapCallFrames(m_currentCallStack);
}

String InspectorDebuggerAgent::sourceMapURLForScript(const Script& script)
{
    bool deprecated;
    String sourceMapURL = ContentSearchUtils::findSourceMapURL(script.source, ContentSearchUtils::JavaScriptMagicComment, &deprecated);
    if (!sourceMapURL.isEmpty()) {
        // FIXME: add deprecated console message here.
        return sourceMapURL;
    }

    if (script.url.isEmpty())
        return String();

    InspectorPageAgent* pageAgent = m_instrumentingAgents->inspectorPageAgent();
    if (!pageAgent)
        return String();
    return pageAgent->resourceSourceMapURL(script.url);
}

// JavaScriptDebugListener functions

void InspectorDebuggerAgent::didParseSource(const String& scriptId, const Script& script)
{
    // Don't send script content to the front end until it's really needed.
    const bool* isContentScript = script.isContentScript ? &script.isContentScript : 0;
    String sourceMapURL = sourceMapURLForScript(script);
    String* sourceMapURLParam = sourceMapURL.isNull() ? 0 : &sourceMapURL;
    String sourceURL;
    if (!script.startLine && !script.startColumn) {
        bool deprecated;
        sourceURL = ContentSearchUtils::findSourceURL(script.source, ContentSearchUtils::JavaScriptMagicComment, &deprecated);
        // FIXME: add deprecated console message here.
    }
    bool hasSourceURL = !sourceURL.isEmpty();
    String scriptURL = hasSourceURL ? sourceURL : script.url;
    bool* hasSourceURLParam = hasSourceURL ? &hasSourceURL : 0;
    m_frontend->scriptParsed(scriptId, scriptURL, script.startLine, script.startColumn, script.endLine, script.endColumn, isContentScript, sourceMapURLParam, hasSourceURLParam);

    m_scripts.set(scriptId, script);

    if (scriptURL.isEmpty())
        return;

    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    for (JSONObject::iterator it = breakpointsCookie->begin(); it != breakpointsCookie->end(); ++it) {
        RefPtr<JSONObject> breakpointObject = it->value->asObject();
        bool isAntibreakpoint;
        breakpointObject->getBoolean(DebuggerAgentState::isAnti, &isAntibreakpoint);
        if (isAntibreakpoint)
            continue;
        bool isRegex;
        breakpointObject->getBoolean(DebuggerAgentState::isRegex, &isRegex);
        String url;
        breakpointObject->getString(DebuggerAgentState::url, &url);
        if (!matches(scriptURL, url, isRegex))
            continue;
        ScriptBreakpoint breakpoint;
        breakpointObject->getNumber(DebuggerAgentState::lineNumber, &breakpoint.lineNumber);
        breakpointObject->getNumber(DebuggerAgentState::columnNumber, &breakpoint.columnNumber);
        breakpointObject->getString(DebuggerAgentState::condition, &breakpoint.condition);
        RefPtr<TypeBuilder::Debugger::Location> location = resolveBreakpoint(it->key, scriptId, breakpoint, UserBreakpointSource);
        if (location)
            m_frontend->breakpointResolved(it->key, location);
    }
}

void InspectorDebuggerAgent::failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage)
{
    m_frontend->scriptFailedToParse(url, data, firstLine, errorLine, errorMessage);
}

void InspectorDebuggerAgent::didPause(ScriptState* scriptState, const ScriptValue& callFrames, const ScriptValue& exception, const Vector<String>& hitBreakpoints)
{
    ASSERT(scriptState && !m_pausedScriptState);
    m_pausedScriptState = scriptState;
    m_currentCallStack = callFrames;

    m_skipStepInCount = numberOfStepsBeforeStepOut;

    if (!exception.hasNoValue()) {
        InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(scriptState);
        if (!injectedScript.hasNoValue()) {
            m_breakReason = InspectorFrontend::Debugger::Reason::Exception;
            m_breakAuxData = injectedScript.wrapObject(exception, "backtrace")->openAccessors();
            // m_breakAuxData might be null after this.
        }
    }

    RefPtr<Array<String> > hitBreakpointIds = Array<String>::create();

    for (Vector<String>::const_iterator i = hitBreakpoints.begin(); i != hitBreakpoints.end(); ++i) {
        DebugServerBreakpointToBreakpointIdAndSourceMap::iterator breakpointIterator = m_serverBreakpoints.find(*i);
        if (breakpointIterator != m_serverBreakpoints.end()) {
            const String& localId = breakpointIterator->value.first;
            hitBreakpointIds->addItem(localId);

            BreakpointSource source = breakpointIterator->value.second;
            if (m_breakReason == InspectorFrontend::Debugger::Reason::Other && source == DebugCommandBreakpointSource)
                m_breakReason = InspectorFrontend::Debugger::Reason::DebugCommand;
        }
    }

    m_frontend->paused(currentCallFrames(), m_breakReason, m_breakAuxData, hitBreakpointIds);
    m_javaScriptPauseScheduled = false;

    if (!m_continueToLocationBreakpointId.isEmpty()) {
        scriptDebugServer().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }
    if (m_listener)
        m_listener->didPause();
}

void InspectorDebuggerAgent::didContinue()
{
    m_pausedScriptState = 0;
    m_currentCallStack = ScriptValue();
    clearBreakDetails();
    m_frontend->resumed();
}

bool InspectorDebuggerAgent::canBreakProgram()
{
    return scriptDebugServer().canBreakProgram();
}

void InspectorDebuggerAgent::breakProgram(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data)
{
    m_breakReason = breakReason;
    m_breakAuxData = data;
    scriptDebugServer().breakProgram();
}

void InspectorDebuggerAgent::clear()
{
    m_pausedScriptState = 0;
    m_currentCallStack = ScriptValue();
    m_scripts.clear();
    m_breakpointIdToDebugServerBreakpointIds.clear();
    m_continueToLocationBreakpointId = String();
    clearBreakDetails();
    m_javaScriptPauseScheduled = false;
    ErrorString error;
    setOverlayMessage(&error, 0);
}

bool InspectorDebuggerAgent::assertPaused(ErrorString* errorString)
{
    if (!m_pausedScriptState) {
        *errorString = "Can only perform operation while paused.";
        return false;
    }
    return true;
}

void InspectorDebuggerAgent::clearBreakDetails()
{
    m_breakReason = InspectorFrontend::Debugger::Reason::Other;
    m_breakAuxData = 0;
}

void InspectorDebuggerAgent::setBreakpoint(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource source, const String& condition)
{
    String breakpointId = generateBreakpointId(scriptId, lineNumber, columnNumber, source);
    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    resolveBreakpoint(breakpointId, scriptId, breakpoint, source);
}

void InspectorDebuggerAgent::removeBreakpoint(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource source)
{
    removeBreakpoint(generateBreakpointId(scriptId, lineNumber, columnNumber, source));
}

void InspectorDebuggerAgent::reset()
{
    m_scripts.clear();
    m_breakpointIdToDebugServerBreakpointIds.clear();
    if (m_frontend)
        m_frontend->globalObjectCleared();
}

} // namespace WebCore

