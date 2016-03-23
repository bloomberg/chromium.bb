// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8DebuggerAgentImpl.h"

#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/IgnoreExceptionsScope.h"
#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/InjectedScriptHost.h"
#include "platform/v8_inspector/InjectedScriptManager.h"
#include "platform/v8_inspector/JavaScriptCallFrame.h"
#include "platform/v8_inspector/PromiseTracker.h"
#include "platform/v8_inspector/RemoteObjectId.h"
#include "platform/v8_inspector/V8AsyncCallTracker.h"
#include "platform/v8_inspector/V8Regex.h"
#include "platform/v8_inspector/V8RuntimeAgentImpl.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8ContentSearchUtil.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"
#include "platform/v8_inspector/public/V8ToProtocolValue.h"

using blink::protocol::Array;
using blink::protocol::Maybe;
using blink::protocol::Debugger::AsyncOperation;
using blink::protocol::Debugger::BreakpointId;
using blink::protocol::Debugger::CallFrame;
using blink::protocol::Debugger::CollectionEntry;
using blink::protocol::Runtime::ExceptionDetails;
using blink::protocol::Debugger::FunctionDetails;
using blink::protocol::Debugger::GeneratorObjectDetails;
using blink::protocol::Debugger::PromiseDetails;
using blink::protocol::Runtime::ScriptId;
using blink::protocol::Runtime::StackTrace;
using blink::protocol::Runtime::RemoteObject;

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

static const int maxSkipStepFrameCount = 128;

const char V8DebuggerAgent::backtraceObjectGroup[] = "backtrace";

const int V8DebuggerAgent::unknownAsyncOperationId = 0;

static String16 breakpointIdSuffix(V8DebuggerAgentImpl::BreakpointSource source)
{
    switch (source) {
    case V8DebuggerAgentImpl::UserBreakpointSource:
        break;
    case V8DebuggerAgentImpl::DebugCommandBreakpointSource:
        return ":debug";
    case V8DebuggerAgentImpl::MonitorCommandBreakpointSource:
        return ":monitor";
    }
    return String16();
}

static String16 generateBreakpointId(const String16& scriptId, int lineNumber, int columnNumber, V8DebuggerAgentImpl::BreakpointSource source)
{
    return scriptId + ":" + String16::number(lineNumber) + ":" + String16::number(columnNumber) + breakpointIdSuffix(source);
}

static bool positionComparator(const std::pair<int, int>& a, const std::pair<int, int>& b)
{
    if (a.first != b.first)
        return a.first < b.first;
    return a.second < b.second;
}

static const LChar hexDigits[17] = "0123456789ABCDEF";

static void appendUnsignedAsHex(unsigned number, String16Builder* destination)
{
    for (size_t i = 0; i < 8; ++i) {
        destination->append(hexDigits[number & 0xF]);
        number >>= 4;
    }
}

// Hash algorithm for substrings is described in "Über die Komplexität der Multiplikation in
// eingeschränkten Branchingprogrammmodellen" by Woelfe.
// http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
static String16 calculateHash(const String16& str)
{
    static uint64_t prime[] = { 0x3FB75161, 0xAB1F4E4F, 0x82675BC5, 0xCD924D35, 0x81ABE279 };
    static uint64_t random[] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    static uint32_t randomOdd[] = { 0xB4663807, 0xCC322BF5, 0xD4F91BBD, 0xA7BEA11D, 0x8F462907 };

    uint64_t hashes[] = { 0, 0, 0, 0, 0 };
    uint64_t zi[] = { 1, 1, 1, 1, 1 };

    const size_t hashesSize = WTF_ARRAY_LENGTH(hashes);

    size_t current = 0;
    const uint32_t* data = nullptr;
    data = reinterpret_cast<const uint32_t*>(str.characters16());
    for (size_t i = 0; i < str.sizeInBytes() / 4; i += 4) {
        uint32_t v = data[i];
        uint64_t xi = v * randomOdd[current] & 0x7FFFFFFF;
        hashes[current] = (hashes[current] + zi[current] * xi) % prime[current];
        zi[current] = (zi[current] * random[current]) % prime[current];
        current = current == hashesSize - 1 ? 0 : current + 1;
    }
    if (str.sizeInBytes() % 4) {
        uint32_t v = 0;
        for (size_t i = str.sizeInBytes() - str.sizeInBytes() % 4; i < str.sizeInBytes(); ++i) {
            v <<= 8;
            v |= reinterpret_cast<const uint8_t*>(data)[i];
        }
        uint64_t xi = v * randomOdd[current] & 0x7FFFFFFF;
        hashes[current] = (hashes[current] + zi[current] * xi) % prime[current];
        zi[current] = (zi[current] * random[current]) % prime[current];
        current = current == hashesSize - 1 ? 0 : current + 1;
    }

    for (size_t i = 0; i < hashesSize; ++i)
        hashes[i] = (hashes[i] + zi[i] * (prime[i] - 1)) % prime[i];

    String16Builder hash;
    for (size_t i = 0; i < hashesSize; ++i)
        appendUnsignedAsHex(hashes[i], &hash);
    return hash.toString();
}

static bool hasInternalError(ErrorString* errorString, bool hasError)
{
    if (hasError)
        *errorString = "Internal error";
    return hasError;
}

static PassOwnPtr<protocol::Debugger::Location> buildProtocolLocation(const String16& scriptId, int lineNumber, int columnNumber)
{
    return protocol::Debugger::Location::create()
        .setScriptId(scriptId)
        .setLineNumber(lineNumber)
        .setColumnNumber(columnNumber).build();
}

PassOwnPtr<V8DebuggerAgent> V8DebuggerAgent::create(V8RuntimeAgent* runtimeAgent)
{
    V8RuntimeAgentImpl* runtimeAgentImpl = static_cast<V8RuntimeAgentImpl*>(runtimeAgent);
    return adoptPtr(new V8DebuggerAgentImpl(runtimeAgentImpl->getInjectedScriptManager(), runtimeAgentImpl->debugger(), runtimeAgentImpl->contextGroupId()));
}

