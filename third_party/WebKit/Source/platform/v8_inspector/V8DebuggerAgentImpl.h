// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8DebuggerAgentImpl_h
#define V8DebuggerAgentImpl_h

#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/inspector_protocol/Frontend.h"
#include "platform/v8_inspector/ScriptBreakpoint.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/public/V8DebuggerAgent.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"

namespace blink {

class AsyncCallChain;
class AsyncCallStack;
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

typedef String ErrorString;

using protocol::Maybe;

class V8DebuggerAgentImpl : public V8DebuggerAgent {
    WTF_MAKE_NONCOPYABLE(V8DebuggerAgentImpl);
    USING_FAST_MALLOC(V8DebuggerAgentImpl);
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
    void disable(ErrorString*);

    bool isPaused() override;

    // Part of the protocol.
    void enable(ErrorString*) override;
    void setBreakpointsActive(ErrorString*, bool active) override;
    void setSkipAllPauses(ErrorString*, bool skipped) override;

    void setBreakpointByUrl(ErrorString*,
        int lineNumber,
        const Maybe<String>& optionalURL,
        const Maybe<String>& optionalURLRegex,
        const Maybe<int>& optionalColumnNumber,
        const Maybe<String>& optionalCondition,
        protocol::Debugger::BreakpointId*,
        OwnPtr<protocol::Array<protocol::Debugger::Location>>* locations) override;
    void setBreakpoint(ErrorString*,
        PassOwnPtr<protocol::Debugger::Location>,
        const Maybe<String>& optionalCondition,
        protocol::Debugger::BreakpointId*,
        OwnPtr<protocol::Debugger::Location>* actualLocation) override;
    void removeBreakpoint(ErrorString*, const String& breakpointId) override;
    void continueToLocation(ErrorString*,
        PassOwnPtr<protocol::Debugger::Location>,
        const Maybe<bool>& interstateLocationOpt) override;
    void getStepInPositions(ErrorString*,
        const String& callFrameId,
        Maybe<protocol::Array<protocol::Debugger::Location>>* positions) override;
    void getBacktrace(ErrorString*,
        OwnPtr<protocol::Array<protocol::Debugger::CallFrame>>*,
        Maybe<protocol::Debugger::StackTrace>*) override;
    void searchInContent(ErrorString*,
        const String& scriptId,
        const String& query,
        const Maybe<bool>& optionalCaseSensitive,
        const Maybe<bool>& optionalIsRegex,
        OwnPtr<protocol::Array<protocol::Debugger::SearchMatch>>*) override;
    void canSetScriptSource(ErrorString*, bool* result) override { *result = true; }
    void setScriptSource(ErrorString*,
        const String& inScriptId,
        const String& inScriptSource,
        const Maybe<bool>& inPreview,
        Maybe<protocol::Array<protocol::Debugger::CallFrame>>* optOutCallFrames,
        Maybe<bool>* optOutStackChanged,
        Maybe<protocol::Debugger::StackTrace>* optOutAsyncStackTrace,
        Maybe<protocol::Debugger::SetScriptSourceError>* optOutCompileError) override;
    void restartFrame(ErrorString*,
        const String& callFrameId,
        OwnPtr<protocol::Array<protocol::Debugger::CallFrame>>* newCallFrames,
        Maybe<protocol::Debugger::StackTrace>* asyncStackTrace) override;
    void getScriptSource(ErrorString*, const String& scriptId, String* scriptSource) override;
    void getFunctionDetails(ErrorString*,
        const String& functionId,
        OwnPtr<protocol::Debugger::FunctionDetails>*) override;
    void getGeneratorObjectDetails(ErrorString*,
        const String& objectId,
        OwnPtr<protocol::Debugger::GeneratorObjectDetails>*) override;
    void getCollectionEntries(ErrorString*,
        const String& objectId,
        OwnPtr<protocol::Array<protocol::Debugger::CollectionEntry>>*) override;
    void pause(ErrorString*) override;
    void resume(ErrorString*) override;
    void stepOver(ErrorString*) override;
    void stepInto(ErrorString*) override;
    void stepOut(ErrorString*) override;
    void stepIntoAsync(ErrorString*) override;
    void setPauseOnExceptions(ErrorString*, const String& pauseState) override;
    void evaluateOnCallFrame(ErrorString*,
        const String& callFrameId,
        const String& expression,
        const Maybe<String>& objectGroup,
        const Maybe<bool>& includeCommandLineAPI,
        const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole,
        const Maybe<bool>& returnByValue,
        const Maybe<bool>& generatePreview,
        OwnPtr<protocol::Runtime::RemoteObject>* result,
        Maybe<bool>* wasThrown,
        Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void setVariableValue(ErrorString*,
        int scopeNumber,
        const String& variableName,
        PassOwnPtr<protocol::Runtime::CallArgument> newValue,
        const Maybe<String>& callFrame,
        const Maybe<String>& functionObjectId) override;
    void setAsyncCallStackDepth(ErrorString*, int depth) override;
    void enablePromiseTracker(ErrorString*,
        const Maybe<bool>& captureStacks) override;
    void disablePromiseTracker(ErrorString*) override;
    void getPromiseById(ErrorString*,
        int promiseId,
        const Maybe<String>& objectGroup,
        OwnPtr<protocol::Runtime::RemoteObject>* promise) override;
    void flushAsyncOperationEvents(ErrorString*) override;
    void setAsyncOperationBreakpoint(ErrorString*, int operationId) override;
    void removeAsyncOperationBreakpoint(ErrorString*, int operationId) override;
    void setBlackboxedRanges(ErrorString*,
        const String& scriptId,
        PassOwnPtr<protocol::Array<protocol::Debugger::ScriptPosition>> positions) override;

    void schedulePauseOnNextStatement(const String& breakReason, PassOwnPtr<protocol::DictionaryValue> data) override;
    void cancelPauseOnNextStatement() override;
    bool canBreakProgram() override;
    void breakProgram(const String& breakReason, PassOwnPtr<protocol::DictionaryValue> data) override;
    void breakProgramOnException(const String& breakReason, PassOwnPtr<protocol::DictionaryValue> data) override;
    void willExecuteScript(int scriptId) override;
    void didExecuteScript() override;

    bool enabled() override;
    V8DebuggerImpl& debugger() override { return *m_debugger; }

    void setBreakpointAt(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource, const String& condition = String());
    void removeBreakpointAt(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource);

    // Async call stacks implementation
    int traceAsyncOperationStarting(const String& description) override;
    void traceAsyncCallbackStarting(int operationId) override;
    void traceAsyncCallbackCompleted() override;
    void traceAsyncOperationCompleted(int operationId) override;
    bool trackingAsyncCalls() const override { return m_maxAsyncCallStackDepth; }

    void didUpdatePromise(const String& eventType, PassOwnPtr<protocol::Debugger::PromiseDetails>);
    void reset() override;

    // Interface for V8DebuggerImpl
    SkipPauseRequest didPause(v8::Local<v8::Context>, v8::Local<v8::Object> callFrames, v8::Local<v8::Value> exception, const Vector<String>& hitBreakpoints, bool isPromiseRejection);
    void didContinue();
    void didParseSource(const V8DebuggerParsedScript&);
    bool v8AsyncTaskEventsEnabled() const;
    void didReceiveV8AsyncTaskEvent(v8::Local<v8::Context>, const String& eventType, const String& eventName, int id);
    bool v8PromiseEventsEnabled() const;
    void didReceiveV8PromiseEvent(v8::Local<v8::Context>, v8::Local<v8::Object> promise, v8::Local<v8::Value> parentPromise, int status);

    v8::Isolate* isolate() { return m_isolate; }
    PassOwnPtr<V8StackTraceImpl> currentAsyncStackTraceForRuntime();

private:
    bool checkEnabled(ErrorString*);
    void enable();

    SkipPauseRequest shouldSkipExceptionPause();
    SkipPauseRequest shouldSkipStepPause();
    bool isMuteBreakpointInstalled();

    void schedulePauseOnNextStatementIfSteppingInto();

    PassOwnPtr<protocol::Array<protocol::Debugger::CallFrame>> currentCallFrames();
    PassOwnPtr<protocol::Debugger::StackTrace> currentAsyncStackTrace();

    void clearCurrentAsyncOperation();
    void resetAsyncCallTracker();

    void changeJavaScriptRecursionLevel(int step);

    void setPauseOnExceptionsImpl(ErrorString*, int);

    PassOwnPtr<protocol::Debugger::Location> resolveBreakpoint(const String& breakpointId, const String& scriptId, const ScriptBreakpoint&, BreakpointSource);
    void removeBreakpoint(const String& breakpointId);
    void clearStepIntoAsync();
    bool assertPaused(ErrorString*);
    void clearBreakDetails();

    bool isCallStackEmptyOrBlackboxed();
    bool isTopCallFrameBlackboxed();
    bool isCallFrameWithUnknownScriptOrBlackboxed(PassRefPtr<JavaScriptCallFrame>);

    void internalSetAsyncCallStackDepth(int);
    void increaseCachedSkipStackGeneration();

    using ScriptsMap = HashMap<String, V8DebuggerScript>;
    using BreakpointIdToDebuggerBreakpointIdsMap = HashMap<String, Vector<String>>;
    using DebugServerBreakpointToBreakpointIdAndSourceMap = HashMap<String, std::pair<String, BreakpointSource>>;
    using MuteBreakpoins = HashMap<String, std::pair<String, int>>;

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
    MuteBreakpoins m_muteBreakpoints;
    String m_continueToLocationBreakpointId;
    String m_breakReason;
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

    using AsyncOperationIdToAsyncCallChain = HashMap<int, RefPtr<AsyncCallChain>>;
    AsyncOperationIdToAsyncCallChain m_asyncOperations;
    int m_lastAsyncOperationId;
    ListHashSet<int> m_asyncOperationNotifications;
    HashSet<int> m_asyncOperationBreakpoints;
    HashSet<int> m_pausingAsyncOperations;
    unsigned m_maxAsyncCallStackDepth;
    RefPtr<AsyncCallChain> m_currentAsyncCallChain;
    unsigned m_nestedAsyncCallCount;
    int m_currentAsyncOperationId;
    bool m_pendingTraceAsyncOperationCompleted;
    bool m_startingStepIntoAsync;
    HashMap<String, Vector<std::pair<int, int>>> m_blackboxedPositions;
};

} // namespace blink


#endif // V8DebuggerAgentImpl_h
