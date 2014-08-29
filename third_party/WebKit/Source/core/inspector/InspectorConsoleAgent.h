/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorConsoleAgent_h
#define InspectorConsoleAgent_h

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptString.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/frame/ConsoleTypes.h"
#include "wtf/Forward.h"
#include "wtf/HashCountedSet.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"

namespace blink {

class ConsoleMessage;
class ConsoleMessageStorage;
class DocumentLoader;
class LocalDOMWindow;
class LocalFrame;
class InspectorConsoleMessage;
class InspectorFrontend;
class InjectedScriptManager;
class InspectorTimelineAgent;
class InspectorTracingAgent;
class InstrumentingAgents;
class ResourceError;
class ResourceLoader;
class ResourceResponse;
class ScriptArguments;
class ScriptCallStack;
class ScriptProfile;
class ThreadableLoaderClient;
class WorkerGlobalScopeProxy;
class XMLHttpRequest;

typedef String ErrorString;

class InspectorConsoleAgent : public InspectorBaseAgent<InspectorConsoleAgent>, public InspectorBackendDispatcher::ConsoleCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorConsoleAgent);
public:
    InspectorConsoleAgent(InspectorTimelineAgent*, InspectorTracingAgent*, InjectedScriptManager*);
    virtual ~InspectorConsoleAgent();
    virtual void trace(Visitor*) OVERRIDE;

    virtual void init() OVERRIDE;
    virtual void enable(ErrorString*) OVERRIDE FINAL;
    virtual void disable(ErrorString*) OVERRIDE FINAL;
    virtual void clearMessages(ErrorString*) OVERRIDE;
    bool enabled() { return m_enabled; }
    void reset();

    virtual void setFrontend(InspectorFrontend*) OVERRIDE FINAL;
    virtual void clearFrontend() OVERRIDE FINAL;
    virtual void restore() OVERRIDE FINAL;

    void addMessageToConsole(ConsoleMessage*);

    void consoleTime(ExecutionContext*, const String& title);
    void consoleTimeEnd(ExecutionContext*, const String& title, ScriptState*);
    void setTracingBasedTimeline(ErrorString*, bool enabled);
    void consoleTimeline(ExecutionContext*, const String& title, ScriptState*);
    void consoleTimelineEnd(ExecutionContext*, const String& title, ScriptState*);

    void consoleCount(ScriptState*, PassRefPtrWillBeRawPtr<ScriptArguments>);

    void frameWindowDiscarded(LocalDOMWindow*);
    void didCommitLoad(LocalFrame*, DocumentLoader*);

    void didFinishXHRLoading(XMLHttpRequest*, ThreadableLoaderClient*, unsigned long requestIdentifier, ScriptString, const AtomicString& method, const String& url, const String& sendURL, unsigned sendLineNumber);
    void didReceiveResourceResponse(LocalFrame*, unsigned long requestIdentifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    void didFailLoading(unsigned long requestIdentifier, const ResourceError&);
    void addProfileFinishedMessageToConsole(PassRefPtrWillBeRawPtr<ScriptProfile>, unsigned lineNumber, const String& sourceURL);
    void addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL);
    virtual void setMonitoringXHREnabled(ErrorString*, bool enabled) OVERRIDE;
    virtual void addInspectedNode(ErrorString*, int nodeId) = 0;
    virtual void addInspectedHeapObject(ErrorString*, int inspectedHeapObjectId) OVERRIDE;

    virtual bool isWorkerAgent() = 0;

protected:
    void sendConsoleMessageToFrontend(ConsoleMessage*, bool generatePreview);
    virtual ConsoleMessageStorage* messageStorage() = 0;

    RawPtrWillBeMember<InspectorTimelineAgent> m_timelineAgent;
    RawPtrWillBeMember<InspectorTracingAgent> m_tracingAgent;
    RawPtrWillBeMember<InjectedScriptManager> m_injectedScriptManager;
    InspectorFrontend::Console* m_frontend;
    HashCountedSet<String> m_counts;
    HashMap<String, double> m_times;
    bool m_enabled;
private:
    static int s_enabledAgentCount;
};

} // namespace blink


#endif // !defined(InspectorConsoleAgent_h)
