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

#include "config.h"
#include "core/inspector/InspectorProfilerAgent.h"

#include "InspectorFrontend.h"
#include "bindings/v8/PageScriptDebugServer.h"
#include "bindings/v8/ScriptObject.h"
#include "bindings/v8/ScriptProfiler.h"
#include "bindings/v8/WorkerScriptDebugServer.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InspectorConsoleAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InspectorValues.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/ScriptProfile.h"
#include "core/page/Console.h"
#include "core/page/ConsoleTypes.h"
#include "core/page/Page.h"
#include "core/platform/KURL.h"
#include <wtf/CurrentTime.h>
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

namespace ProfilerAgentState {
static const char userInitiatedProfiling[] = "userInitiatedProfiling";
static const char profilerEnabled[] = "profilerEnabled";
static const char profileHeadersRequested[] = "profileHeadersRequested";
}

static const char* const UserInitiatedProfileName = "org.webkit.profiles.user-initiated";
static const char* const CPUProfileType = "CPU";


class PageProfilerAgent : public InspectorProfilerAgent {
public:
    PageProfilerAgent(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, Page* inspectedPage, InspectorCompositeState* state, InjectedScriptManager* injectedScriptManager)
        : InspectorProfilerAgent(instrumentingAgents, consoleAgent, state, injectedScriptManager), m_inspectedPage(inspectedPage) { }
    virtual ~PageProfilerAgent() { }

private:
    virtual void startProfiling(const String& title)
    {
        ScriptProfiler::startForPage(m_inspectedPage, title);
    }

    virtual PassRefPtr<ScriptProfile> stopProfiling(const String& title)
    {
        return ScriptProfiler::stopForPage(m_inspectedPage, title);
    }

    Page* m_inspectedPage;
};

PassOwnPtr<InspectorProfilerAgent> InspectorProfilerAgent::create(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, Page* inspectedPage, InspectorCompositeState* inspectorState, InjectedScriptManager* injectedScriptManager)
{
    return adoptPtr(new PageProfilerAgent(instrumentingAgents, consoleAgent, inspectedPage, inspectorState, injectedScriptManager));
}

class WorkerProfilerAgent : public InspectorProfilerAgent {
public:
    WorkerProfilerAgent(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, WorkerContext* workerContext, InspectorCompositeState* state, InjectedScriptManager* injectedScriptManager)
        : InspectorProfilerAgent(instrumentingAgents, consoleAgent, state, injectedScriptManager), m_workerContext(workerContext) { }
    virtual ~WorkerProfilerAgent() { }

private:
    virtual void startProfiling(const String& title)
    {
        ScriptProfiler::startForWorkerContext(m_workerContext, title);
    }

    virtual PassRefPtr<ScriptProfile> stopProfiling(const String& title)
    {
        return ScriptProfiler::stopForWorkerContext(m_workerContext, title);
    }

    WorkerContext* m_workerContext;
};

PassOwnPtr<InspectorProfilerAgent> InspectorProfilerAgent::create(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, WorkerContext* workerContext, InspectorCompositeState* inspectorState, InjectedScriptManager* injectedScriptManager)
{
    return adoptPtr(new WorkerProfilerAgent(instrumentingAgents, consoleAgent, workerContext, inspectorState, injectedScriptManager));
}

InspectorProfilerAgent::InspectorProfilerAgent(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, InspectorCompositeState* inspectorState, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorProfilerAgent>("Profiler", instrumentingAgents, inspectorState)
    , m_consoleAgent(consoleAgent)
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
    , m_enabled(false)
    , m_recordingCPUProfile(false)
    , m_currentUserInitiatedProfileNumber(-1)
    , m_nextUserInitiatedProfileNumber(1)
    , m_profileNameIdleTimeMap(ScriptProfiler::currentProfileNameIdleTimeMap())
    , m_previousTaskEndTime(0.0)
{
    m_instrumentingAgents->setInspectorProfilerAgent(this);
}

InspectorProfilerAgent::~InspectorProfilerAgent()
{
    m_instrumentingAgents->setInspectorProfilerAgent(0);
}

void InspectorProfilerAgent::addProfile(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL)
{
    RefPtr<ScriptProfile> profile = prpProfile;
    m_profiles.add(profile->uid(), profile);
    if (m_frontend && m_state->getBoolean(ProfilerAgentState::profileHeadersRequested))
        m_frontend->addProfileHeader(createProfileHeader(*profile));
    addProfileFinishedMessageToConsole(profile, lineNumber, sourceURL);
}

void InspectorProfilerAgent::addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL)
{
    if (!m_frontend)
        return;
    RefPtr<ScriptProfile> profile = prpProfile;
    String message = makeString(profile->title(), '#', String::number(profile->uid()));
    m_consoleAgent->addMessageToConsole(ConsoleAPIMessageSource, ProfileEndMessageType, DebugMessageLevel, message, sourceURL, lineNumber);
}

