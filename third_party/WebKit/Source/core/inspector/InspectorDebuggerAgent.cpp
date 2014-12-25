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

#include "config.h"
#include "core/inspector/InspectorDebuggerAgent.h"

#include "bindings/core/v8/ScriptDebugServer.h"
#include "bindings/core/v8/ScriptRegexp.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8RecursionScope.h"
#include "core/dom/Microtask.h"
#include "core/inspector/AsyncCallChain.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ContentSearchUtils.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/JavaScriptCallFrame.h"
#include "core/inspector/ScriptAsyncCallStack.h"
#include "core/inspector/ScriptCallFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/V8AsyncCallTracker.h"
#include "platform/JSONValues.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

using blink::TypeBuilder::Array;
using blink::TypeBuilder::Debugger::BreakpointId;
using blink::TypeBuilder::Debugger::CallFrame;
using blink::TypeBuilder::Debugger::CollectionEntry;
using blink::TypeBuilder::Debugger::ExceptionDetails;
using blink::TypeBuilder::Debugger::FunctionDetails;
using blink::TypeBuilder::Debugger::GeneratorObjectDetails;
using blink::TypeBuilder::Debugger::PromiseDetails;
using blink::TypeBuilder::Debugger::ScriptId;
using blink::TypeBuilder::Debugger::StackTrace;
using blink::TypeBuilder::Runtime::RemoteObject;

namespace blink {

namespace DebuggerAgentState {
static const char debuggerEnabled[] = "debuggerEnabled";
static const char javaScriptBreakpoints[] = "javaScriptBreakopints";
static const char pauseOnExceptionsState[] = "pauseOnExceptionsState";
static const char asyncCallStackDepth[] = "asyncCallStackDepth";
static const char promiseTrackerEnabled[] = "promiseTrackerEnabled";
static const char promiseTrackerCaptureStacks[] = "promiseTrackerCaptureStacks";

// Breakpoint properties.
static const char url[] = "url";
static const char isRegex[] = "isRegex";
static const char lineNumber[] = "lineNumber";
static const char columnNumber[] = "columnNumber";
static const char condition[] = "condition";
static const char skipStackPattern[] = "skipStackPattern";
static const char skipContentScripts[] = "skipContentScripts";
static const char skipAllPauses[] = "skipAllPauses";
static const char skipAllPausesExpiresOnReload[] = "skipAllPausesExpiresOnReload";

};

static const int maxSkipStepFrameCount = 128;

const char InspectorDebuggerAgent::backtraceObjectGroup[] = "backtrace";

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

InspectorDebuggerAgent::InspectorDebuggerAgent(InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorDebuggerAgent>("Debugger")
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
    , m_pausedScriptState(nullptr)
    , m_breakReason(InspectorFrontend::Debugger::Reason::Other)
    , m_scheduledDebuggerStep(NoStep)
    , m_skipNextDebuggerStepOut(false)
    , m_javaScriptPauseScheduled(false)
    , m_steppingFromFramework(false)
    , m_pausingOnNativeEvent(false)
    , m_inAsyncOperationForStepInto(false)
    , m_listener(nullptr)
    , m_skippedStepFrameCount(0)
    , m_recursionLevelForStepOut(0)
    , m_recursionLevelForStepFrame(0)
    , m_skipAllPauses(false)
    , m_skipContentScripts(false)
    , m_cachedSkipStackGeneration(0)
    , m_promiseTracker(PromiseTracker::create())
    , m_maxAsyncCallStackDepth(0)
    , m_currentAsyncCallChain(nullptr)
    , m_nestedAsyncCallCount(0)
    , m_performingAsyncStepIn(false)
{
    m_v8AsyncCallTracker = adoptPtrWillBeNoop(new V8AsyncCallTracker(this));
}

InspectorDebuggerAgent::~InspectorDebuggerAgent()
{
#if !ENABLE(OILPAN)
    ASSERT(!m_instrumentingAgents->inspectorDebuggerAgent());
#endif
}

void InspectorDebuggerAgent::init()
{
    // FIXME: make breakReason optional so that there was no need to init it with "other".
    clearBreakDetails();
    m_state->setLong(DebuggerAgentState::pauseOnExceptionsState, ScriptDebugServer::DontPauseOnExceptions);
}

void InspectorDebuggerAgent::enable()
{
    m_instrumentingAgents->setInspectorDebuggerAgent(this);

    startListeningScriptDebugServer();
    // FIXME(WK44513): breakpoints activated flag should be synchronized between all front-ends
    scriptDebugServer().setBreakpointsActivated(true);

    if (m_listener)
        m_listener->debuggerWasEnabled();
}

void InspectorDebuggerAgent::disable()
{
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, JSONObject::create());
    m_state->setLong(DebuggerAgentState::pauseOnExceptionsState, ScriptDebugServer::DontPauseOnExceptions);
    m_state->setString(DebuggerAgentState::skipStackPattern, "");
    m_state->setBoolean(DebuggerAgentState::skipContentScripts, false);
    m_state->setLong(DebuggerAgentState::asyncCallStackDepth, 0);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, false);
    m_instrumentingAgents->setInspectorDebuggerAgent(0);

