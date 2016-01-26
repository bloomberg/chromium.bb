// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/V8ProfilerAgentImpl.h"

#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/inspector/ScriptCallStack.h"
#include "wtf/Atomics.h"
#include <v8-profiler.h>

namespace blink {

namespace ProfilerAgentState {
static const char samplingInterval[] = "samplingInterval";
static const char userInitiatedProfiling[] = "userInitiatedProfiling";
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
    RefPtr<ScriptCallStack> callStack(currentScriptCallStack(1));
    const ScriptCallFrame& lastCaller = callStack->at(0);
    RefPtr<TypeBuilder::Debugger::Location> location = TypeBuilder::Debugger::Location::create()
        .setScriptId(lastCaller.scriptId())
        .setLineNumber(lastCaller.lineNumber());
    location->setColumnNumber(lastCaller.columnNumber());
    return location.release();
}

volatile int s_lastProfileId = 0;

} // namespace

class V8ProfilerAgentImpl::ProfileDescriptor {
public:
    ProfileDescriptor(const String& id, const String& title)
        : m_id(id)
        , m_title(title) { }
    String m_id;
    String m_title;
};

PassOwnPtr<V8ProfilerAgent> V8ProfilerAgent::create(v8::Isolate* isolate)
{
    return adoptPtr(new V8ProfilerAgentImpl(isolate));
}

V8ProfilerAgentImpl::V8ProfilerAgentImpl(v8::Isolate* isolate)
    : m_isolate(isolate)
    , m_state(nullptr)
    , m_frontend(nullptr)
    , m_enabled(false)
    , m_recordingCPUProfile(false)
{
}

V8ProfilerAgentImpl::~V8ProfilerAgentImpl()
{
}

void V8ProfilerAgentImpl::consoleProfile(const String& title)
{
    ASSERT(m_frontend && m_enabled);
    String id = nextProfileId();
    m_startedProfiles.append(ProfileDescriptor(id, title));
    startProfiling(id);
    m_frontend->consoleProfileStarted(id, currentDebugLocation(), title.isNull() ? 0 : &title);
}

void V8ProfilerAgentImpl::consoleProfileEnd(const String& title)
{
    ASSERT(m_frontend && m_enabled);
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
    m_frontend->consoleProfileFinished(id, location, profile, resolvedTitle.isNull() ? 0 : &resolvedTitle);
}

void V8ProfilerAgentImpl::enable(ErrorString*)
{
    m_enabled = true;
}

void V8ProfilerAgentImpl::disable(ErrorString*)
{
    for (Vector<ProfileDescriptor>::reverse_iterator it = m_startedProfiles.rbegin(); it != m_startedProfiles.rend(); ++it)
        stopProfiling(it->m_id, false);
    m_startedProfiles.clear();
    stop(0, 0);
    m_enabled = false;
}

void V8ProfilerAgentImpl::setSamplingInterval(ErrorString* error, int interval)
{
    if (m_recordingCPUProfile) {
        *error = "Cannot change sampling interval when profiling.";
        return;
    }
    m_state->setNumber(ProfilerAgentState::samplingInterval, interval);
    m_isolate->GetCpuProfiler()->SetSamplingInterval(interval);
}

void V8ProfilerAgentImpl::clearFrontend()
{
    ErrorString error;
    disable(&error);
    ASSERT(m_frontend);
    m_frontend = nullptr;
}

void V8ProfilerAgentImpl::restore()
{
    m_enabled = true;
    long interval = 0;
    m_state->getNumber(ProfilerAgentState::samplingInterval, &interval);
    if (interval)
        m_isolate->GetCpuProfiler()->SetSamplingInterval(interval);
    if (m_state->booleanProperty(ProfilerAgentState::userInitiatedProfiling, false)) {
        ErrorString error;
        start(&error);
    }
}

void V8ProfilerAgentImpl::start(ErrorString* error)
{
    if (m_recordingCPUProfile)
        return;
    if (!m_enabled) {
        *error = "Profiler is not enabled";
        return;
    }
    m_recordingCPUProfile = true;
    m_frontendInitiatedProfileId = nextProfileId();
    startProfiling(m_frontendInitiatedProfileId);
    m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, true);
}

void V8ProfilerAgentImpl::stop(ErrorString* errorString, RefPtr<TypeBuilder::Profiler::CPUProfile>& profile)
{
    stop(errorString, &profile);
}

void V8ProfilerAgentImpl::stop(ErrorString* errorString, RefPtr<TypeBuilder::Profiler::CPUProfile>* profile)
{
    if (!m_recordingCPUProfile) {
        if (errorString)
            *errorString = "No recording profiles found";
        return;
    }
    m_recordingCPUProfile = false;
    RefPtr<TypeBuilder::Profiler::CPUProfile> cpuProfile = stopProfiling(m_frontendInitiatedProfileId, !!profile);
    if (profile) {
        *profile = cpuProfile;
        if (!cpuProfile && errorString)
            *errorString = "Profile is not found";
    }
    m_frontendInitiatedProfileId = String();
    m_state->setBoolean(ProfilerAgentState::userInitiatedProfiling, false);
}

String V8ProfilerAgentImpl::nextProfileId()
{
    return String::number(atomicIncrement(&s_lastProfileId));
}

void V8ProfilerAgentImpl::startProfiling(const String& title)
{
    v8::HandleScope handleScope(m_isolate);
    m_isolate->GetCpuProfiler()->StartProfiling(v8String(m_isolate, title), true);
}

PassRefPtr<TypeBuilder::Profiler::CPUProfile> V8ProfilerAgentImpl::stopProfiling(const String& title, bool serialize)
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

bool V8ProfilerAgentImpl::isRecording() const
{
    return m_recordingCPUProfile || !m_startedProfiles.isEmpty();
}

void V8ProfilerAgentImpl::idleFinished()
{
    if (!isRecording())
        return;
    m_isolate->GetCpuProfiler()->SetIdle(false);
}

void V8ProfilerAgentImpl::idleStarted()
{
    if (!isRecording())
        return;
    m_isolate->GetCpuProfiler()->SetIdle(true);
}

} // namespace blink