V8DebuggerAgentImpl::V8DebuggerAgentImpl(InjectedScriptManager* injectedScriptManager, V8DebuggerImpl* debugger, int contextGroupId)
    : m_injectedScriptManager(injectedScriptManager)
    , m_debugger(debugger)
    , m_contextGroupId(contextGroupId)
    , m_enabled(false)
    , m_state(nullptr)
    , m_frontend(nullptr)
    , m_isolate(debugger->isolate())
    , m_breakReason(protocol::Debugger::Paused::ReasonEnum::Other)
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
    debugger().addDebuggerAgent(m_contextGroupId, this);
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

    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, protocol::DictionaryValue::create());
    m_state->setNumber(DebuggerAgentState::pauseOnExceptionsState, V8DebuggerImpl::DontPauseOnExceptions);
    m_state->setNumber(DebuggerAgentState::asyncCallStackDepth, 0);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, false);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerCaptureStacks, false);

    debugger().removeDebuggerAgent(m_contextGroupId);
    m_pausedContext.Reset();
    m_currentCallStack.clear();
    m_scripts.clear();
    m_blackboxedPositions.clear();
    m_breakpointIdToDebuggerBreakpointIds.clear();
    internalSetAsyncCallStackDepth(0);
    m_promiseTracker->setEnabled(false, false);
    m_continueToLocationBreakpointId = String16();
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

void V8DebuggerAgentImpl::setInspectorState(protocol::DictionaryValue* state)
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
    enable();
    ErrorString error;

    int pauseState = V8DebuggerImpl::DontPauseOnExceptions;
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

static PassOwnPtr<protocol::DictionaryValue> buildObjectForBreakpointCookie(const String16& url, int lineNumber, int columnNumber, const String16& condition, bool isRegex)
{
    OwnPtr<protocol::DictionaryValue> breakpointObject = protocol::DictionaryValue::create();
    breakpointObject->setString(DebuggerAgentState::url, url);
    breakpointObject->setNumber(DebuggerAgentState::lineNumber, lineNumber);
    breakpointObject->setNumber(DebuggerAgentState::columnNumber, columnNumber);
    breakpointObject->setString(DebuggerAgentState::condition, condition);
    breakpointObject->setBoolean(DebuggerAgentState::isRegex, isRegex);
    return breakpointObject.release();
}

static bool matches(V8DebuggerImpl* debugger, const String16& url, const String16& pattern, bool isRegex)
{
    if (isRegex) {
        V8Regex regex(debugger, pattern, true);
        return regex.match(url) != -1;
    }
    return url == pattern;
}

void V8DebuggerAgentImpl::setBreakpointByUrl(ErrorString* errorString,
    int lineNumber,
    const Maybe<String16>& optionalURL,
    const Maybe<String16>& optionalURLRegex,
    const Maybe<int>& optionalColumnNumber,
    const Maybe<String16>& optionalCondition,
    String16* outBreakpointId,
    OwnPtr<protocol::Array<protocol::Debugger::Location>>* locations)
{
    *locations = Array<protocol::Debugger::Location>::create();
    if (optionalURL.isJust() == optionalURLRegex.isJust()) {
        *errorString = "Either url or urlRegex must be specified.";
        return;
    }

    String16 url = optionalURL.isJust() ? optionalURL.fromJust() : optionalURLRegex.fromJust();
    int columnNumber = 0;
    if (optionalColumnNumber.isJust()) {
        columnNumber = optionalColumnNumber.fromJust();
        if (columnNumber < 0) {
            *errorString = "Incorrect column number";
            return;
        }
    }
    String16 condition = optionalCondition.fromMaybe("");
    bool isRegex = optionalURLRegex.isJust();

    String16 breakpointId = (isRegex ? "/" + url + "/" : url) + ":" + String16::number(lineNumber) + ":" + String16::number(columnNumber);
    protocol::DictionaryValue* breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    if (!breakpointsCookie) {
        OwnPtr<protocol::DictionaryValue> newValue = protocol::DictionaryValue::create();
        breakpointsCookie = newValue.get();
        m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, newValue.release());
    }
    if (breakpointsCookie->get(breakpointId)) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }

    breakpointsCookie->setObject(breakpointId, buildObjectForBreakpointCookie(url, lineNumber, columnNumber, condition, isRegex));

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    for (auto& script : m_scripts) {
        if (!matches(m_debugger, script.second->sourceURL(), url, isRegex))
            continue;
        OwnPtr<protocol::Debugger::Location> location = resolveBreakpoint(breakpointId, script.first, breakpoint, UserBreakpointSource);
        if (location)
            (*locations)->addItem(location.release());
    }

    *outBreakpointId = breakpointId;
}

static bool parseLocation(ErrorString* errorString, PassOwnPtr<protocol::Debugger::Location> location, String16* scriptId, int* lineNumber, int* columnNumber)
{
    *scriptId = location->getScriptId();
    *lineNumber = location->getLineNumber();
    *columnNumber = location->getColumnNumber(0);
    return true;
}

void V8DebuggerAgentImpl::setBreakpoint(ErrorString* errorString,
    PassOwnPtr<protocol::Debugger::Location> location,
    const Maybe<String16>& optionalCondition,
    String16* outBreakpointId,
    OwnPtr<protocol::Debugger::Location>* actualLocation)
{
    String16 scriptId;
    int lineNumber;
    int columnNumber;

    if (!parseLocation(errorString, location, &scriptId, &lineNumber, &columnNumber))
        return;

    String16 condition = optionalCondition.fromMaybe("");

    String16 breakpointId = generateBreakpointId(scriptId, lineNumber, columnNumber, UserBreakpointSource);
    if (m_breakpointIdToDebuggerBreakpointIds.contains(breakpointId)) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }
    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    *actualLocation = resolveBreakpoint(breakpointId, scriptId, breakpoint, UserBreakpointSource);
    if (*actualLocation)
        *outBreakpointId = breakpointId;
    else
        *errorString = "Could not resolve breakpoint";
}