void InspectorProfilerAgent::addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL)
{
    if (!m_frontend)
        return;
    m_consoleAgent->addMessageToConsole(ConsoleAPIMessageSource, ProfileMessageType, DebugMessageLevel, title, sourceURL, lineNumber);
}

PassRefPtr<TypeBuilder::Profiler::ProfileHeader> InspectorProfilerAgent::createProfileHeader(const ScriptProfile& profile)
{
    return TypeBuilder::Profiler::ProfileHeader::create()
        .setTypeId(TypeBuilder::Profiler::ProfileHeader::TypeId::CPU)
        .setUid(profile.uid())
        .setTitle(profile.title())
        .release();
}

void InspectorProfilerAgent::enable(ErrorString*)
{
    if (enabled())
        return;
    m_state->setBoolean(ProfilerAgentState::profilerEnabled, true);
    enable(false);
}

void InspectorProfilerAgent::disable(ErrorString*)
{
    m_state->setBoolean(ProfilerAgentState::profilerEnabled, false);
    disable();
}

void InspectorProfilerAgent::disable()
{
    if (!m_enabled)
        return;
    m_enabled = false;
    m_state->setBoolean(ProfilerAgentState::profileHeadersRequested, false);
}

void InspectorProfilerAgent::enable(bool skipRecompile)
{
    if (m_enabled)
        return;
    m_enabled = true;
}

String InspectorProfilerAgent::getCurrentUserInitiatedProfileName(bool incrementProfileNumber)
{
    if (incrementProfileNumber)
        m_currentUserInitiatedProfileNumber = m_nextUserInitiatedProfileNumber++;

    return makeString(UserInitiatedProfileName, '.', String::number(m_currentUserInitiatedProfileNumber));
}

void InspectorProfilerAgent::getProfileHeaders(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::ProfileHeader> >& headers)
{
    m_state->setBoolean(ProfilerAgentState::profileHeadersRequested, true);
    headers = TypeBuilder::Array<TypeBuilder::Profiler::ProfileHeader>::create();

    ProfilesMap::iterator profilesEnd = m_profiles.end();
    for (ProfilesMap::iterator it = m_profiles.begin(); it != profilesEnd; ++it)
        headers->addItem(createProfileHeader(*it->value));
}

void InspectorProfilerAgent::getCPUProfile(ErrorString* errorString, int rawUid, RefPtr<TypeBuilder::Profiler::CPUProfile>& profileObject)
{
    unsigned uid = static_cast<unsigned>(rawUid);
    ProfilesMap::iterator it = m_profiles.find(uid);
    if (it == m_profiles.end()) {
        *errorString = "Profile wasn't found";
        return;
    }
    profileObject = TypeBuilder::Profiler::CPUProfile::create();
    profileObject->setHead(it->value->buildInspectorObjectForHead());
    profileObject->setIdleTime(it->value->idleTime());
    profileObject->setSamples(it->value->buildInspectorObjectForSamples());
}

void InspectorProfilerAgent::removeProfile(ErrorString*, const String& type, int rawUid)
{
    unsigned uid = static_cast<unsigned>(rawUid);
    if (type == CPUProfileType) {
        if (m_profiles.contains(uid))
            m_profiles.remove(uid);
    }
}

