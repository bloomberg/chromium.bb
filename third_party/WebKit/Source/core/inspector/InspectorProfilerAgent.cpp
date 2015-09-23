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

#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/ScriptCallStack.h"
#include <v8-profiler.h>

namespace blink {

namespace ProfilerAgentState {
static const char samplingInterval[] = "samplingInterval";
static const char userInitiatedProfiling[] = "userInitiatedProfiling";
static const char profilerEnabled[] = "profilerEnabled";
static const char nextProfileId[] = "nextProfileId";
}

namespace {

PassRefPtr<TypeBuilder::Array<TypeBuilder::Profiler::PositionTickInfo>> buildInspectorObjectForPositionTicks(const v8::CpuProfileNode* node)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::PositionTickInfo>> array = TypeBuilder::Array<TypeBuilder::Profiler::PositionTickInfo>::create();
    unsigned lineCount = node->GetHitLineCount();
    if (!lineCount)
        return array.release();

    Vector<v8::CpuProfileNode::LineTick> entries(lineCount);
    if (node->GetLineTicks(&entries[0], lineCount)) {
        for (unsigned i = 0; i < lineCount; i++) {
            RefPtr<TypeBuilder::Profiler::PositionTickInfo> line = TypeBuilder::Profiler::PositionTickInfo::create()
                .setLine(entries[i].line)
                .setTicks(entries[i].hit_count);
            array->addItem(line);
        }
    }

    return array.release();
}

PassRefPtr<TypeBuilder::Profiler::CPUProfileNode> buildInspectorObjectFor(const v8::CpuProfileNode* node)
{
    v8::HandleScope handleScope(v8::Isolate::GetCurrent());

    RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::CPUProfileNode>> children = TypeBuilder::Array<TypeBuilder::Profiler::CPUProfileNode>::create();
    const int childrenCount = node->GetChildrenCount();
    for (int i = 0; i < childrenCount; i++) {
        const v8::CpuProfileNode* child = node->GetChild(i);
        children->addItem(buildInspectorObjectFor(child));
    }

    RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::PositionTickInfo>> positionTicks = buildInspectorObjectForPositionTicks(node);

    RefPtr<TypeBuilder::Profiler::CPUProfileNode> result = TypeBuilder::Profiler::CPUProfileNode::create()
        .setFunctionName(toCoreString(node->GetFunctionName()))
        .setScriptId(String::number(node->GetScriptId()))
        .setUrl(toCoreString(node->GetScriptResourceName()))
        .setLineNumber(node->GetLineNumber())
        .setColumnNumber(node->GetColumnNumber())
        .setHitCount(node->GetHitCount())
        .setCallUID(node->GetCallUid())
        .setChildren(children.release())
        .setPositionTicks(positionTicks.release())
        .setDeoptReason(node->GetBailoutReason())
        .setId(node->GetNodeId());
    return result.release();
}

PassRefPtr<TypeBuilder::Array<int>> buildInspectorObjectForSamples(v8::CpuProfile* v8profile)
{
    RefPtr<TypeBuilder::Array<int>> array = TypeBuilder::Array<int>::create();
    int count = v8profile->GetSamplesCount();
    for (int i = 0; i < count; i++)
        array->addItem(v8profile->GetSample(i)->GetNodeId());
    return array.release();
}

PassRefPtr<TypeBuilder::Array<double>> buildInspectorObjectForTimestamps(v8::CpuProfile* v8profile)
{
    RefPtr<TypeBuilder::Array<double>> array = TypeBuilder::Array<double>::create();
    int count = v8profile->GetSamplesCount();
    for (int i = 0; i < count; i++)
        array->addItem(v8profile->GetSampleTimestamp(i));
    return array.release();
}

PassRefPtr<TypeBuilder::Profiler::CPUProfile> createCPUProfile(v8::CpuProfile* v8profile)
{
    RefPtr<TypeBuilder::Profiler::CPUProfile> profile = TypeBuilder::Profiler::CPUProfile::create()
        .setHead(buildInspectorObjectFor(v8profile->GetTopDownRoot()))
        .setStartTime(static_cast<double>(v8profile->GetStartTime()) / 1000000)
        .setEndTime(static_cast<double>(v8profile->GetEndTime()) / 1000000);
    profile->setSamples(buildInspectorObjectForSamples(v8profile));
    profile->setTimestamps(buildInspectorObjectForTimestamps(v8profile));
    return profile.release();
}

PassRefPtr<TypeBuilder::Debugger::Location> currentDebugLocation()
{
    RefPtrWillBeRawPtr<ScriptCallStack> callStack(currentScriptCallStack(1));
    const ScriptCallFrame& lastCaller = callStack->at(0);
    RefPtr<TypeBuilder::Debugger::Location> location = TypeBuilder::Debugger::Location::create()
        .setScriptId(lastCaller.scriptId())
        .setLineNumber(lastCaller.lineNumber());
    location->setColumnNumber(lastCaller.columnNumber());
    return location.release();
}

} // namespace

