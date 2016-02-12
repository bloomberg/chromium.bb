// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8HeapProfilerAgentImpl_h
#define V8HeapProfilerAgentImpl_h

#include "core/CoreExport.h"
#include "core/inspector/v8/public/V8HeapProfilerAgent.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class V8RuntimeAgent;

typedef String ErrorString;

class CORE_EXPORT V8HeapProfilerAgentImpl : public V8HeapProfilerAgent {
    WTF_MAKE_NONCOPYABLE(V8HeapProfilerAgentImpl);
public:
    explicit V8HeapProfilerAgentImpl(v8::Isolate*, V8RuntimeAgent*);
    ~V8HeapProfilerAgentImpl() override;

    void setInspectorState(PassRefPtr<JSONObject> state) override { m_state = state; }
    void setFrontend(protocol::Frontend::HeapProfiler* frontend) override { m_frontend = frontend; }
    void clearFrontend() override;
    void restore() override;

    void collectGarbage(ErrorString*) override;

    void enable(ErrorString*) override;
    void startTrackingHeapObjects(ErrorString*, const bool* trackAllocations) override;
    void stopTrackingHeapObjects(ErrorString*, const bool* reportProgress) override;

    void disable(ErrorString*) override;

    void takeHeapSnapshot(ErrorString*, const bool* reportProgress) override;

    void getObjectByHeapObjectId(ErrorString*, const String& heapSnapshotObjectId, const String* objectGroup, RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>& result) override;
    void addInspectedHeapObject(ErrorString*, const String& inspectedHeapObjectId) override;
    void getHeapObjectId(ErrorString*, const String& objectId, String* heapSnapshotObjectId) override;

    void requestHeapStatsUpdate() override;

private:
    void startTrackingHeapObjectsInternal(bool trackAllocations);
    void stopTrackingHeapObjectsInternal();

    v8::Isolate* m_isolate;
    V8RuntimeAgent* m_runtimeAgent;
    protocol::Frontend::HeapProfiler* m_frontend;
    RefPtr<JSONObject> m_state;
};

} // namespace blink

#endif // !defined(V8HeapProfilerAgentImpl_h)
