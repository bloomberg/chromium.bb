// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8DebuggerAgentImpl_h
#define V8DebuggerAgentImpl_h

#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/inspector_protocol/Frontend.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/v8_inspector/ScriptBreakpoint.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/public/V8DebuggerAgent.h"

namespace blink {

class DevToolsFunctionInfo;
class InjectedScript;
class InjectedScriptManager;
class JavaScriptCallFrame;
class PromiseTracker;
class RemoteCallFrameId;
class ScriptRegexp;
class V8AsyncCallTracker;
class V8StackTraceImpl;

namespace protocol {
class DictionaryValue;
}


using protocol::Maybe;

class V8DebuggerAgentImpl : public V8DebuggerAgent {
    PROTOCOL_DISALLOW_COPY(V8DebuggerAgentImpl);
public:
    enum SkipPauseRequest {
        RequestNoSkip,
        RequestContinue,
        RequestStepInto,
        RequestStepOut,
        RequestStepFrame
    };

    enum BreakpointSource {
        UserBreakpointSource,
        DebugCommandBreakpointSource,
        MonitorCommandBreakpointSource
    };

    V8DebuggerAgentImpl(InjectedScriptManager*, V8DebuggerImpl*, int contextGroupId);
    ~V8DebuggerAgentImpl() override;

    void setInspectorState(protocol::DictionaryValue*) override;
    void setFrontend(protocol::Frontend::Debugger* frontend) override { m_frontend = frontend; }
    void clearFrontend() override;
    void restore() override;
    void disable(ErrorString*) override;

    bool isPaused() override;

    // Part of the protocol.
    void enable(ErrorString*) override;
    void setBreakpointsActive(ErrorString*, bool active) override;
    void setSkipAllPauses(ErrorString*, bool skipped) override;