void V8DebuggerAgentImpl::removeBreakpoint(ErrorString* errorString, const String16& breakpointId)
{
    if (!checkEnabled(errorString))
        return;
    protocol::DictionaryValue* breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    if (breakpointsCookie)
        breakpointsCookie->remove(breakpointId);
    removeBreakpoint(breakpointId);
}

void V8DebuggerAgentImpl::removeBreakpoint(const String16& breakpointId)
{
    ASSERT(enabled());
    BreakpointIdToDebuggerBreakpointIdsMap::iterator debuggerBreakpointIdsIterator = m_breakpointIdToDebuggerBreakpointIds.find(breakpointId);
    if (debuggerBreakpointIdsIterator == m_breakpointIdToDebuggerBreakpointIds.end())
        return;
    protocol::Vector<String16>* ids = debuggerBreakpointIdsIterator->second;
    for (size_t i = 0; i < ids->size(); ++i) {
        const String16& debuggerBreakpointId = ids->at(i);

        debugger().removeBreakpoint(debuggerBreakpointId);
        m_serverBreakpoints.remove(debuggerBreakpointId);
    }
    m_breakpointIdToDebuggerBreakpointIds.remove(breakpointId);
}

void V8DebuggerAgentImpl::continueToLocation(ErrorString* errorString,
    PassOwnPtr<protocol::Debugger::Location> location,
    const protocol::Maybe<bool>& interstateLocationOpt)
{
    if (!checkEnabled(errorString))
        return;
    if (!m_continueToLocationBreakpointId.isEmpty()) {
        debugger().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }

    String16 scriptId;
    int lineNumber;
    int columnNumber;

    if (!parseLocation(errorString, location, &scriptId, &lineNumber, &columnNumber))
        return;

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, "");
    m_continueToLocationBreakpointId = debugger().setBreakpoint(scriptId, breakpoint, &lineNumber, &columnNumber, interstateLocationOpt.fromMaybe(false));
    resume(errorString);
}

void V8DebuggerAgentImpl::getBacktrace(ErrorString* errorString, OwnPtr<Array<CallFrame>>* callFrames, Maybe<StackTrace>* asyncStackTrace)
{
    if (!assertPaused(errorString))
        return;
    m_currentCallStack = debugger().currentCallFrames();
    *callFrames = currentCallFrames(errorString);
    if (!*callFrames)
        return;
    *asyncStackTrace = currentAsyncStackTrace();
}

bool V8DebuggerAgentImpl::isCallStackEmptyOrBlackboxed()
{
    ASSERT(enabled());
    for (int index = 0; ; ++index) {
        OwnPtr<JavaScriptCallFrame> frame = debugger().callFrame(index);
        if (!frame)
            break;
        if (!isCallFrameWithUnknownScriptOrBlackboxed(frame.get()))
            return false;
    }
    return true;
}

bool V8DebuggerAgentImpl::isTopCallFrameBlackboxed()
{
    ASSERT(enabled());
    return isCallFrameWithUnknownScriptOrBlackboxed(debugger().callFrame(0).get());
}

bool V8DebuggerAgentImpl::isCallFrameWithUnknownScriptOrBlackboxed(JavaScriptCallFrame* frame)
{
    if (!frame)
        return true;
    ScriptsMap::iterator it = m_scripts.find(String16::number(frame->sourceID()));
    if (it == m_scripts.end()) {
        // Unknown scripts are blackboxed.
        return true;
    }
    auto itBlackboxedPositions = m_blackboxedPositions.find(String16::number(frame->sourceID()));
    if (itBlackboxedPositions == m_blackboxedPositions.end())
        return false;

    protocol::Vector<std::pair<int, int>>* ranges = itBlackboxedPositions->second;
    auto itRange = std::lower_bound(ranges->begin(), ranges->end(), std::make_pair(frame->line(), frame->column()), positionComparator);
    // Ranges array contains positions in script where blackbox state is changed.
    // [(0,0) ... ranges[0]) isn't blackboxed, [ranges[0] ... ranges[1]) is blackboxed...
    return std::distance(ranges->begin(), itRange) % 2;
}

V8DebuggerAgentImpl::SkipPauseRequest V8DebuggerAgentImpl::shouldSkipExceptionPause()
{
    if (m_steppingFromFramework)
        return RequestNoSkip;
    if (isTopCallFrameBlackboxed())
        return RequestContinue;
    return RequestNoSkip;
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

PassOwnPtr<protocol::Debugger::Location> V8DebuggerAgentImpl::resolveBreakpoint(const String16& breakpointId, const String16& scriptId, const ScriptBreakpoint& breakpoint, BreakpointSource source)
{
    ASSERT(enabled());
    // FIXME: remove these checks once crbug.com/520702 is resolved.
    RELEASE_ASSERT(!breakpointId.isEmpty());
    RELEASE_ASSERT(!scriptId.isEmpty());
    ScriptsMap::iterator scriptIterator = m_scripts.find(scriptId);
    if (scriptIterator == m_scripts.end())
        return nullptr;
    V8DebuggerScript* script = scriptIterator->second;
    if (breakpoint.lineNumber < script->startLine() || script->endLine() < breakpoint.lineNumber)
        return nullptr;

    int actualLineNumber;
    int actualColumnNumber;
    String16 debuggerBreakpointId = debugger().setBreakpoint(scriptId, breakpoint, &actualLineNumber, &actualColumnNumber, false);
    if (debuggerBreakpointId.isEmpty())
        return nullptr;

    m_serverBreakpoints.set(debuggerBreakpointId, std::make_pair(breakpointId, source));
    RELEASE_ASSERT(!breakpointId.isEmpty());
    if (!m_breakpointIdToDebuggerBreakpointIds.contains(breakpointId))
        m_breakpointIdToDebuggerBreakpointIds.set(breakpointId, protocol::Vector<String16>());

    BreakpointIdToDebuggerBreakpointIdsMap::iterator debuggerBreakpointIdsIterator = m_breakpointIdToDebuggerBreakpointIds.find(breakpointId);
    debuggerBreakpointIdsIterator->second->append(debuggerBreakpointId);

    return buildProtocolLocation(scriptId, actualLineNumber, actualColumnNumber);
}

void V8DebuggerAgentImpl::searchInContent(ErrorString* error, const String16& scriptId, const String16& query,
    const Maybe<bool>& optionalCaseSensitive,
    const Maybe<bool>& optionalIsRegex,
    OwnPtr<Array<protocol::Debugger::SearchMatch>>* results)
{
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it != m_scripts.end())
        *results = V8ContentSearchUtil::searchInTextByLines(m_debugger, it->second->source(), query, optionalCaseSensitive.fromMaybe(false), optionalIsRegex.fromMaybe(false));
    else
        *error = String16("No script for id: " + scriptId);
}