class InspectorProfilerAgent::ProfileDescriptor {
public:
    ProfileDescriptor(const String& id, const String& title)
        : m_id(id)
        , m_title(title) { }
    String m_id;
    String m_title;
};

PassOwnPtrWillBeRawPtr<InspectorProfilerAgent> InspectorProfilerAgent::create(v8::Isolate* isolate, InjectedScriptManager* injectedScriptManager, Client* client)
{
    return adoptPtrWillBeNoop(new InspectorProfilerAgent(isolate, injectedScriptManager, client));
}

InspectorProfilerAgent::InspectorProfilerAgent(v8::Isolate* isolate, InjectedScriptManager* injectedScriptManager, Client* client)
    : InspectorBaseAgent<InspectorProfilerAgent, InspectorFrontend::Profiler>("Profiler")
    , m_isolate(isolate)
    , m_injectedScriptManager(injectedScriptManager)
    , m_recordingCPUProfile(false)
    , m_client(client)
{
}

InspectorProfilerAgent::~InspectorProfilerAgent()
{
}

void InspectorProfilerAgent::consoleProfile(ExecutionContext* context, const String& title)
{
    UseCounter::count(context, UseCounter::DevToolsConsoleProfile);
    ASSERT(frontend() && enabled());
    String id = nextProfileId();
    m_startedProfiles.append(ProfileDescriptor(id, title));
    startProfiling(id);
    frontend()->consoleProfileStarted(id, currentDebugLocation(), title.isNull() ? 0 : &title);
}

void InspectorProfilerAgent::consoleProfileEnd(const String& title)
{
    ASSERT(frontend() && enabled());
    String id;
    String resolvedTitle;
    // Take last started profile if no title was passed.
    if (title.isNull()) {
        if (m_startedProfiles.isEmpty())
            return;
        id = m_startedProfiles.last().m_id;
        resolvedTitle = m_startedProfiles.last().m_title;
        m_startedProfiles.removeLast();
    } else {
        for (size_t i = 0; i < m_startedProfiles.size(); i++) {
            if (m_startedProfiles[i].m_title == title) {
                resolvedTitle = title;
                id = m_startedProfiles[i].m_id;
                m_startedProfiles.remove(i);
                break;
            }
        }
        if (id.isEmpty())
            return;
    }
    RefPtr<TypeBuilder::Profiler::CPUProfile> profile = stopProfiling(id, true);
    if (!profile)
        return;
    RefPtr<TypeBuilder::Debugger::Location> location = currentDebugLocation();
    frontend()->consoleProfileFinished(id, location, profile, resolvedTitle.isNull() ? 0 : &resolvedTitle);
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
    for (Vector<ProfileDescriptor>::reverse_iterator it = m_startedProfiles.rbegin(); it != m_startedProfiles.rend(); ++it)
        stopProfiling(it->m_id, false);
    m_startedProfiles.clear();
    stop(0, 0);
    m_instrumentingAgents->setInspectorProfilerAgent(nullptr);
    m_state->setBoolean(ProfilerAgentState::profilerEnabled, false);
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
    m_isolate->GetCpuProfiler()->SetSamplingInterval(interval);
}

