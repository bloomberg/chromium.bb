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

#include "core/inspector/WorkerInspectorController.h"

#include "core/inspector/InspectorConsoleAgent.h"
#include "core/inspector/InspectorHeapProfilerAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorProfilerAgent.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/WorkerConsoleAgent.h"
#include "core/inspector/WorkerDebuggerAgent.h"
#include "core/inspector/WorkerRuntimeAgent.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/inspector_protocol/Frontend.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"
#include "platform/v8_inspector/public/V8RuntimeAgent.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

WorkerInspectorController* WorkerInspectorController::create(WorkerGlobalScope* workerGlobalScope)
{
    WorkerThreadDebugger* debugger = WorkerThreadDebugger::from(workerGlobalScope->thread()->isolate());
    return debugger ? new WorkerInspectorController(workerGlobalScope, debugger) : nullptr;
}

WorkerInspectorController::WorkerInspectorController(WorkerGlobalScope* workerGlobalScope, WorkerThreadDebugger* debugger)
    : m_debugger(debugger)
    , m_workerGlobalScope(workerGlobalScope)
    , m_instrumentingAgents(InstrumentingAgents::create())
    , m_agents(m_instrumentingAgents.get())
{
}

WorkerInspectorController::~WorkerInspectorController()
{
}

void WorkerInspectorController::initializeAgents()
{
    m_v8Session = m_debugger->debugger()->connect(m_debugger->contextGroupId());
    m_agents.append(WorkerRuntimeAgent::create(m_v8Session->runtimeAgent(), m_workerGlobalScope, this));
    m_agents.append(WorkerDebuggerAgent::create(m_v8Session->debuggerAgent(), m_workerGlobalScope));
    m_agents.append(InspectorProfilerAgent::create(m_v8Session->profilerAgent(), nullptr));
    m_agents.append(InspectorHeapProfilerAgent::create(m_workerGlobalScope->thread()->isolate(), m_v8Session->heapProfilerAgent()));

    WorkerConsoleAgent* workerConsoleAgent = WorkerConsoleAgent::create(m_v8Session->runtimeAgent(), m_v8Session->debuggerAgent(), m_workerGlobalScope);
    m_agents.append(workerConsoleAgent);
    m_v8Session->runtimeAgent()->setClearConsoleCallback(bind<>(&InspectorConsoleAgent::clearAllMessages, workerConsoleAgent));
}

void WorkerInspectorController::destroyAgents()
{
    m_agents.discardAgents();
    m_instrumentingAgents->reset();
    m_v8Session.clear();
}

void WorkerInspectorController::connectFrontend()
{
    initializeAgents();
    ASSERT(!m_frontend);
    m_frontend = adoptPtr(new protocol::Frontend(this));
    m_backendDispatcher = protocol::Dispatcher::create(this);
    m_agents.registerInDispatcher(m_backendDispatcher.get());
    m_agents.setFrontend(m_frontend.get());
    InspectorInstrumentation::frontendCreated();
}

void WorkerInspectorController::disconnectFrontend()
{
    if (!m_frontend)
        return;
    m_backendDispatcher->clearFrontend();
    m_backendDispatcher.clear();
    m_agents.clearFrontend();
    m_frontend.clear();
    destroyAgents();
    InspectorInstrumentation::frontendDeleted();
}

void WorkerInspectorController::dispatchMessageFromFrontend(const String& message)
{
    if (m_backendDispatcher) {
        // sessionId will be overwritten by WebDevToolsAgent::sendProtocolNotifications call.
        m_backendDispatcher->dispatch(0, message);
    }
}

void WorkerInspectorController::dispose()
{
    disconnectFrontend();
}

void WorkerInspectorController::resumeStartup()
{
    m_workerGlobalScope->thread()->stopRunningDebuggerTasksOnPause();
}

void WorkerInspectorController::sendProtocolResponse(int sessionId, int callId, PassOwnPtr<protocol::DictionaryValue> message)
{
    // Worker messages are wrapped, no need to handle callId.
    m_workerGlobalScope->thread()->workerReportingProxy().postMessageToPageInspector(message->toJSONString());
}

void WorkerInspectorController::sendProtocolNotification(PassOwnPtr<protocol::DictionaryValue> message)
{
    m_workerGlobalScope->thread()->workerReportingProxy().postMessageToPageInspector(message->toJSONString());
}

void WorkerInspectorController::flush()
{
}

DEFINE_TRACE(WorkerInspectorController)
{
    visitor->trace(m_workerGlobalScope);
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_agents);
}

} // namespace blink