    scriptDebugServer().clearBreakpoints();
    scriptDebugServer().clearCompiledScripts();
    scriptDebugServer().clearPreprocessor();
    stopListeningScriptDebugServer();
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

static PassOwnPtr<ScriptRegexp> compileSkipCallFramePattern(String patternText)
{
    if (patternText.isEmpty())
        return nullptr;
    OwnPtr<ScriptRegexp> result = adoptPtr(new ScriptRegexp(patternText, TextCaseSensitive));
    if (!result->isValid())
        result.clear();
    return result.release();
}

void InspectorDebuggerAgent::increaseCachedSkipStackGeneration()
{
    ++m_cachedSkipStackGeneration;
    if (!m_cachedSkipStackGeneration)
        m_cachedSkipStackGeneration = 1;
}

void InspectorDebuggerAgent::internalSetAsyncCallStackDepth(int depth)
{
    if (depth <= 0) {
        m_maxAsyncCallStackDepth = 0;
        resetAsyncCallTracker();
    } else {
        m_maxAsyncCallStackDepth = depth;
    }
    for (auto& listener: m_asyncCallTrackingListeners)
        listener->asyncCallTrackingStateChanged(m_maxAsyncCallStackDepth);
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
        increaseCachedSkipStackGeneration();
        m_skipContentScripts = m_state->getBoolean(DebuggerAgentState::skipContentScripts);
        m_skipAllPauses = m_state->getBoolean(DebuggerAgentState::skipAllPauses);
        if (m_skipAllPauses && m_state->getBoolean(DebuggerAgentState::skipAllPausesExpiresOnReload)) {
            m_skipAllPauses = false;
            m_state->setBoolean(DebuggerAgentState::skipAllPauses, false);
        }
        internalSetAsyncCallStackDepth(m_state->getLong(DebuggerAgentState::asyncCallStackDepth));
        promiseTracker().setEnabled(m_state->getBoolean(DebuggerAgentState::promiseTrackerEnabled), m_state->getBoolean(DebuggerAgentState::promiseTrackerCaptureStacks));
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
    m_state->setBoolean(DebuggerAgentState::skipAllPauses, m_skipAllPauses);
    m_state->setBoolean(DebuggerAgentState::skipAllPausesExpiresOnReload, asBool(untilReload));
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

void InspectorDebuggerAgent::addMessageToConsole(ConsoleMessage* consoleMessage)
{
    if (consoleMessage->type() == AssertMessageType && scriptDebugServer().pauseOnExceptionsState() != ScriptDebugServer::DontPauseOnExceptions)
        breakProgram(InspectorFrontend::Debugger::Reason::Assert, nullptr);
}

String InspectorDebuggerAgent::preprocessEventListener(LocalFrame* frame, const String& source, const String& url, const String& functionName)
{
    return scriptDebugServer().preprocessEventListener(frame, source, url, functionName);
}

PassOwnPtr<ScriptSourceCode> InspectorDebuggerAgent::preprocess(LocalFrame* frame, const ScriptSourceCode& sourceCode)
{
    return scriptDebugServer().preprocess(frame, sourceCode);
}

static PassRefPtr<JSONObject> buildObjectForBreakpointCookie(const String& url, int lineNumber, int columnNumber, const String& condition, bool isRegex)
{
    RefPtr<JSONObject> breakpointObject = JSONObject::create();
    breakpointObject->setString(DebuggerAgentState::url, url);
    breakpointObject->setNumber(DebuggerAgentState::lineNumber, lineNumber);
    breakpointObject->setNumber(DebuggerAgentState::columnNumber, columnNumber);
    breakpointObject->setString(DebuggerAgentState::condition, condition);
    breakpointObject->setBoolean(DebuggerAgentState::isRegex, isRegex);
    return breakpointObject;
}

static bool matches(const String& url, const String& pattern, bool isRegex)
{
    if (isRegex) {
        ScriptRegexp regex(pattern, TextCaseSensitive);
        return regex.match(url) != -1;
    }
    return url == pattern;
}

void InspectorDebuggerAgent::setBreakpointByUrl(ErrorString* errorString, int lineNumber, const String* const optionalURL, const String* const optionalURLRegex, const int* const optionalColumnNumber, const String* const optionalCondition, BreakpointId* outBreakpointId, RefPtr<Array<TypeBuilder::Debugger::Location> >& locations)
{
    locations = Array<TypeBuilder::Debugger::Location>::create();
    if (!optionalURL == !optionalURLRegex) {
        *errorString = "Either url or urlRegex must be specified.";
        return;
    }

    String url = optionalURL ? *optionalURL : *optionalURLRegex;
    int columnNumber = 0;
    if (optionalColumnNumber) {
        columnNumber = *optionalColumnNumber;
        if (columnNumber < 0) {
            *errorString = "Incorrect column number";
            return;
        }
    }
    String condition = optionalCondition ? *optionalCondition : "";
    bool isRegex = optionalURLRegex;

    String breakpointId = (isRegex ? "/" + url + "/" : url) + ':' + String::number(lineNumber) + ':' + String::number(columnNumber);
    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    if (breakpointsCookie->find(breakpointId) != breakpointsCookie->end()) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }

    breakpointsCookie->setObject(breakpointId, buildObjectForBreakpointCookie(url, lineNumber, columnNumber, condition, isRegex));
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    for (auto& script : m_scripts) {
        if (!matches(script.value.sourceURL(), url, isRegex))
            continue;
        RefPtr<TypeBuilder::Debugger::Location> location = resolveBreakpoint(breakpointId, script.key, breakpoint, UserBreakpointSource);
        if (location)
            locations->addItem(location);
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

void InspectorDebuggerAgent::setBreakpoint(ErrorString* errorString, const RefPtr<JSONObject>& location, const String* const optionalCondition, BreakpointId* outBreakpointId, RefPtr<TypeBuilder::Debugger::Location>& actualLocation)
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
    breakpointsCookie->remove(breakpointId);
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);
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
    m_continueToLocationBreakpointId = scriptDebugServer().setBreakpoint(scriptId, breakpoint, &lineNumber, &columnNumber, asBool(interstateLocationOpt));
    resume(errorString);
}

void InspectorDebuggerAgent::getStepInPositions(ErrorString* errorString, const String& callFrameId, RefPtr<Array<TypeBuilder::Debugger::Location> >& positions)
{
    if (!isPaused() || m_currentCallStack.isEmpty()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(callFrameId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    injectedScript.getStepInPositions(errorString, m_currentCallStack, callFrameId, positions);
}

void InspectorDebuggerAgent::getBacktrace(ErrorString* errorString, RefPtr<Array<CallFrame> >& callFrames, RefPtr<StackTrace>& asyncStackTrace)
{
    if (!assertPaused(errorString))
        return;
    m_currentCallStack = scriptDebugServer().currentCallFrames();
    callFrames = currentCallFrames();
    asyncStackTrace = currentAsyncStackTrace();
}

bool InspectorDebuggerAgent::isCallStackEmptyOrBlackboxed()
{
    String scriptURL;
    bool isBlackboxed = false;
    for (int index = 0; topCallFrameSkipUnknownSources(&scriptURL, &isBlackboxed, &index); ++index) {
        if (!isBlackboxed)
            return false;
    }
    return true;
}

PassRefPtrWillBeRawPtr<JavaScriptCallFrame> InspectorDebuggerAgent::topCallFrameSkipUnknownSources(String* scriptURL, bool* isBlackboxed, int* index)
{
    for (int i = index ? *index : 0; ; ++i) {
        if (index)
            *index = i;
        RefPtrWillBeRawPtr<JavaScriptCallFrame> frame = scriptDebugServer().callFrameNoScopes(i);
        if (!frame)
            return nullptr;
        ScriptsMap::iterator it = m_scripts.find(String::number(frame->sourceID()));
        if (it == m_scripts.end())
            continue;
        *scriptURL = it->value.sourceURL();
        if (m_skipContentScripts && it->value.isContentScript()) {
            *isBlackboxed = true;
        } else if (m_cachedSkipStackRegExp && !scriptURL->isEmpty()) {
            if (!it->value.getBlackboxedState(m_cachedSkipStackGeneration, isBlackboxed)) {
                *isBlackboxed = m_cachedSkipStackRegExp->match(*scriptURL) != -1;
                it->value.setBlackboxedState(m_cachedSkipStackGeneration, *isBlackboxed);
            }
        } else {
            *isBlackboxed = false;
        }
        return frame.release();
    }
}

ScriptDebugListener::SkipPauseRequest InspectorDebuggerAgent::shouldSkipExceptionPause()
{
    if (m_steppingFromFramework)
        return ScriptDebugListener::NoSkip;
    // Fast return.
    if (!m_skipContentScripts && !m_cachedSkipStackRegExp)
        return ScriptDebugListener::NoSkip;

    String scriptUrl;
    bool isBlackboxed = false;
    RefPtrWillBeRawPtr<JavaScriptCallFrame> topFrame = topCallFrameSkipUnknownSources(&scriptUrl, &isBlackboxed);
    if (topFrame && isBlackboxed)
        return ScriptDebugListener::Continue;

    return ScriptDebugListener::NoSkip;
}

ScriptDebugListener::SkipPauseRequest InspectorDebuggerAgent::shouldSkipStepPause()
{
    if (m_steppingFromFramework)
        return ScriptDebugListener::NoSkip;
    // Fast return.
    if (!m_skipContentScripts && !m_cachedSkipStackRegExp)
        return ScriptDebugListener::NoSkip;

    if (m_skipNextDebuggerStepOut) {
        m_skipNextDebuggerStepOut = false;
        if (m_scheduledDebuggerStep == StepOut)
            return ScriptDebugListener::StepOut;
    }

    String scriptUrl;
    bool isBlackboxed = false;
    RefPtrWillBeRawPtr<JavaScriptCallFrame> topFrame = topCallFrameSkipUnknownSources(&scriptUrl, &isBlackboxed);
    if (!topFrame || !isBlackboxed)
        return ScriptDebugListener::NoSkip;

    if (m_skippedStepFrameCount >= maxSkipStepFrameCount)
        return ScriptDebugListener::StepOut;

    if (!m_skippedStepFrameCount)
        m_recursionLevelForStepFrame = 1;

    ++m_skippedStepFrameCount;
    return ScriptDebugListener::StepFrame;
}

bool InspectorDebuggerAgent::isTopCallFrameInFramework()
{
    if (!m_skipContentScripts && !m_cachedSkipStackRegExp)
        return false;

    String scriptUrl;
    bool isBlackboxed = false;
    RefPtrWillBeRawPtr<JavaScriptCallFrame> topFrame = topCallFrameSkipUnknownSources(&scriptUrl, &isBlackboxed);
    return topFrame && isBlackboxed;
}

PassRefPtr<TypeBuilder::Debugger::Location> InspectorDebuggerAgent::resolveBreakpoint(const String& breakpointId, const String& scriptId, const ScriptBreakpoint& breakpoint, BreakpointSource source)
{
    ScriptsMap::iterator scriptIterator = m_scripts.find(scriptId);
    if (scriptIterator == m_scripts.end())
        return nullptr;
    Script& script = scriptIterator->value;
    if (breakpoint.lineNumber < script.startLine() || script.endLine() < breakpoint.lineNumber)
        return nullptr;

    int actualLineNumber;
    int actualColumnNumber;
    String debugServerBreakpointId = scriptDebugServer().setBreakpoint(scriptId, breakpoint, &actualLineNumber, &actualColumnNumber, false);
    if (debugServerBreakpointId.isEmpty())
        return nullptr;

    m_serverBreakpoints.set(debugServerBreakpointId, std::make_pair(breakpointId, source));

    BreakpointIdToDebugServerBreakpointIdsMap::iterator debugServerBreakpointIdsIterator = m_breakpointIdToDebugServerBreakpointIds.find(breakpointId);
    if (debugServerBreakpointIdsIterator == m_breakpointIdToDebugServerBreakpointIds.end())
        m_breakpointIdToDebugServerBreakpointIds.set(breakpointId, Vector<String>()).storedValue->value.append(debugServerBreakpointId);
    else
        debugServerBreakpointIdsIterator->value.append(debugServerBreakpointId);

    RefPtr<TypeBuilder::Debugger::Location> location = TypeBuilder::Debugger::Location::create()
        .setScriptId(scriptId)
        .setLineNumber(actualLineNumber);
    location->setColumnNumber(actualColumnNumber);
    return location;
}

void InspectorDebuggerAgent::searchInContent(ErrorString* error, const String& scriptId, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<Array<blink::TypeBuilder::Page::SearchMatch> >& results)
{
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it != m_scripts.end())
        results = ContentSearchUtils::searchInTextByLines(it->value.source(), query, asBool(optionalCaseSensitive), asBool(optionalIsRegex));
    else
        *error = "No script for id: " + scriptId;
}

void InspectorDebuggerAgent::setScriptSource(ErrorString* error, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>& errorData, const String& scriptId, const String& newContent, const bool* const preview, RefPtr<Array<CallFrame> >& newCallFrames, RefPtr<JSONObject>& result, RefPtr<StackTrace>& asyncStackTrace)
{
    if (!scriptDebugServer().setScriptSource(scriptId, newContent, asBool(preview), error, errorData, &m_currentCallStack, &result))
        return;

    newCallFrames = currentCallFrames();
    asyncStackTrace = currentAsyncStackTrace();

    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it == m_scripts.end())
        return;
    String url = it->value.url();
    if (url.isEmpty())
        return;
    if (InspectorPageAgent* pageAgent = m_instrumentingAgents->inspectorPageAgent())
        pageAgent->addEditedResourceContent(url, newContent);
}

void InspectorDebuggerAgent::restartFrame(ErrorString* errorString, const String& callFrameId, RefPtr<Array<CallFrame> >& newCallFrames, RefPtr<JSONObject>& result, RefPtr<StackTrace>& asyncStackTrace)
{
    if (!isPaused() || m_currentCallStack.isEmpty()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(callFrameId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    injectedScript.restartFrame(errorString, m_currentCallStack, callFrameId, &result);
    m_currentCallStack = scriptDebugServer().currentCallFrames();
    newCallFrames = currentCallFrames();
    asyncStackTrace = currentAsyncStackTrace();
}

void InspectorDebuggerAgent::getScriptSource(ErrorString* error, const String& scriptId, String* scriptSource)
{
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it == m_scripts.end()) {
        *error = "No script for id: " + scriptId;
        return;
    }

    String url = it->value.url();
    if (!url.isEmpty()) {
        if (InspectorPageAgent* pageAgent = m_instrumentingAgents->inspectorPageAgent()) {
            bool success = pageAgent->getEditedResourceContent(url, scriptSource);
            if (success)
                return;
        }
    }
    *scriptSource = it->value.source();
}

void InspectorDebuggerAgent::getFunctionDetails(ErrorString* errorString, const String& functionId, RefPtr<FunctionDetails>& details)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(functionId);
    if (injectedScript.isEmpty()) {
        *errorString = "Function object id is obsolete";
        return;
    }
    injectedScript.getFunctionDetails(errorString, functionId, &details);
}

void InspectorDebuggerAgent::getGeneratorObjectDetails(ErrorString* errorString, const String& objectId, RefPtr<GeneratorObjectDetails>& details)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    injectedScript.getGeneratorObjectDetails(errorString, objectId, &details);
}

