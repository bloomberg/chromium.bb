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

namespace blink {

class V8DebuggerAgent;

using protocol::Array;

class CORE_EXPORT InspectorDebuggerAgent
    : public InspectorBaseAgent<InspectorDebuggerAgent, protocol::Frontend::Debugger>
    , public protocol::Backend::Debugger {
public:
    explicit InspectorDebuggerAgent(V8DebuggerAgent*);
    ~InspectorDebuggerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    // protocol::Dispatcher::DebuggerCommandHandler implementation.
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void setBreakpointsActive(ErrorString*, bool active) override;
    void setSkipAllPauses(ErrorString*, bool skipped) override;
    void setBreakpointByUrl(ErrorString*, int lineNumber, const Maybe<String16>& url, const Maybe<String16>& urlRegex, const Maybe<int>& columnNumber, const Maybe<String16>& condition, protocol::Debugger::BreakpointId*, OwnPtr<protocol::Array<protocol::Debugger::Location>>* locations) override;
    void setBreakpoint(ErrorString*, PassOwnPtr<protocol::Debugger::Location>, const Maybe<String16>& condition, protocol::Debugger::BreakpointId*, OwnPtr<protocol::Debugger::Location>* actualLocation) override;
    void removeBreakpoint(ErrorString*, const protocol::Debugger::BreakpointId&) override;
    void continueToLocation(ErrorString*, PassOwnPtr<protocol::Debugger::Location>, const Maybe<bool>& interstatementLocation) override;
    void stepOver(ErrorString*) override;
    void stepInto(ErrorString*) override;
    void stepOut(ErrorString*) override;
    void pause(ErrorString*) override;
    void resume(ErrorString*) override;
    void searchInContent(ErrorString*, const String16& scriptId, const String16& query, const Maybe<bool>& caseSensitive, const Maybe<bool>& isRegex, OwnPtr<protocol::Array<protocol::Debugger::SearchMatch>>* result) override;
    void canSetScriptSource(ErrorString*, bool* result) override;
    void setScriptSource(ErrorString*, const String16& scriptId, const String16& scriptSource, const Maybe<bool>& preview, Maybe<protocol::Array<protocol::Debugger::CallFrame>>* callFrames, Maybe<bool>* stackChanged, Maybe<protocol::Runtime::StackTrace>* asyncStackTrace, Maybe<protocol::Debugger::SetScriptSourceError>* compileError) override;
    void restartFrame(ErrorString*, const String16& callFrameId, OwnPtr<protocol::Array<protocol::Debugger::CallFrame>>* callFrames, Maybe<protocol::Runtime::StackTrace>* asyncStackTrace) override;
    void getScriptSource(ErrorString*, const String16& scriptId, String16* scriptSource) override;
    void getFunctionDetails(ErrorString*, const String16& functionId, OwnPtr<protocol::Debugger::FunctionDetails>*) override;
    void getGeneratorObjectDetails(ErrorString*, const String16& objectId, OwnPtr<protocol::Debugger::GeneratorObjectDetails>*) override;
    void getCollectionEntries(ErrorString*, const String16& objectId, OwnPtr<protocol::Array<protocol::Debugger::CollectionEntry>>* entries) override;
    void setPauseOnExceptions(ErrorString*, const String16& state) override;
    void evaluateOnCallFrame(ErrorString*, const String16& callFrameId, const String16& expression, const Maybe<String16>& objectGroup, const Maybe<bool>& includeCommandLineAPI, const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole, const Maybe<bool>& returnByValue, const Maybe<bool>& generatePreview, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<bool>* wasThrown, Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void setVariableValue(ErrorString*, int scopeNumber, const String16& variableName, PassOwnPtr<protocol::Runtime::CallArgument> newValue, const String16& callFrameId) override;
    void getBacktrace(ErrorString*, OwnPtr<protocol::Array<protocol::Debugger::CallFrame>>* callFrames, Maybe<protocol::Runtime::StackTrace>* asyncStackTrace) override;
    void setAsyncCallStackDepth(ErrorString*, int maxDepth) override;
    void setBlackboxPatterns(ErrorString*, PassOwnPtr<protocol::Array<String16>> patterns) override;
    void setBlackboxedRanges(ErrorString*, const String16& scriptId, PassOwnPtr<protocol::Array<protocol::Debugger::ScriptPosition>> positions) override;

    // InspectorBaseAgent overrides.
    void init(InstrumentingAgents*, protocol::Frontend*, protocol::Dispatcher*, protocol::DictionaryValue*) override;
    void dispose() override;
    void restore() override;

protected:
    V8DebuggerAgent* m_v8DebuggerAgent;
};

} // namespace blink


#endif // !defined(InspectorDebuggerAgent_h)
