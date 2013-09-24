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

#include "InspectorFrontend.h"
#include "bindings/v8/ScriptState.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/ScriptBreakpoint.h"
#include "core/inspector/ScriptDebugListener.h"
#include "core/page/ConsoleTypes.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

class InjectedScriptManager;
class InspectorFrontend;
class InstrumentingAgents;
class JSONObject;
class ScriptArguments;
class ScriptCallStack;
class ScriptDebugServer;
class ScriptSourceCode;
class ScriptValue;
class RegularExpression;

typedef String ErrorString;

class InspectorDebuggerAgent : public InspectorBaseAgent<InspectorDebuggerAgent>, public ScriptDebugListener, public InspectorBackendDispatcher::DebuggerCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDebuggerAgent); WTF_MAKE_FAST_ALLOCATED;
public:
    enum BreakpointSource {
        UserBreakpointSource,
        DebugCommandBreakpointSource,
        MonitorCommandBreakpointSource
    };

    static const char* backtraceObjectGroup;

    virtual ~InspectorDebuggerAgent();

    virtual void canSetScriptSource(ErrorString*, bool* result) { *result = true; }

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void restore();

    bool isPaused();
    bool runningNestedMessageLoop();
    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String&, PassRefPtr<ScriptCallStack>, unsigned long);
    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String&, ScriptState*, PassRefPtr<ScriptArguments>, unsigned long);

    String preprocessEventListener(Frame*, const String& source, const String& url, const String& functionName);
    PassOwnPtr<ScriptSourceCode> preprocess(Frame*, const ScriptSourceCode&);

    // Part of the protocol.
    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);
    virtual void setBreakpointsActive(ErrorString*, bool active);
    virtual void setSkipAllPauses(ErrorString*, bool skipped, const bool* untilReload);

    virtual void setBreakpointByUrl(ErrorString*, int lineNumber, const String* optionalURL, const String* optionalURLRegex, const int* optionalColumnNumber, const String* optionalCondition, const bool* isAntiBreakpoint, TypeBuilder::Debugger::BreakpointId*, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location> >& locations);
    virtual void setBreakpoint(ErrorString*, const RefPtr<JSONObject>& location, const String* optionalCondition, TypeBuilder::Debugger::BreakpointId*, RefPtr<TypeBuilder::Debugger::Location>& actualLocation);
    virtual void removeBreakpoint(ErrorString*, const String& breakpointId);
    virtual void continueToLocation(ErrorString*, const RefPtr<JSONObject>& location, const bool* interstateLocationOpt);
    virtual void getStepInPositions(ErrorString*, const String& callFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location> >& positions);
    virtual void getBacktrace(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame> >&);

    virtual void searchInContent(ErrorString*, const String& scriptId, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Page::SearchMatch> >&);
    virtual void setScriptSource(ErrorString*, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>&, const String& scriptId, const String& newContent, const bool* preview, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame> >& newCallFrames, RefPtr<JSONObject>& result);
    virtual void restartFrame(ErrorString*, const String& callFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame> >& newCallFrames, RefPtr<JSONObject>& result);
    virtual void getScriptSource(ErrorString*, const String& scriptId, String* scriptSource);
    virtual void getFunctionDetails(ErrorString*, const String& functionId, RefPtr<TypeBuilder::Debugger::FunctionDetails>&);
    virtual void pause(ErrorString*);
    virtual void resume(ErrorString*);
    virtual void stepOver(ErrorString*, const String* callFrameId);
    virtual void stepInto(ErrorString*);
    virtual void stepOut(ErrorString*);
    virtual void setPauseOnExceptions(ErrorString*, const String& pauseState);
    virtual void evaluateOnCallFrame(ErrorString*,
                             const String& callFrameId,
                             const String& expression,
                             const String* objectGroup,
                             const bool* includeCommandLineAPI,
                             const bool* doNotPauseOnExceptionsAndMuteConsole,
                             const bool* returnByValue,
                             const bool* generatePreview,
                             RefPtr<TypeBuilder::Runtime::RemoteObject>& result,
                             TypeBuilder::OptOutput<bool>* wasThrown);
    void compileScript(ErrorString*, const String& expression, const String& sourceURL, TypeBuilder::OptOutput<TypeBuilder::Debugger::ScriptId>*, TypeBuilder::OptOutput<String>* syntaxErrorMessage);
    void runScript(ErrorString*, const TypeBuilder::Debugger::ScriptId&, const int* executionContextId, const String* objectGroup, const bool* doNotPauseOnExceptionsAndMuteConsole, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown);
    virtual void setOverlayMessage(ErrorString*, const String*);
    virtual void setVariableValue(ErrorString*, int in_scopeNumber, const String& in_variableName, const RefPtr<JSONObject>& in_newValue, const String* in_callFrame, const String* in_functionObjectId);
    virtual void skipStackFrames(ErrorString*, const String* pattern);

    void schedulePauseOnNextStatement(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data);
    void didFireTimer();
    void didHandleEvent();
    bool canBreakProgram();
    void breakProgram(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data);
    virtual void scriptExecutionBlockedByCSP(const String& directiveText);

    class Listener {
    public:
        virtual ~Listener() { }
        virtual void debuggerWasEnabled() = 0;
        virtual void debuggerWasDisabled() = 0;
        virtual void stepInto() = 0;
        virtual void didPause() = 0;
    };
    void setListener(Listener* listener) { m_listener = listener; }

    virtual ScriptDebugServer& scriptDebugServer() = 0;

    void setBreakpoint(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource, const String& condition = String());
    void removeBreakpoint(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource);

    SkipPauseRequest shouldSkipExceptionPause(RefPtr<JavaScriptCallFrame>& topFrame);
    SkipPauseRequest shouldSkipBreakpointPause(RefPtr<JavaScriptCallFrame>& topFrame);
    SkipPauseRequest shouldSkipStepPause(RefPtr<JavaScriptCallFrame>& topFrame);