void InspectorDebuggerAgent::getCollectionEntries(ErrorString* errorString, const String& objectId, RefPtr<TypeBuilder::Array<CollectionEntry> >& entries)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    injectedScript.getCollectionEntries(errorString, objectId, &entries);
}

void InspectorDebuggerAgent::schedulePauseOnNextStatement(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data)
{
    if (m_scheduledDebuggerStep == StepInto || m_javaScriptPauseScheduled || isPaused())
        return;
    m_breakReason = breakReason;
    m_breakAuxData = data;
    m_pausingOnNativeEvent = true;
    m_skipNextDebuggerStepOut = false;
    scriptDebugServer().setPauseOnNextStatement(true);
}

void InspectorDebuggerAgent::schedulePauseOnNextStatementIfSteppingInto()
{
    if (m_scheduledDebuggerStep != StepInto || m_javaScriptPauseScheduled || isPaused())
        return;
    clearBreakDetails();
    m_pausingOnNativeEvent = false;
    m_skippedStepFrameCount = 0;
    m_recursionLevelForStepFrame = 0;
    scriptDebugServer().setPauseOnNextStatement(true);
}

void InspectorDebuggerAgent::didFireTimer()
{
    cancelPauseOnNextStatement();
}

void InspectorDebuggerAgent::didHandleEvent()
{
    cancelPauseOnNextStatement();
}

