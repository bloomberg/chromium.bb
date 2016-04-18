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

#ifndef WorkerInspectorController_h
#define WorkerInspectorController_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorRuntimeAgent.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "platform/inspector_protocol/FrontendChannel.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class InstrumentingAgents;
class V8Debugger;
class V8InspectorSession;
class WorkerDebuggerAgent;
class WorkerGlobalScope;
class WorkerRuntimeAgent;

namespace protocol {
class Dispatcher;
class Frontend;
class FrontendChannel;
}

class WorkerInspectorController final : public GarbageCollectedFinalized<WorkerInspectorController>, public InspectorRuntimeAgent::Client, public protocol::FrontendChannel {
    WTF_MAKE_NONCOPYABLE(WorkerInspectorController);
public:
    static WorkerInspectorController* create(WorkerGlobalScope*);
    ~WorkerInspectorController();
    DECLARE_TRACE();

    InstrumentingAgents* instrumentingAgents() const { return m_instrumentingAgents.get(); }

    void connectFrontend();
    void disconnectFrontend();
    void dispatchMessageFromFrontend(const String&);
    void dispose();

private:
    WorkerInspectorController(WorkerGlobalScope*, PassOwnPtr<V8InspectorSession>);

    // InspectorRuntimeAgent::Client implementation.
    void resumeStartup() override;

    // protocol::FrontendChannel implementation.
    void sendProtocolResponse(int sessionId, int callId, PassOwnPtr<protocol::DictionaryValue> message) override;
    void sendProtocolNotification(PassOwnPtr<protocol::DictionaryValue> message) override;
    void flush() override;

    Member<WorkerGlobalScope> m_workerGlobalScope;
    Member<InstrumentingAgents> m_instrumentingAgents;
    InspectorAgentRegistry m_agents;
    OwnPtr<V8InspectorSession> m_v8Session;
    OwnPtr<protocol::Frontend> m_frontend;
    OwnPtr<protocol::Dispatcher> m_backendDispatcher;
    Member<WorkerDebuggerAgent> m_workerDebuggerAgent;
    Member<WorkerRuntimeAgent> m_workerRuntimeAgent;
};

} // namespace blink

#endif // WorkerInspectorController_h
