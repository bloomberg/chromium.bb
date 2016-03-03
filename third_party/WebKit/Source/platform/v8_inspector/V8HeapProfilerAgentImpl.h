// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8HeapProfilerAgentImpl_h
#define V8HeapProfilerAgentImpl_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/v8_inspector/public/V8HeapProfilerAgent.h"

namespace blink {

class V8RuntimeAgentImpl;

typedef String ErrorString;

using protocol::Maybe;

class V8HeapProfilerAgentImpl : public V8HeapProfilerAgent {
    PROTOCOL_DISALLOW_COPY(V8HeapProfilerAgentImpl);
public:
    explicit V8HeapProfilerAgentImpl(v8::Isolate*, V8RuntimeAgent*);
    ~V8HeapProfilerAgentImpl() override;

    void setInspectorState(protocol::DictionaryValue* state) override { m_state = state; }
    void setFrontend(protocol::Frontend::HeapProfiler* frontend) override { m_frontend = frontend; }
    void clearFrontend() override;
    void restore() override;

    void collectGarbage(ErrorString*) override;

    void enable(ErrorString*) override;
    void startTrackingHeapObjects(ErrorString*, const Maybe<bool>& trackAllocations) override;
    void stopTrackingHeapObjects(ErrorString*, const Maybe<bool>& reportProgress) override;

    void disable(ErrorString*) override;

    void takeHeapSnapshot(ErrorString*, const Maybe<bool>& reportProgress) override;

    void getObjectByHeapObjectId(ErrorString*, const String& heapSnapshotObjectId, const Maybe<String>& objectGroup, OwnPtr<protocol::Runtime::RemoteObject>* result) override;
    void addInspectedHeapObject(ErrorString*, const String& inspectedHeapObjectId) override;
    void getHeapObjectId(ErrorString*, const String& objectId, String* heapSnapshotObjectId) override;

    void startSampling(ErrorString*) override;
    void stopSampling(ErrorString*, OwnPtr<protocol::HeapProfiler::SamplingHeapProfile>*) override;

    void requestHeapStatsUpdate() override;

private:
    void startTrackingHeapObjectsInternal(bool trackAllocations);
    void stopTrackingHeapObjectsInternal();

    v8::Isolate* m_isolate;
    V8RuntimeAgentImpl* m_runtimeAgent;
    protocol::Frontend::HeapProfiler* m_frontend;
    protocol::DictionaryValue* m_state;
};

} // namespace blink

#endif // !defined(V8HeapProfilerAgentImpl_h)