void InspectorDebuggerAgent::cancelPauseOnNextStatement()
{
    if (m_javaScriptPauseScheduled || isPaused())
        return;
    clearBreakDetails();
    m_pausingOnNativeEvent = false;
    scriptDebugServer().setPauseOnNextStatement(false);
}

bool InspectorDebuggerAgent::v8AsyncTaskEventsEnabled() const
{
    return trackingAsyncCalls();
}

void InspectorDebuggerAgent::didReceiveV8AsyncTaskEvent(ScriptState* state, const String& eventType, const String& eventName, int id)
{
    ASSERT(trackingAsyncCalls());
    m_v8AsyncCallTracker->didReceiveV8AsyncTaskEvent(state, eventType, eventName, id);
}

bool InspectorDebuggerAgent::v8PromiseEventsEnabled() const
{
    return promiseTracker().isEnabled() || (m_listener && m_listener->canPauseOnPromiseEvent());
}

void InspectorDebuggerAgent::didReceiveV8PromiseEvent(ScriptState* scriptState, v8::Local<v8::Object> promise, v8::Local<v8::Value> parentPromise, int status)
{
    if (promiseTracker().isEnabled())
        promiseTracker().didReceiveV8PromiseEvent(scriptState, promise, parentPromise, status);
    if (!m_listener)
        return;
    if (!parentPromise.IsEmpty() && parentPromise->IsObject())
        return;
    if (status < 0)
        m_listener->didRejectPromise();
    else if (status > 0)
        m_listener->didResolvePromise();
    else
        m_listener->didCreatePromise();
}

void InspectorDebuggerAgent::pause(ErrorString*)
{
    if (m_javaScriptPauseScheduled || isPaused())
        return;
    clearBreakDetails();
    clearStepIntoAsync();
    m_javaScriptPauseScheduled = true;
    m_scheduledDebuggerStep = NoStep;
    m_skippedStepFrameCount = 0;
    m_steppingFromFramework = false;
    scriptDebugServer().setPauseOnNextStatement(true);
}

void InspectorDebuggerAgent::resume(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_scheduledDebuggerStep = NoStep;
    m_steppingFromFramework = false;
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().continueProgram();
}

void InspectorDebuggerAgent::stepOver(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    // StepOver at function return point should fallback to StepInto.
    RefPtrWillBeRawPtr<JavaScriptCallFrame> frame = scriptDebugServer().callFrameNoScopes(0);
    if (frame && frame->isAtReturn()) {
        stepInto(errorString);
        return;
    }
    m_scheduledDebuggerStep = StepOver;
    m_steppingFromFramework = isTopCallFrameInFramework();
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().stepOverStatement();
}

void InspectorDebuggerAgent::stepInto(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_scheduledDebuggerStep = StepInto;
    m_steppingFromFramework = isTopCallFrameInFramework();
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().stepIntoStatement();
}

void InspectorDebuggerAgent::stepOut(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_scheduledDebuggerStep = StepOut;
    m_skipNextDebuggerStepOut = false;
    m_recursionLevelForStepOut = 1;
    m_steppingFromFramework = isTopCallFrameInFramework();
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().stepOutOfFunction();
}