void V8DebuggerAgentImpl::setScriptSource(ErrorString* errorString,
    const String16& scriptId,
    const String16& newContent,
    const Maybe<bool>& preview,
    Maybe<protocol::Array<protocol::Debugger::CallFrame>>* newCallFrames,
    Maybe<bool>* stackChanged,
    Maybe<StackTrace>* asyncStackTrace,
    Maybe<protocol::Debugger::SetScriptSourceError>* optOutCompileError)
{
    if (!checkEnabled(errorString))
        return;
    if (!debugger().setScriptSource(scriptId, newContent, preview.fromMaybe(false), errorString, optOutCompileError, &m_currentCallStack, stackChanged))
        return;

    OwnPtr<Array<CallFrame>> callFrames = currentCallFrames(errorString);
    if (!callFrames)
        return;
    *newCallFrames = callFrames.release();
    *asyncStackTrace = currentAsyncStackTrace();

    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it == m_scripts.end())
        return;
    it->second->setSource(newContent);
}

void V8DebuggerAgentImpl::restartFrame(ErrorString* errorString,
    const String16& callFrameId,
    OwnPtr<Array<CallFrame>>* newCallFrames,
    Maybe<StackTrace>* asyncStackTrace)
{
    if (!isPaused() || !m_currentCallStack) {
        *errorString = "Attempt to access call frame when debugger is not on pause";
        return;
    }
    OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(errorString, callFrameId);
    if (!remoteId)
        return;
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(errorString, remoteId.get());
    if (!injectedScript)
        return;

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Context> localContext = injectedScript->context();

    v8::TryCatch tryCatch(m_isolate);

    OwnPtr<JavaScriptCallFrame> javaScriptCallFrame = debugger().callFrame(remoteId->frameOrdinal());
    if (!javaScriptCallFrame) {
        *errorString = "Could not find call frame with given id";
        return;
    }
    v8::Local<v8::Value> resultValue;
    v8::Local<v8::Boolean> result;
    if (!javaScriptCallFrame->restart().ToLocal(&resultValue) || tryCatch.HasCaught() || !resultValue->ToBoolean(localContext).ToLocal(&result) || !result->Value()) {
        *errorString = "Internal error";
        return;
    }

    m_currentCallStack = debugger().currentCallFrames();

    *newCallFrames = currentCallFrames(errorString);
    if (!*newCallFrames)
        return;
    *asyncStackTrace = currentAsyncStackTrace();
}

void V8DebuggerAgentImpl::getScriptSource(ErrorString* error, const String16& scriptId, String16* scriptSource)
{
    if (!checkEnabled(error))
        return;
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it == m_scripts.end()) {
        *error = "No script for id: " + scriptId;
        return;
    }
    *scriptSource = it->second->source();
}

void V8DebuggerAgentImpl::getFunctionDetails(ErrorString* errorString, const String16& functionId, OwnPtr<FunctionDetails>* details)
{
    if (!checkEnabled(errorString))
        return;
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(errorString, functionId);
    if (!remoteId)
        return;
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(errorString, remoteId.get());
    if (!injectedScript)
        return;

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Context> context = injectedScript->context();
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Value> value;
    if (!injectedScript->findObject(errorString, *remoteId, &value))
        return;
    if (!value->IsFunction()) {
        *errorString = "Value with given id is not a function";
        return;
    }
    v8::Local<v8::Function> function = value.As<v8::Function>();

    v8::Local<v8::Value> scopesValue;
    v8::Local<v8::Array> scopes;
    String16 groupName = injectedScript->objectGroupName(*remoteId);
    if (m_debugger->functionScopes(function).ToLocal(&scopesValue) && scopesValue->IsArray()) {
        scopes = scopesValue.As<v8::Array>();
        if (!injectedScript->wrapPropertyInArray(errorString, scopes, toV8StringInternalized(injectedScript->isolate(), "object"), groupName))
            return;
    }

    OwnPtr<FunctionDetails> functionDetails = FunctionDetails::create()
        .setLocation(buildProtocolLocation(String16::number(function->ScriptId()), function->GetScriptLineNumber(), function->GetScriptColumnNumber()))
        .setFunctionName(toProtocolStringWithTypeCheck(function->GetDebugName()))
        .setIsGenerator(function->IsGeneratorFunction()).build();

    if (!scopes.IsEmpty()) {
        protocol::ErrorSupport errorSupport;
        OwnPtr<protocol::Array<protocol::Debugger::Scope>> scopeChain = protocol::Array<protocol::Debugger::Scope>::parse(toProtocolValue(injectedScript->context(), scopes).get(), &errorSupport);
        if (hasInternalError(errorString, errorSupport.hasErrors()))
            return;
        functionDetails->setScopeChain(scopeChain.release());
    }

    *details = functionDetails.release();
}