protected:
    InspectorDebuggerAgent(InstrumentingAgents*, InspectorCompositeState*, InjectedScriptManager*);

    virtual void startListeningScriptDebugServer() = 0;
    virtual void stopListeningScriptDebugServer() = 0;
    virtual void muteConsole() = 0;
    virtual void unmuteConsole() = 0;
    InjectedScriptManager* injectedScriptManager() { return m_injectedScriptManager; }
    virtual InjectedScript injectedScriptForEval(ErrorString*, const int* executionContextId) = 0;

    virtual void enable();
    virtual void disable();
    virtual void didPause(ScriptState*, const ScriptValue& callFrames, const ScriptValue& exception, const Vector<String>& hitBreakpoints);
    virtual void didContinue();
    void reset();
    void pageDidCommitLoad();

private:
    void cancelPauseOnNextStatement();
    void addMessageToConsole(MessageSource, MessageType);

    bool enabled();

    PassRefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame> > currentCallFrames();

    virtual void didParseSource(const String& scriptId, const Script&);
    virtual void failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage);

    void setPauseOnExceptionsImpl(ErrorString*, int);

    PassRefPtr<TypeBuilder::Debugger::Location> resolveBreakpoint(const String& breakpointId, const String& scriptId, const ScriptBreakpoint&, BreakpointSource);
    void removeBreakpoint(const String& breakpointId);
    void clear();
    bool assertPaused(ErrorString*);
    void clearBreakDetails();

    String sourceMapURLForScript(const Script&);

    String scriptURL(JavaScriptCallFrame*);

    typedef HashMap<String, Script> ScriptsMap;
    typedef HashMap<String, Vector<String> > BreakpointIdToDebugServerBreakpointIdsMap;
    typedef HashMap<String, std::pair<String, BreakpointSource> > DebugServerBreakpointToBreakpointIdAndSourceMap;

    InjectedScriptManager* m_injectedScriptManager;
    InspectorFrontend::Debugger* m_frontend;
    ScriptState* m_pausedScriptState;
    ScriptValue m_currentCallStack;
    ScriptsMap m_scripts;
    BreakpointIdToDebugServerBreakpointIdsMap m_breakpointIdToDebugServerBreakpointIds;
    DebugServerBreakpointToBreakpointIdAndSourceMap m_serverBreakpoints;
    String m_continueToLocationBreakpointId;
    InspectorFrontend::Debugger::Reason::Enum m_breakReason;
    RefPtr<JSONObject> m_breakAuxData;
    bool m_javaScriptPauseScheduled;
    Listener* m_listener;

    int m_skipStepInCount;
    bool m_skipAllPauses;
    OwnPtr<RegularExpression> m_cachedSkipStackRegExp;
};

} // namespace WebCore


#endif // !defined(InspectorDebuggerAgent_h)
