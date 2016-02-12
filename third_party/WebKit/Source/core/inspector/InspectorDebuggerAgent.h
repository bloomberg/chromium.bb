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
#include "core/inspector/v8/public/V8DebuggerAgent.h"

namespace blink {

class CORE_EXPORT InspectorDebuggerAgent
    : public InspectorBaseAgent<InspectorDebuggerAgent, protocol::Frontend::Debugger>
    , public protocol::Dispatcher::DebuggerCommandHandler {
public:
    ~InspectorDebuggerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    // protocol::Dispatcher::DebuggerCommandHandler implementation.
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void setBreakpointsActive(ErrorString*, bool inActive) override;
    void setSkipAllPauses(ErrorString*, bool inSkipped) override;
    void setBreakpointByUrl(ErrorString*, int inLineNumber, const String* inUrl, const String* inUrlRegex, const int* inColumnNumber, const String* inCondition, protocol::TypeBuilder::Debugger::BreakpointId* outBreakpointId, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::Location>>& outLocations) override;
    void setBreakpoint(ErrorString*, const RefPtr<JSONObject>& inLocation, const String* inCondition, protocol::TypeBuilder::Debugger::BreakpointId* outBreakpointId, RefPtr<protocol::TypeBuilder::Debugger::Location>& outActualLocation) override;
    void removeBreakpoint(ErrorString*, const String& inBreakpointId) override;
    void continueToLocation(ErrorString*, const RefPtr<JSONObject>& inLocation, const bool* inInterstatementLocation) override;
    void stepOver(ErrorString*) override;
    void stepInto(ErrorString*) override;
    void stepOut(ErrorString*) override;
    void pause(ErrorString*) override;
    void resume(ErrorString*) override;
    void stepIntoAsync(ErrorString*) override;
    void searchInContent(ErrorString*, const String& inScriptId, const String& inQuery, const bool* inCaseSensitive, const bool* inIsRegex, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::SearchMatch>>& outResult) override;
    void canSetScriptSource(ErrorString*, bool* outResult) override;
    void setScriptSource(ErrorString*, RefPtr<protocol::TypeBuilder::Debugger::SetScriptSourceError>& errorData, const String& inScriptId, const String& inScriptSource, const bool* inPreview, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::CallFrame>>& optOutCallFrames, protocol::TypeBuilder::OptOutput<bool>* optOutStackChanged, RefPtr<protocol::TypeBuilder::Debugger::StackTrace>& optOutAsyncStackTrace) override;
    void restartFrame(ErrorString*, const String& inCallFrameId, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::CallFrame>>& outCallFrames, RefPtr<protocol::TypeBuilder::Debugger::StackTrace>& optOutAsyncStackTrace) override;
    void getScriptSource(ErrorString*, const String& inScriptId, String* outScriptSource) override;
    void getFunctionDetails(ErrorString*, const String& inFunctionId, RefPtr<protocol::TypeBuilder::Debugger::FunctionDetails>& outDetails) override;
    void getGeneratorObjectDetails(ErrorString*, const String& inObjectId, RefPtr<protocol::TypeBuilder::Debugger::GeneratorObjectDetails>& outDetails) override;
    void getCollectionEntries(ErrorString*, const String& inObjectId, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::CollectionEntry>>& outEntries) override;
    void setPauseOnExceptions(ErrorString*, const String& inState) override;
    void evaluateOnCallFrame(ErrorString*, const String& inCallFrameId, const String& inExpression, const String* inObjectGroup, const bool* inIncludeCommandLineAPI, const bool* inDoNotPauseOnExceptionsAndMuteConsole, const bool* inReturnByValue, const bool* inGeneratePreview, RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>& outResult, protocol::TypeBuilder::OptOutput<bool>* optOutWasThrown, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>& optOutExceptionDetails) override;
    void setVariableValue(ErrorString*, int inScopeNumber, const String& inVariableName, const RefPtr<JSONObject>& inNewValue, const String* inCallFrameId, const String* inFunctionObjectId) override;
    void getStepInPositions(ErrorString*, const String& inCallFrameId, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::Location>>& optOutStepInPositions) override;
    void getBacktrace(ErrorString*, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::CallFrame>>& outCallFrames, RefPtr<protocol::TypeBuilder::Debugger::StackTrace>& optOutAsyncStackTrace) override;
    void setAsyncCallStackDepth(ErrorString*, int inMaxDepth) override;
    void enablePromiseTracker(ErrorString*, const bool* inCaptureStacks) override;
    void disablePromiseTracker(ErrorString*) override;
    void getPromiseById(ErrorString*, int inPromiseId, const String* inObjectGroup, RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>& outPromise) override;
    void flushAsyncOperationEvents(ErrorString*) override;
    void setAsyncOperationBreakpoint(ErrorString*, int inOperationId) override;
    void removeAsyncOperationBreakpoint(ErrorString*, int inOperationId) override;
    void setBlackboxedRanges(ErrorString*, const String& inScriptId, const RefPtr<JSONArray>& inPositions) override;

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