void InspectorDebuggerAgent::stepIntoAsync(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    if (!trackingAsyncCalls()) {
        *errorString = "Can only perform operation if async call stacks are enabled.";
        return;
    }
    m_inAsyncOperationForStepInto = false;
    m_asyncOperationsForStepInto.clear();
    m_performingAsyncStepIn = true;
    m_scheduledDebuggerStep = NoStep;
    m_steppingFromFramework = isTopCallFrameInFramework();
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().continueProgram();
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

void InspectorDebuggerAgent::evaluateOnCallFrame(ErrorString* errorString, const String& callFrameId, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const doNotPauseOnExceptionsAndMuteConsole, const bool* const returnByValue, const bool* generatePreview, RefPtr<RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& exceptionDetails)
{
    if (!isPaused() || m_currentCallStack.isEmpty()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(callFrameId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = scriptDebugServer().pauseOnExceptionsState();
    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        if (previousPauseOnExceptionsState != ScriptDebugServer::DontPauseOnExceptions)
            scriptDebugServer().setPauseOnExceptionsState(ScriptDebugServer::DontPauseOnExceptions);
        muteConsole();
    }

    Vector<ScriptValue> asyncCallStacks;
    const AsyncCallChain* asyncChain = trackingAsyncCalls() ? currentAsyncCallChain() : 0;
    if (asyncChain) {
        const AsyncCallStackVector& callStacks = asyncChain->callStacks();
        asyncCallStacks.resize(callStacks.size());
        AsyncCallStackVector::const_iterator it = callStacks.begin();
        for (size_t i = 0; it != callStacks.end(); ++it, ++i)
            asyncCallStacks[i] = (*it)->callFrames();
    }

    injectedScript.evaluateOnCallFrame(errorString, m_currentCallStack, asyncCallStacks, callFrameId, expression, objectGroup ? *objectGroup : "", asBool(includeCommandLineAPI), asBool(returnByValue), asBool(generatePreview), &result, wasThrown, &exceptionDetails);
    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        unmuteConsole();
        if (scriptDebugServer().pauseOnExceptionsState() != previousPauseOnExceptionsState)
            scriptDebugServer().setPauseOnExceptionsState(previousPauseOnExceptionsState);
    }
}

void InspectorDebuggerAgent::compileScript(ErrorString* errorString, const String& expression, const String& sourceURL, const int* executionContextId, TypeBuilder::OptOutput<ScriptId>* scriptId, RefPtr<ExceptionDetails>& exceptionDetails)
{
    InjectedScript injectedScript = injectedScriptForEval(errorString, executionContextId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    String scriptIdValue;
    String exceptionDetailsText;
    int lineNumberValue = 0;
    int columnNumberValue = 0;
    RefPtrWillBeRawPtr<ScriptCallStack> stackTraceValue;
    scriptDebugServer().compileScript(injectedScript.scriptState(), expression, sourceURL, &scriptIdValue, &exceptionDetailsText, &lineNumberValue, &columnNumberValue, &stackTraceValue);
    if (!scriptIdValue && !exceptionDetailsText) {
        *errorString = "Script compilation failed";
        return;
    }
    *scriptId = scriptIdValue;
    if (!scriptIdValue.isEmpty())
        return;

    exceptionDetails = ExceptionDetails::create().setText(exceptionDetailsText);
    exceptionDetails->setLine(lineNumberValue);
    exceptionDetails->setColumn(columnNumberValue);
    if (stackTraceValue && stackTraceValue->size() > 0)
        exceptionDetails->setStackTrace(stackTraceValue->buildInspectorArray());
}

void InspectorDebuggerAgent::runScript(ErrorString* errorString, const ScriptId& scriptId, const int* executionContextId, const String* const objectGroup, const bool* const doNotPauseOnExceptionsAndMuteConsole, RefPtr<RemoteObject>& result, RefPtr<ExceptionDetails>& exceptionDetails)
{
    InjectedScript injectedScript = injectedScriptForEval(errorString, executionContextId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = scriptDebugServer().pauseOnExceptionsState();
    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        if (previousPauseOnExceptionsState != ScriptDebugServer::DontPauseOnExceptions)
            scriptDebugServer().setPauseOnExceptionsState(ScriptDebugServer::DontPauseOnExceptions);
        muteConsole();
    }

    ScriptValue value;
    bool wasThrownValue;
    String exceptionDetailsText;
    int lineNumberValue = 0;
    int columnNumberValue = 0;
    RefPtrWillBeRawPtr<ScriptCallStack> stackTraceValue;
    scriptDebugServer().runScript(injectedScript.scriptState(), scriptId, &value, &wasThrownValue, &exceptionDetailsText, &lineNumberValue, &columnNumberValue, &stackTraceValue);
    if (value.isEmpty()) {
        *errorString = "Script execution failed";
        return;
    }
    result = injectedScript.wrapObject(value, objectGroup ? *objectGroup : "");
    if (wasThrownValue) {
        exceptionDetails = ExceptionDetails::create().setText(exceptionDetailsText);
        exceptionDetails->setLine(lineNumberValue);
        exceptionDetails->setColumn(columnNumberValue);
        if (stackTraceValue && stackTraceValue->size() > 0)
            exceptionDetails->setStackTrace(stackTraceValue->buildInspectorArray());
    }

    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
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
        if (!isPaused() || m_currentCallStack.isEmpty()) {
            *errorString = "Attempt to access callframe when debugger is not on pause";
            return;
        }
        injectedScript = m_injectedScriptManager->injectedScriptForObjectId(*callFrameId);
        if (injectedScript.isEmpty()) {
            *errorString = "Inspected frame has gone";
            return;
        }
    } else if (functionObjectId) {
        injectedScript = m_injectedScriptManager->injectedScriptForObjectId(*functionObjectId);
        if (injectedScript.isEmpty()) {
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

void InspectorDebuggerAgent::skipStackFrames(ErrorString* errorString, const String* pattern, const bool* skipContentScripts)
{
    OwnPtr<ScriptRegexp> compiled;
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
    increaseCachedSkipStackGeneration();
    m_skipContentScripts = asBool(skipContentScripts);
    m_state->setBoolean(DebuggerAgentState::skipContentScripts, m_skipContentScripts);
}

void InspectorDebuggerAgent::setAsyncCallStackDepth(ErrorString*, int depth)
{
    m_state->setLong(DebuggerAgentState::asyncCallStackDepth, depth);
    internalSetAsyncCallStackDepth(depth);
    if (!trackingAsyncCalls())
        clearStepIntoAsync();
}

void InspectorDebuggerAgent::enablePromiseTracker(ErrorString*, const bool* captureStacks)
{
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, true);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerCaptureStacks, asBool(captureStacks));
    promiseTracker().setEnabled(true, asBool(captureStacks));
}

void InspectorDebuggerAgent::disablePromiseTracker(ErrorString*)
{
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, false);
    promiseTracker().setEnabled(false, false);
}

void InspectorDebuggerAgent::getPromises(ErrorString* errorString, RefPtr<Array<PromiseDetails> >& promises)
{
    if (!promiseTracker().isEnabled()) {
        *errorString = "Promise tracking is disabled";
        return;
    }
    promises = promiseTracker().promises();
}

void InspectorDebuggerAgent::getPromiseById(ErrorString* errorString, int promiseId, const String* objectGroup, RefPtr<RemoteObject>& promise)
{
    if (!promiseTracker().isEnabled()) {
        *errorString = "Promise tracking is disabled";
        return;
    }
    ScriptValue value = promiseTracker().promiseById(promiseId);
    if (value.isEmpty()) {
        *errorString = "Promise with specified ID not found.";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(value.scriptState());
    promise = injectedScript.wrapObject(value, objectGroup ? *objectGroup : "");
}

const AsyncCallChain* InspectorDebuggerAgent::currentAsyncCallChain() const
{
    return m_currentAsyncCallChain.get();
}

PassRefPtrWillBeRawPtr<AsyncCallChain> InspectorDebuggerAgent::createAsyncCallChain(const String& description)
{
    ScriptValue callFrames = scriptDebugServer().currentCallFramesForAsyncStack();
    if (callFrames.isEmpty()) {
        if (!m_currentAsyncCallChain)
            return nullptr;
        RefPtrWillBeRawPtr<AsyncCallChain> chain = AsyncCallChain::create(nullptr, m_currentAsyncCallChain.get(), m_maxAsyncCallStackDepth);
        didCreateAsyncCallChain(chain.get());
        return chain.release(); // Propagate async call stack chain.
    }
    RefPtrWillBeRawPtr<AsyncCallChain> chain = AsyncCallChain::create(adoptRefWillBeNoop(new AsyncCallStack(description, callFrames)), m_currentAsyncCallChain.get(), m_maxAsyncCallStackDepth);
    didCreateAsyncCallChain(chain.get());
    return chain.release();
}

void InspectorDebuggerAgent::didCreateAsyncCallChain(AsyncCallChain* chain)
{
    if (!m_performingAsyncStepIn)
        return;
    if (m_inAsyncOperationForStepInto || m_asyncOperationsForStepInto.isEmpty())
        m_asyncOperationsForStepInto.add(chain);
}

void InspectorDebuggerAgent::clearCurrentAsyncCallChain()
{
    if (!m_nestedAsyncCallCount)
        return;
    ASSERT(m_currentAsyncCallChain);
    --m_nestedAsyncCallCount;
    if (!m_nestedAsyncCallCount) {
        m_currentAsyncCallChain.clear();
        if (!m_performingAsyncStepIn)
            return;
        if (!m_inAsyncOperationForStepInto)
            return;
        m_inAsyncOperationForStepInto = false;
        m_scheduledDebuggerStep = NoStep;
        scriptDebugServer().setPauseOnNextStatement(false);
        if (m_asyncOperationsForStepInto.isEmpty())
            clearStepIntoAsync();
    }
}

void InspectorDebuggerAgent::setCurrentAsyncCallChain(v8::Isolate* isolate, PassRefPtrWillBeRawPtr<AsyncCallChain> chain)
{
    int recursionLevel = V8RecursionScope::recursionLevel(isolate);
    if (chain && (!recursionLevel || (recursionLevel == 1 && Microtask::performingCheckpoint(isolate)))) {
        // Current AsyncCallChain corresponds to the bottommost JS call frame.
        m_currentAsyncCallChain = chain;
        m_nestedAsyncCallCount = 1;
        if (!m_performingAsyncStepIn)
            return;
        if (!m_asyncOperationsForStepInto.contains(m_currentAsyncCallChain.get()))
            return;
        m_inAsyncOperationForStepInto = true;
        m_scheduledDebuggerStep = StepInto;
        m_skippedStepFrameCount = 0;
        m_recursionLevelForStepFrame = 0;
        scriptDebugServer().setPauseOnNextStatement(true);
    } else {
        if (m_currentAsyncCallChain)
            ++m_nestedAsyncCallCount;
    }
}

void InspectorDebuggerAgent::didCompleteAsyncOperation(AsyncCallChain* chain)
{
    if (!m_performingAsyncStepIn)
        return;
    m_asyncOperationsForStepInto.remove(chain);
    if (!m_inAsyncOperationForStepInto && m_asyncOperationsForStepInto.isEmpty())
        clearStepIntoAsync();
}

void InspectorDebuggerAgent::resetAsyncCallTracker()
{
    m_currentAsyncCallChain.clear();
    m_nestedAsyncCallCount = 0;
    for (auto& listener: m_asyncCallTrackingListeners)
        listener->resetAsyncCallChains();
}

void InspectorDebuggerAgent::scriptExecutionBlockedByCSP(const String& directiveText)
{
    if (scriptDebugServer().pauseOnExceptionsState() != ScriptDebugServer::DontPauseOnExceptions) {
        RefPtr<JSONObject> directive = JSONObject::create();
        directive->setString("directiveText", directiveText);
        breakProgram(InspectorFrontend::Debugger::Reason::CSPViolation, directive.release());
    }
}


void InspectorDebuggerAgent::addAsyncCallTrackingListener(AsyncCallTrackingListener* listener)
{
    m_asyncCallTrackingListeners.append(listener);
}

void InspectorDebuggerAgent::removeAsyncCallTrackingListener(AsyncCallTrackingListener* listener)
{
    size_t index = m_asyncCallTrackingListeners.find(listener);
    if (index == kNotFound)
        return;
    m_asyncCallTrackingListeners.remove(index);
}

void InspectorDebuggerAgent::willCallFunction(ExecutionContext*, int scriptId, const String&, int)
{
    changeJavaScriptRecursionLevel(+1);
    // Fast return.
    if (m_scheduledDebuggerStep != StepInto)
        return;
    // Skip unknown scripts (e.g. InjectedScript).
    if (!m_scripts.contains(String::number(scriptId)))
        return;
    schedulePauseOnNextStatementIfSteppingInto();
}

void InspectorDebuggerAgent::didCallFunction()
{
    changeJavaScriptRecursionLevel(-1);
}

void InspectorDebuggerAgent::willEvaluateScript(LocalFrame*, const String&, int)
{
    changeJavaScriptRecursionLevel(+1);
    schedulePauseOnNextStatementIfSteppingInto();
}

void InspectorDebuggerAgent::didEvaluateScript()
{
    changeJavaScriptRecursionLevel(-1);
}

void InspectorDebuggerAgent::changeJavaScriptRecursionLevel(int step)
{
    if (m_javaScriptPauseScheduled && !m_skipAllPauses && !isPaused()) {
        // Do not ever loose user's pause request until we have actually paused.
        scriptDebugServer().setPauseOnNextStatement(true);
    }
    if (m_scheduledDebuggerStep == StepOut) {
        m_recursionLevelForStepOut += step;
        if (!m_recursionLevelForStepOut) {
            // When StepOut crosses a task boundary (i.e. js -> blink_c++) from where it was requested,
            // switch stepping to step into a next JS task, as if we exited to a blackboxed framework.
            m_scheduledDebuggerStep = StepInto;
            m_skipNextDebuggerStepOut = false;
        }
    }
    if (m_recursionLevelForStepFrame) {
        m_recursionLevelForStepFrame += step;
        if (!m_recursionLevelForStepFrame) {
            // We have walked through a blackboxed framework and got back to where we started.
            // If there was no stepping scheduled, we should cancel the stepping explicitly,
            // since there may be a scheduled StepFrame left.
            // Otherwise, if we were stepping in/over, the StepFrame will stop at the right location,
            // whereas if we were stepping out, we should continue doing so after debugger pauses
            // from the old StepFrame.
            m_skippedStepFrameCount = 0;
            if (m_scheduledDebuggerStep == NoStep)
                scriptDebugServer().clearStepping();
            else if (m_scheduledDebuggerStep == StepOut)
                m_skipNextDebuggerStepOut = true;
        }
    }
}

PassRefPtr<Array<CallFrame> > InspectorDebuggerAgent::currentCallFrames()
{
    if (!m_pausedScriptState || m_currentCallStack.isEmpty())
        return Array<CallFrame>::create();
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(m_pausedScriptState.get());
    if (injectedScript.isEmpty()) {
        ASSERT_NOT_REACHED();
        return Array<CallFrame>::create();
    }
    return injectedScript.wrapCallFrames(m_currentCallStack, 0);
}

PassRefPtr<StackTrace> InspectorDebuggerAgent::currentAsyncStackTrace()
{
    if (!m_pausedScriptState || !trackingAsyncCalls())
        return nullptr;
    const AsyncCallChain* chain = currentAsyncCallChain();
    if (!chain)
        return nullptr;
    const AsyncCallStackVector& callStacks = chain->callStacks();
    if (callStacks.isEmpty())
        return nullptr;
    RefPtr<StackTrace> result;
    int asyncOrdinal = callStacks.size();
    for (AsyncCallStackVector::const_reverse_iterator it = callStacks.rbegin(); it != callStacks.rend(); ++it, --asyncOrdinal) {
        ScriptValue callFrames = (*it)->callFrames();
        ScriptState* scriptState = callFrames.scriptState();
        InjectedScript injectedScript = scriptState ? m_injectedScriptManager->injectedScriptFor(scriptState) : InjectedScript();
        if (injectedScript.isEmpty()) {
            result.clear();
            continue;
        }
        RefPtr<StackTrace> next = StackTrace::create()
            .setCallFrames(injectedScript.wrapCallFrames(callFrames, asyncOrdinal))
            .release();
        next->setDescription((*it)->description());
        if (result)
            next->setAsyncStackTrace(result.release());
        result.swap(next);
    }
    return result.release();
}

static PassRefPtrWillBeRawPtr<ScriptCallStack> toScriptCallStack(JavaScriptCallFrame* callFrame)
{
    Vector<ScriptCallFrame> frames;
    for (; callFrame; callFrame = callFrame->caller()) {
        StringBuilder stringBuilder;
        stringBuilder.appendNumber(callFrame->sourceID());
        String scriptId = stringBuilder.toString();
        // FIXME(WK62725): Debugger line/column are 0-based, while console ones are 1-based.
        int line = callFrame->line() + 1;
        int column = callFrame->column() + 1;
        frames.append(ScriptCallFrame(callFrame->functionName(), scriptId, callFrame->scriptName(), line, column));
    }
    return ScriptCallStack::create(frames);
}

PassRefPtrWillBeRawPtr<ScriptAsyncCallStack> InspectorDebuggerAgent::currentAsyncStackTraceForConsole()
{
    if (!trackingAsyncCalls())
        return nullptr;
    const AsyncCallChain* chain = currentAsyncCallChain();
    if (!chain)
        return nullptr;
    const AsyncCallStackVector& callStacks = chain->callStacks();
    if (callStacks.isEmpty())
        return nullptr;
    RefPtrWillBeRawPtr<ScriptAsyncCallStack> result = nullptr;
    for (AsyncCallStackVector::const_reverse_iterator it = callStacks.rbegin(); it != callStacks.rend(); ++it) {
        RefPtrWillBeRawPtr<JavaScriptCallFrame> callFrame = ScriptDebugServer::toJavaScriptCallFrameUnsafe((*it)->callFrames());
        if (!callFrame)
            break;
        result = ScriptAsyncCallStack::create((*it)->description(), toScriptCallStack(callFrame.get()), result.release());
    }
    return result.release();
}

String InspectorDebuggerAgent::sourceMapURLForScript(const Script& script, CompileResult compileResult)
{
    bool hasSyntaxError = compileResult != CompileSuccess;
    if (hasSyntaxError) {
        String sourceMapURL = ContentSearchUtils::findSourceMapURL(script.source(), ContentSearchUtils::JavaScriptMagicComment);
        if (!sourceMapURL.isEmpty())
            return sourceMapURL;
    }

    if (!script.sourceMappingURL().isEmpty())
        return script.sourceMappingURL();

    if (script.url().isEmpty())
        return String();

    InspectorPageAgent* pageAgent = m_instrumentingAgents->inspectorPageAgent();
    if (!pageAgent)
        return String();
    return pageAgent->resourceSourceMapURL(script.url());
}

// ScriptDebugListener functions

void InspectorDebuggerAgent::didParseSource(const String& scriptId, const Script& parsedScript, CompileResult compileResult)
{
    Script script = parsedScript;

    bool hasSyntaxError = compileResult != CompileSuccess;
    if (hasSyntaxError)
        script.setSourceURL(ContentSearchUtils::findSourceURL(script.source(), ContentSearchUtils::JavaScriptMagicComment));

    bool isContentScript = script.isContentScript();
    bool hasSourceURL = script.hasSourceURL();
    String scriptURL = script.sourceURL();
    String sourceMapURL = sourceMapURLForScript(script, compileResult);

    const String* sourceMapURLParam = sourceMapURL.isNull() ? 0 : &sourceMapURL;
    const bool* isContentScriptParam = isContentScript ? &isContentScript : 0;
    const bool* hasSourceURLParam = hasSourceURL ? &hasSourceURL : 0;
    if (!hasSyntaxError)
        m_frontend->scriptParsed(scriptId, scriptURL, script.startLine(), script.startColumn(), script.endLine(), script.endColumn(), isContentScriptParam, sourceMapURLParam, hasSourceURLParam);
    else
        m_frontend->scriptFailedToParse(scriptId, scriptURL, script.startLine(), script.startColumn(), script.endLine(), script.endColumn(), isContentScriptParam, sourceMapURLParam, hasSourceURLParam);

    m_scripts.set(scriptId, script);

    if (scriptURL.isEmpty() || hasSyntaxError)
        return;

    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    for (auto& cookie : *breakpointsCookie) {
        RefPtr<JSONObject> breakpointObject = cookie.value->asObject();
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
        RefPtr<TypeBuilder::Debugger::Location> location = resolveBreakpoint(cookie.key, scriptId, breakpoint, UserBreakpointSource);
        if (location)
            m_frontend->breakpointResolved(cookie.key, location);
    }
}

ScriptDebugListener::SkipPauseRequest InspectorDebuggerAgent::didPause(ScriptState* scriptState, const ScriptValue& callFrames, const ScriptValue& exception, const Vector<String>& hitBreakpoints, bool isPromiseRejection)
{
    ScriptDebugListener::SkipPauseRequest result;
    if (callFrames.isEmpty())
        result = ScriptDebugListener::Continue; // Skip pauses inside V8 internal scripts and on syntax errors.
    else if (m_skipAllPauses)
        result = ScriptDebugListener::Continue;
    else if (!hitBreakpoints.isEmpty())
        result = ScriptDebugListener::NoSkip; // Don't skip explicit breakpoints even if set in frameworks.
    else if (!exception.isEmpty())
        result = shouldSkipExceptionPause();
    else if (m_scheduledDebuggerStep != NoStep || m_javaScriptPauseScheduled || m_pausingOnNativeEvent)
        result = shouldSkipStepPause();
    else
        result = ScriptDebugListener::NoSkip;

    m_skipNextDebuggerStepOut = false;
    if (result != ScriptDebugListener::NoSkip)
        return result;

    ASSERT(scriptState);
    ASSERT(!m_pausedScriptState);
    m_pausedScriptState = scriptState;
    m_currentCallStack = callFrames;

    if (!exception.isEmpty()) {
        InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(scriptState);
        if (!injectedScript.isEmpty()) {
            m_breakReason = isPromiseRejection ? InspectorFrontend::Debugger::Reason::PromiseRejection : InspectorFrontend::Debugger::Reason::Exception;
            m_breakAuxData = injectedScript.wrapObject(exception, InspectorDebuggerAgent::backtraceObjectGroup)->openAccessors();
            // m_breakAuxData might be null after this.
        }
    }

    RefPtr<Array<String> > hitBreakpointIds = Array<String>::create();

    for (const auto& point : hitBreakpoints) {
        DebugServerBreakpointToBreakpointIdAndSourceMap::iterator breakpointIterator = m_serverBreakpoints.find(point);
        if (breakpointIterator != m_serverBreakpoints.end()) {
            const String& localId = breakpointIterator->value.first;
            hitBreakpointIds->addItem(localId);

            BreakpointSource source = breakpointIterator->value.second;
            if (m_breakReason == InspectorFrontend::Debugger::Reason::Other && source == DebugCommandBreakpointSource)
                m_breakReason = InspectorFrontend::Debugger::Reason::DebugCommand;
        }
    }

    m_frontend->paused(currentCallFrames(), m_breakReason, m_breakAuxData, hitBreakpointIds, currentAsyncStackTrace());
    m_scheduledDebuggerStep = NoStep;
    m_javaScriptPauseScheduled = false;
    m_steppingFromFramework = false;
    m_pausingOnNativeEvent = false;
    m_skippedStepFrameCount = 0;
    m_recursionLevelForStepFrame = 0;
    clearStepIntoAsync();

    if (!m_continueToLocationBreakpointId.isEmpty()) {
        scriptDebugServer().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }
    return result;
}

void InspectorDebuggerAgent::didContinue()
{
    m_pausedScriptState = nullptr;
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
    if (m_skipAllPauses || m_pausedScriptState || isCallStackEmptyOrBlackboxed())
        return;
    m_breakReason = breakReason;
    m_breakAuxData = data;
    m_scheduledDebuggerStep = NoStep;
    m_steppingFromFramework = false;
    m_pausingOnNativeEvent = false;
    clearStepIntoAsync();
    scriptDebugServer().breakProgram();
}

void InspectorDebuggerAgent::clear()
{
    m_pausedScriptState = nullptr;
    m_currentCallStack = ScriptValue();
    m_scripts.clear();
    m_breakpointIdToDebugServerBreakpointIds.clear();
    internalSetAsyncCallStackDepth(0);
    promiseTracker().clear();
    m_continueToLocationBreakpointId = String();
    clearBreakDetails();
    m_scheduledDebuggerStep = NoStep;
    m_skipNextDebuggerStepOut = false;
    m_javaScriptPauseScheduled = false;
    m_steppingFromFramework = false;
    m_pausingOnNativeEvent = false;
    m_skippedStepFrameCount = 0;
    m_recursionLevelForStepFrame = 0;
    clearStepIntoAsync();
    ErrorString error;
    setOverlayMessage(&error, 0);
}

void InspectorDebuggerAgent::clearStepIntoAsync()
{
    m_performingAsyncStepIn = false;
    m_asyncOperationsForStepInto.clear();
    m_inAsyncOperationForStepInto = false;
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
    m_breakAuxData = nullptr;
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
    m_scheduledDebuggerStep = NoStep;
    m_scripts.clear();
    m_breakpointIdToDebugServerBreakpointIds.clear();
    resetAsyncCallTracker();
    promiseTracker().clear();
    if (m_frontend)
        m_frontend->globalObjectCleared();
}

void InspectorDebuggerAgent::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_injectedScriptManager);
    visitor->trace(m_listener);
    visitor->trace(m_v8AsyncCallTracker);
    visitor->trace(m_promiseTracker);
    visitor->trace(m_asyncOperationsForStepInto);
    visitor->trace(m_currentAsyncCallChain);
    visitor->trace(m_asyncCallTrackingListeners);
#endif
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink
