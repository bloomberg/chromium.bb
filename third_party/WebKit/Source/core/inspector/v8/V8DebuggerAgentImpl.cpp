// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/V8DebuggerAgentImpl.h"

#include "core/inspector/v8/AsyncCallChain.h"
#include "core/inspector/v8/IgnoreExceptionsScope.h"
#include "core/inspector/v8/InjectedScript.h"
#include "core/inspector/v8/InjectedScriptHost.h"
#include "core/inspector/v8/InjectedScriptManager.h"
#include "core/inspector/v8/JavaScriptCallFrame.h"
#include "core/inspector/v8/PromiseTracker.h"
#include "core/inspector/v8/RemoteObjectId.h"
#include "core/inspector/v8/V8AsyncCallTracker.h"
#include "core/inspector/v8/V8JavaScriptCallFrame.h"
#include "core/inspector/v8/V8Regex.h"
#include "core/inspector/v8/V8RuntimeAgentImpl.h"
#include "core/inspector/v8/V8StackTraceImpl.h"
#include "core/inspector/v8/V8StringUtil.h"
#include "core/inspector/v8/public/V8ContentSearchUtil.h"
#include "core/inspector/v8/public/V8Debugger.h"
#include "core/inspector/v8/public/V8DebuggerClient.h"
#include "platform/JSONValues.h"
#include "wtf/Optional.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

using blink::protocol::TypeBuilder::Array;
using blink::protocol::TypeBuilder::Debugger::AsyncOperation;
using blink::protocol::TypeBuilder::Debugger::BreakpointId;
using blink::protocol::TypeBuilder::Debugger::CallFrame;
using blink::protocol::TypeBuilder::Debugger::CollectionEntry;
using blink::protocol::TypeBuilder::Runtime::ExceptionDetails;
using blink::protocol::TypeBuilder::Debugger::FunctionDetails;
using blink::protocol::TypeBuilder::Debugger::GeneratorObjectDetails;
using blink::protocol::TypeBuilder::Debugger::PromiseDetails;
using blink::protocol::TypeBuilder::Runtime::ScriptId;
using blink::protocol::TypeBuilder::Debugger::StackTrace;
using blink::protocol::TypeBuilder::Runtime::RemoteObject;

namespace blink {

namespace DebuggerAgentState {
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
static const char skipAllPauses[] = "skipAllPauses";

} // namespace DebuggerAgentState;

inline static bool asBool(const bool* const b)
{
    return b ? *b : false;
}

static const int maxSkipStepFrameCount = 128;

const char V8DebuggerAgent::backtraceObjectGroup[] = "backtrace";

const int V8DebuggerAgent::unknownAsyncOperationId = 0;

static String breakpointIdSuffix(V8DebuggerAgentImpl::BreakpointSource source)
{
    switch (source) {
    case V8DebuggerAgentImpl::UserBreakpointSource:
        break;
    case V8DebuggerAgentImpl::DebugCommandBreakpointSource:
        return ":debug";
    case V8DebuggerAgentImpl::MonitorCommandBreakpointSource:
        return ":monitor";
    }
    return String();
}

static String generateBreakpointId(const String& scriptId, int lineNumber, int columnNumber, V8DebuggerAgentImpl::BreakpointSource source)
{
    return scriptId + ':' + String::number(lineNumber) + ':' + String::number(columnNumber) + breakpointIdSuffix(source);
}

static V8StackTraceImpl::Frame toScriptCallFrame(JavaScriptCallFrame* callFrame)
{
    String scriptId = String::number(callFrame->sourceID());
    // FIXME(WK62725): Debugger line/column are 0-based, while console ones are 1-based.
    int line = callFrame->line() + 1;
    int column = callFrame->column() + 1;
    return V8StackTraceImpl::Frame(callFrame->functionName(), scriptId, callFrame->scriptName(), line, column);
}

static void toScriptCallFrames(JavaScriptCallFrame* callFrame, Vector<V8StackTraceImpl::Frame>& frames)
{
    for (; callFrame; callFrame = callFrame->caller())
        frames.append(toScriptCallFrame(callFrame));
}

static PassRefPtr<JavaScriptCallFrame> toJavaScriptCallFrame(v8::Local<v8::Context> context, v8::Local<v8::Object> value)
{
    if (value.IsEmpty())
        return nullptr;
    return V8JavaScriptCallFrame::unwrap(context, value);
}

static bool positionComparator(const std::pair<int, int>& a, const std::pair<int, int>& b)
{
    if (a.first != b.first)
        return a.first < b.first;
    return a.second < b.second;
}

PassOwnPtr<V8DebuggerAgent> V8DebuggerAgent::create(V8RuntimeAgent* runtimeAgent, int contextGroupId)
{
    V8RuntimeAgentImpl* runtimeAgentImpl = static_cast<V8RuntimeAgentImpl*>(runtimeAgent);
    return adoptPtr(new V8DebuggerAgentImpl(runtimeAgentImpl->injectedScriptManager(), runtimeAgentImpl->debugger(), contextGroupId));
}

V8DebuggerAgentImpl::V8DebuggerAgentImpl(InjectedScriptManager* injectedScriptManager, V8DebuggerImpl* debugger, int contextGroupId)
    : m_injectedScriptManager(injectedScriptManager)
    , m_debugger(debugger)
    , m_contextGroupId(contextGroupId)
    , m_enabled(false)
    , m_state(nullptr)
    , m_frontend(nullptr)
    , m_isolate(debugger->isolate())
    , m_breakReason(protocol::Frontend::Debugger::Reason::Other)
    , m_scheduledDebuggerStep(NoStep)
    , m_skipNextDebuggerStepOut(false)
    , m_javaScriptPauseScheduled(false)
    , m_steppingFromFramework(false)
    , m_pausingOnNativeEvent(false)
    , m_pausingOnAsyncOperation(false)
    , m_skippedStepFrameCount(0)
    , m_recursionLevelForStepOut(0)
    , m_recursionLevelForStepFrame(0)
    , m_skipAllPauses(false)
    , m_lastAsyncOperationId(0)
    , m_maxAsyncCallStackDepth(0)
    , m_currentAsyncCallChain(nullptr)
    , m_nestedAsyncCallCount(0)
    , m_currentAsyncOperationId(unknownAsyncOperationId)
    , m_pendingTraceAsyncOperationCompleted(false)
    , m_startingStepIntoAsync(false)
{
    ASSERT(contextGroupId);
    m_injectedScriptManager->injectedScriptHost()->setDebuggerAgent(this);

    // FIXME: remove once InjectedScriptManager moves to v8.
    m_v8AsyncCallTracker = V8AsyncCallTracker::create(this);
    m_promiseTracker = PromiseTracker::create(this, m_isolate);
    clearBreakDetails();
}

V8DebuggerAgentImpl::~V8DebuggerAgentImpl()
{
}

bool V8DebuggerAgentImpl::checkEnabled(ErrorString* errorString)
{
    if (enabled())
        return true;
    *errorString = "Debugger agent is not enabled";
    return false;
}

void V8DebuggerAgentImpl::enable()
{
    // debugger().addListener may result in reporting all parsed scripts to
    // the agent so it should already be in enabled state by then.
    m_enabled = true;
    debugger().addAgent(m_contextGroupId, this);
    // FIXME(WK44513): breakpoints activated flag should be synchronized between all front-ends
    debugger().setBreakpointsActivated(true);
}

bool V8DebuggerAgentImpl::enabled()
{
    return m_enabled;
}

void V8DebuggerAgentImpl::enable(ErrorString*)
{
    if (enabled())
        return;

    enable();

    ASSERT(m_frontend);
}

void V8DebuggerAgentImpl::disable(ErrorString*)
{
    if (!enabled())
        return;

    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, JSONObject::create());
    m_state->setNumber(DebuggerAgentState::pauseOnExceptionsState, V8DebuggerImpl::DontPauseOnExceptions);
    m_state->setNumber(DebuggerAgentState::asyncCallStackDepth, 0);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, false);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerCaptureStacks, false);

    debugger().removeAgent(m_contextGroupId);
    m_pausedContext.Reset();
    m_currentCallStack.Reset();
    m_scripts.clear();
    m_blackboxedPositions.clear();
    m_breakpointIdToDebuggerBreakpointIds.clear();
    internalSetAsyncCallStackDepth(0);
    m_promiseTracker->setEnabled(false, false);
    m_continueToLocationBreakpointId = String();
    clearBreakDetails();
    m_scheduledDebuggerStep = NoStep;
    m_skipNextDebuggerStepOut = false;
    m_javaScriptPauseScheduled = false;
    m_steppingFromFramework = false;
    m_pausingOnNativeEvent = false;
    m_skippedStepFrameCount = 0;
    m_recursionLevelForStepFrame = 0;
    m_asyncOperationNotifications.clear();
    clearStepIntoAsync();
    m_skipAllPauses = false;
    m_enabled = false;
}

void V8DebuggerAgentImpl::internalSetAsyncCallStackDepth(int depth)
{
    if (depth <= 0) {
        m_maxAsyncCallStackDepth = 0;
        resetAsyncCallTracker();
    } else {
        m_maxAsyncCallStackDepth = depth;
    }
    m_v8AsyncCallTracker->asyncCallTrackingStateChanged(m_maxAsyncCallStackDepth);
}

void V8DebuggerAgentImpl::setInspectorState(PassRefPtr<JSONObject> state)
{
    m_state = state;
}

void V8DebuggerAgentImpl::clearFrontend()
{
    ErrorString error;
    disable(&error);
    ASSERT(m_frontend);
    m_frontend = nullptr;
}

void V8DebuggerAgentImpl::restore()
{
    ASSERT(!m_enabled);
    m_frontend->globalObjectCleared();
    enable();
    String error;

    long pauseState = V8DebuggerImpl::DontPauseOnExceptions;
    m_state->getNumber(DebuggerAgentState::pauseOnExceptionsState, &pauseState);
    setPauseOnExceptionsImpl(&error, pauseState);

    m_skipAllPauses = m_state->booleanProperty(DebuggerAgentState::skipAllPauses, false);

    int asyncCallStackDepth = 0;
    m_state->getNumber(DebuggerAgentState::asyncCallStackDepth, &asyncCallStackDepth);
    internalSetAsyncCallStackDepth(asyncCallStackDepth);

    m_promiseTracker->setEnabled(m_state->booleanProperty(DebuggerAgentState::promiseTrackerEnabled, false), m_state->booleanProperty(DebuggerAgentState::promiseTrackerCaptureStacks, false));
}

void V8DebuggerAgentImpl::setBreakpointsActive(ErrorString* errorString, bool active)
{
    if (!checkEnabled(errorString))
        return;
    debugger().setBreakpointsActivated(active);
}

void V8DebuggerAgentImpl::setSkipAllPauses(ErrorString*, bool skipped)
{
    m_skipAllPauses = skipped;
    m_state->setBoolean(DebuggerAgentState::skipAllPauses, m_skipAllPauses);
}

bool V8DebuggerAgentImpl::isPaused()
{
    return debugger().isPaused();
}

static PassRefPtr<JSONObject> buildObjectForBreakpointCookie(const String& url, int lineNumber, int columnNumber, const String& condition, bool isRegex)
{
    RefPtr<JSONObject> breakpointObject = JSONObject::create();
    breakpointObject->setString(DebuggerAgentState::url, url);
    breakpointObject->setNumber(DebuggerAgentState::lineNumber, lineNumber);
    breakpointObject->setNumber(DebuggerAgentState::columnNumber, columnNumber);
    breakpointObject->setString(DebuggerAgentState::condition, condition);
    breakpointObject->setBoolean(DebuggerAgentState::isRegex, isRegex);
    return breakpointObject.release();
}

static bool matches(V8DebuggerImpl* debugger, const String& url, const String& pattern, bool isRegex)
{
    if (isRegex) {
        V8Regex regex(debugger, pattern, TextCaseSensitive);
        return regex.match(url) != -1;
    }
    return url == pattern;
}

