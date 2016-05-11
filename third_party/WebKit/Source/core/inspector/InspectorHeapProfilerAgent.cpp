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

#include "platform/v8_inspector/public/V8HeapProfilerAgent.h"
#include <v8-profiler.h>

namespace blink {

InspectorHeapProfilerAgent::InspectorHeapProfilerAgent(V8HeapProfilerAgent* agent)
    : InspectorBaseAgent<InspectorHeapProfilerAgent, protocol::Frontend::HeapProfiler>("HeapProfiler")
    , m_v8HeapProfilerAgent(agent)
{
}

InspectorHeapProfilerAgent::~InspectorHeapProfilerAgent()
{
}

// InspectorBaseAgent overrides.
void InspectorHeapProfilerAgent::init(InstrumentingAgents* instrumentingAgents, protocol::Frontend* baseFrontend, protocol::Dispatcher* dispatcher, protocol::DictionaryValue* state)
{
    InspectorBaseAgent::init(instrumentingAgents, baseFrontend, dispatcher, state);
    m_v8HeapProfilerAgent->setInspectorState(m_state);
    m_v8HeapProfilerAgent->setFrontend(frontend());
}

void InspectorHeapProfilerAgent::dispose()
{
    m_v8HeapProfilerAgent->clearFrontend();
    InspectorBaseAgent::dispose();
}

void InspectorHeapProfilerAgent::restore()
{
    m_v8HeapProfilerAgent->restore();
}

// Protocol implementation.
void InspectorHeapProfilerAgent::collectGarbage(ErrorString* error)
{
    m_v8HeapProfilerAgent->collectGarbage(error);
}

void InspectorHeapProfilerAgent::startTrackingHeapObjects(ErrorString* error, const protocol::Maybe<bool>& trackAllocations)
{
    m_v8HeapProfilerAgent->startTrackingHeapObjects(error, trackAllocations);
}

void InspectorHeapProfilerAgent::stopTrackingHeapObjects(ErrorString* error, const protocol::Maybe<bool>& reportProgress)
{
    m_v8HeapProfilerAgent->stopTrackingHeapObjects(error, reportProgress);
}

void InspectorHeapProfilerAgent::enable(ErrorString* error)
{
    m_v8HeapProfilerAgent->enable(error);
}

void InspectorHeapProfilerAgent::disable(ErrorString* error)
{
    m_v8HeapProfilerAgent->disable(error);
}

void InspectorHeapProfilerAgent::takeHeapSnapshot(ErrorString* errorString, const protocol::Maybe<bool>& reportProgress)
{
    m_v8HeapProfilerAgent->takeHeapSnapshot(errorString, reportProgress);
}

void InspectorHeapProfilerAgent::getObjectByHeapObjectId(ErrorString* error, const String16& heapSnapshotObjectId, const protocol::Maybe<String16>& objectGroup, OwnPtr<protocol::Runtime::RemoteObject>* result)
{
    m_v8HeapProfilerAgent->getObjectByHeapObjectId(error, heapSnapshotObjectId, objectGroup, result);
}

void InspectorHeapProfilerAgent::addInspectedHeapObject(ErrorString* error, const String16& inspectedHeapObjectId)
{
    m_v8HeapProfilerAgent->addInspectedHeapObject(error, inspectedHeapObjectId);
}

void InspectorHeapProfilerAgent::getHeapObjectId(ErrorString* errorString, const String16& objectId, String16* heapSnapshotObjectId)
{
    m_v8HeapProfilerAgent->getHeapObjectId(errorString, objectId, heapSnapshotObjectId);
}

void InspectorHeapProfilerAgent::startSampling(ErrorString* errorString, const Maybe<double>& samplingInterval)
{
    m_v8HeapProfilerAgent->startSampling(errorString, samplingInterval);
}

void InspectorHeapProfilerAgent::stopSampling(ErrorString* errorString, OwnPtr<protocol::HeapProfiler::SamplingHeapProfile>* profile)
{
    m_v8HeapProfilerAgent->stopSampling(errorString, profile);
}

} // namespace blink
