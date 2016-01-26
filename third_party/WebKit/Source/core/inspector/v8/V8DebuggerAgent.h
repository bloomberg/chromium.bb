// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8DebuggerAgent_h
#define V8DebuggerAgent_h

#include "core/CoreExport.h"
#include "core/InspectorFrontend.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class InjectedScript;
class InjectedScriptManager;
class JSONObject;
class RemoteCallFrameId;
class ScriptAsyncCallStack;
class V8Debugger;

typedef String ErrorString;

class CORE_EXPORT V8DebuggerAgent {
public:
    // FIXME: remove this enum from public interface once InjectedScriptHost is moved to the implementation.
    enum BreakpointSource {
        UserBreakpointSource,
        DebugCommandBreakpointSource,
        MonitorCommandBreakpointSource
    };

    static const char backtraceObjectGroup[];

    // FIXME: injected script management should be an implementation details.
    static PassOwnPtr<V8DebuggerAgent> create(InjectedScriptManager*, V8Debugger*, int contextGroupId);
    virtual ~V8DebuggerAgent() { }

    // Protocol methods.
    virtual void enable(ErrorString*) = 0;
    virtual void disable(ErrorString*) = 0;
    virtual void setBreakpointsActive(ErrorString*, bool in_active) = 0;
    virtual void setSkipAllPauses(ErrorString*, bool in_skipped) = 0;
    virtual void setBreakpointByUrl(ErrorString*, int in_lineNumber, const String* in_url, const String* in_urlRegex, const int* in_columnNumber, const String* in_condition, TypeBuilder::Debugger::BreakpointId* out_breakpointId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location>>& out_locations) = 0;
    virtual void setBreakpoint(ErrorString*, const RefPtr<JSONObject>& in_location, const String* in_condition, TypeBuilder::Debugger::BreakpointId* out_breakpointId, RefPtr<TypeBuilder::Debugger::Location>& out_actualLocation) = 0;
    virtual void removeBreakpoint(ErrorString*, const String& in_breakpointId) = 0;
    virtual void continueToLocation(ErrorString*, const RefPtr<JSONObject>& in_location, const bool* in_interstatementLocation) = 0;
    virtual void stepOver(ErrorString*) = 0;
    virtual void stepInto(ErrorString*) = 0;
    virtual void stepOut(ErrorString*) = 0;
    virtual void pause(ErrorString*) = 0;
    virtual void resume(ErrorString*) = 0;
    virtual void stepIntoAsync(ErrorString*) = 0;
    virtual void searchInContent(ErrorString*, const String& in_scriptId, const String& in_query, const bool* in_caseSensitive, const bool* in_isRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::SearchMatch>>& out_result) = 0;
    virtual void canSetScriptSource(ErrorString*, bool* out_result) = 0;
    virtual void setScriptSource(ErrorString*, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>& errorData, const String& in_scriptId, const String& in_scriptSource, const bool* in_preview, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& opt_out_callFrames, TypeBuilder::OptOutput<bool>* opt_out_stackChanged, RefPtr<TypeBuilder::Debugger::StackTrace>& opt_out_asyncStackTrace) = 0;
    virtual void restartFrame(ErrorString*, const String& in_callFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& out_callFrames, RefPtr<TypeBuilder::Debugger::StackTrace>& opt_out_asyncStackTrace) = 0;
    virtual void getScriptSource(ErrorString*, const String& in_scriptId, String* out_scriptSource) = 0;
    virtual void getFunctionDetails(ErrorString*, const String& in_functionId, RefPtr<TypeBuilder::Debugger::FunctionDetails>& out_details) = 0;
    virtual void getGeneratorObjectDetails(ErrorString*, const String& in_objectId, RefPtr<TypeBuilder::Debugger::GeneratorObjectDetails>& out_details) = 0;
    virtual void getCollectionEntries(ErrorString*, const String& in_objectId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CollectionEntry>>& out_entries) = 0;
    virtual void setPauseOnExceptions(ErrorString*, const String& in_state) = 0;
    virtual void evaluateOnCallFrame(ErrorString*, const String& in_callFrameId, const String& in_expression, const String* in_objectGroup, const bool* in_includeCommandLineAPI, const bool* in_doNotPauseOnExceptionsAndMuteConsole, const bool* in_returnByValue, const bool* in_generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>& out_result, TypeBuilder::OptOutput<bool>* opt_out_wasThrown, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& opt_out_exceptionDetails) = 0;
    virtual void compileScript(ErrorString*, const String& in_expression, const String& in_sourceURL, bool in_persistScript, int in_executionContextId, TypeBuilder::OptOutput<TypeBuilder::Debugger::ScriptId>* opt_out_scriptId, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& opt_out_exceptionDetails) = 0;
    virtual void runScript(ErrorString*, const String& in_scriptId, int in_executionContextId, const String* in_objectGroup, const bool* in_doNotPauseOnExceptionsAndMuteConsole, RefPtr<TypeBuilder::Runtime::RemoteObject>& out_result, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& opt_out_exceptionDetails) = 0;
    virtual void setVariableValue(ErrorString*, int in_scopeNumber, const String& in_variableName, const RefPtr<JSONObject>& in_newValue, const String* in_callFrameId, const String* in_functionObjectId) = 0;
    virtual void getStepInPositions(ErrorString*, const String& in_callFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location>>& opt_out_stepInPositions) = 0;
    virtual void getBacktrace(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& out_callFrames, RefPtr<TypeBuilder::Debugger::StackTrace>& opt_out_asyncStackTrace) = 0;
    virtual void skipStackFrames(ErrorString*, const String* in_script, const bool* in_skipContentScripts) = 0;
    virtual void setAsyncCallStackDepth(ErrorString*, int in_maxDepth) = 0;
    virtual void enablePromiseTracker(ErrorString*, const bool* in_captureStacks) = 0;
    virtual void disablePromiseTracker(ErrorString*) = 0;
    virtual void getPromiseById(ErrorString*, int in_promiseId, const String* in_objectGroup, RefPtr<TypeBuilder::Runtime::RemoteObject>& out_promise) = 0;
    virtual void flushAsyncOperationEvents(ErrorString*) = 0;
    virtual void setAsyncOperationBreakpoint(ErrorString*, int in_operationId) = 0;
    virtual void removeAsyncOperationBreakpoint(ErrorString*, int in_operationId) = 0;