void InspectorProfilerAgent::clearProfiles(ErrorString*)
{
    stop();
    m_profiles.clear();
    m_currentUserInitiatedProfileNumber = 1;
    m_nextUserInitiatedProfileNumber = 1;
    resetFrontendProfiles();
    m_injectedScriptManager->injectedScriptHost()->clearInspectedObjects();
}

void InspectorProfilerAgent::resetFrontendProfiles()
{
    if (!m_frontend)
        return;
    if (!m_state->getBoolean(ProfilerAgentState::profileHeadersRequested))
        return;
    if (m_profiles.isEmpty())
        m_frontend->resetProfiles();
}

void InspectorProfilerAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->profiler();
}

void InspectorProfilerAgent::clearFrontend()
{
    m_frontend = 0;
    stop();
    ErrorString error;
    disable(&error);
}

void InspectorProfilerAgent::restore()
{
    // Need to restore enablement state here as in setFrontend m_state wasn't loaded yet.
    restoreEnablement();
    resetFrontendProfiles();
    if (m_state->getBoolean(ProfilerAgentState::userInitiatedProfiling))
        start();
}

void InspectorProfilerAgent::restoreEnablement()
{
    if (m_state->getBoolean(ProfilerAgentState::profilerEnabled)) {
        ErrorString error;
        enable(&error);
    }
}

void InspectorProfilerAgent::start(ErrorString*)
{
    if (m_recordingCPUProfile)
        return;
    if (!enabled()) {
        enable(true);
    }
    m_recordingCPUProfile = true;
    String title = getCurrentUserInitiatedProfileName(true);
    startProfiling(title);
    addStartProfilingMessageToConsole(title, 0, String());
    toggleRecordButton(true);
    m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, true);
}

void InspectorProfilerAgent::stop(ErrorString* errorString, RefPtr<TypeBuilder::Profiler::ProfileHeader>& header)
{
    header = stop(errorString);
}

PassRefPtr<TypeBuilder::Profiler::ProfileHeader> InspectorProfilerAgent::stop(ErrorString* errorString)
{
    if (!m_recordingCPUProfile)
        return 0;
    m_recordingCPUProfile = false;
    String title = getCurrentUserInitiatedProfileName();
    RefPtr<ScriptProfile> profile = stopProfiling(title);
    RefPtr<TypeBuilder::Profiler::ProfileHeader> profileHeader;
    if (profile) {
        addProfile(profile, 0, String());
        profileHeader = createProfileHeader(*profile);
    } else if (errorString)
        *errorString = "Profile wasn't found";
    toggleRecordButton(false);
    m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, false);
    return profileHeader;
}

void InspectorProfilerAgent::toggleRecordButton(bool isProfiling)
{
    if (m_frontend)
        m_frontend->setRecordingProfile(isProfiling);
}

void InspectorProfilerAgent::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::InspectorProfilerAgent);
    InspectorBaseAgent<InspectorProfilerAgent>::reportMemoryUsage(memoryObjectInfo);
    info.addMember(m_consoleAgent, "consoleAgent");
    info.addMember(m_injectedScriptManager, "injectedScriptManager");
    info.addWeakPointer(m_frontend);
    info.addMember(m_profiles, "profiles");
        info.addMember(m_profileNameIdleTimeMap, "profileNameIdleTimeMap");
}

void InspectorProfilerAgent::willProcessTask()
{
    if (!m_profileNameIdleTimeMap || !m_profileNameIdleTimeMap->size())
        return;
    if (!m_previousTaskEndTime)
        return;

    double idleTime = WTF::monotonicallyIncreasingTime() - m_previousTaskEndTime;
    m_previousTaskEndTime = 0.0;
    ProfileNameIdleTimeMap::iterator end = m_profileNameIdleTimeMap->end();
    for (ProfileNameIdleTimeMap::iterator it = m_profileNameIdleTimeMap->begin(); it != end; ++it)
        it->value += idleTime;
}

void InspectorProfilerAgent::didProcessTask()
{
    if (!m_profileNameIdleTimeMap || !m_profileNameIdleTimeMap->size())
        return;
    m_previousTaskEndTime = WTF::monotonicallyIncreasingTime();
}

} // namespace WebCore