void V8DebuggerAgentImpl::setBreakpointByUrl(ErrorString* errorString, int lineNumber, const String* const optionalURL, const String* const optionalURLRegex, const int* const optionalColumnNumber, const String* const optionalCondition, BreakpointId* outBreakpointId, RefPtr<Array<protocol::TypeBuilder::Debugger::Location>>& locations)
{
    locations = Array<protocol::TypeBuilder::Debugger::Location>::create();
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
    if (!breakpointsCookie) {
        breakpointsCookie = JSONObject::create();
        m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);
    }
    if (breakpointsCookie->find(breakpointId) != breakpointsCookie->end()) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }

    breakpointsCookie->setObject(breakpointId, buildObjectForBreakpointCookie(url, lineNumber, columnNumber, condition, isRegex));
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    for (auto& script : m_scripts) {
        if (!matches(m_debugger, script.value.sourceURL(), url, isRegex))
            continue;
        RefPtr<protocol::TypeBuilder::Debugger::Location> location = resolveBreakpoint(breakpointId, script.key, breakpoint, UserBreakpointSource);
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

void V8DebuggerAgentImpl::setBreakpoint(ErrorString* errorString, const RefPtr<JSONObject>& location, const String* const optionalCondition, BreakpointId* outBreakpointId, RefPtr<protocol::TypeBuilder::Debugger::Location>& actualLocation)
{
    String scriptId;
    int lineNumber;
    int columnNumber;

    if (!parseLocation(errorString, location, &scriptId, &lineNumber, &columnNumber))
        return;

    String condition = optionalCondition ? *optionalCondition : emptyString();

    String breakpointId = generateBreakpointId(scriptId, lineNumber, columnNumber, UserBreakpointSource);
    if (m_breakpointIdToDebuggerBreakpointIds.find(breakpointId) != m_breakpointIdToDebuggerBreakpointIds.end()) {
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

void V8DebuggerAgentImpl::removeBreakpoint(ErrorString* errorString, const String& breakpointId)
{
    if (!checkEnabled(errorString))
        return;
    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    if (breakpointsCookie)
        breakpointsCookie->remove(breakpointId);
    removeBreakpoint(breakpointId);
}

void V8DebuggerAgentImpl::removeBreakpoint(const String& breakpointId)
{
    ASSERT(enabled());
    BreakpointIdToDebuggerBreakpointIdsMap::iterator debuggerBreakpointIdsIterator = m_breakpointIdToDebuggerBreakpointIds.find(breakpointId);
    if (debuggerBreakpointIdsIterator == m_breakpointIdToDebuggerBreakpointIds.end())
        return;
    for (size_t i = 0; i < debuggerBreakpointIdsIterator->value.size(); ++i) {
        const String& debuggerBreakpointId = debuggerBreakpointIdsIterator->value[i];
        debugger().removeBreakpoint(debuggerBreakpointId);
        m_serverBreakpoints.remove(debuggerBreakpointId);
        m_muteBreakpoints.remove(debuggerBreakpointId);
    }
    m_breakpointIdToDebuggerBreakpointIds.remove(debuggerBreakpointIdsIterator);
}

void V8DebuggerAgentImpl::continueToLocation(ErrorString* errorString, const RefPtr<JSONObject>& location, const bool* interstateLocationOpt)
{
    if (!checkEnabled(errorString))
        return;
    if (!m_continueToLocationBreakpointId.isEmpty()) {
        debugger().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }

    String scriptId;
    int lineNumber;
    int columnNumber;

    if (!parseLocation(errorString, location, &scriptId, &lineNumber, &columnNumber))
        return;

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, "");
    m_continueToLocationBreakpointId = debugger().setBreakpoint(scriptId, breakpoint, &lineNumber, &columnNumber, asBool(interstateLocationOpt));
    resume(errorString);
}

void V8DebuggerAgentImpl::getStepInPositions(ErrorString* errorString, const String& callFrameId, RefPtr<Array<protocol::TypeBuilder::Debugger::Location>>& positions)
{
    if (!isPaused() || m_currentCallStack.IsEmpty()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(callFrameId);
    if (!remoteId) {
        *errorString = "Invalid call frame id";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (!injectedScript) {
        *errorString = "Inspected frame has gone";
        return;
    }

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Object> callStack = m_currentCallStack.Get(m_isolate);
    injectedScript->getStepInPositions(errorString, callStack, callFrameId, positions);
}

void V8DebuggerAgentImpl::getBacktrace(ErrorString* errorString, RefPtr<Array<CallFrame>>& callFrames, RefPtr<StackTrace>& asyncStackTrace)
{
    if (!assertPaused(errorString))
        return;
    m_currentCallStack.Reset(m_isolate, debugger().currentCallFrames());
    callFrames = currentCallFrames();
    asyncStackTrace = currentAsyncStackTrace();
}

bool V8DebuggerAgentImpl::isCallStackEmptyOrBlackboxed()
{
    ASSERT(enabled());
    for (int index = 0; ; ++index) {
        RefPtr<JavaScriptCallFrame> frame = debugger().callFrameNoScopes(index);
        if (!frame)
            break;
        if (!isCallFrameWithUnknownScriptOrBlackboxed(frame.release()))
            return false;
    }
    return true;
}

bool V8DebuggerAgentImpl::isTopCallFrameBlackboxed()
{
    ASSERT(enabled());
    return isCallFrameWithUnknownScriptOrBlackboxed(debugger().callFrameNoScopes(0));
}

bool V8DebuggerAgentImpl::isCallFrameWithUnknownScriptOrBlackboxed(PassRefPtr<JavaScriptCallFrame> pFrame)
{
    RefPtr<JavaScriptCallFrame> frame = pFrame;
    if (!frame)
        return true;
    ScriptsMap::iterator it = m_scripts.find(String::number(frame->sourceID()));
    if (it == m_scripts.end()) {
        // Unknown scripts are blackboxed.
        return true;
    }
    auto itBlackboxedPositions = m_blackboxedPositions.find(String::number(frame->sourceID()));
    if (itBlackboxedPositions == m_blackboxedPositions.end())
        return false;

    const Vector<std::pair<int, int>>& ranges = itBlackboxedPositions->value;
    auto itRange = std::lower_bound(ranges.begin(), ranges.end(), std::make_pair(frame->line(), frame->column()), positionComparator);
    // Ranges array contains positions in script where blackbox state is changed.
    // [(0,0) ... ranges[0]) isn't blackboxed, [ranges[0] ... ranges[1]) is blackboxed...
    return std::distance(ranges.begin(), itRange) % 2;
}

V8DebuggerAgentImpl::SkipPauseRequest V8DebuggerAgentImpl::shouldSkipExceptionPause()
{
    if (m_steppingFromFramework)
        return RequestNoSkip;
    if (isTopCallFrameBlackboxed())
        return RequestContinue;
    return RequestNoSkip;
}

bool V8DebuggerAgentImpl::isMuteBreakpointInstalled()
{
    if (!m_muteBreakpoints.size())
        return false;
    RefPtr<JavaScriptCallFrame> frame = debugger().callFrameNoScopes(0);
    if (!frame)
        return false;
    String sourceID = String::number(frame->sourceID());
    int line = frame->line();
    for (auto it : m_muteBreakpoints.values()) {
        if (it.first == sourceID && it.second == line)
            return true;
    }
    return false;
}

V8DebuggerAgentImpl::SkipPauseRequest V8DebuggerAgentImpl::shouldSkipStepPause()
{
    if (m_steppingFromFramework)
        return RequestNoSkip;

    if (m_skipNextDebuggerStepOut) {
        m_skipNextDebuggerStepOut = false;
        if (m_scheduledDebuggerStep == StepOut)
            return RequestStepOut;
    }

    if (!isTopCallFrameBlackboxed())
        return RequestNoSkip;

    if (m_skippedStepFrameCount >= maxSkipStepFrameCount)
        return RequestStepOut;

    if (!m_skippedStepFrameCount)
        m_recursionLevelForStepFrame = 1;

    ++m_skippedStepFrameCount;
    return RequestStepFrame;
}

PassRefPtr<protocol::TypeBuilder::Debugger::Location> V8DebuggerAgentImpl::resolveBreakpoint(const String& breakpointId, const String& scriptId, const ScriptBreakpoint& breakpoint, BreakpointSource source)
{
    ASSERT(enabled());
    // FIXME: remove these checks once crbug.com/520702 is resolved.
    RELEASE_ASSERT(!breakpointId.isEmpty());
    RELEASE_ASSERT(!scriptId.isEmpty());
    ScriptsMap::iterator scriptIterator = m_scripts.find(scriptId);
    if (scriptIterator == m_scripts.end())
        return nullptr;
    V8DebuggerScript& script = scriptIterator->value;
    if (breakpoint.lineNumber < script.startLine() || script.endLine() < breakpoint.lineNumber)
        return nullptr;

    int actualLineNumber;
    int actualColumnNumber;
    String debuggerBreakpointId = debugger().setBreakpoint(scriptId, breakpoint, &actualLineNumber, &actualColumnNumber, false);
    if (debuggerBreakpointId.isEmpty())
        return nullptr;

    m_serverBreakpoints.set(debuggerBreakpointId, std::make_pair(breakpointId, source));
    if (breakpoint.condition == "false")
        m_muteBreakpoints.set(debuggerBreakpointId, std::make_pair(scriptId, breakpoint.lineNumber));

    RELEASE_ASSERT(!breakpointId.isEmpty());
    BreakpointIdToDebuggerBreakpointIdsMap::iterator debuggerBreakpointIdsIterator = m_breakpointIdToDebuggerBreakpointIds.find(breakpointId);
    if (debuggerBreakpointIdsIterator == m_breakpointIdToDebuggerBreakpointIds.end())
        m_breakpointIdToDebuggerBreakpointIds.set(breakpointId, Vector<String>()).storedValue->value.append(debuggerBreakpointId);
    else
        debuggerBreakpointIdsIterator->value.append(debuggerBreakpointId);

    RefPtr<protocol::TypeBuilder::Debugger::Location> location = protocol::TypeBuilder::Debugger::Location::create()
        .setScriptId(scriptId)
        .setLineNumber(actualLineNumber);
    location->setColumnNumber(actualColumnNumber);
    return location;
}

void V8DebuggerAgentImpl::searchInContent(ErrorString* error, const String& scriptId, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<Array<protocol::TypeBuilder::Debugger::SearchMatch>>& results)
{
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it != m_scripts.end())
        results = V8ContentSearchUtil::searchInTextByLines(m_debugger, it->value.source(), query, asBool(optionalCaseSensitive), asBool(optionalIsRegex));
    else
        *error = "No script for id: " + scriptId;
}

void V8DebuggerAgentImpl::setScriptSource(ErrorString* error, RefPtr<protocol::TypeBuilder::Debugger::SetScriptSourceError>& errorData, const String& scriptId, const String& newContent, const bool* const preview, RefPtr<Array<CallFrame>>& newCallFrames, protocol::TypeBuilder::OptOutput<bool>* stackChanged, RefPtr<StackTrace>& asyncStackTrace)
{
    if (!checkEnabled(error))
        return;
    if (!debugger().setScriptSource(scriptId, newContent, asBool(preview), error, errorData, &m_currentCallStack, stackChanged))
        return;

    newCallFrames = currentCallFrames();
    asyncStackTrace = currentAsyncStackTrace();

    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it == m_scripts.end())
        return;
    it->value.setSource(newContent);
}

void V8DebuggerAgentImpl::restartFrame(ErrorString* errorString, const String& callFrameId, RefPtr<Array<CallFrame>>& newCallFrames, RefPtr<StackTrace>& asyncStackTrace)
{
    if (!isPaused() || m_currentCallStack.IsEmpty()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(callFrameId);
    if (!remoteId) {
        *errorString = "Invalid call frame id";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (!injectedScript) {
        *errorString = "Inspected frame has gone";
        return;
    }

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Object> callStack = m_currentCallStack.Get(m_isolate);
    injectedScript->restartFrame(errorString, callStack, callFrameId);
    m_currentCallStack.Reset(m_isolate, debugger().currentCallFrames());
    newCallFrames = currentCallFrames();
    asyncStackTrace = currentAsyncStackTrace();
}

void V8DebuggerAgentImpl::getScriptSource(ErrorString* error, const String& scriptId, String* scriptSource)
{
    if (!checkEnabled(error))
        return;
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it == m_scripts.end()) {
        *error = "No script for id: " + scriptId;
        return;
    }
    *scriptSource = it->value.source();
}

void V8DebuggerAgentImpl::getFunctionDetails(ErrorString* errorString, const String& functionId, RefPtr<FunctionDetails>& details)
{
    if (!checkEnabled(errorString))
        return;
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(functionId);
    if (!remoteId) {
        *errorString = "Invalid object id";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (!injectedScript) {
        *errorString = "Function object id is obsolete";
        return;
    }
    injectedScript->getFunctionDetails(errorString, functionId, &details);
}

void V8DebuggerAgentImpl::getGeneratorObjectDetails(ErrorString* errorString, const String& objectId, RefPtr<GeneratorObjectDetails>& details)
{
    if (!checkEnabled(errorString))
        return;
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
    injectedScript->getGeneratorObjectDetails(errorString, objectId, &details);
}

void V8DebuggerAgentImpl::getCollectionEntries(ErrorString* errorString, const String& objectId, RefPtr<protocol::TypeBuilder::Array<CollectionEntry>>& entries)
{
    if (!checkEnabled(errorString))
        return;
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
    injectedScript->getCollectionEntries(errorString, objectId, &entries);
}

void V8DebuggerAgentImpl::schedulePauseOnNextStatement(protocol::Frontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data)
{
    ASSERT(enabled());
    if (m_scheduledDebuggerStep == StepInto || m_javaScriptPauseScheduled || isPaused())
        return;
    m_breakReason = breakReason;
    m_breakAuxData = data;
    m_pausingOnNativeEvent = true;
    m_skipNextDebuggerStepOut = false;
    debugger().setPauseOnNextStatement(true);
}

void V8DebuggerAgentImpl::schedulePauseOnNextStatementIfSteppingInto()
{
    ASSERT(enabled());
    if (m_scheduledDebuggerStep != StepInto || m_javaScriptPauseScheduled || isPaused())
        return;
    clearBreakDetails();
    m_pausingOnNativeEvent = false;
    m_skippedStepFrameCount = 0;
    m_recursionLevelForStepFrame = 0;
    debugger().setPauseOnNextStatement(true);
}

void V8DebuggerAgentImpl::cancelPauseOnNextStatement()
{
    if (m_javaScriptPauseScheduled || isPaused())
        return;
    clearBreakDetails();
    m_pausingOnNativeEvent = false;
    debugger().setPauseOnNextStatement(false);
}

bool V8DebuggerAgentImpl::v8AsyncTaskEventsEnabled() const
{
    return trackingAsyncCalls();
}

void V8DebuggerAgentImpl::didReceiveV8AsyncTaskEvent(v8::Local<v8::Context> context, const String& eventType, const String& eventName, int id)
{
    ASSERT(trackingAsyncCalls());
    m_v8AsyncCallTracker->didReceiveV8AsyncTaskEvent(context, eventType, eventName, id);
}

bool V8DebuggerAgentImpl::v8PromiseEventsEnabled() const
{
    return m_promiseTracker->isEnabled();
}

void V8DebuggerAgentImpl::didReceiveV8PromiseEvent(v8::Local<v8::Context> context, v8::Local<v8::Object> promise, v8::Local<v8::Value> parentPromise, int status)
{
    ASSERT(m_promiseTracker->isEnabled());
    m_promiseTracker->didReceiveV8PromiseEvent(context, promise, parentPromise, status);
}

void V8DebuggerAgentImpl::pause(ErrorString* errorString)
{
    if (!checkEnabled(errorString))
        return;
    if (m_javaScriptPauseScheduled || isPaused())
        return;
    clearBreakDetails();
    clearStepIntoAsync();
    m_javaScriptPauseScheduled = true;
    m_scheduledDebuggerStep = NoStep;
    m_skippedStepFrameCount = 0;
    m_steppingFromFramework = false;
    debugger().setPauseOnNextStatement(true);
}

void V8DebuggerAgentImpl::resume(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_scheduledDebuggerStep = NoStep;
    m_steppingFromFramework = false;
    m_injectedScriptManager->releaseObjectGroup(V8DebuggerAgentImpl::backtraceObjectGroup);
    debugger().continueProgram();
}

void V8DebuggerAgentImpl::stepOver(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    // StepOver at function return point should fallback to StepInto.
    RefPtr<JavaScriptCallFrame> frame = debugger().callFrameNoScopes(0);
    if (frame && frame->isAtReturn()) {
        stepInto(errorString);
        return;
    }
    m_scheduledDebuggerStep = StepOver;
    m_steppingFromFramework = isTopCallFrameBlackboxed();
    m_injectedScriptManager->releaseObjectGroup(V8DebuggerAgentImpl::backtraceObjectGroup);
    debugger().stepOverStatement();
}

void V8DebuggerAgentImpl::stepInto(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_scheduledDebuggerStep = StepInto;
    m_steppingFromFramework = isTopCallFrameBlackboxed();
    m_injectedScriptManager->releaseObjectGroup(V8DebuggerAgentImpl::backtraceObjectGroup);
    debugger().stepIntoStatement();
}

void V8DebuggerAgentImpl::stepOut(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_scheduledDebuggerStep = StepOut;
    m_skipNextDebuggerStepOut = false;
    m_recursionLevelForStepOut = 1;
    m_steppingFromFramework = isTopCallFrameBlackboxed();
    m_injectedScriptManager->releaseObjectGroup(V8DebuggerAgentImpl::backtraceObjectGroup);
    debugger().stepOutOfFunction();
}

void V8DebuggerAgentImpl::stepIntoAsync(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    if (!trackingAsyncCalls()) {
        *errorString = "Can only perform operation if async call stacks are enabled.";
        return;
    }
    clearStepIntoAsync();
    m_startingStepIntoAsync = true;
    stepInto(errorString);
}

void V8DebuggerAgentImpl::setPauseOnExceptions(ErrorString* errorString, const String& stringPauseState)
{
    if (!checkEnabled(errorString))
        return;
    V8DebuggerImpl::PauseOnExceptionsState pauseState;
    if (stringPauseState == "none") {
        pauseState = V8DebuggerImpl::DontPauseOnExceptions;
    } else if (stringPauseState == "all") {
        pauseState = V8DebuggerImpl::PauseOnAllExceptions;
    } else if (stringPauseState == "uncaught") {
        pauseState = V8DebuggerImpl::PauseOnUncaughtExceptions;
    } else {
        *errorString = "Unknown pause on exceptions mode: " + stringPauseState;
        return;
    }
    setPauseOnExceptionsImpl(errorString, pauseState);
}

void V8DebuggerAgentImpl::setPauseOnExceptionsImpl(ErrorString* errorString, int pauseState)
{
    debugger().setPauseOnExceptionsState(static_cast<V8DebuggerImpl::PauseOnExceptionsState>(pauseState));
    if (debugger().pauseOnExceptionsState() != pauseState)
        *errorString = "Internal error. Could not change pause on exceptions state";
    else
        m_state->setNumber(DebuggerAgentState::pauseOnExceptionsState, pauseState);
}

bool V8DebuggerAgentImpl::callStackForId(ErrorString* errorString, const RemoteCallFrameId& callFrameId, v8::Local<v8::Object>* callStack, bool* isAsync)
{
    unsigned asyncOrdinal = callFrameId.asyncStackOrdinal(); // 0 is current call stack
    if (!asyncOrdinal) {
        *callStack = m_currentCallStack.Get(m_isolate);
        *isAsync = false;
        return true;
    }
    if (!m_currentAsyncCallChain || asyncOrdinal < 1 || asyncOrdinal > m_currentAsyncCallChain->callStacks().size()) {
        *errorString = "Async call stack not found";
        return false;
    }
    RefPtr<AsyncCallStack> asyncStack = m_currentAsyncCallChain->callStacks()[asyncOrdinal - 1];
    *callStack = asyncStack->callFrames(m_isolate);
    *isAsync = true;
    return true;
}

void V8DebuggerAgentImpl::evaluateOnCallFrame(ErrorString* errorString, const String& callFrameId, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const doNotPauseOnExceptionsAndMuteConsole, const bool* const returnByValue, const bool* generatePreview, RefPtr<RemoteObject>& result, protocol::TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>& exceptionDetails)
{
    if (!isPaused() || m_currentCallStack.IsEmpty()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(callFrameId);
    if (!remoteId) {
        *errorString = "Invalid call frame id";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (!injectedScript) {
        *errorString = "Inspected frame has gone";
        return;
    }

    v8::HandleScope scope(m_isolate);
    bool isAsync = false;
    v8::Local<v8::Object> callStack;
    if (!callStackForId(errorString, *remoteId, &callStack, &isAsync))
        return;
    ASSERT(!callStack.IsEmpty());

    Optional<IgnoreExceptionsScope> ignoreExceptionsScope;
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        ignoreExceptionsScope.emplace(m_debugger);

    injectedScript->evaluateOnCallFrame(errorString, callStack, isAsync, callFrameId, expression, objectGroup ? *objectGroup : "", asBool(includeCommandLineAPI), asBool(returnByValue), asBool(generatePreview), &result, wasThrown, &exceptionDetails);
}

void V8DebuggerAgentImpl::setVariableValue(ErrorString* errorString, int scopeNumber, const String& variableName, const RefPtr<JSONObject>& newValue, const String* callFrameId, const String* functionObjectId)
{
    if (!checkEnabled(errorString))
        return;
    InjectedScript* injectedScript = nullptr;
    if (callFrameId) {
        if (!isPaused() || m_currentCallStack.IsEmpty()) {
            *errorString = "Attempt to access callframe when debugger is not on pause";
            return;
        }
        OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(*callFrameId);
        if (!remoteId) {
            *errorString = "Invalid call frame id";
            return;
        }
        injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
        if (!injectedScript) {
            *errorString = "Inspected frame has gone";
            return;
        }
    } else if (functionObjectId) {
        OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(*functionObjectId);
        if (!remoteId) {
            *errorString = "Invalid object id";
            return;
        }
        injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
        if (!injectedScript) {
            *errorString = "Function object id cannot be resolved";
            return;
        }
    } else {
        *errorString = "Either call frame or function object must be specified";
        return;
    }
    String newValueString = newValue->toJSONString();

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Object> currentCallStack = m_currentCallStack.Get(m_isolate);
    injectedScript->setVariableValue(errorString, currentCallStack, callFrameId, functionObjectId, scopeNumber, variableName, newValueString);
}

void V8DebuggerAgentImpl::setAsyncCallStackDepth(ErrorString* errorString, int depth)
{
    if (!checkEnabled(errorString))
        return;
    m_state->setNumber(DebuggerAgentState::asyncCallStackDepth, depth);
    internalSetAsyncCallStackDepth(depth);
}

void V8DebuggerAgentImpl::enablePromiseTracker(ErrorString* errorString, const bool* captureStacks)
{
    if (!checkEnabled(errorString))
        return;
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, true);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerCaptureStacks, asBool(captureStacks));
    m_promiseTracker->setEnabled(true, asBool(captureStacks));
}

void V8DebuggerAgentImpl::disablePromiseTracker(ErrorString* errorString)
{
    if (!checkEnabled(errorString))
        return;
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, false);
    m_promiseTracker->setEnabled(false, false);
}

void V8DebuggerAgentImpl::getPromiseById(ErrorString* errorString, int promiseId, const String* objectGroup, RefPtr<RemoteObject>& promise)
{
    if (!checkEnabled(errorString))
        return;
    if (!m_promiseTracker->isEnabled()) {
        *errorString = "Promise tracking is disabled";
        return;
    }
    v8::HandleScope handles(m_isolate);
    v8::Local<v8::Object> value = m_promiseTracker->promiseById(promiseId);
    if (value.IsEmpty()) {
        *errorString = "Promise with specified ID not found.";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(value->CreationContext());
    promise = injectedScript->wrapObject(value, objectGroup ? *objectGroup : "");
}

void V8DebuggerAgentImpl::didUpdatePromise(protocol::Frontend::Debugger::EventType::Enum eventType, PassRefPtr<protocol::TypeBuilder::Debugger::PromiseDetails> promise)
{
    if (m_frontend)
        m_frontend->promiseUpdated(eventType, promise);
}

int V8DebuggerAgentImpl::traceAsyncOperationStarting(const String& description)
{
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Object> callFrames = debugger().currentCallFramesForAsyncStack();
    RefPtr<AsyncCallChain> chain;
    if (callFrames.IsEmpty()) {
        if (m_currentAsyncCallChain)
            chain = AsyncCallChain::create(m_currentAsyncCallChain.get()->creationContext(m_isolate), nullptr, m_currentAsyncCallChain.get(), m_maxAsyncCallStackDepth);
    } else {
        chain = AsyncCallChain::create(debugger().isPaused() ? debugger().pausedContext() : m_isolate->GetCurrentContext(), adoptRef(new AsyncCallStack(description, callFrames)), m_currentAsyncCallChain.get(), m_maxAsyncCallStackDepth);
    }
    do {
        ++m_lastAsyncOperationId;
        if (m_lastAsyncOperationId <= 0)
            m_lastAsyncOperationId = 1;
    } while (m_asyncOperations.contains(m_lastAsyncOperationId));
    m_asyncOperations.set(m_lastAsyncOperationId, chain);
    if (chain)
        m_asyncOperationNotifications.add(m_lastAsyncOperationId);

    if (m_startingStepIntoAsync) {
        // We have successfully started a StepIntoAsync, so revoke the debugger's StepInto
        // and wait for the corresponding async operation breakpoint.
        ASSERT(m_pausingAsyncOperations.isEmpty());
        m_pausingAsyncOperations.add(m_lastAsyncOperationId);
        m_startingStepIntoAsync = false;
        m_scheduledDebuggerStep = NoStep;
        debugger().clearStepping();
    } else if (m_pausingOnAsyncOperation) {
        m_pausingAsyncOperations.add(m_lastAsyncOperationId);
    }

    if (!m_pausedContext.IsEmpty())
        flushAsyncOperationEvents(nullptr);
    return m_lastAsyncOperationId;
}

void V8DebuggerAgentImpl::traceAsyncCallbackStarting(int operationId)
{
    ASSERT(operationId > 0 || operationId == unknownAsyncOperationId);
    AsyncCallChain* chain = operationId > 0 ? m_asyncOperations.get(operationId) : nullptr;
    // FIXME: extract recursion check into a delegate.
    bool hasRecursionLevel = m_debugger->client()->hasRecursionLevel();
    if (chain && !hasRecursionLevel) {
        // There can be still an old m_currentAsyncCallChain set if we start running Microtasks
        // right after executing a JS callback but before the corresponding traceAsyncCallbackCompleted().
        // In this case just call traceAsyncCallbackCompleted() now, and the subsequent ones will be ignored.
        //
        // The nested levels count may be greater than 1, for example, when events are guarded via custom
        // traceAsync* calls, like in window.postMessage(). In this case there will be a willHandleEvent
        // instrumentation with unknownAsyncOperationId bumping up the nested levels count.
        if (m_currentAsyncCallChain) {
            ASSERT(m_nestedAsyncCallCount >= 1);
            m_nestedAsyncCallCount = 1;
            traceAsyncCallbackCompleted();
        }

        // Current AsyncCallChain corresponds to the bottommost JS call frame.
        ASSERT(!m_currentAsyncCallChain);
        m_currentAsyncCallChain = chain;
        m_currentAsyncOperationId = operationId;
        m_pendingTraceAsyncOperationCompleted = false;
        m_nestedAsyncCallCount = 1;

        if (m_pausingAsyncOperations.contains(operationId) || m_asyncOperationBreakpoints.contains(operationId)) {
            m_pausingOnAsyncOperation = true;
            m_scheduledDebuggerStep = StepInto;
            m_skippedStepFrameCount = 0;
            m_recursionLevelForStepFrame = 0;
            debugger().setPauseOnNextStatement(true);
        }
    } else {
        if (m_currentAsyncCallChain)
            ++m_nestedAsyncCallCount;
    }
}

void V8DebuggerAgentImpl::traceAsyncCallbackCompleted()
{
    if (!m_nestedAsyncCallCount)
        return;
    ASSERT(m_currentAsyncCallChain);
    --m_nestedAsyncCallCount;
    if (!m_nestedAsyncCallCount) {
        clearCurrentAsyncOperation();
        if (!m_pausingOnAsyncOperation)
            return;
        m_pausingOnAsyncOperation = false;
        m_scheduledDebuggerStep = NoStep;
        debugger().setPauseOnNextStatement(false);
        if (m_startingStepIntoAsync && m_pausingAsyncOperations.isEmpty())
            clearStepIntoAsync();
    }
}

void V8DebuggerAgentImpl::traceAsyncOperationCompleted(int operationId)
{
    ASSERT(operationId > 0 || operationId == unknownAsyncOperationId);
    bool shouldNotify = false;
    if (operationId > 0) {
        if (m_currentAsyncOperationId == operationId) {
            if (m_pendingTraceAsyncOperationCompleted) {
                m_pendingTraceAsyncOperationCompleted = false;
            } else {
                // Delay traceAsyncOperationCompleted() until the last async callback (being currently executed) is done.
                m_pendingTraceAsyncOperationCompleted = true;
                return;
            }
        }
        m_asyncOperations.remove(operationId);
        m_asyncOperationBreakpoints.remove(operationId);
        m_pausingAsyncOperations.remove(operationId);
        shouldNotify = !m_asyncOperationNotifications.take(operationId);
    }
    if (m_startingStepIntoAsync) {
        if (!m_pausingOnAsyncOperation && m_pausingAsyncOperations.isEmpty())
            clearStepIntoAsync();
    }
    if (m_frontend && shouldNotify)
        m_frontend->asyncOperationCompleted(operationId);
}

void V8DebuggerAgentImpl::flushAsyncOperationEvents(ErrorString*)
{
    if (!m_frontend)
        return;

    for (int operationId : m_asyncOperationNotifications) {
        RefPtr<AsyncCallChain> chain = m_asyncOperations.get(operationId);
        ASSERT(chain);
        const AsyncCallStackVector& callStacks = chain->callStacks();
        ASSERT(!callStacks.isEmpty());

        OwnPtr<V8StackTraceImpl> stack;
        v8::HandleScope scope(m_isolate);
        for (int i = callStacks.size() - 1; i >= 0; --i) {
            v8::Local<v8::Object> callFrames = callStacks.at(i)->callFrames(m_isolate);
            RefPtr<JavaScriptCallFrame> jsCallFrame = toJavaScriptCallFrame(chain->creationContext(m_isolate), callFrames);
            if (!jsCallFrame)
                continue;
            Vector<V8StackTraceImpl::Frame> frames;
            toScriptCallFrames(jsCallFrame.get(), frames);
            stack = V8StackTraceImpl::create(callStacks.at(i)->description(), frames, stack.release());
        }

        if (stack) {
            RefPtr<AsyncOperation> operation = AsyncOperation::create()
                .setId(operationId)
                .release();
            operation->setStack(stack->buildInspectorObject());
            m_frontend->asyncOperationStarted(operation.release());
        }
    }

    m_asyncOperationNotifications.clear();
}

void V8DebuggerAgentImpl::clearCurrentAsyncOperation()
{
    if (m_pendingTraceAsyncOperationCompleted && m_currentAsyncOperationId != unknownAsyncOperationId)
        traceAsyncOperationCompleted(m_currentAsyncOperationId);

    m_currentAsyncOperationId = unknownAsyncOperationId;
    m_pendingTraceAsyncOperationCompleted = false;
    m_nestedAsyncCallCount = 0;
    m_currentAsyncCallChain.clear();
}

void V8DebuggerAgentImpl::resetAsyncCallTracker()
{
    clearCurrentAsyncOperation();
    clearStepIntoAsync();
    m_v8AsyncCallTracker->resetAsyncOperations();
    m_asyncOperations.clear();
    m_asyncOperationNotifications.clear();
    m_asyncOperationBreakpoints.clear();
}

void V8DebuggerAgentImpl::setAsyncOperationBreakpoint(ErrorString* errorString, int operationId)
{
    if (!trackingAsyncCalls()) {
        *errorString = "Can only perform operation while tracking async call stacks.";
        return;
    }
    if (operationId <= 0) {
        *errorString = "Wrong async operation id.";
        return;
    }
    if (!m_asyncOperations.contains(operationId)) {
        *errorString = "Unknown async operation id.";
        return;
    }
    m_asyncOperationBreakpoints.add(operationId);
}

void V8DebuggerAgentImpl::removeAsyncOperationBreakpoint(ErrorString* errorString, int operationId)
{
    if (!trackingAsyncCalls()) {
        *errorString = "Can only perform operation while tracking async call stacks.";
        return;
    }
    if (operationId <= 0) {
        *errorString = "Wrong async operation id.";
        return;
    }
    m_asyncOperationBreakpoints.remove(operationId);
}

void V8DebuggerAgentImpl::setBlackboxedRanges(ErrorString* error, const String& scriptId, const RefPtr<JSONArray>& inPositions)
{
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it == m_scripts.end()) {
        *error = "No script with passed id.";
        return;
    }

    if (!inPositions->length()) {
        m_blackboxedPositions.remove(scriptId);
        return;
    }

    Vector<std::pair<int, int>> positions(inPositions->length());
    for (size_t i = 0; i < positions.size(); ++i) {
        RefPtr<JSONObject> positionObj;
        int line = 0;
        int column = 0;
        inPositions->get(i)->asObject(&positionObj);
        if (!positionObj->getNumber("line", &line) || line < 0) {
            *error = "Position missing 'line' or 'line' < 0.";
            return;
        }
        if (!positionObj->getNumber("column", &column) || column < 0) {
            *error = "Position missing 'column' or 'column' < 0.";
            return;
        }
        positions[i] = std::make_pair(line, column);
    }

    for (size_t i = 1; i < positions.size(); ++i) {
        if (positions[i - 1].first < positions[i].first)
            continue;
        if (positions[i - 1].first == positions[i].first && positions[i - 1].second < positions[i].second)
            continue;
        *error = "Input positions array is not sorted or contains duplicate values.";
        return;
    }

    m_blackboxedPositions.set(scriptId, positions);
}

void V8DebuggerAgentImpl::willExecuteScript(int scriptId)
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

void V8DebuggerAgentImpl::didExecuteScript()
{
    changeJavaScriptRecursionLevel(-1);
}

void V8DebuggerAgentImpl::changeJavaScriptRecursionLevel(int step)
{
    if (m_javaScriptPauseScheduled && !m_skipAllPauses && !isPaused()) {
        // Do not ever loose user's pause request until we have actually paused.
        debugger().setPauseOnNextStatement(true);
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
                debugger().clearStepping();
            else if (m_scheduledDebuggerStep == StepOut)
                m_skipNextDebuggerStepOut = true;
        }
    }
}

PassRefPtr<Array<CallFrame>> V8DebuggerAgentImpl::currentCallFrames()
{
    if (m_pausedContext.IsEmpty() || m_currentCallStack.IsEmpty())
        return Array<CallFrame>::create();
    v8::HandleScope handles(m_isolate);
    InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(m_pausedContext.Get(m_isolate));
    if (!injectedScript) {
        // Context has been reported as removed while on pause.
        return Array<CallFrame>::create();
    }

    v8::Local<v8::Object> currentCallStack = m_currentCallStack.Get(m_isolate);
    return injectedScript->wrapCallFrames(currentCallStack, 0);
}

PassRefPtr<StackTrace> V8DebuggerAgentImpl::currentAsyncStackTrace()
{
    if (m_pausedContext.IsEmpty() || !trackingAsyncCalls())
        return nullptr;
    const AsyncCallChain* chain = m_currentAsyncCallChain.get();
    if (!chain)
        return nullptr;
    const AsyncCallStackVector& callStacks = chain->callStacks();
    if (callStacks.isEmpty())
        return nullptr;
    RefPtr<StackTrace> result;
    int asyncOrdinal = callStacks.size();
    for (AsyncCallStackVector::const_reverse_iterator it = callStacks.rbegin(); it != callStacks.rend(); ++it, --asyncOrdinal) {
        v8::HandleScope scope(m_isolate);
        v8::Local<v8::Object> callFrames = (*it)->callFrames(m_isolate);
        InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(callFrames->CreationContext());
        if (!injectedScript) {
            result.clear();
            continue;
        }
        RefPtr<StackTrace> next = StackTrace::create()
            .setCallFrames(injectedScript->wrapCallFrames(callFrames, asyncOrdinal))
            .release();
        next->setDescription((*it)->description());
        if (result)
            next->setAsyncStackTrace(result.release());
        result.swap(next);
    }
    return result.release();
}

PassOwnPtr<V8StackTraceImpl> V8DebuggerAgentImpl::currentAsyncStackTraceForRuntime()
{
    if (!trackingAsyncCalls())
        return nullptr;
    const AsyncCallChain* chain = m_currentAsyncCallChain.get();
    if (!chain)
        return nullptr;
    const AsyncCallStackVector& callStacks = chain->callStacks();
    if (callStacks.isEmpty())
        return nullptr;
    OwnPtr<V8StackTraceImpl> result;
    v8::HandleScope scope(m_isolate);
    for (AsyncCallStackVector::const_reverse_iterator it = callStacks.rbegin(); it != callStacks.rend(); ++it) {
        RefPtr<JavaScriptCallFrame> callFrame = toJavaScriptCallFrame(chain->creationContext(m_isolate), (*it)->callFrames(m_isolate));
        if (!callFrame)
            break;
        Vector<V8StackTraceImpl::Frame> frames;
        toScriptCallFrames(callFrame.get(), frames);
        result = V8StackTraceImpl::create((*it)->description(), frames, result.release());
    }
    return result.release();
}

String V8DebuggerAgentImpl::sourceMapURLForScript(const V8DebuggerScript& script, bool success)
{
    if (success)
        return script.sourceMappingURL();
    return V8ContentSearchUtil::findSourceMapURL(script.source(), false);
}

void V8DebuggerAgentImpl::didParseSource(const V8DebuggerParsedScript& parsedScript)
{
    V8DebuggerScript script = parsedScript.script;

    if (!parsedScript.success)
        script.setSourceURL(V8ContentSearchUtil::findSourceURL(script.source(), false));

    int executionContextId = script.executionContextId();
    bool isContentScript = script.isContentScript();
    bool isInternalScript = script.isInternalScript();
    bool isLiveEdit = script.isLiveEdit();
    bool hasSourceURL = script.hasSourceURL();
    String scriptURL = script.sourceURL();
    String sourceMapURL = sourceMapURLForScript(script, parsedScript.success);

    const String* sourceMapURLParam = sourceMapURL.isNull() ? nullptr : &sourceMapURL;
    const bool* isContentScriptParam = isContentScript ? &isContentScript : nullptr;
    const bool* isInternalScriptParam = isInternalScript ? &isInternalScript : nullptr;
    const bool* isLiveEditParam = isLiveEdit ? &isLiveEdit : nullptr;
    const bool* hasSourceURLParam = hasSourceURL ? &hasSourceURL : nullptr;
    if (parsedScript.success)
        m_frontend->scriptParsed(parsedScript.scriptId, scriptURL, script.startLine(), script.startColumn(), script.endLine(), script.endColumn(), executionContextId, isContentScriptParam, isInternalScriptParam, isLiveEditParam, sourceMapURLParam, hasSourceURLParam);
    else
        m_frontend->scriptFailedToParse(parsedScript.scriptId, scriptURL, script.startLine(), script.startColumn(), script.endLine(), script.endColumn(), executionContextId, isContentScriptParam, isInternalScriptParam, sourceMapURLParam, hasSourceURLParam);

    m_scripts.set(parsedScript.scriptId, script);

    if (scriptURL.isEmpty() || !parsedScript.success)
        return;

    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    if (!breakpointsCookie)
        return;

    for (auto& cookie : *breakpointsCookie) {
        RefPtr<JSONObject> breakpointObject = cookie.value->asObject();
        bool isRegex;
        breakpointObject->getBoolean(DebuggerAgentState::isRegex, &isRegex);
        String url;
        breakpointObject->getString(DebuggerAgentState::url, &url);
        if (!matches(m_debugger, scriptURL, url, isRegex))
            continue;
        ScriptBreakpoint breakpoint;
        breakpointObject->getNumber(DebuggerAgentState::lineNumber, &breakpoint.lineNumber);
        breakpointObject->getNumber(DebuggerAgentState::columnNumber, &breakpoint.columnNumber);
        breakpointObject->getString(DebuggerAgentState::condition, &breakpoint.condition);
        RefPtr<protocol::TypeBuilder::Debugger::Location> location = resolveBreakpoint(cookie.key, parsedScript.scriptId, breakpoint, UserBreakpointSource);
        if (location)
            m_frontend->breakpointResolved(cookie.key, location);
    }
}

V8DebuggerAgentImpl::SkipPauseRequest V8DebuggerAgentImpl::didPause(v8::Local<v8::Context> context, v8::Local<v8::Object> callFrames, v8::Local<v8::Value> exception, const Vector<String>& hitBreakpoints, bool isPromiseRejection)
{
    if (isMuteBreakpointInstalled())
        return RequestContinue;

    V8DebuggerAgentImpl::SkipPauseRequest result;
    if (m_skipAllPauses)
        result = RequestContinue;
    else if (!hitBreakpoints.isEmpty())
        result = RequestNoSkip; // Don't skip explicit breakpoints even if set in frameworks.
    else if (!exception.IsEmpty())
        result = shouldSkipExceptionPause();
    else if (m_scheduledDebuggerStep != NoStep || m_javaScriptPauseScheduled || m_pausingOnNativeEvent)
        result = shouldSkipStepPause();
    else
        result = RequestNoSkip;

    m_skipNextDebuggerStepOut = false;
    if (result != RequestNoSkip)
        return result;

    // Skip pauses inside V8 internal scripts and on syntax errors.
    if (callFrames.IsEmpty())
        return RequestContinue;

    ASSERT(m_pausedContext.IsEmpty());
    m_pausedContext.Reset(m_isolate, context);
    m_currentCallStack.Reset(m_isolate, callFrames);
    v8::HandleScope handles(m_isolate);

    if (!exception.IsEmpty()) {
        InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(context);
        if (injectedScript) {
            m_breakReason = isPromiseRejection ? protocol::Frontend::Debugger::Reason::PromiseRejection : protocol::Frontend::Debugger::Reason::Exception;
            m_breakAuxData = injectedScript->wrapObject(exception, V8DebuggerAgentImpl::backtraceObjectGroup)->openAccessors();
            // m_breakAuxData might be null after this.
        }
    } else if (m_pausingOnAsyncOperation) {
        m_breakReason = protocol::Frontend::Debugger::Reason::AsyncOperation;
        m_breakAuxData = JSONObject::create();
        m_breakAuxData->setNumber("operationId", m_currentAsyncOperationId);
    }

    RefPtr<Array<String>> hitBreakpointIds = Array<String>::create();

    for (const auto& point : hitBreakpoints) {
        DebugServerBreakpointToBreakpointIdAndSourceMap::iterator breakpointIterator = m_serverBreakpoints.find(point);
        if (breakpointIterator != m_serverBreakpoints.end()) {
            const String& localId = breakpointIterator->value.first;
            hitBreakpointIds->addItem(localId);

            BreakpointSource source = breakpointIterator->value.second;
            if (m_breakReason == protocol::Frontend::Debugger::Reason::Other && source == DebugCommandBreakpointSource)
                m_breakReason = protocol::Frontend::Debugger::Reason::DebugCommand;
        }
    }

    if (!m_asyncOperationNotifications.isEmpty())
        flushAsyncOperationEvents(nullptr);

    m_frontend->paused(currentCallFrames(), m_breakReason, m_breakAuxData, hitBreakpointIds, currentAsyncStackTrace());
    m_scheduledDebuggerStep = NoStep;
    m_javaScriptPauseScheduled = false;
    m_steppingFromFramework = false;
    m_pausingOnNativeEvent = false;
    m_skippedStepFrameCount = 0;
    m_recursionLevelForStepFrame = 0;
    clearStepIntoAsync();

    if (!m_continueToLocationBreakpointId.isEmpty()) {
        debugger().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }
    return result;
}

void V8DebuggerAgentImpl::didContinue()
{
    m_pausedContext.Reset();
    m_currentCallStack.Reset();
    clearBreakDetails();
    m_frontend->resumed();
}

bool V8DebuggerAgentImpl::canBreakProgram()
{
    return debugger().canBreakProgram();
}

void V8DebuggerAgentImpl::breakProgram(protocol::Frontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data)
{
    ASSERT(enabled());
    if (m_skipAllPauses || !m_pausedContext.IsEmpty() || isCallStackEmptyOrBlackboxed())
        return;
    m_breakReason = breakReason;
    m_breakAuxData = data;
    m_scheduledDebuggerStep = NoStep;
    m_steppingFromFramework = false;
    m_pausingOnNativeEvent = false;
    clearStepIntoAsync();
    debugger().breakProgram();
}

void V8DebuggerAgentImpl::breakProgramOnException(protocol::Frontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data)
{
    if (m_debugger->pauseOnExceptionsState() == V8DebuggerImpl::DontPauseOnExceptions)
        return;
    breakProgram(breakReason, data);
}

void V8DebuggerAgentImpl::clearStepIntoAsync()
{
    m_startingStepIntoAsync = false;
    m_pausingOnAsyncOperation = false;
    m_pausingAsyncOperations.clear();
}

bool V8DebuggerAgentImpl::assertPaused(ErrorString* errorString)
{
    if (m_pausedContext.IsEmpty()) {
        *errorString = "Can only perform operation while paused.";
        return false;
    }
    return true;
}

void V8DebuggerAgentImpl::clearBreakDetails()
{
    m_breakReason = protocol::Frontend::Debugger::Reason::Other;
    m_breakAuxData = nullptr;
}

void V8DebuggerAgentImpl::setBreakpointAt(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource source, const String& condition)
{
    String breakpointId = generateBreakpointId(scriptId, lineNumber, columnNumber, source);
    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    resolveBreakpoint(breakpointId, scriptId, breakpoint, source);
}

void V8DebuggerAgentImpl::removeBreakpointAt(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource source)
{
    removeBreakpoint(generateBreakpointId(scriptId, lineNumber, columnNumber, source));
}

void V8DebuggerAgentImpl::reset()
{
    m_scheduledDebuggerStep = NoStep;
    m_scripts.clear();
    m_blackboxedPositions.clear();
    m_breakpointIdToDebuggerBreakpointIds.clear();
    resetAsyncCallTracker();
    m_promiseTracker->clear();
    if (m_frontend)
        m_frontend->globalObjectCleared();
}

} // namespace blink
