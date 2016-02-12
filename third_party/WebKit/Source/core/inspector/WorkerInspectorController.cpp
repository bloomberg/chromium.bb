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

#include "core/InspectorBackendDispatcher.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorConsoleAgent.h"
#include "core/inspector/InspectorFrontendChannel.h"
#include "core/inspector/InspectorHeapProfilerAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorProfilerAgent.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/WorkerConsoleAgent.h"
#include "core/inspector/WorkerDebuggerAgent.h"
#include "core/inspector/WorkerRuntimeAgent.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/inspector/v8/public/V8RuntimeAgent.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

namespace {

class RunInspectorCommandsTask final : public InspectorTaskRunner::Task {
public:
    explicit RunInspectorCommandsTask(WorkerThread* thread)
        : m_thread(thread) { }
    ~RunInspectorCommandsTask() override { }
    void run() override
    {
        // Process all queued debugger commands. WorkerThread is certainly
        // alive if this task is being executed.
        m_thread->willRunDebuggerTasks();
        while (WorkerThread::TaskReceived == m_thread->runDebuggerTask(WorkerThread::DontWaitForTask)) { }
        m_thread->didRunDebuggerTasks();
    }

private:
    WorkerThread* m_thread;
};

} // namespace

class WorkerInspectorController::PageInspectorProxy final : public NoBaseWillBeGarbageCollectedFinalized<WorkerInspectorController::PageInspectorProxy>, public InspectorFrontendChannel {
    USING_FAST_MALLOC_WILL_BE_REMOVED(PageInspectorProxy);
public:
    static PassOwnPtrWillBeRawPtr<PageInspectorProxy> create(WorkerGlobalScope* workerGlobalScope)
    {
        return adoptPtrWillBeNoop(new PageInspectorProxy(workerGlobalScope));
    }
    ~PageInspectorProxy() override { }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_workerGlobalScope);
    }

private:
    explicit PageInspectorProxy(WorkerGlobalScope* workerGlobalScope)
        : m_workerGlobalScope(workerGlobalScope)
    {
    }

    void sendProtocolResponse(int sessionId, int callId, PassRefPtr<JSONObject> message) override
    {
        // Worker messages are wrapped, no need to handle callId.
        m_workerGlobalScope->thread()->workerReportingProxy().postMessageToPageInspector(message->toJSONString());
    }
    void sendProtocolNotification(PassRefPtr<JSONObject> message) override
    {
        m_workerGlobalScope->thread()->workerReportingProxy().postMessageToPageInspector(message->toJSONString());
    }
    void flush() override { }

    RawPtrWillBeMember<WorkerGlobalScope> m_workerGlobalScope;
};

WorkerInspectorController::WorkerInspectorController(WorkerGlobalScope* workerGlobalScope)
    : m_workerGlobalScope(workerGlobalScope)
    , m_instrumentingAgents(InstrumentingAgents::create())
    , m_agents(m_instrumentingAgents.get())
    , m_inspectorTaskRunner(adoptPtr(new InspectorTaskRunner(v8::Isolate::GetCurrent())))
    , m_beforeInitlizedScope(adoptPtr(new InspectorTaskRunner::IgnoreInterruptsScope(m_inspectorTaskRunner.get())))
    , m_paused(false)
{
    v8::Isolate* isolate = workerGlobalScope->thread()->isolate();
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    ThreadDebugger* threadDebugger = data->threadDebugger();
    if (threadDebugger) {
        ASSERT(threadDebugger->isWorker());
        m_workerThreadDebugger = static_cast<WorkerThreadDebugger*>(threadDebugger);
    } else {
        OwnPtr<WorkerThreadDebugger> newDebugger = adoptPtr(new WorkerThreadDebugger(workerGlobalScope->thread()));
        m_workerThreadDebugger = newDebugger.get();
        data->setThreadDebugger(newDebugger.release());
    }

    V8Debugger* debugger = m_workerThreadDebugger->debugger();

    OwnPtrWillBeRawPtr<WorkerRuntimeAgent> workerRuntimeAgent = WorkerRuntimeAgent::create(debugger, workerGlobalScope, this);
    m_workerRuntimeAgent = workerRuntimeAgent.get();
    m_agents.append(workerRuntimeAgent.release());

    OwnPtrWillBeRawPtr<WorkerDebuggerAgent> workerDebuggerAgent = WorkerDebuggerAgent::create(debugger, workerGlobalScope, m_workerRuntimeAgent->v8Agent());
    m_workerDebuggerAgent = workerDebuggerAgent.get();
    m_agents.append(workerDebuggerAgent.release());

    m_agents.append(InspectorProfilerAgent::create(debugger, 0));
    m_agents.append(InspectorHeapProfilerAgent::create(isolate, m_workerRuntimeAgent->v8Agent()));

    OwnPtrWillBeRawPtr<WorkerConsoleAgent> workerConsoleAgent = WorkerConsoleAgent::create(m_workerRuntimeAgent->v8Agent(), workerGlobalScope);
    WorkerConsoleAgent* workerConsoleAgentPtr = workerConsoleAgent.get();
    workerConsoleAgentPtr->setDebuggerAgent(m_workerDebuggerAgent->v8Agent());
    m_agents.append(workerConsoleAgent.release());

    m_workerRuntimeAgent->v8Agent()->setClearConsoleCallback(bind<>(&InspectorConsoleAgent::clearAllMessages, workerConsoleAgentPtr));
}

