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
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/RemoteObjectId.h"
#include "platform/Timer.h"
#include "wtf/CurrentTime.h"
#include <v8-profiler.h>

namespace blink {

typedef uint32_t SnapshotObjectId;

namespace HeapProfilerAgentState {
static const char heapProfilerEnabled[] = "heapProfilerEnabled";
static const char heapObjectsTrackingEnabled[] = "heapObjectsTrackingEnabled";
static const char allocationTrackingEnabled[] = "allocationTrackingEnabled";
}

namespace {

class HeapSnapshotProgress final : public v8::ActivityControl {
public:
    HeapSnapshotProgress(InspectorFrontend::HeapProfiler* frontend)
        : m_frontend(frontend) { }
    ControlOption ReportProgressValue(int done, int total) override
    {
        m_frontend->reportHeapSnapshotProgress(done, total, nullptr);
        if (done >= total) {
            const bool finished = true;
            m_frontend->reportHeapSnapshotProgress(total, total, &finished);
        }
        m_frontend->flush();
        return kContinue;
    }
private:
    InspectorFrontend::HeapProfiler* m_frontend;
};

class GlobalObjectNameResolver final : public v8::HeapProfiler::ObjectNameResolver {
public:
    explicit GlobalObjectNameResolver(v8::Isolate* isolate) : m_isolate(isolate) { }
    const char* GetName(v8::Local<v8::Object> object) override
    {
        DOMWindow* window = toDOMWindow(m_isolate, object);
        if (!window)
            return 0;
        CString url = toLocalDOMWindow(window)->document()->url().string().utf8();
        m_strings.append(url);
        return url.data();
    }

private:
    v8::Isolate* m_isolate;
    Vector<CString> m_strings;
};

class HeapSnapshotOutputStream final : public v8::OutputStream {
public:
    HeapSnapshotOutputStream(InspectorFrontend::HeapProfiler* frontend)
        : m_frontend(frontend) { }
    void EndOfStream() override { }
    int GetChunkSize() override { return 102400; }
    WriteResult WriteAsciiChunk(char* data, int size) override
    {
        m_frontend->addHeapSnapshotChunk(String(data, size));
        m_frontend->flush();
        return kContinue;
    }
private:
    InspectorFrontend::HeapProfiler* m_frontend;
};

v8::Local<v8::Object> objectByHeapObjectId(v8::Isolate* isolate, unsigned id)
{
    v8::HeapProfiler* profiler = isolate->GetHeapProfiler();
    v8::Local<v8::Value> value = profiler->FindObjectById(id);
    if (value.IsEmpty() || !value->IsObject())
        return v8::Local<v8::Object>();

    v8::Local<v8::Object> object = value.As<v8::Object>();

    if (object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount) {
        v8::Local<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
        // Skip wrapper boilerplates which are like regular wrappers but don't have
        // native object.
        if (!wrapper.IsEmpty() && wrapper->IsUndefined())
            return v8::Local<v8::Object>();
    }

    return object;
}

class InspectableHeapObject final : public InjectedScriptHost::InspectableObject {
public:
    explicit InspectableHeapObject(unsigned heapObjectId) : m_heapObjectId(heapObjectId) { }
    v8::Local<v8::Value> get(v8::Local<v8::Context> context) override
    {
        return objectByHeapObjectId(context->GetIsolate(), m_heapObjectId);
    }
private:
    unsigned m_heapObjectId;
};

class HeapStatsStream final : public v8::OutputStream {
public:
    HeapStatsStream(InspectorFrontend::HeapProfiler* frontend)
        : m_frontend(frontend)
    {
    }

    void EndOfStream() override { }

    WriteResult WriteAsciiChunk(char* data, int size) override
    {
        ASSERT(false);
        return kAbort;
    }