    void setBreakpointByUrl(ErrorString*,
        int lineNumber,
        const Maybe<String16>& optionalURL,
        const Maybe<String16>& optionalURLRegex,
        const Maybe<int>& optionalColumnNumber,
        const Maybe<String16>& optionalCondition,
        String16*,
        OwnPtr<protocol::Array<protocol::Debugger::Location>>* locations) override;
    void setBreakpoint(ErrorString*,
        PassOwnPtr<protocol::Debugger::Location>,
        const Maybe<String16>& optionalCondition,
        String16*,
        OwnPtr<protocol::Debugger::Location>* actualLocation) override;
    void removeBreakpoint(ErrorString*, const String16& breakpointId) override;
    void continueToLocation(ErrorString*,
        PassOwnPtr<protocol::Debugger::Location>,
        const Maybe<bool>& interstateLocationOpt) override;
    void getBacktrace(ErrorString*,
        OwnPtr<protocol::Array<protocol::Debugger::CallFrame>>*,
        Maybe<protocol::Runtime::StackTrace>*) override;
    void searchInContent(ErrorString*,
        const String16& scriptId,
        const String16& query,
        const Maybe<bool>& optionalCaseSensitive,
        const Maybe<bool>& optionalIsRegex,
        OwnPtr<protocol::Array<protocol::Debugger::SearchMatch>>*) override;
    void canSetScriptSource(ErrorString*, bool* result) override { *result = true; }
    void setScriptSource(ErrorString*,
        const String16& inScriptId,
        const String16& inScriptSource,
        const Maybe<bool>& inPreview,
        Maybe<protocol::Array<protocol::Debugger::CallFrame>>* optOutCallFrames,
        Maybe<bool>* optOutStackChanged,
        Maybe<protocol::Runtime::StackTrace>* optOutAsyncStackTrace,
        Maybe<protocol::Debugger::SetScriptSourceError>* optOutCompileError) override;
    void restartFrame(ErrorString*,
        const String16& callFrameId,
        OwnPtr<protocol::Array<protocol::Debugger::CallFrame>>* newCallFrames,
        Maybe<protocol::Runtime::StackTrace>* asyncStackTrace) override;
    void getScriptSource(ErrorString*, const String16& scriptId, String16* scriptSource) override;
    void getFunctionDetails(ErrorString*,
        const String16& functionId,
        OwnPtr<protocol::Debugger::FunctionDetails>*) override;
    void getGeneratorObjectDetails(ErrorString*,
        const String16& objectId,
        OwnPtr<protocol::Debugger::GeneratorObjectDetails>*) override;
    void getCollectionEntries(ErrorString*,
        const String16& objectId,
        OwnPtr<protocol::Array<protocol::Debugger::CollectionEntry>>*) override;
    void pause(ErrorString*) override;
    void resume(ErrorString*) override;
    void stepOver(ErrorString*) override;
    void stepInto(ErrorString*) override;
    void stepOut(ErrorString*) override;
    void stepIntoAsync(ErrorString*) override;
    void setPauseOnExceptions(ErrorString*, const String16& pauseState) override;
    void evaluateOnCallFrame(ErrorString*,
        const String16& callFrameId,
        const String16& expression,
        const Maybe<String16>& objectGroup,
        const Maybe<bool>& includeCommandLineAPI,
        const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
        const Maybe<bool>& returnByValue,
        const Maybe<bool>& generatePreview,
        OwnPtr<protocol::Runtime::RemoteObject>* result,
        Maybe<bool>* wasThrown,
        Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void setVariableValue(ErrorString*,
        int scopeNumber,
        const String16& variableName,
        PassOwnPtr<protocol::Runtime::CallArgument> newValue,
        const Maybe<String16>& callFrame,
        const Maybe<String16>& functionObjectId) override;
    void setAsyncCallStackDepth(ErrorString*, int depth) override;
    void enablePromiseTracker(ErrorString*,
        const Maybe<bool>& captureStacks) override;
    void disablePromiseTracker(ErrorString*) override;
    void getPromiseById(ErrorString*,
        int promiseId,
        const Maybe<String16>& objectGroup,
        OwnPtr<protocol::Runtime::RemoteObject>* promise) override;
    void flushAsyncOperationEvents(ErrorString*) override;
    void setAsyncOperationBreakpoint(ErrorString*, int operationId) override;
    void removeAsyncOperationBreakpoint(ErrorString*, int operationId) override;
    void setBlackboxedRanges(ErrorString*,
        const String16& scriptId,
        PassOwnPtr<protocol::Array<protocol::Debugger::ScriptPosition>> positions) override;

    void schedulePauseOnNextStatement(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data) override;
    void cancelPauseOnNextStatement() override;
    bool canBreakProgram() override;
    void breakProgram(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data) override;
    void breakProgramOnException(const String16& breakReason, PassOwnPtr<protocol::DictionaryValue> data) override;
    void willExecuteScript(int scriptId) override;
    void didExecuteScript() override;

    bool enabled() override;
    V8DebuggerImpl& debugger() override { return *m_debugger; }

    void setBreakpointAt(const String16& scriptId, int lineNumber, int columnNumber, BreakpointSource, const String16& condition = String16());
    void removeBreakpointAt(const String16& scriptId, int lineNumber, int columnNumber, BreakpointSource);

    // Async call stacks implementation
    int traceAsyncOperationStarting(const String16& description) override;
    void traceAsyncCallbackStarting(int operationId) override;
    void traceAsyncCallbackCompleted() override;
    void traceAsyncOperationCompleted(int operationId) override;
    bool trackingAsyncCalls() const override { return m_maxAsyncCallStackDepth; }

    void didUpdatePromise(const String16& eventType, PassOwnPtr<protocol::Debugger::PromiseDetails>);
    void reset() override;

    // Interface for V8DebuggerImpl
    SkipPauseRequest didPause(v8::Local<v8::Context>, v8::Local<v8::Object> callFrames, v8::Local<v8::Value> exception, const protocol::Vector<String16>& hitBreakpoints, bool isPromiseRejection);
    void didContinue();
    void didParseSource(const V8DebuggerParsedScript&);
    bool v8AsyncTaskEventsEnabled() const;
    void didReceiveV8AsyncTaskEvent(v8::Local<v8::Context>, const String16& eventType, const String16& eventName, int id);
    bool v8PromiseEventsEnabled() const;
    void didReceiveV8PromiseEvent(v8::Local<v8::Context>, v8::Local<v8::Object> promise, v8::Local<v8::Value> parentPromise, int status);

    v8::Isolate* isolate() { return m_isolate; }
    int maxAsyncCallChainDepth() { return m_maxAsyncCallStackDepth; }
    V8StackTraceImpl* currentAsyncCallChain();

private:
    bool checkEnabled(ErrorString*);
    void enable();

    SkipPauseRequest shouldSkipExceptionPause();
    SkipPauseRequest shouldSkipStepPause();

    void schedulePauseOnNextStatementIfSteppingInto();

    PassOwnPtr<protocol::Array<protocol::Debugger::CallFrame>> currentCallFrames();
    PassOwnPtr<protocol::Runtime::StackTrace> currentAsyncStackTrace();

    void clearCurrentAsyncOperation();
    void resetAsyncCallTracker();

    void changeJavaScriptRecursionLevel(int step);

    void setPauseOnExceptionsImpl(ErrorString*, int);

    PassOwnPtr<protocol::Debugger::Location> resolveBreakpoint(const String16& breakpointId, const String16& scriptId, const ScriptBreakpoint&, BreakpointSource);
    void removeBreakpoint(const String16& breakpointId);
    void clearStepIntoAsync();
    bool assertPaused(ErrorString*);
    void clearBreakDetails();

    bool isCallStackEmptyOrBlackboxed();
    bool isTopCallFrameBlackboxed();
    bool isCallFrameWithUnknownScriptOrBlackboxed(JavaScriptCallFrame*);

    void internalSetAsyncCallStackDepth(int);
    void increaseCachedSkipStackGeneration();

    using ScriptsMap = protocol::HashMap<String16, V8DebuggerScript>;
    using BreakpointIdToDebuggerBreakpointIdsMap = protocol::HashMap<String16, protocol::Vector<String16>>;
    using DebugServerBreakpointToBreakpointIdAndSourceMap = protocol::HashMap<String16, std::pair<String16, BreakpointSource>>;
    using MuteBreakpoins = protocol::HashMap<String16, std::pair<String16, int>>;

    enum DebuggerStep {
        NoStep = 0,
        StepInto,
        StepOver,
        StepOut
    };

    InjectedScriptManager* m_injectedScriptManager;
    V8DebuggerImpl* m_debugger;
    int m_contextGroupId;
    bool m_enabled;
    protocol::DictionaryValue* m_state;
    protocol::Frontend::Debugger* m_frontend;
    v8::Isolate* m_isolate;
    v8::Global<v8::Context> m_pausedContext;
    v8::Global<v8::Object> m_currentCallStack;
    ScriptsMap m_scripts;
    BreakpointIdToDebuggerBreakpointIdsMap m_breakpointIdToDebuggerBreakpointIds;
    DebugServerBreakpointToBreakpointIdAndSourceMap m_serverBreakpoints;
    String16 m_continueToLocationBreakpointId;
    String16 m_breakReason;
    OwnPtr<protocol::DictionaryValue> m_breakAuxData;
    DebuggerStep m_scheduledDebuggerStep;
    bool m_skipNextDebuggerStepOut;
    bool m_javaScriptPauseScheduled;
    bool m_steppingFromFramework;
    bool m_pausingOnNativeEvent;
    bool m_pausingOnAsyncOperation;

    int m_skippedStepFrameCount;
    int m_recursionLevelForStepOut;
    int m_recursionLevelForStepFrame;
    bool m_skipAllPauses;

    // This field must be destroyed before the listeners set above.
    OwnPtr<V8AsyncCallTracker> m_v8AsyncCallTracker;
    OwnPtr<PromiseTracker> m_promiseTracker;

    using AsyncOperationIdToStackTrace = protocol::HashMap<int, OwnPtr<V8StackTraceImpl>>;
    AsyncOperationIdToStackTrace m_asyncOperations;
    int m_lastAsyncOperationId;
    protocol::HashSet<int> m_asyncOperationNotifications;
    protocol::HashSet<int> m_asyncOperationBreakpoints;
    protocol::HashSet<int> m_pausingAsyncOperations;
    int m_maxAsyncCallStackDepth;
    OwnPtr<V8StackTraceImpl> m_currentAsyncCallChain;
    unsigned m_nestedAsyncCallCount;
    int m_currentAsyncOperationId;
    bool m_pendingTraceAsyncOperationCompleted;
    bool m_startingStepIntoAsync;
    protocol::HashMap<String16, protocol::Vector<std::pair<int, int>>> m_blackboxedPositions;
};

} // namespace blink


#endif // V8DebuggerAgentImpl_h
