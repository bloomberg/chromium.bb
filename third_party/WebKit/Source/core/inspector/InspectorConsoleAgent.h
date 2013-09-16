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

#include "InspectorFrontend.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptString.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/page/ConsoleTypes.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

class ConsoleMessage;
class DocumentLoader;
class DOMWindow;
class Frame;
class InspectorFrontend;
class InjectedScriptManager;
class InspectorTimelineAgent;
class InstrumentingAgents;
class ResourceError;
class ResourceLoader;
class ResourceResponse;
class ScriptArguments;
class ScriptCallStack;
class ScriptProfile;
class ThreadableLoaderClient;

typedef String ErrorString;

class InspectorConsoleAgent : public InspectorBaseAgent<InspectorConsoleAgent>, public InspectorBackendDispatcher::ConsoleCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorConsoleAgent);
public:
    InspectorConsoleAgent(InstrumentingAgents*, InspectorTimelineAgent*, InspectorCompositeState*, InjectedScriptManager*);
    virtual ~InspectorConsoleAgent();

    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);
    virtual void clearMessages(ErrorString*);
    bool enabled() { return m_enabled; }
    void reset();

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void restore();

    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, ScriptState*, PassRefPtr<ScriptArguments>, unsigned long requestIdentifier = 0);
    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, const String& scriptId, unsigned lineNumber, unsigned columnNumber = 0, ScriptState* = 0, unsigned long requestIdentifier = 0);

    // FIXME: Remove once we no longer generate stacks outside of Inspector.
    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptCallStack>, unsigned long requestIdentifier = 0);

    Vector<unsigned> consoleMessageArgumentCounts();

    void consoleTime(ScriptExecutionContext*, const String& title);
    void consoleTimeEnd(ScriptExecutionContext*, const String& title, ScriptState*);
    void consoleTimeline(ScriptExecutionContext*, const String& title, ScriptState*);
    void consoleTimelineEnd(ScriptExecutionContext*, const String& title, ScriptState*);

    void consoleCount(ScriptState*, PassRefPtr<ScriptArguments>);

    void frameWindowDiscarded(DOMWindow*);
    void didCommitLoad(Frame*, DocumentLoader*);

    void didFinishXHRLoading(ThreadableLoaderClient*, unsigned long requestIdentifier, ScriptString, const String& url, const String& sendURL, unsigned sendLineNumber);
    void didReceiveResourceResponse(unsigned long requestIdentifier, DocumentLoader*, const ResourceResponse& response, ResourceLoader*);
    void didFailLoading(unsigned long requestIdentifier, DocumentLoader*, const ResourceError&);
    void addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile>, unsigned lineNumber, const String& sourceURL);
    void addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL);
    virtual void setMonitoringXHREnabled(ErrorString*, bool enabled);
    virtual void addInspectedNode(ErrorString*, int nodeId) = 0;
    virtual void addInspectedHeapObject(ErrorString*, int inspectedHeapObjectId);

    virtual bool isWorkerAgent() = 0;

protected:
    void addConsoleMessage(PassOwnPtr<ConsoleMessage>);

    InspectorTimelineAgent* m_timelineAgent;
    InjectedScriptManager* m_injectedScriptManager;
    InspectorFrontend::Console* m_frontend;
    ConsoleMessage* m_previousMessage;
    Vector<OwnPtr<ConsoleMessage> > m_consoleMessages;
    int m_expiredConsoleMessageCount;
    HashMap<String, unsigned> m_counts;
    HashMap<String, double> m_times;
    bool m_enabled;
private:
    static int s_enabledAgentCount;
};

} // namespace WebCore


#endif // !defined(InspectorConsoleAgent_h)