WorkerInspectorController::~WorkerInspectorController()
{
}

void WorkerInspectorController::registerModuleAgent(PassOwnPtrWillBeRawPtr<InspectorAgent> agent)
{
    m_agents.append(agent);
}

void WorkerInspectorController::connectFrontend()
{
    ASSERT(!m_frontend);
    InspectorTaskRunner::IgnoreInterruptsScope scope(m_inspectorTaskRunner.get());
    m_pageInspectorProxy = PageInspectorProxy::create(m_workerGlobalScope);
    m_frontend = adoptPtr(new InspectorFrontend(frontendChannel()));
    m_backendDispatcher = InspectorBackendDispatcher::create(frontendChannel());
    m_agents.registerInDispatcher(m_backendDispatcher.get());
    m_agents.setFrontend(m_frontend.get());
    InspectorInstrumentation::frontendCreated();
}

void WorkerInspectorController::disconnectFrontend()
{
    if (!m_frontend)
        return;
    InspectorTaskRunner::IgnoreInterruptsScope scope(m_inspectorTaskRunner.get());
    m_backendDispatcher->clearFrontend();
    m_backendDispatcher.clear();
    m_agents.clearFrontend();
    m_frontend.clear();
    InspectorInstrumentation::frontendDeleted();
    m_pageInspectorProxy.clear();
}

void WorkerInspectorController::restoreInspectorStateFromCookie(const String& inspectorCookie)
{
    ASSERT(!m_frontend);
    connectFrontend();
    m_agents.restore(inspectorCookie);
}

void WorkerInspectorController::dispatchMessageFromFrontend(const String& message)
{
    InspectorTaskRunner::IgnoreInterruptsScope scope(m_inspectorTaskRunner.get());
    if (m_backendDispatcher) {
        // sessionId will be overwritten by WebDevToolsAgent::sendProtocolNotifications call.
        m_backendDispatcher->dispatch(0, message);
    }
}

void WorkerInspectorController::dispose()
{
    m_instrumentingAgents->reset();
    disconnectFrontend();
}

void WorkerInspectorController::interruptAndDispatchInspectorCommands()
{
    m_inspectorTaskRunner->interruptAndRun(adoptPtr(new RunInspectorCommandsTask(m_workerGlobalScope->thread())));
}

void WorkerInspectorController::resumeStartup()
{
    if (m_paused)
        m_workerThreadDebugger->quitMessageLoopOnPause();
}

bool WorkerInspectorController::isRunRequired()
{
    return m_paused;
}

InspectorFrontendChannel* WorkerInspectorController::frontendChannel() const
{
    return static_cast<InspectorFrontendChannel*>(m_pageInspectorProxy.get());
}

void WorkerInspectorController::workerContextInitialized(bool shouldPauseOnStart)
{
    m_beforeInitlizedScope.clear();
    if (shouldPauseOnStart)
        pauseOnStart();
}

void WorkerInspectorController::pauseOnStart()
{
    m_paused = true;
    m_workerThreadDebugger->runMessageLoopOnPause(WorkerThreadDebugger::contextGroupId());
    m_paused = false;
}

DEFINE_TRACE(WorkerInspectorController)
{
    visitor->trace(m_workerGlobalScope);
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_agents);
    visitor->trace(m_pageInspectorProxy);
    visitor->trace(m_workerDebuggerAgent);
    visitor->trace(m_workerRuntimeAgent);
}

} // namespace blink
