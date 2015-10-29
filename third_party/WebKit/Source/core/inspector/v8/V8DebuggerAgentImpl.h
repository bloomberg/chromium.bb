// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8DebuggerAgentImpl_h
#define V8DebuggerAgentImpl_h

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/PromiseTracker.h"
#include "core/inspector/v8/ScriptBreakpoint.h"
#include "core/inspector/v8/V8DebuggerAgent.h"
#include "core/inspector/v8/V8DebuggerImpl.h"
#include "core/inspector/v8/V8DebuggerListener.h"
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
class JSONObject;
class RemoteCallFrameId;
class ScriptAsyncCallStack;
class ScriptRegexp;
class V8AsyncCallTracker;

typedef String ErrorString;

class CORE_EXPORT V8DebuggerAgentImpl
    : public V8DebuggerAgent
    , public V8DebuggerListener
    , public InspectorBackendDispatcher::DebuggerCommandHandler
    , public PromiseTracker::Listener {
    WTF_MAKE_NONCOPYABLE(V8DebuggerAgentImpl);
    USING_FAST_MALLOC(V8DebuggerAgentImpl);
public:
    V8DebuggerAgentImpl(InjectedScriptManager*, V8DebuggerImpl*, int contextGroupId);
    ~V8DebuggerAgentImpl() override;
    DECLARE_TRACE();

    void setInspectorState(InspectorState* state) override { m_state = state; }
    void setFrontend(InspectorFrontend::Debugger* frontend) override { m_frontend = frontend; }
    void clearFrontend() override;
    void restore() override;
    void disable(ErrorString*) final;

    bool isPaused() override;

    // Part of the protocol.
    void enable(ErrorString*) override;
    void setBreakpointsActive(ErrorString*, bool active) final;
    void setSkipAllPauses(ErrorString*, bool skipped) final;

    void setBreakpointByUrl(ErrorString*, int lineNumber, const String* optionalURL, const String* optionalURLRegex, const int* optionalColumnNumber, const String* optionalCondition, TypeBuilder::Debugger::BreakpointId*, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location>>& locations) final;
    void setBreakpoint(ErrorString*, const RefPtr<JSONObject>& location, const String* optionalCondition, TypeBuilder::Debugger::BreakpointId*, RefPtr<TypeBuilder::Debugger::Location>& actualLocation) final;
    void removeBreakpoint(ErrorString*, const String& breakpointId) final;
    void continueToLocation(ErrorString*, const RefPtr<JSONObject>& location, const bool* interstateLocationOpt) final;
    void getStepInPositions(ErrorString*, const String& callFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location>>& positions) final;
    void getBacktrace(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>&, RefPtr<TypeBuilder::Debugger::StackTrace>&) final;
    void searchInContent(ErrorString*, const String& scriptId, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::SearchMatch>>&) final;
    void canSetScriptSource(ErrorString*, bool* result) final { *result = true; }
    void setScriptSource(ErrorString*, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>&, const String& scriptId, const String& newContent, const bool* preview, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& newCallFrames, TypeBuilder::OptOutput<bool>* stackChanged, RefPtr<TypeBuilder::Debugger::StackTrace>& asyncStackTrace) final;
    void restartFrame(ErrorString*, const String& callFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& newCallFrames, RefPtr<TypeBuilder::Debugger::StackTrace>& asyncStackTrace) final;
    void getScriptSource(ErrorString*, const String& scriptId, String* scriptSource) final;
    void getFunctionDetails(ErrorString*, const String& functionId, RefPtr<TypeBuilder::Debugger::FunctionDetails>&) final;
    void getGeneratorObjectDetails(ErrorString*, const String& objectId, RefPtr<TypeBuilder::Debugger::GeneratorObjectDetails>&) final;
    void getCollectionEntries(ErrorString*, const String& objectId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CollectionEntry>>&) final;
    void pause(ErrorString*) final;
    void resume(ErrorString*) final;
    void stepOver(ErrorString*) final;
    void stepInto(ErrorString*) final;
    void stepOut(ErrorString*) final;
    void stepIntoAsync(ErrorString*) final;
    void setPauseOnExceptions(ErrorString*, const String& pauseState) final;
    void evaluateOnCallFrame(ErrorString*,
        const String& callFrameId,
        const String& expression,
        const String* objectGroup,
        const bool* includeCommandLineAPI,
        const bool* doNotPauseOnExceptionsAndMuteConsole,
        const bool* returnByValue,
        const bool* generatePreview,
        RefPtr<TypeBuilder::Runtime::RemoteObject>& result,
        TypeBuilder::OptOutput<bool>* wasThrown,
        RefPtr<TypeBuilder::Debugger::ExceptionDetails>&) final;
    void compileScript(ErrorString*, const String& expression, const String& sourceURL, bool persistScript, int executionContextId, TypeBuilder::OptOutput<TypeBuilder::Debugger::ScriptId>*, RefPtr<TypeBuilder::Debugger::ExceptionDetails>&) override;
    void runScript(ErrorString*, const TypeBuilder::Debugger::ScriptId&, int executionContextId, const String* objectGroup, const bool* doNotPauseOnExceptionsAndMuteConsole, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, RefPtr<TypeBuilder::Debugger::ExceptionDetails>&) override;
    void setVariableValue(ErrorString*, int in_scopeNumber, const String& in_variableName, const RefPtr<JSONObject>& in_newValue, const String* in_callFrame, const String* in_functionObjectId) final;
    void skipStackFrames(ErrorString*, const String* pattern, const bool* skipContentScripts) final;
    void setAsyncCallStackDepth(ErrorString*, int depth) final;
    void enablePromiseTracker(ErrorString*, const bool* captureStacks) final;
    void disablePromiseTracker(ErrorString*) final;
    void getPromiseById(ErrorString*, int promiseId, const String* objectGroup, RefPtr<TypeBuilder::Runtime::RemoteObject>& promise) final;
    void flushAsyncOperationEvents(ErrorString*) final;
    void setAsyncOperationBreakpoint(ErrorString*, int operationId) final;
    void removeAsyncOperationBreakpoint(ErrorString*, int operationId) final;

    void schedulePauseOnNextStatement(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data) override;
    void cancelPauseOnNextStatement() override;
    bool canBreakProgram() override;
    void breakProgram(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data) override;
    void willExecuteScript(int scriptId) override;
    void didExecuteScript() override;

    bool enabled() override;
    V8DebuggerImpl& debugger() override { return *m_debugger; }

    void setBreakpoint(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource, const String& condition = String()) override;
    void removeBreakpoint(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource) override;

    // Async call stacks implementation
    PassRefPtrWillBeRawPtr<ScriptAsyncCallStack> currentAsyncStackTraceForConsole() override;
    int traceAsyncOperationStarting(const String& description) override;
    void traceAsyncCallbackStarting(int operationId) override;
    void traceAsyncCallbackCompleted() override;
    void traceAsyncOperationCompleted(int operationId) override;
    bool trackingAsyncCalls() const override { return m_maxAsyncCallStackDepth; }

    // PromiseTracker::Listener
    void didUpdatePromise(InspectorFrontend::Debugger::EventType::Enum, PassRefPtr<TypeBuilder::Debugger::PromiseDetails>) final;

    void reset() override;

private:
    bool checkEnabled(ErrorString*);
    void enable();

    SkipPauseRequest didPause(v8::Local<v8::Context>, v8::Local<v8::Object> callFrames, v8::Local<v8::Value> exception, const Vector<String>& hitBreakpoints, bool isPromiseRejection) final;
    void didContinue() final;

    SkipPauseRequest shouldSkipExceptionPause();
    SkipPauseRequest shouldSkipStepPause();

    void schedulePauseOnNextStatementIfSteppingInto();

    PassRefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>> currentCallFrames();
    PassRefPtr<TypeBuilder::Debugger::StackTrace> currentAsyncStackTrace();
    bool callStackForId(ErrorString*, const RemoteCallFrameId&, v8::Local<v8::Object>* callStack, bool* isAsync);

    void clearCurrentAsyncOperation();
    void resetAsyncCallTracker();

    void changeJavaScriptRecursionLevel(int step);

    void didParseSource(const ParsedScript&) final;
    bool v8AsyncTaskEventsEnabled() const final;
    void didReceiveV8AsyncTaskEvent(v8::Local<v8::Context>, const String& eventType, const String& eventName, int id) final;
    bool v8PromiseEventsEnabled() const final;
    void didReceiveV8PromiseEvent(v8::Local<v8::Context>, v8::Local<v8::Object> promise, v8::Local<v8::Value> parentPromise, int status) final;

    void setPauseOnExceptionsImpl(ErrorString*, int);

    PassRefPtr<TypeBuilder::Debugger::Location> resolveBreakpoint(const String& breakpointId, const String& scriptId, const ScriptBreakpoint&, BreakpointSource);
    void removeBreakpoint(const String& breakpointId);
    void clearStepIntoAsync();
    bool assertPaused(ErrorString*);
    void clearBreakDetails();

    String sourceMapURLForScript(const Script&, CompileResult);

    bool isCallStackEmptyOrBlackboxed();
    bool isTopCallFrameBlackboxed();
    bool isCallFrameWithUnknownScriptOrBlackboxed(PassRefPtr<JavaScriptCallFrame>);

    void internalSetAsyncCallStackDepth(int);
    void increaseCachedSkipStackGeneration();
    PassRefPtr<TypeBuilder::Debugger::ExceptionDetails> createExceptionDetails(v8::Isolate*, v8::Local<v8::Message>);

    typedef HashMap<String, Script> ScriptsMap;
    typedef HashMap<String, Vector<String>> BreakpointIdToDebuggerBreakpointIdsMap;
    typedef HashMap<String, std::pair<String, BreakpointSource>> DebugServerBreakpointToBreakpointIdAndSourceMap;

    enum DebuggerStep {
        NoStep = 0,
        StepInto,
        StepOver,
        StepOut
    };

    RawPtrWillBeWeakPersistent<InjectedScriptManager> m_injectedScriptManager;
    V8DebuggerImpl* m_debugger;
    int m_contextGroupId;
    bool m_enabled;
    InspectorState* m_state;
    InspectorFrontend::Debugger* m_frontend;
    v8::Isolate* m_isolate;
    RefPtr<ScriptState> m_pausedScriptState;
    v8::Global<v8::Object> m_currentCallStack;
    ScriptsMap m_scripts;
    BreakpointIdToDebuggerBreakpointIdsMap m_breakpointIdToDebuggerBreakpointIds;
    DebugServerBreakpointToBreakpointIdAndSourceMap m_serverBreakpoints;
    String m_continueToLocationBreakpointId;
    InspectorFrontend::Debugger::Reason::Enum m_breakReason;
    RefPtr<JSONObject> m_breakAuxData;
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
    bool m_skipContentScripts;
    OwnPtr<ScriptRegexp> m_cachedSkipStackRegExp;
    unsigned m_cachedSkipStackGeneration;
    // This field must be destroyed before the listeners set above.
    OwnPtrWillBePersistent<V8AsyncCallTracker> m_v8AsyncCallTracker;
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
    V8GlobalValueMap<String, v8::Script, v8::kNotWeak> m_compiledScripts;
};

} // namespace blink


#endif // V8DebuggerAgentImpl_h
