/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/InspectorHeapProfilerAgent.h"

#include "bindings/core/v8/V8Binding.h"
#include "platform/Timer.h"
#include "platform/v8_inspector/public/V8HeapProfilerAgent.h"
#include <v8-profiler.h>

namespace blink {

namespace HeapProfilerAgentState {
static const char heapObjectsTrackingEnabled[] = "heapObjectsTrackingEnabled";
}

class InspectorHeapProfilerAgent::HeapStatsUpdateTask final {
public:
    explicit HeapStatsUpdateTask(V8HeapProfilerAgent*);
    void startTimer();
    void resetTimer() { m_timer.stop(); }
    void onTimer(Timer<HeapStatsUpdateTask>*);

private:
    V8HeapProfilerAgent* m_heapProfilerAgent;
    Timer<HeapStatsUpdateTask> m_timer;
};

InspectorHeapProfilerAgent::HeapStatsUpdateTask::HeapStatsUpdateTask(V8HeapProfilerAgent* heapProfilerAgent)
    : m_heapProfilerAgent(heapProfilerAgent)
    , m_timer(this, &HeapStatsUpdateTask::onTimer)
{
}

void InspectorHeapProfilerAgent::HeapStatsUpdateTask::onTimer(Timer<HeapStatsUpdateTask>*)
{
    // The timer is stopped on m_heapProfilerAgent destruction,
    // so this method will never be called after m_heapProfilerAgent has been destroyed.
    m_heapProfilerAgent->requestHeapStatsUpdate();
}

void InspectorHeapProfilerAgent::HeapStatsUpdateTask::startTimer()
{
    ASSERT(!m_timer.isActive());
    m_timer.startRepeating(0.05, BLINK_FROM_HERE);
}

PassOwnPtrWillBeRawPtr<InspectorHeapProfilerAgent> InspectorHeapProfilerAgent::create(v8::Isolate* isolate, V8RuntimeAgent* runtimeAgent)
{
    return adoptPtrWillBeNoop(new InspectorHeapProfilerAgent(isolate, runtimeAgent));
}

InspectorHeapProfilerAgent::InspectorHeapProfilerAgent(v8::Isolate* isolate, V8RuntimeAgent* runtimeAgent)
    : InspectorBaseAgent<InspectorHeapProfilerAgent, protocol::Frontend::HeapProfiler>("HeapProfiler")
    , m_v8HeapProfilerAgent(V8HeapProfilerAgent::create(isolate, runtimeAgent))
    , m_isolate(isolate)
{
}

InspectorHeapProfilerAgent::~InspectorHeapProfilerAgent()
{
}

// InspectorBaseAgent overrides.
void InspectorHeapProfilerAgent::setState(PassRefPtr<JSONObject> state)
{
    InspectorBaseAgent::setState(state);
    m_v8HeapProfilerAgent->setInspectorState(m_state);
}

void InspectorHeapProfilerAgent::setFrontend(protocol::Frontend* frontend)
{
    InspectorBaseAgent::setFrontend(frontend);
    m_v8HeapProfilerAgent->setFrontend(protocol::Frontend::HeapProfiler::from(frontend));
}

void InspectorHeapProfilerAgent::clearFrontend()
{
    m_v8HeapProfilerAgent->clearFrontend();
    InspectorBaseAgent::clearFrontend();
}

void InspectorHeapProfilerAgent::restore()
{
    m_v8HeapProfilerAgent->restore();
    if (m_state->booleanProperty(HeapProfilerAgentState::heapObjectsTrackingEnabled, false))
        startUpdateStatsTimer();
}

// Protocol implementation.
void InspectorHeapProfilerAgent::collectGarbage(ErrorString* error)
{
    m_v8HeapProfilerAgent->collectGarbage(error);
}

void InspectorHeapProfilerAgent::startTrackingHeapObjects(ErrorString* error, const bool* trackAllocations)
{
    m_v8HeapProfilerAgent->startTrackingHeapObjects(error, trackAllocations);
    startUpdateStatsTimer();
}

void InspectorHeapProfilerAgent::stopTrackingHeapObjects(ErrorString* error, const bool* reportProgress)
{
    m_v8HeapProfilerAgent->stopTrackingHeapObjects(error, reportProgress);
    stopUpdateStatsTimer();
}

void InspectorHeapProfilerAgent::startUpdateStatsTimer()
{
    if (m_heapStatsUpdateTask)
        return;
    m_heapStatsUpdateTask = adoptPtr(new HeapStatsUpdateTask(m_v8HeapProfilerAgent.get()));
    m_heapStatsUpdateTask->startTimer();
}

void InspectorHeapProfilerAgent::stopUpdateStatsTimer()
{
    if (!m_heapStatsUpdateTask)
        return;
    m_heapStatsUpdateTask->resetTimer();
    m_heapStatsUpdateTask.clear();
}

void InspectorHeapProfilerAgent::enable(ErrorString* error)
{
    m_v8HeapProfilerAgent->enable(error);
}

void InspectorHeapProfilerAgent::disable(ErrorString* error)
{
    m_v8HeapProfilerAgent->disable(error);
    stopUpdateStatsTimer();
}

void InspectorHeapProfilerAgent::takeHeapSnapshot(ErrorString* errorString, const bool* reportProgress)
{
    m_v8HeapProfilerAgent->takeHeapSnapshot(errorString, reportProgress);
}

void InspectorHeapProfilerAgent::getObjectByHeapObjectId(ErrorString* error, const String& heapSnapshotObjectId, const String* objectGroup, RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>& result)
{
    bool ok;
    unsigned id = heapSnapshotObjectId.toUInt(&ok);
    if (!ok) {
        *error = "Invalid heap snapshot object id";
        return;
    }

    if (!isInspectableHeapObject(id)) {
        *error = "Object is not available";
        return;
    }

    m_v8HeapProfilerAgent->getObjectByHeapObjectId(error, heapSnapshotObjectId, objectGroup, result);
}

void InspectorHeapProfilerAgent::addInspectedHeapObject(ErrorString* error, const String& inspectedHeapObjectId)
{
    bool ok;
    unsigned id = inspectedHeapObjectId.toUInt(&ok);
    if (!ok) {
        *error = "Invalid heap snapshot object id";
        return;
    }

    if (!isInspectableHeapObject(id)) {
        *error = "Object is not available";
        return;
    }

    m_v8HeapProfilerAgent->addInspectedHeapObject(error, inspectedHeapObjectId);
}

void InspectorHeapProfilerAgent::getHeapObjectId(ErrorString* errorString, const String& objectId, String* heapSnapshotObjectId)
{
    m_v8HeapProfilerAgent->getHeapObjectId(errorString, objectId, heapSnapshotObjectId);
}

bool InspectorHeapProfilerAgent::isInspectableHeapObject(unsigned id)
{
    v8::HandleScope scope(m_isolate);
    v8::HeapProfiler* profiler = m_isolate->GetHeapProfiler();
    v8::Local<v8::Value> value = profiler->FindObjectById(id);
    if (value.IsEmpty() || !value->IsObject())
        return false;
    v8::Local<v8::Object> object = value.As<v8::Object>();
    if (object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount) {
        v8::Local<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
        // Skip wrapper boilerplates which are like regular wrappers but don't have
        // native object.
        if (!wrapper.IsEmpty() && wrapper->IsUndefined())
            return false;
    }
    return true;
}

} // namespace blink
