/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/v8/ScriptProfiler.h"

#include "V8ArrayBufferView.h"
#include "V8DOMWindow.h"
#include "V8Node.h"
#include "bindings/v8/RetainedDOMInfo.h"
#include "bindings/v8/ScriptObject.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/inspector/BindingVisitors.h"

#include <v8-profiler.h>
#include <v8.h>

#include "wtf/ThreadSpecific.h"

namespace WebCore {

typedef HashMap<String, double> ProfileNameIdleTimeMap;

void ScriptProfiler::start(ScriptState* state, const String& title)
{
    ProfileNameIdleTimeMap* profileNameIdleTimeMap = ScriptProfiler::currentProfileNameIdleTimeMap();
    if (profileNameIdleTimeMap->contains(title))
        return;
    profileNameIdleTimeMap->add(title, 0);

    v8::Isolate* isolate = state ? state->isolate() : v8::Isolate::GetCurrent();
    v8::CpuProfiler* profiler = isolate->GetCpuProfiler();
    if (!profiler)
        return;
    v8::HandleScope handleScope(isolate);
    profiler->StartCpuProfiling(v8String(title, isolate), true);
}

void ScriptProfiler::startForPage(Page*, const String& title)
{
    return start(0, title);
}

void ScriptProfiler::startForWorkerContext(WorkerContext*, const String& title)
{
    return start(0, title);
}

PassRefPtr<ScriptProfile> ScriptProfiler::stop(ScriptState* state, const String& title)
{
    v8::Isolate* isolate = state ? state->isolate() : v8::Isolate::GetCurrent();
    v8::CpuProfiler* profiler = isolate->GetCpuProfiler();
    if (!profiler)
        return 0;
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> securityToken;
    if (state)
        securityToken = state->context()->GetSecurityToken();
    const v8::CpuProfile* profile = profiler->StopCpuProfiling(v8String(title, isolate), securityToken);
    if (!profile)
        return 0;

    String profileTitle = toWebCoreString(profile->GetTitle());
    double idleTime = 0.0;
    ProfileNameIdleTimeMap* profileNameIdleTimeMap = ScriptProfiler::currentProfileNameIdleTimeMap();
    ProfileNameIdleTimeMap::iterator profileIdleTime = profileNameIdleTimeMap->find(profileTitle);
    if (profileIdleTime != profileNameIdleTimeMap->end()) {
        idleTime = profileIdleTime->value * 1000.0;
        profileNameIdleTimeMap->remove(profileIdleTime);
    }

    return ScriptProfile::create(profile, idleTime);
}

PassRefPtr<ScriptProfile> ScriptProfiler::stopForPage(Page*, const String& title)
{
    // Use null script state to avoid filtering by context security token.
    // All functions from all iframes should be visible from Inspector UI.
    return stop(0, title);
}

PassRefPtr<ScriptProfile> ScriptProfiler::stopForWorkerContext(WorkerContext*, const String& title)
{
    return stop(0, title);
}

void ScriptProfiler::collectGarbage()
{
    v8::V8::LowMemoryNotification();
}

ScriptObject ScriptProfiler::objectByHeapObjectId(unsigned id)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HeapProfiler* profiler = isolate->GetHeapProfiler();
    if (!profiler)
        return ScriptObject();
    // As ids are unique, it doesn't matter which HeapSnapshot owns HeapGraphNode.
    // We need to find first HeapSnapshot containing a node with the specified id.
    const v8::HeapGraphNode* node = 0;
    for (int i = 0, l = profiler->GetSnapshotCount(); i < l; ++i) {
        const v8::HeapSnapshot* snapshot = profiler->GetHeapSnapshot(i);
        node = snapshot->GetNodeById(id);
        if (node)
            break;
    }
    if (!node)
        return ScriptObject();

    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> value = node->GetHeapValue();
    if (!value->IsObject())
        return ScriptObject();

    v8::Handle<v8::Object> object = value.As<v8::Object>();