void InspectorProfilerAgent::restore()
{
    if (m_state->getBoolean(ProfilerAgentState::profilerEnabled))
        doEnable();
    if (long interval = m_state->getLong(ProfilerAgentState::samplingInterval, 0))
        m_isolate->GetCpuProfiler()->SetSamplingInterval(interval);
    if (m_state->getBoolean(ProfilerAgentState::userInitiatedProfiling)) {
        ErrorString error;
        start(&error);
    }
}

void InspectorProfilerAgent::start(ErrorString* error)
{
    if (m_recordingCPUProfile)
        return;
    if (!enabled()) {
        *error = "Profiler is not enabled";
        return;
    }
    m_recordingCPUProfile = true;
    if (m_client)
        m_client->profilingStarted();
    m_frontendInitiatedProfileId = nextProfileId();
    startProfiling(m_frontendInitiatedProfileId);
    m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, true);
}

void InspectorProfilerAgent::stop(ErrorString* errorString, RefPtr<TypeBuilder::Profiler::CPUProfile>& profile)
{
    stop(errorString, &profile);
}

void InspectorProfilerAgent::stop(ErrorString* errorString, RefPtr<TypeBuilder::Profiler::CPUProfile>* profile)
{
    if (!m_recordingCPUProfile) {
        if (errorString)
            *errorString = "No recording profiles found";
        return;
    }
    m_recordingCPUProfile = false;
    if (m_client)
        m_client->profilingStopped();
    RefPtr<TypeBuilder::Profiler::CPUProfile> cpuProfile = stopProfiling(m_frontendInitiatedProfileId, !!profile);
    if (profile) {
        *profile = cpuProfile;
        if (!cpuProfile && errorString)
            *errorString = "Profile wasn't found";
    }
    m_frontendInitiatedProfileId = String();
    m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, false);
}

String InspectorProfilerAgent::nextProfileId()
{
    long nextId = m_state->getLong(ProfilerAgentState::nextProfileId, 1);
    m_state->setLong(ProfilerAgentState::nextProfileId, nextId + 1);
    return String::number(nextId);
}

void InspectorProfilerAgent::startProfiling(const String& title)
{
    v8::HandleScope handleScope(m_isolate);
    m_isolate->GetCpuProfiler()->StartProfiling(v8String(m_isolate, title), true);
}

PassRefPtr<TypeBuilder::Profiler::CPUProfile> InspectorProfilerAgent::stopProfiling(const String& title, bool serialize)
{
    v8::HandleScope handleScope(m_isolate);
    v8::CpuProfile* profile = m_isolate->GetCpuProfiler()->StopProfiling(v8String(m_isolate, title));
    if (!profile)
        return nullptr;
    RefPtr<TypeBuilder::Profiler::CPUProfile> result;
    if (serialize)
        result = createCPUProfile(profile);
    profile->Delete();
    return result.release();
}

bool InspectorProfilerAgent::isRecording() const
{
    return m_recordingCPUProfile || !m_startedProfiles.isEmpty();
}

void InspectorProfilerAgent::idleFinished()
{
    if (!isRecording())
        return;
    m_isolate->GetCpuProfiler()->SetIdle(false);
}

void InspectorProfilerAgent::idleStarted()
{
    if (!isRecording())
        return;
    m_isolate->GetCpuProfiler()->SetIdle(true);
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

DEFINE_TRACE(InspectorProfilerAgent)
{
    visitor->trace(m_injectedScriptManager);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink
