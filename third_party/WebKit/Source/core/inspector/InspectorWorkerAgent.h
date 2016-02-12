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

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"

namespace blink {
class PageConsoleAgent;
class KURL;
class WorkerInspectorProxy;

using ErrorString = String;

class CORE_EXPORT InspectorWorkerAgent final : public InspectorBaseAgent<InspectorWorkerAgent, protocol::Frontend::Worker>, public protocol::Dispatcher::WorkerCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorWorkerAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorWorkerAgent> create(PageConsoleAgent*);
    ~InspectorWorkerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    void init() override;
    void disable(ErrorString*) override;
    void restore() override;

    // Called from InspectorInstrumentation
    bool shouldPauseDedicatedWorkerOnStart();
    void didStartWorker(WorkerInspectorProxy*, const KURL&);
    void workerTerminated(WorkerInspectorProxy*);

    // Called from Dispatcher
    void enable(ErrorString*) override;
    void connectToWorker(ErrorString*, const String& workerId) override;
    void disconnectFromWorker(ErrorString*, const String& workerId) override;
    void sendMessageToWorker(ErrorString*, const String& workerId, const String& message) override;
    void setAutoconnectToWorkers(ErrorString*, bool value) override;

    void setTracingSessionId(const String&);

    class WorkerAgentClient final : public WorkerInspectorProxy::PageInspector {
        USING_FAST_MALLOC_WILL_BE_REMOVED(InspectorWorkerAgent::WorkerAgentClient);
    public:
        static PassOwnPtrWillBeRawPtr<WorkerAgentClient> create(protocol::Frontend::Worker*, WorkerInspectorProxy*, const String& id, PageConsoleAgent*);
        WorkerAgentClient(protocol::Frontend::Worker*, WorkerInspectorProxy*, const String& id, PageConsoleAgent*);
        ~WorkerAgentClient() override;
        DECLARE_VIRTUAL_TRACE();

        String id() const { return m_id; }
        WorkerInspectorProxy* proxy() const { return m_proxy; }

        void connectToWorker();
        void dispose();

    private:
        // WorkerInspectorProxy::PageInspector implementation
        void dispatchMessageFromWorker(const String& message) override;
        void workerConsoleAgentEnabled(WorkerGlobalScopeProxy*) override;

        protocol::Frontend::Worker* m_frontend;
        RawPtrWillBeMember<WorkerInspectorProxy> m_proxy;
        String m_id;
        bool m_connected;
        RawPtrWillBeMember<PageConsoleAgent> m_consoleAgent;
    };

private:
    InspectorWorkerAgent(PageConsoleAgent*);
    void createWorkerAgentClientsForExistingWorkers();
    void createWorkerAgentClient(WorkerInspectorProxy*, const String& url, const String& id);
    void destroyWorkerAgentClients();

    class WorkerInfo {
    public:
        WorkerInfo() { }
        WorkerInfo(const String& url, const String& id) : url(url), id(id) { }
        String url;
        String id;
    };

    using WorkerClients = WillBeHeapHashMap<String, OwnPtrWillBeMember<WorkerAgentClient>>;
    WorkerClients m_idToClient;
    using WorkerInfos = WillBeHeapHashMap<RawPtrWillBeMember<WorkerInspectorProxy>, WorkerInfo>;
    WorkerInfos m_workerInfos;
    String m_tracingSessionId;
    RawPtrWillBeMember<PageConsoleAgent> m_consoleAgent;
};

} // namespace blink

#endif // !defined(InspectorWorkerAgent_h)