    ScriptState* scriptState = ScriptState::forContext(object->CreationContext());
    return ScriptObject(scriptState, object);
}

unsigned ScriptProfiler::getHeapObjectId(const ScriptValue& value)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HeapProfiler* profiler = isolate->GetHeapProfiler();
    v8::SnapshotObjectId id = profiler->GetObjectId(value.v8Value());
    return id;
}

namespace {

class ActivityControlAdapter : public v8::ActivityControl {
public:
    ActivityControlAdapter(ScriptProfiler::HeapSnapshotProgress* progress)
            : m_progress(progress), m_firstReport(true) { }
    ControlOption ReportProgressValue(int done, int total)
    {
        ControlOption result = m_progress->isCanceled() ? kAbort : kContinue;
        if (m_firstReport) {
            m_firstReport = false;
            m_progress->Start(total);
        } else
            m_progress->Worked(done);
        if (done >= total)
            m_progress->Done();
        return result;
    }
private:
    ScriptProfiler::HeapSnapshotProgress* m_progress;
    bool m_firstReport;
};

class GlobalObjectNameResolver : public v8::HeapProfiler::ObjectNameResolver {
public:
    virtual const char* GetName(v8::Handle<v8::Object> object)
    {
        if (V8DOMWrapper::isWrapperOfType(object, &V8DOMWindow::info)) {
            DOMWindow* window = V8DOMWindow::toNative(object);
            if (window) {
                CString url = window->document()->url().string().utf8();
                m_strings.append(url);
                return url.data();
            }
        }
        return 0;
    }

private:
    Vector<CString> m_strings;
};

} // namespace

void ScriptProfiler::startTrackingHeapObjects()
{
    v8::Isolate::GetCurrent()->GetHeapProfiler()->StartTrackingHeapObjects();
}

namespace {

class HeapStatsStream : public v8::OutputStream {
public:
    HeapStatsStream(ScriptProfiler::OutputStream* stream) : m_stream(stream) { }
    virtual void EndOfStream() OVERRIDE { }

    virtual WriteResult WriteAsciiChunk(char* data, int size) OVERRIDE
    {
        ASSERT(false);
        return kAbort;
    }

    virtual WriteResult WriteHeapStatsChunk(v8::HeapStatsUpdate* updateData, int count) OVERRIDE
    {
        Vector<uint32_t> rawData(count * 3);
        for (int i = 0; i < count; ++i) {
            int offset = i * 3;
            rawData[offset] = updateData[i].index;
            rawData[offset + 1] = updateData[i].count;
            rawData[offset + 2] = updateData[i].size;
        }
        m_stream->write(rawData.data(), rawData.size());
        return kContinue;
    }

private:
    ScriptProfiler::OutputStream* m_stream;
};

}

unsigned ScriptProfiler::requestHeapStatsUpdate(ScriptProfiler::OutputStream* stream)
{
    HeapStatsStream heapStatsStream(stream);
    return v8::Isolate::GetCurrent()->GetHeapProfiler()->GetHeapStats(&heapStatsStream);
}

void ScriptProfiler::stopTrackingHeapObjects()
{
    v8::Isolate::GetCurrent()->GetHeapProfiler()->StopTrackingHeapObjects();
}

// FIXME: This method should receive a ScriptState, from which we should retrieve an Isolate.
PassRefPtr<ScriptHeapSnapshot> ScriptProfiler::takeHeapSnapshot(const String& title, HeapSnapshotProgress* control)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HeapProfiler* profiler = isolate->GetHeapProfiler();
    if (!profiler)
        return 0;
    v8::HandleScope handleScope(isolate);
    ASSERT(control);
    ActivityControlAdapter adapter(control);
    GlobalObjectNameResolver resolver;
    const v8::HeapSnapshot* snapshot = profiler->TakeHeapSnapshot(v8String(title, isolate), &adapter, &resolver);
    return snapshot ? ScriptHeapSnapshot::create(snapshot) : 0;
}