void V8DebuggerAgentImpl::getGeneratorObjectDetails(ErrorString* errorString, const String16& objectId, OwnPtr<GeneratorObjectDetails>* outDetails)
{
    if (!checkEnabled(errorString))
        return;
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(errorString, objectId);
    if (!remoteId)
        return;
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(errorString, remoteId.get());
    if (!injectedScript)
        return;

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Context> context = injectedScript->context();
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Object> object;
    v8::Local<v8::Value> value;
    if (!injectedScript->findObject(errorString, *remoteId, &value))
        return;
    if (!value->IsObject() || !value->ToObject(context).ToLocal(&object)) {
        *errorString = "Value with given id is not an Object";
        return;
    }

    v8::Local<v8::Object> detailsObject;
    v8::Local<v8::Value> detailsValue = debugger().generatorObjectDetails(object);
    if (hasInternalError(errorString, !detailsValue->IsObject() || !detailsValue->ToObject(context).ToLocal(&detailsObject)))
        return;

    String16 groupName = injectedScript->objectGroupName(*remoteId);
    if (!injectedScript->wrapObjectProperty(errorString, detailsObject, toV8StringInternalized(m_isolate, "function"), groupName))
        return;

    protocol::ErrorSupport errors;
    OwnPtr<GeneratorObjectDetails> protocolDetails = GeneratorObjectDetails::parse(toProtocolValue(context, detailsObject).get(), &errors);
    if (hasInternalError(errorString, !protocolDetails))
        return;
    *outDetails = protocolDetails.release();
}

void V8DebuggerAgentImpl::getCollectionEntries(ErrorString* errorString, const String16& objectId, OwnPtr<protocol::Array<CollectionEntry>>* outEntries)
{
    if (!checkEnabled(errorString))
        return;
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(errorString, objectId);
    if (!remoteId)
        return;
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(errorString, remoteId.get());
    if (!injectedScript)
        return;

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Context> context = injectedScript->context();
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Value> value;
    if (!injectedScript->findObject(errorString, *remoteId, &value))
        return;
    if (!value->IsObject()) {
        *errorString = "Object with given id is not a collection";
        return;
    }
    v8::Local<v8::Object> object = value.As<v8::Object>();
    v8::Local<v8::Value> entriesValue = m_debugger->collectionEntries(object);
    if (hasInternalError(errorString, entriesValue.IsEmpty()))
        return;
    if (entriesValue->IsUndefined()) {
        *errorString = "Object with given id is not a collection";
        return;
    }
    if (hasInternalError(errorString, !entriesValue->IsArray()))
        return;
    String16 groupName = injectedScript->objectGroupName(*remoteId);
    v8::Local<v8::Array> entriesArray = entriesValue.As<v8::Array>();
    if (!injectedScript->wrapPropertyInArray(errorString, entriesArray, toV8StringInternalized(injectedScript->isolate(), "key"), groupName))
        return;
    if (!injectedScript->wrapPropertyInArray(errorString, entriesArray, toV8StringInternalized(injectedScript->isolate(), "value"), groupName))
        return;
    protocol::ErrorSupport errors;
    OwnPtr<protocol::Array<CollectionEntry>> entries = protocol::Array<CollectionEntry>::parse(toProtocolValue(injectedScript->context(), entriesArray).get(), &errors);
    if (hasInternalError(errorString, !entries))
        return;
    *outEntries = entries.release();
}

void V8DebuggerAgentImpl::schedulePauseOnNextStatement(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data)
{
    ASSERT(enabled());
    if (m_scheduledDebuggerStep == StepInto || m_javaScriptPauseScheduled || isPaused() || !debugger().breakpointsActivated())
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

void V8DebuggerAgentImpl::didReceiveV8AsyncTaskEvent(v8::Local<v8::Context> context, const String16& eventType, const String16& eventName, int id)
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
    OwnPtr<JavaScriptCallFrame> frame = debugger().callFrame(0);
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

void V8DebuggerAgentImpl::setPauseOnExceptions(ErrorString* errorString, const String16& stringPauseState)
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
    if (debugger().getPauseOnExceptionsState() != pauseState)
        *errorString = "Internal error. Could not change pause on exceptions state";
    else
        m_state->setNumber(DebuggerAgentState::pauseOnExceptionsState, pauseState);
}

void V8DebuggerAgentImpl::evaluateOnCallFrame(ErrorString* errorString,
    const String16& callFrameId,
    const String16& expression,
    const Maybe<String16>& objectGroup,
    const Maybe<bool>& includeCommandLineAPI,
    const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
    const Maybe<bool>& returnByValue,
    const Maybe<bool>& generatePreview,
    OwnPtr<RemoteObject>* result,
    Maybe<bool>* wasThrown,
    Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    if (!isPaused() || !m_currentCallStack) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(errorString, callFrameId);
    if (!remoteId)
        return;
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(errorString, remoteId.get());
    if (!injectedScript)
        return;

    v8::HandleScope scope(injectedScript->isolate());

    if (!injectedScript->canAccessInspectedWindow()) {
        *errorString = "Can not access given context";
        return;
    }

    OwnPtr<JavaScriptCallFrame> javaScriptCallFrame = debugger().callFrame(remoteId->frameOrdinal());
    if (!javaScriptCallFrame) {
        *errorString = "Could not find call frame with given id";
        return;
    }

    v8::MaybeLocal<v8::Object> commandLineAPI = includeCommandLineAPI.fromMaybe(false) ? injectedScript->commandLineAPI(errorString) : v8::MaybeLocal<v8::Object>();
    if (includeCommandLineAPI.fromMaybe(false) && commandLineAPI.IsEmpty())
        return;

    InjectedScriptManager::ScopedGlobalObjectExtension scopeExtension(injectedScript, m_injectedScriptManager, commandLineAPI);

    v8::TryCatch tryCatch(injectedScript->isolate());

    v8::MaybeLocal<v8::Value> maybeResultValue = javaScriptCallFrame->evaluate(toV8String(injectedScript->isolate(), expression));

    // InjectedScript may be gone after any evaluate call - find it again.
    injectedScript = m_injectedScriptManager->findInjectedScript(errorString, remoteId.get());
    if (!injectedScript)
        return;

    injectedScript->wrapEvaluateResult(errorString,
        maybeResultValue,
        tryCatch,
        objectGroup.fromMaybe(""),
        returnByValue.fromMaybe(false),
        generatePreview.fromMaybe(false),
        result,
        wasThrown,
        exceptionDetails);
}