    // State management methods.
    virtual void setInspectorState(PassRefPtr<JSONObject>) = 0;
    virtual void setFrontend(InspectorFrontend::Debugger*) = 0;
    virtual void clearFrontend() = 0;
    virtual void restore() = 0;

    // API for the embedder to report native activities.
    virtual void schedulePauseOnNextStatement(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data) = 0;
    virtual void cancelPauseOnNextStatement() = 0;
    virtual bool canBreakProgram() = 0;
    virtual void breakProgram(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data) = 0;
    virtual void breakProgramOnException(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data) = 0;
    virtual void willExecuteScript(int scriptId) = 0;
    virtual void didExecuteScript() = 0;
    virtual void reset() = 0;

    virtual bool isPaused() = 0;
    virtual bool enabled() = 0;
    virtual V8Debugger& debugger() = 0;

    virtual void setBreakpoint(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource, const String& condition = String()) = 0;
    virtual void removeBreakpoint(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource) = 0;

    // Async call stacks implementation
    virtual PassRefPtr<ScriptAsyncCallStack> currentAsyncStackTraceForConsole() = 0;
    static const int unknownAsyncOperationId;
    virtual int traceAsyncOperationStarting(const String& description) = 0;
    virtual void traceAsyncCallbackStarting(int operationId) = 0;
    virtual void traceAsyncCallbackCompleted() = 0;
    virtual void traceAsyncOperationCompleted(int operationId) = 0;
    virtual bool trackingAsyncCalls() const = 0;
};

} // namespace blink


#endif // V8DebuggerAgent_h