    WriteResult WriteHeapStatsChunk(v8::HeapStatsUpdate* updateData, int count) override
    {
        ASSERT(count > 0);
        RefPtr<TypeBuilder::Array<int>> statsDiff = TypeBuilder::Array<int>::create();
        for (int i = 0; i < count; ++i) {
            statsDiff->addItem(updateData[i].index);
            statsDiff->addItem(updateData[i].count);
            statsDiff->addItem(updateData[i].size);
        }
        m_frontend->heapStatsUpdate(statsDiff.release());
        return kContinue;
    }

private:
    InspectorFrontend::HeapProfiler* m_frontend;
};

} // namespace

class InspectorHeapProfilerAgent::HeapStatsUpdateTask final : public NoBaseWillBeGarbageCollectedFinalized<InspectorHeapProfilerAgent::HeapStatsUpdateTask> {
public:
    explicit HeapStatsUpdateTask(InspectorHeapProfilerAgent*);
    void startTimer();
    void resetTimer() { m_timer.stop(); }
    void onTimer(Timer<HeapStatsUpdateTask>*);
    DECLARE_TRACE();

private:
    RawPtrWillBeMember<InspectorHeapProfilerAgent> m_heapProfilerAgent;
    Timer<HeapStatsUpdateTask> m_timer;
};

PassOwnPtrWillBeRawPtr<InspectorHeapProfilerAgent> InspectorHeapProfilerAgent::create(v8::Isolate* isolate, InjectedScriptManager* injectedScriptManager)
{
    return adoptPtrWillBeNoop(new InspectorHeapProfilerAgent(isolate, injectedScriptManager));
}

InspectorHeapProfilerAgent::InspectorHeapProfilerAgent(v8::Isolate* isolate, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorHeapProfilerAgent, InspectorFrontend::HeapProfiler>("HeapProfiler")
    , m_isolate(isolate)
    , m_injectedScriptManager(injectedScriptManager)
{
}

InspectorHeapProfilerAgent::~InspectorHeapProfilerAgent()
{
}

void InspectorHeapProfilerAgent::restore()
{
    if (m_state->booleanProperty(HeapProfilerAgentState::heapProfilerEnabled, false))
        frontend()->resetProfiles();
    if (m_state->booleanProperty(HeapProfilerAgentState::heapObjectsTrackingEnabled, false))
        startTrackingHeapObjectsInternal(m_state->booleanProperty(HeapProfilerAgentState::allocationTrackingEnabled, false));
}

void InspectorHeapProfilerAgent::collectGarbage(ErrorString*)
{
    m_isolate->LowMemoryNotification();
}

InspectorHeapProfilerAgent::HeapStatsUpdateTask::HeapStatsUpdateTask(InspectorHeapProfilerAgent* heapProfilerAgent)
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

DEFINE_TRACE(InspectorHeapProfilerAgent::HeapStatsUpdateTask)
{
    visitor->trace(m_heapProfilerAgent);
}

void InspectorHeapProfilerAgent::startTrackingHeapObjects(ErrorString*, const bool* trackAllocations)
{
    m_state->setBoolean(HeapProfilerAgentState::heapObjectsTrackingEnabled, true);
    bool allocationTrackingEnabled = asBool(trackAllocations);
    m_state->setBoolean(HeapProfilerAgentState::allocationTrackingEnabled, allocationTrackingEnabled);
    startTrackingHeapObjectsInternal(allocationTrackingEnabled);
}

void InspectorHeapProfilerAgent::requestHeapStatsUpdate()
{
    if (!frontend())
        return;
    HeapStatsStream stream(frontend());
    SnapshotObjectId lastSeenObjectId = m_isolate->GetHeapProfiler()->GetHeapStats(&stream);
    frontend()->lastSeenObjectId(lastSeenObjectId, WTF::currentTimeMS());
}

void InspectorHeapProfilerAgent::stopTrackingHeapObjects(ErrorString* error, const bool* reportProgress)
{
    if (!m_heapStatsUpdateTask) {
        *error = "Heap object tracking is not started.";
        return;
    }
    requestHeapStatsUpdate();
    takeHeapSnapshot(error, reportProgress);
    stopTrackingHeapObjectsInternal();
}

void InspectorHeapProfilerAgent::startTrackingHeapObjectsInternal(bool trackAllocations)
{
    if (m_heapStatsUpdateTask)
        return;
    m_isolate->GetHeapProfiler()->StartTrackingHeapObjects(trackAllocations);
    m_heapStatsUpdateTask = adoptPtrWillBeNoop(new HeapStatsUpdateTask(this));
    m_heapStatsUpdateTask->startTimer();
}

void InspectorHeapProfilerAgent::stopTrackingHeapObjectsInternal()
{
    if (!m_heapStatsUpdateTask)
        return;
    m_isolate->GetHeapProfiler()->StopTrackingHeapObjects();
    m_heapStatsUpdateTask->resetTimer();
    m_heapStatsUpdateTask.clear();
    m_state->setBoolean(HeapProfilerAgentState::heapObjectsTrackingEnabled, false);
    m_state->setBoolean(HeapProfilerAgentState::allocationTrackingEnabled, false);
}

void InspectorHeapProfilerAgent::enable(ErrorString*)
{
    m_state->setBoolean(HeapProfilerAgentState::heapProfilerEnabled, true);
}

void InspectorHeapProfilerAgent::disable(ErrorString* error)
{
    stopTrackingHeapObjectsInternal();
    m_isolate->GetHeapProfiler()->ClearObjectIds();
    m_state->setBoolean(HeapProfilerAgentState::heapProfilerEnabled, false);
}

void InspectorHeapProfilerAgent::takeHeapSnapshot(ErrorString* errorString, const bool* reportProgress)
{
    v8::HeapProfiler* profiler = m_isolate->GetHeapProfiler();
    if (!profiler) {
        *errorString = "Cannot access v8 heap profiler";
        return;
    }
    OwnPtr<HeapSnapshotProgress> progress;
    if (asBool(reportProgress))
        progress = adoptPtr(new HeapSnapshotProgress(frontend()));

    v8::HandleScope handleScope(m_isolate); // Remove?
    GlobalObjectNameResolver resolver(m_isolate);
    const v8::HeapSnapshot* snapshot = profiler->TakeHeapSnapshot(progress.get(), &resolver);
    if (!snapshot) {
        *errorString = "Failed to take heap snapshot";
        return;
    }
    HeapSnapshotOutputStream stream(frontend());
    snapshot->Serialize(&stream);
    const_cast<v8::HeapSnapshot*>(snapshot)->Delete();
}

void InspectorHeapProfilerAgent::getObjectByHeapObjectId(ErrorString* error, const String& heapSnapshotObjectId, const String* objectGroup, RefPtr<TypeBuilder::Runtime::RemoteObject>& result)
{
    bool ok;
    unsigned id = heapSnapshotObjectId.toUInt(&ok);
    if (!ok) {
        *error = "Invalid heap snapshot object id";
        return;
    }

    v8::HandleScope handles(m_isolate);
    v8::Local<v8::Object> heapObject = objectByHeapObjectId(m_isolate, id);
    if (heapObject.IsEmpty()) {
        *error = "Object is not available";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->injectedScriptFor(heapObject->CreationContext());
    if (!injectedScript) {
        *error = "Object is not available. Inspected context is gone";
        return;
    }
    result = injectedScript->wrapObject(heapObject, objectGroup ? *objectGroup : "");
    if (!result)
        *error = "Failed to wrap object";
}

void InspectorHeapProfilerAgent::addInspectedHeapObject(ErrorString* errorString, const String& inspectedHeapObjectId)
{
    bool ok;
    unsigned id = inspectedHeapObjectId.toUInt(&ok);
    if (!ok) {
        *errorString = "Invalid heap snapshot object id";
        return;
    }
    m_injectedScriptManager->injectedScriptHost()->addInspectedObject(adoptPtr(new InspectableHeapObject(id)));
}

void InspectorHeapProfilerAgent::getHeapObjectId(ErrorString* errorString, const String& objectId, String* heapSnapshotObjectId)
{
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(objectId);
    if (!remoteId) {
        *errorString = "Invalid object id";
        return;
    }
    InjectedScript* injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (!injectedScript) {
        *errorString = "Inspected context has gone";
        return;
    }

    v8::HandleScope handles(injectedScript->isolate());
    v8::Local<v8::Value> value = injectedScript->findObject(*remoteId);
    if (value.IsEmpty() || value->IsUndefined()) {
        *errorString = "Object with given id not found";
        return;
    }

    v8::SnapshotObjectId id = m_isolate->GetHeapProfiler()->GetObjectId(value);
    *heapSnapshotObjectId = String::number(id);
}

DEFINE_TRACE(InspectorHeapProfilerAgent)
{
    visitor->trace(m_heapStatsUpdateTask);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink

