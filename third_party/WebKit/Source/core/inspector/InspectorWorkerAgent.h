/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorWorkerAgent_h
#define InspectorWorkerAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"

namespace blink {
class InstrumentingAgents;
class JSONObject;
class KURL;
class WorkerInspectorProxy;

typedef String ErrorString;

class InspectorWorkerAgent final : public InspectorBaseAgent<InspectorWorkerAgent>, public InspectorBackendDispatcher::WorkerCommandHandler {
public:
    static PassOwnPtrWillBeRawPtr<InspectorWorkerAgent> create();
    virtual ~InspectorWorkerAgent();

    virtual void init() override;
    virtual void setFrontend(InspectorFrontend*) override;
    virtual void restore() override;
    virtual void clearFrontend() override;

    // Called from InspectorInstrumentation
    bool shouldPauseDedicatedWorkerOnStart();
    void didStartWorker(WorkerInspectorProxy*, const KURL&);
    void workerTerminated(WorkerInspectorProxy*);

    // Called from InspectorBackendDispatcher
    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    virtual void canInspectWorkers(ErrorString*, bool*) override;
    virtual void connectToWorker(ErrorString*, int workerId) override;
    virtual void disconnectFromWorker(ErrorString*, int workerId) override;
    virtual void sendMessageToWorker(ErrorString*, int workerId, const RefPtr<JSONObject>& message) override;
    virtual void setAutoconnectToWorkers(ErrorString*, bool value) override;

    void setTracingSessionId(const String&);

private:
    InspectorWorkerAgent();
    void createWorkerFrontendChannelsForExistingWorkers();
    void createWorkerFrontendChannel(WorkerInspectorProxy*, const String& url);
    void destroyWorkerFrontendChannels();

    InspectorFrontend::Worker* m_frontend;

    class WorkerFrontendChannel;
    typedef HashMap<int, WorkerFrontendChannel*> WorkerChannels;
    WorkerChannels m_idToChannel;
    typedef HashMap<WorkerInspectorProxy*, String> WorkerIds;
    WorkerIds m_workerIds;
    String m_tracingSessionId;
};

} // namespace blink

#endif // !defined(InspectorWorkerAgent_h)
