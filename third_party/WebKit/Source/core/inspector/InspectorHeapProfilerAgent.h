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

#ifndef InspectorHeapProfilerAgent_h
#define InspectorHeapProfilerAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class V8HeapProfilerAgent;

class CORE_EXPORT InspectorHeapProfilerAgent final : public InspectorBaseAgent<InspectorHeapProfilerAgent, protocol::Frontend::HeapProfiler>, public protocol::Backend::HeapProfiler {
    WTF_MAKE_NONCOPYABLE(InspectorHeapProfilerAgent);
public:
    explicit InspectorHeapProfilerAgent(V8HeapProfilerAgent*);
    ~InspectorHeapProfilerAgent() override;

    // InspectorBaseAgent overrides.
    void init(InstrumentingAgents*, protocol::Frontend*, protocol::Dispatcher*, protocol::DictionaryValue*) override;
    void dispose() override;
    void restore() override;

    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void startTrackingHeapObjects(ErrorString*, const Maybe<bool>& trackAllocations) override;
    void stopTrackingHeapObjects(ErrorString*, const Maybe<bool>& reportProgress) override;
    void takeHeapSnapshot(ErrorString*, const Maybe<bool>& reportProgress) override;
    void collectGarbage(ErrorString*) override;
    void getObjectByHeapObjectId(ErrorString*, const String16& objectId, const Maybe<String16>& objectGroup, OwnPtr<protocol::Runtime::RemoteObject>* result) override;
    void addInspectedHeapObject(ErrorString*, const String16& heapObjectId) override;
    void getHeapObjectId(ErrorString*, const String16& objectId, String16* heapSnapshotObjectId) override;
    void startSampling(ErrorString*, const Maybe<double>& samplingInterval) override;
    void stopSampling(ErrorString*, OwnPtr<protocol::HeapProfiler::SamplingHeapProfile>*) override;

private:
    V8HeapProfilerAgent* m_v8HeapProfilerAgent;
};

} // namespace blink

#endif // !defined(InspectorHeapProfilerAgent_h)
