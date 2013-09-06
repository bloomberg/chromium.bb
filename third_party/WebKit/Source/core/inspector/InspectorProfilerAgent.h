/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorProfilerAgent_h
#define InspectorProfilerAgent_h

#include "InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class InjectedScriptManager;
class InspectorConsoleAgent;
class InspectorFrontend;
class InstrumentingAgents;
class ScriptCallStack;
class ScriptProfile;

typedef String ErrorString;

class InspectorProfilerAgent : public InspectorBaseAgent<InspectorProfilerAgent>, public InspectorBackendDispatcher::ProfilerCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorProfilerAgent); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<InspectorProfilerAgent> create(InstrumentingAgents*, InspectorConsoleAgent*, InspectorCompositeState*, InjectedScriptManager*);
    virtual ~InspectorProfilerAgent();

    void addProfile(PassRefPtr<ScriptProfile> prpProfile, PassRefPtr<ScriptCallStack>);
    void addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile>, unsigned lineNumber, const String& sourceURL);
    void addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL);
    virtual void clearProfiles(ErrorString*);

    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);
    virtual void start(ErrorString* = 0);
    virtual void stop(ErrorString*, RefPtr<TypeBuilder::Profiler::ProfileHeader>& header);

    bool enabled();
    String getCurrentUserInitiatedProfileName(bool incrementProfileNumber = false);
    virtual void getProfileHeaders(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::ProfileHeader> >&);
    virtual void getCPUProfile(ErrorString*, int uid, RefPtr<TypeBuilder::Profiler::CPUProfile>&);
    virtual void removeProfile(ErrorString*, const String& type, int uid);

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void restore();

    void toggleRecordButton(bool isProfiling);

    void willProcessTask();
    void didProcessTask();
    void willEnterNestedRunLoop();
    void didLeaveNestedRunLoop();

private:
    InspectorProfilerAgent(InstrumentingAgents*, InspectorConsoleAgent*, InspectorCompositeState*, InjectedScriptManager*);

    void addProfile(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL);

    void resetFrontendProfiles();
    PassRefPtr<TypeBuilder::Profiler::ProfileHeader> stop(ErrorString* = 0);

    PassRefPtr<TypeBuilder::Profiler::ProfileHeader> createProfileHeader(const ScriptProfile&);

    InspectorConsoleAgent* m_consoleAgent;
    InjectedScriptManager* m_injectedScriptManager;
    InspectorFrontend::Profiler* m_frontend;
    bool m_recordingCPUProfile;
    int m_currentUserInitiatedProfileNumber;
    unsigned m_nextUserInitiatedProfileNumber;
    typedef HashMap<unsigned, RefPtr<ScriptProfile> > ProfilesMap;
    ProfilesMap m_profiles;

    typedef HashMap<String, double> ProfileNameIdleTimeMap;
    ProfileNameIdleTimeMap* m_profileNameIdleTimeMap;
    double m_idleStartTime;

    void idleStarted();
    void idleFinished();
};

} // namespace WebCore


#endif // !defined(InspectorProfilerAgent_h)
