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
#include "bindings/v8/ScriptProfiler.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InspectorConsoleAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/ScriptProfile.h"
#include "core/page/ConsoleTypes.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/StringConcatenate.h"

namespace WebCore {

namespace ProfilerAgentState {
static const char samplingInterval[] = "samplingInterval";
static const char userInitiatedProfiling[] = "userInitiatedProfiling";
static const char profilerEnabled[] = "profilerEnabled";
static const char profileHeadersRequested[] = "profileHeadersRequested";
}

static const char* const CPUProfileType = "CPU";

PassOwnPtr<InspectorProfilerAgent> InspectorProfilerAgent::create(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, InspectorCompositeState* inspectorState, InjectedScriptManager* injectedScriptManager)
{
    return adoptPtr(new InspectorProfilerAgent(instrumentingAgents, consoleAgent, inspectorState, injectedScriptManager));
}

InspectorProfilerAgent::InspectorProfilerAgent(InstrumentingAgents* instrumentingAgents, InspectorConsoleAgent* consoleAgent, InspectorCompositeState* inspectorState, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorProfilerAgent>("Profiler", instrumentingAgents, inspectorState)
    , m_consoleAgent(consoleAgent)
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
    , m_recordingCPUProfile(false)
    , m_currentUserInitiatedProfileNumber(-1)
    , m_nextUserInitiatedProfileNumber(1)
    , m_profileNameIdleTimeMap(ScriptProfiler::currentProfileNameIdleTimeMap())
    , m_idleStartTime(0.0)
{
}

InspectorProfilerAgent::~InspectorProfilerAgent()
{
}

void InspectorProfilerAgent::addProfile(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL)
{
    RefPtr<ScriptProfile> profile = prpProfile;
    m_profiles.add(profile->uid(), profile);
    if (m_frontend && m_state->getBoolean(ProfilerAgentState::profileHeadersRequested))
        m_frontend->addProfileHeader(createProfileHeader(*profile));
    addProfileFinishedMessageToConsole(profile, lineNumber, sourceURL);
}

void InspectorProfilerAgent::addProfile(PassRefPtr<ScriptProfile> prpProfile, PassRefPtr<ScriptCallStack> callStack)
{
    const ScriptCallFrame& lastCaller = callStack->at(0);
    addProfile(prpProfile, lastCaller.lineNumber(), lastCaller.sourceURL());
}

void InspectorProfilerAgent::addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile> prpProfile, unsigned lineNumber, const String& sourceURL)
{
    if (!m_frontend)
        return;
    RefPtr<ScriptProfile> profile = prpProfile;
    String message = profile->title() + "#" + String::number(profile->uid());
    m_consoleAgent->addMessageToConsole(ConsoleAPIMessageSource, ProfileEndMessageType, DebugMessageLevel, message, sourceURL, lineNumber);
}

PassRefPtr<TypeBuilder::Profiler::ProfileHeader> InspectorProfilerAgent::createProfileHeader(const ScriptProfile& profile)
{
    return TypeBuilder::Profiler::ProfileHeader::create()
        .setUid(profile.uid())
        .setTitle(profile.title())
        .release();
}

void InspectorProfilerAgent::enable(ErrorString*)
{
    m_state->setBoolean(ProfilerAgentState::profilerEnabled, true);
    doEnable();
}

void InspectorProfilerAgent::doEnable()
{
    m_instrumentingAgents->setInspectorProfilerAgent(this);
}

void InspectorProfilerAgent::disable(ErrorString*)
{
    m_instrumentingAgents->setInspectorProfilerAgent(0);
    m_state->setBoolean(ProfilerAgentState::profilerEnabled, false);
    m_state->setBoolean(ProfilerAgentState::profileHeadersRequested, false);
}

bool InspectorProfilerAgent::enabled()
{
    return m_state->getBoolean(ProfilerAgentState::profilerEnabled);
}

void InspectorProfilerAgent::setSamplingInterval(ErrorString* error, int interval)
{
    if (m_recordingCPUProfile) {
        *error = "Cannot change sampling interval when profiling.";
        return;
    }
    m_state->setLong(ProfilerAgentState::samplingInterval, interval);
    ScriptProfiler::setSamplingInterval(interval);
}

String InspectorProfilerAgent::getCurrentUserInitiatedProfileName(bool incrementProfileNumber)
{
    if (incrementProfileNumber)
        m_currentUserInitiatedProfileNumber = m_nextUserInitiatedProfileNumber++;

    return "Profile " + String::number(m_currentUserInitiatedProfileNumber);
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
    profileObject = TypeBuilder::Profiler::CPUProfile::create()
        .setHead(it->value->buildInspectorObjectForHead())
        .setStartTime(it->value->startTime())
        .setEndTime(it->value->endTime());
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
    if (m_state->getBoolean(ProfilerAgentState::profilerEnabled))
        doEnable();
    resetFrontendProfiles();
    if (long interval = m_state->getLong(ProfilerAgentState::samplingInterval, 0))
        ScriptProfiler::setSamplingInterval(interval);
    if (m_state->getBoolean(ProfilerAgentState::userInitiatedProfiling))
        start();
}

void InspectorProfilerAgent::start(ErrorString*)
{
    if (m_recordingCPUProfile)
        return;
    if (!enabled()) {
        ErrorString error;
        enable(&error);
    }
    m_recordingCPUProfile = true;
    String title = getCurrentUserInitiatedProfileName(true);
    ScriptProfiler::start(title);
    toggleRecordButton(true);
    m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, true);
}

void InspectorProfilerAgent::stop(ErrorString* errorString, RefPtr<TypeBuilder::Profiler::ProfileHeader>& header)
{
    header = stop(errorString);
}

PassRefPtr<TypeBuilder::Profiler::ProfileHeader> InspectorProfilerAgent::stop(ErrorString* errorString)
{
    if (!m_recordingCPUProfile) {
        if (errorString)
            *errorString = "No recording profiles found";
        return 0;
    }
    m_recordingCPUProfile = false;
    String title = getCurrentUserInitiatedProfileName();
    RefPtr<ScriptProfile> profile = ScriptProfiler::stop(title);
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

void InspectorProfilerAgent::idleFinished()
{
    if (!m_profileNameIdleTimeMap || !m_profileNameIdleTimeMap->size())
        return;
    ScriptProfiler::setIdle(false);
    if (!m_idleStartTime)
        return;

    double idleTime = WTF::monotonicallyIncreasingTime() - m_idleStartTime;
    m_idleStartTime = 0.0;
    ProfileNameIdleTimeMap::iterator end = m_profileNameIdleTimeMap->end();
    for (ProfileNameIdleTimeMap::iterator it = m_profileNameIdleTimeMap->begin(); it != end; ++it)
        it->value += idleTime;
}

void InspectorProfilerAgent::idleStarted()
{
    if (!m_profileNameIdleTimeMap || !m_profileNameIdleTimeMap->size())
        return;
    m_idleStartTime = WTF::monotonicallyIncreasingTime();
    ScriptProfiler::setIdle(true);
}

void InspectorProfilerAgent::willProcessTask()
{
    idleFinished();
}

void InspectorProfilerAgent::didProcessTask()
{
    idleStarted();
}

void InspectorProfilerAgent::willEnterNestedRunLoop()
{
    idleStarted();
}

void InspectorProfilerAgent::didLeaveNestedRunLoop()
{
    idleFinished();
}

} // namespace WebCore