void V8DebuggerAgentImpl::setVariableValue(ErrorString* errorString,
    int scopeNumber,
    const String16& variableName,
    PassOwnPtr<protocol::Runtime::CallArgument> newValueArgument,
    const String16& callFrameId)
{
    if (!checkEnabled(errorString))
        return;
    if (!isPaused() || !m_currentCallStack) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(errorString, callFrameId);
    if (!remoteId)
        return;
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(errorString, remoteId.get());
    if (!injectedScript)
        return;

    v8::HandleScope scope(m_isolate);
    v8::TryCatch tryCatch(m_isolate);

    v8::Local<v8::Value> newValue;
    if (!injectedScript->resolveCallArgument(errorString, newValueArgument.get()).ToLocal(&newValue))
        return;

    OwnPtr<JavaScriptCallFrame> javaScriptCallFrame = debugger().callFrame(remoteId->frameOrdinal());
    if (!javaScriptCallFrame) {
        *errorString = "Could not find call frame with given id";
        return;
    }
    v8::MaybeLocal<v8::Value> result = javaScriptCallFrame->setVariableValue(scopeNumber, toV8String(m_isolate, variableName), newValue);
    if (tryCatch.HasCaught() || result.IsEmpty()) {
        *errorString = "Internal error";
        return;
    }
}

void V8DebuggerAgentImpl::setAsyncCallStackDepth(ErrorString* errorString, int depth)
{
    if (!checkEnabled(errorString))
        return;
    m_state->setNumber(DebuggerAgentState::asyncCallStackDepth, depth);
    internalSetAsyncCallStackDepth(depth);
}

void V8DebuggerAgentImpl::enablePromiseTracker(ErrorString* errorString,
    const Maybe<bool>& captureStacks)
{
    if (!checkEnabled(errorString))
        return;
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, true);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerCaptureStacks, captureStacks.fromMaybe(false));
    m_promiseTracker->setEnabled(true, captureStacks.fromMaybe(false));
}

void V8DebuggerAgentImpl::disablePromiseTracker(ErrorString* errorString)
{
    if (!checkEnabled(errorString))
        return;
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, false);
    m_promiseTracker->setEnabled(false, false);
}

void V8DebuggerAgentImpl::getPromiseById(ErrorString* errorString, int promiseId, const Maybe<String16>& objectGroup, OwnPtr<RemoteObject>* promise)
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
    *promise = injectedScript->wrapObject(errorString, value, objectGroup.fromMaybe(""));
}

void V8DebuggerAgentImpl::didUpdatePromise(const String16& eventType, PassOwnPtr<protocol::Debugger::PromiseDetails> promise)
{
    if (m_frontend)
        m_frontend->promiseUpdated(eventType, promise);
}

