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

#ifndef InspectorDebuggerAgent_h
#define InspectorDebuggerAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "platform/v8_inspector/public/V8DebuggerAgent.h"

namespace blink {

using protocol::Array;

class CORE_EXPORT InspectorDebuggerAgent
    : public InspectorBaseAgent<InspectorDebuggerAgent, protocol::Frontend::Debugger>
    , public protocol::Dispatcher::DebuggerCommandHandler {
public:
    ~InspectorDebuggerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    // protocol::Dispatcher::DebuggerCommandHandler implementation.
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void setBreakpointsActive(ErrorString*, bool active) override;
    void setSkipAllPauses(ErrorString*, bool skipped) override;
    void setBreakpointByUrl(ErrorString*, int lineNumber, const OptionalValue<String>& url, const OptionalValue<String>& urlRegex, const OptionalValue<int>& columnNumber, const OptionalValue<String>& condition, String* breakpointId, OwnPtr<protocol::Array<protocol::Debugger::Location>>* locations) override;
    void setBreakpoint(ErrorString*, PassOwnPtr<protocol::Debugger::Location>, const OptionalValue<String>& condition, String* breakpointId, OwnPtr<protocol::Debugger::Location>* actualLocation) override;
    void removeBreakpoint(ErrorString*, const String& breakpointId) override;
    void continueToLocation(ErrorString*, PassOwnPtr<protocol::Debugger::Location>, const OptionalValue<bool>& interstatementLocation) override;
    void stepOver(ErrorString*) override;
    void stepInto(ErrorString*) override;
    void stepOut(ErrorString*) override;
    void pause(ErrorString*) override;
    void resume(ErrorString*) override;
    void stepIntoAsync(ErrorString*) override;
    void searchInContent(ErrorString*, const String& scriptId, const String& query, const OptionalValue<bool>& caseSensitive, const OptionalValue<bool>& isRegex, OwnPtr<protocol::Array<protocol::Debugger::SearchMatch>>* result) override;
    void canSetScriptSource(ErrorString*, bool* result) override;
    void setScriptSource(ErrorString*, const String& scriptId, const String& scriptSource, const OptionalValue<bool>& preview, OwnPtr<protocol::Array<protocol::Debugger::CallFrame>>* callFrames, OptionalValue<bool>* stackChanged, OwnPtr<protocol::Debugger::StackTrace>* asyncStackTrace, OwnPtr<protocol::Debugger::SetScriptSourceError>* compileError) override;
    void restartFrame(ErrorString*, const String& callFrameId, OwnPtr<protocol::Array<protocol::Debugger::CallFrame>>* callFrames, OwnPtr<protocol::Debugger::StackTrace>* asyncStackTrace) override;
    void getScriptSource(ErrorString*, const String& scriptId, String* scriptSource) override;
    void getFunctionDetails(ErrorString*, const String& functionId, OwnPtr<protocol::Debugger::FunctionDetails>*) override;
    void getGeneratorObjectDetails(ErrorString*, const String& objectId, OwnPtr<protocol::Debugger::GeneratorObjectDetails>*) override;
    void getCollectionEntries(ErrorString*, const String& objectId, OwnPtr<protocol::Array<protocol::Debugger::CollectionEntry>>* entries) override;
    void setPauseOnExceptions(ErrorString*, const String& state) override;
    void evaluateOnCallFrame(ErrorString*, const String& callFrameId, const String& expression, const OptionalValue<String>& objectGroup, const OptionalValue<bool>& includeCommandLineAPI, const OptionalValue<bool>& doNotPauseOnExceptionsAndMuteConsole, const OptionalValue<bool>& returnByValue, const OptionalValue<bool>& generatePreview, OwnPtr<protocol::Runtime::RemoteObject>* result, OptionalValue<bool>* wasThrown, OwnPtr<protocol::Runtime::ExceptionDetails>*) override;
    void setVariableValue(ErrorString*, int scopeNumber, const String& variableName, PassOwnPtr<protocol::Runtime::CallArgument> newValue, const OptionalValue<String>& callFrameId, const OptionalValue<String>& functionObjectId) override;
    void getStepInPositions(ErrorString*, const String& callFrameId, OwnPtr<protocol::Array<protocol::Debugger::Location>>* stepInPositions) override;
    void getBacktrace(ErrorString*, OwnPtr<protocol::Array<protocol::Debugger::CallFrame>>* callFrames, OwnPtr<protocol::Debugger::StackTrace>* asyncStackTrace) override;
    void setAsyncCallStackDepth(ErrorString*, int maxDepth) override;
    void enablePromiseTracker(ErrorString*, const OptionalValue<bool>& captureStacks) override;
    void disablePromiseTracker(ErrorString*) override;
    void getPromiseById(ErrorString*, int promiseId, const OptionalValue<String>& objectGroup, OwnPtr<protocol::Runtime::RemoteObject>* promise) override;
    void flushAsyncOperationEvents(ErrorString*) override;
    void setAsyncOperationBreakpoint(ErrorString*, int operationId) override;
    void removeAsyncOperationBreakpoint(ErrorString*, int operationId) override;
    void setBlackboxedRanges(ErrorString*, const String& scriptId, PassOwnPtr<protocol::Array<protocol::Debugger::ScriptPosition>> positions) override;

    // Called by InspectorInstrumentation.
    bool isPaused();
    void scriptExecutionBlockedByCSP(const String& directiveText);
    void willExecuteScript(int scriptId);
    void didExecuteScript();

    // InspectorBaseAgent overrides.
    void setState(PassRefPtr<JSONObject>) override;
    void init() override;
    void setFrontend(protocol::Frontend*) override;
    void clearFrontend() override;
    void restore() override;

    V8DebuggerAgent* v8Agent() const { return m_v8DebuggerAgent.get(); }

    virtual void muteConsole() = 0;
    virtual void unmuteConsole() = 0;

    V8Debugger* debugger() { return m_debugger; }
protected:
    InspectorDebuggerAgent(V8RuntimeAgent*, V8Debugger*, int contextGroupId);

    OwnPtr<V8DebuggerAgent> m_v8DebuggerAgent;
    OwnPtrWillBeMember<AsyncCallTracker> m_asyncCallTracker;

private:
    void setTrackingAsyncCalls(bool);
    V8Debugger* m_debugger;
};

} // namespace blink


#endif // !defined(InspectorDebuggerAgent_h)