static v8::RetainedObjectInfo* retainedDOMInfo(uint16_t classId, v8::Handle<v8::Value> wrapper)
{
    ASSERT(classId == v8DOMNodeClassId);
    if (!wrapper->IsObject())
        return 0;
    Node* node = V8Node::toNative(wrapper.As<v8::Object>());
    return node ? new RetainedDOMInfo(node) : 0;
}

void ScriptProfiler::initialize()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HeapProfiler* profiler = isolate->GetHeapProfiler();
    if (profiler)
        profiler->SetWrapperClassInfoProvider(v8DOMNodeClassId, &retainedDOMInfo);
}

void ScriptProfiler::visitNodeWrappers(WrappedNodeVisitor* visitor)
{
    // visitNodeWrappers() should receive a ScriptState and retrieve an Isolate
    // from the ScriptState.
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);

    class DOMNodeWrapperVisitor : public v8::PersistentHandleVisitor {
    public:
        DOMNodeWrapperVisitor(WrappedNodeVisitor* visitor, v8::Isolate* isolate)
            : m_visitor(visitor)
            , m_isolate(isolate)
        {
        }

        virtual void VisitPersistentHandle(v8::Persistent<v8::Value> value, uint16_t classId) OVERRIDE
        {
            if (classId != v8DOMNodeClassId)
                return;
            UNUSED_PARAM(m_isolate);
            ASSERT(V8Node::HasInstance(value, m_isolate, worldType(m_isolate)));
            ASSERT(value->IsObject());
            v8::Persistent<v8::Object> wrapper = v8::Persistent<v8::Object>::Cast(value);
            m_visitor->visitNode(V8Node::toNative(wrapper));
        }

    private:
        WrappedNodeVisitor* m_visitor;
        v8::Isolate* m_isolate;
    } wrapperVisitor(visitor, isolate);

    v8::V8::VisitHandlesWithClassIds(&wrapperVisitor);
}

void ScriptProfiler::visitExternalStrings(ExternalStringVisitor* visitor)
{
    V8PerIsolateData::current()->visitExternalStrings(visitor);
}

void ScriptProfiler::visitExternalArrays(ExternalArrayVisitor* visitor)
{
    class DOMObjectWrapperVisitor : public v8::PersistentHandleVisitor {
    public:
        explicit DOMObjectWrapperVisitor(ExternalArrayVisitor* visitor)
            : m_visitor(visitor)
        {
        }

        virtual void VisitPersistentHandle(v8::Persistent<v8::Value> value, uint16_t classId) OVERRIDE
        {
            if (classId != v8DOMObjectClassId)
                return;
            ASSERT(value->IsObject());
            v8::Persistent<v8::Object> wrapper = v8::Persistent<v8::Object>::Cast(value);
            if (!toWrapperTypeInfo(wrapper)->isSubclass(&V8ArrayBufferView::info))
                return;
            m_visitor->visitJSExternalArray(V8ArrayBufferView::toNative(wrapper));
        }

    private:
        ExternalArrayVisitor* m_visitor;
    } wrapperVisitor(visitor);

    v8::V8::VisitHandlesWithClassIds(&wrapperVisitor);
}

void ScriptProfiler::collectBindingMemoryInfo(MemoryInstrumentation* instrumentation)
{
    V8PerIsolateData* data = V8PerIsolateData::current();
    instrumentation->addRootObject(data);
}

size_t ScriptProfiler::profilerSnapshotsSize()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HeapProfiler* profiler = isolate->GetHeapProfiler();
    if (!profiler)
        return 0;
    return profiler->GetProfilerMemorySize();
}

ProfileNameIdleTimeMap* ScriptProfiler::currentProfileNameIdleTimeMap()
{
    AtomicallyInitializedStatic(WTF::ThreadSpecific<ProfileNameIdleTimeMap>*, map = new WTF::ThreadSpecific<ProfileNameIdleTimeMap>);
    return *map;
}

} // namespace WebCore