int V8DebuggerAgentImpl::traceAsyncOperationStarting(const String16& description)
{
    v8::HandleScope scope(m_isolate);
    OwnPtr<V8StackTraceImpl> chain = V8StackTraceImpl::capture(this, V8StackTrace::maxCallStackSizeToCapture, description);

    do {
        ++m_lastAsyncOperationId;
        if (m_lastAsyncOperationId <= 0)
            m_lastAsyncOperationId = 1;
    } while (m_asyncOperations.contains(m_lastAsyncOperationId));

    if (chain) {
        m_asyncOperationNotifications.add(m_lastAsyncOperationId);
        m_asyncOperations.set(m_lastAsyncOperationId, chain.release());
    }

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
    V8StackTraceImpl* chain = operationId > 0 ? m_asyncOperations.get(operationId) : nullptr;
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
        m_currentAsyncCallChain = chain->clone();
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
//    ASSERT(m_currentAsyncCallChain);
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

    for (auto& operationId : m_asyncOperationNotifications) {
        V8StackTraceImpl* chain = m_asyncOperations.get(operationId.first);
        ASSERT(chain);
        if (!chain->isEmpty()) {
            OwnPtr<AsyncOperation> operation = AsyncOperation::create()
                .setId(operationId.first)
                .setStack(chain->buildInspectorObject()).build();
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

void V8DebuggerAgentImpl::setBlackboxedRanges(ErrorString* error, const String16& scriptId, PassOwnPtr<protocol::Array<protocol::Debugger::ScriptPosition>> inPositions)
{
    if (!m_scripts.contains(scriptId)) {
        *error = "No script with passed id.";
        return;
    }

    if (!inPositions->length()) {
        m_blackboxedPositions.remove(scriptId);
        return;
    }

    protocol::Vector<std::pair<int, int>> positions(inPositions->length());
    for (size_t i = 0; i < positions.size(); ++i) {
        protocol::Debugger::ScriptPosition* position = inPositions->get(i);
        if (position->getLine() < 0) {
            *error = "Position missing 'line' or 'line' < 0.";
            return;
        }
        if (position->getColumn() < 0) {
            *error = "Position missing 'column' or 'column' < 0.";
            return;
        }
        positions[i] = std::make_pair(position->getLine(), position->getColumn());
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
    if (!m_scripts.contains(String16::number(scriptId)))
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

PassOwnPtr<Array<CallFrame>> V8DebuggerAgentImpl::currentCallFrames(ErrorString* errorString)
{
    if (m_pausedContext.IsEmpty() || !m_currentCallStack)
        return Array<CallFrame>::create();
    InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(m_pausedContext.Get(m_isolate));
    if (!injectedScript) {
        // Context has been reported as removed while on pause.
        return Array<CallFrame>::create();
    }

    v8::Isolate* isolate = injectedScript->isolate();
    v8::HandleScope handles(isolate);
    v8::Local<v8::Context> context = injectedScript->context();
    v8::Context::Scope contextScope(context);

    JavaScriptCallFrame* currentCallFrame = m_currentCallStack.get();
    int callFrameIndex = 0;
    OwnPtr<JavaScriptCallFrame> currentCallFrameOwner;
    v8::Local<v8::Array> objects = v8::Array::New(isolate);
    while (currentCallFrame) {
        v8::Local<v8::Object> details = currentCallFrame->details();
        if (hasInternalError(errorString, details.IsEmpty()))
            return Array<CallFrame>::create();

        String16 callFrameId = RemoteCallFrameId::serialize(injectedScript->contextId(), callFrameIndex);
        if (hasInternalError(errorString, !details->Set(context, toV8StringInternalized(isolate, "callFrameId"), toV8String(isolate, callFrameId)).FromMaybe(false)))
            return Array<CallFrame>::create();

        v8::Local<v8::Value> scopeChain;
        if (hasInternalError(errorString, !details->Get(context, toV8StringInternalized(isolate, "scopeChain")).ToLocal(&scopeChain) || !scopeChain->IsArray()))
            return Array<CallFrame>::create();
        v8::Local<v8::Array> scopeChainArray = scopeChain.As<v8::Array>();
        if (!injectedScript->wrapPropertyInArray(errorString, scopeChainArray, toV8StringInternalized(isolate, "object"), V8DebuggerAgentImpl::backtraceObjectGroup))
            return Array<CallFrame>::create();

        if (!injectedScript->wrapObjectProperty(errorString, details, toV8StringInternalized(isolate, "this"), V8DebuggerAgentImpl::backtraceObjectGroup))
            return Array<CallFrame>::create();

        if (details->Has(context, toV8StringInternalized(isolate, "returnValue")).FromMaybe(false)) {
            if (!injectedScript->wrapObjectProperty(errorString, details, toV8StringInternalized(isolate, "returnValue"), V8DebuggerAgentImpl::backtraceObjectGroup))
                return Array<CallFrame>::create();
        }

        if (hasInternalError(errorString, !objects->Set(context, callFrameIndex, details).FromMaybe(false)))
            return Array<CallFrame>::create();

        currentCallFrameOwner = currentCallFrame->caller();
        currentCallFrame = currentCallFrameOwner.get();
        ++callFrameIndex;
    }

    protocol::ErrorSupport errorSupport;
    OwnPtr<Array<CallFrame>> callFrames = Array<CallFrame>::parse(toProtocolValue(context, objects).get(), &errorSupport);
    if (hasInternalError(errorString, !callFrames))
        return Array<CallFrame>::create();
    return callFrames.release();
}

PassOwnPtr<StackTrace> V8DebuggerAgentImpl::currentAsyncStackTrace()
{
    if (m_pausedContext.IsEmpty() || !trackingAsyncCalls() || !m_currentAsyncCallChain)
        return nullptr;

    return m_currentAsyncCallChain->buildInspectorObjectForTail(this);
}

V8StackTraceImpl* V8DebuggerAgentImpl::currentAsyncCallChain()
{
    return trackingAsyncCalls() ? m_currentAsyncCallChain.get() : nullptr;
}

void V8DebuggerAgentImpl::didParseSource(const V8DebuggerParsedScript& parsedScript)
{
    V8DebuggerScript script = parsedScript.script;

    bool isDeprecatedSourceURL = false;
    if (!parsedScript.success)
        script.setSourceURL(V8ContentSearchUtil::findSourceURL(script.source(), false, &isDeprecatedSourceURL));
    else if (script.hasSourceURL())
        V8ContentSearchUtil::findSourceURL(script.source(), false, &isDeprecatedSourceURL);

    bool isDeprecatedSourceMappingURL = false;
    if (!parsedScript.success)
        script.setSourceMappingURL(V8ContentSearchUtil::findSourceMapURL(script.source(), false, &isDeprecatedSourceMappingURL));
    else if (!script.sourceMappingURL().isEmpty())
        V8ContentSearchUtil::findSourceMapURL(script.source(), false, &isDeprecatedSourceMappingURL);

    script.setHash(calculateHash(script.source()));

    int executionContextId = script.executionContextId();
    bool isContentScript = script.isContentScript();
    bool isInternalScript = script.isInternalScript();
    bool isLiveEdit = script.isLiveEdit();
    bool hasSourceURL = script.hasSourceURL();
    String16 scriptURL = script.sourceURL();
    String16 sourceMapURL = script.sourceMappingURL();
    bool deprecatedCommentWasUsed = isDeprecatedSourceURL || isDeprecatedSourceMappingURL;

    const Maybe<String16>& sourceMapURLParam = sourceMapURL;
    const bool* isContentScriptParam = isContentScript ? &isContentScript : nullptr;
    const bool* isInternalScriptParam = isInternalScript ? &isInternalScript : nullptr;
    const bool* isLiveEditParam = isLiveEdit ? &isLiveEdit : nullptr;
    const bool* hasSourceURLParam = hasSourceURL ? &hasSourceURL : nullptr;
    const bool* deprecatedCommentWasUsedParam = deprecatedCommentWasUsed ? &deprecatedCommentWasUsed : nullptr;
    if (parsedScript.success)
        m_frontend->scriptParsed(parsedScript.scriptId, scriptURL, script.startLine(), script.startColumn(), script.endLine(), script.endColumn(), executionContextId, script.hash(), isContentScriptParam, isInternalScriptParam, isLiveEditParam, sourceMapURLParam, hasSourceURLParam, deprecatedCommentWasUsedParam);
    else
        m_frontend->scriptFailedToParse(parsedScript.scriptId, scriptURL, script.startLine(), script.startColumn(), script.endLine(), script.endColumn(), executionContextId, script.hash(), isContentScriptParam, isInternalScriptParam, sourceMapURLParam, hasSourceURLParam, deprecatedCommentWasUsedParam);

    m_scripts.set(parsedScript.scriptId, script);

    if (scriptURL.isEmpty() || !parsedScript.success)
        return;

    protocol::DictionaryValue* breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    if (!breakpointsCookie)
        return;

    for (size_t i = 0; i < breakpointsCookie->size(); ++i) {
        auto cookie = breakpointsCookie->at(i);
        protocol::DictionaryValue* breakpointObject = protocol::DictionaryValue::cast(cookie.second);
        bool isRegex;
        breakpointObject->getBoolean(DebuggerAgentState::isRegex, &isRegex);
        String16 url;
        breakpointObject->getString(DebuggerAgentState::url, &url);
        if (!matches(m_debugger, scriptURL, url, isRegex))
            continue;
        ScriptBreakpoint breakpoint;
        breakpointObject->getNumber(DebuggerAgentState::lineNumber, &breakpoint.lineNumber);
        breakpointObject->getNumber(DebuggerAgentState::columnNumber, &breakpoint.columnNumber);
        breakpointObject->getString(DebuggerAgentState::condition, &breakpoint.condition);
        OwnPtr<protocol::Debugger::Location> location = resolveBreakpoint(cookie.first, parsedScript.scriptId, breakpoint, UserBreakpointSource);
        if (location)
            m_frontend->breakpointResolved(cookie.first, location.release());
    }
}

V8DebuggerAgentImpl::SkipPauseRequest V8DebuggerAgentImpl::didPause(v8::Local<v8::Context> context, PassOwnPtr<JavaScriptCallFrame> callFrames, v8::Local<v8::Value> exception, const protocol::Vector<String16>& hitBreakpoints, bool isPromiseRejection)
{
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
    if (!callFrames)
        return RequestContinue;

    ASSERT(m_pausedContext.IsEmpty());
    m_pausedContext.Reset(m_isolate, context);
    m_currentCallStack = callFrames;
    v8::HandleScope handles(m_isolate);

    if (!exception.IsEmpty()) {
        InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(context);
        if (injectedScript) {
            m_breakReason = isPromiseRejection ? protocol::Debugger::Paused::ReasonEnum::PromiseRejection : protocol::Debugger::Paused::ReasonEnum::Exception;
            ErrorString errorString;
            auto obj = injectedScript->wrapObject(&errorString, exception, V8DebuggerAgentImpl::backtraceObjectGroup);
            m_breakAuxData = obj ? obj->serialize() : nullptr;
            // m_breakAuxData might be null after this.
        }
    } else if (m_pausingOnAsyncOperation) {
        m_breakReason = protocol::Debugger::Paused::ReasonEnum::AsyncOperation;
        m_breakAuxData = protocol::DictionaryValue::create();
        m_breakAuxData->setNumber("operationId", m_currentAsyncOperationId);
    }

    OwnPtr<Array<String16>> hitBreakpointIds = Array<String16>::create();

    for (const auto& point : hitBreakpoints) {
        DebugServerBreakpointToBreakpointIdAndSourceMap::iterator breakpointIterator = m_serverBreakpoints.find(point);
        if (breakpointIterator != m_serverBreakpoints.end()) {
            const String16& localId = breakpointIterator->second->first;
            hitBreakpointIds->addItem(localId);

            BreakpointSource source = breakpointIterator->second->second;
            if (m_breakReason == protocol::Debugger::Paused::ReasonEnum::Other && source == DebugCommandBreakpointSource)
                m_breakReason = protocol::Debugger::Paused::ReasonEnum::DebugCommand;
        }
    }

    if (!m_asyncOperationNotifications.isEmpty())
        flushAsyncOperationEvents(nullptr);

    ErrorString errorString;
    m_frontend->paused(currentCallFrames(&errorString), m_breakReason, m_breakAuxData.release(), hitBreakpointIds.release(), currentAsyncStackTrace());
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
    m_currentCallStack.clear();
    clearBreakDetails();
    m_frontend->resumed();
}

bool V8DebuggerAgentImpl::canBreakProgram()
{
    return debugger().canBreakProgram();
}

void V8DebuggerAgentImpl::breakProgram(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data)
{
    ASSERT(enabled());
    if (m_skipAllPauses || !m_pausedContext.IsEmpty() || isCallStackEmptyOrBlackboxed() || !debugger().breakpointsActivated())
        return;
    m_breakReason = breakReason;
    m_breakAuxData = data;
    m_scheduledDebuggerStep = NoStep;
    m_steppingFromFramework = false;
    m_pausingOnNativeEvent = false;
    clearStepIntoAsync();
    debugger().breakProgram();
}

void V8DebuggerAgentImpl::breakProgramOnException(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data)
{
    if (m_debugger->getPauseOnExceptionsState() == V8DebuggerImpl::DontPauseOnExceptions)
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
    m_breakReason = protocol::Debugger::Paused::ReasonEnum::Other;
    m_breakAuxData = nullptr;
}

void V8DebuggerAgentImpl::setBreakpointAt(const String16& scriptId, int lineNumber, int columnNumber, BreakpointSource source, const String16& condition)
{
    String16 breakpointId = generateBreakpointId(scriptId, lineNumber, columnNumber, source);
    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    resolveBreakpoint(breakpointId, scriptId, breakpoint, source);
}

void V8DebuggerAgentImpl::removeBreakpointAt(const String16& scriptId, int lineNumber, int columnNumber, BreakpointSource source)
{
    removeBreakpoint(generateBreakpointId(scriptId, lineNumber, columnNumber, source));
}

void V8DebuggerAgentImpl::reset()
{
    if (!enabled())
        return;
    m_scheduledDebuggerStep = NoStep;
    m_scripts.clear();
    m_blackboxedPositions.clear();
    m_breakpointIdToDebuggerBreakpointIds.clear();
    resetAsyncCallTracker();
    m_promiseTracker->clear();
}

} // namespace blink
