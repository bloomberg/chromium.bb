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

#include "core/inspector/PageConsoleAgent.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/workers/WorkerInspectorProxy.h"

namespace blink {

int PageConsoleAgent::s_enabledAgentCount = 0;

PageConsoleAgent::PageConsoleAgent(V8RuntimeAgent* runtimeAgent, InspectorDOMAgent* domAgent, InspectedFrames* inspectedFrames)
    : InspectorConsoleAgent(runtimeAgent)
    , m_inspectorDOMAgent(domAgent)
    , m_inspectedFrames(inspectedFrames)
{
}

PageConsoleAgent::~PageConsoleAgent()
{
}

DEFINE_TRACE(PageConsoleAgent)
{
    visitor->trace(m_inspectorDOMAgent);
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_workersWithEnabledConsole);
    InspectorConsoleAgent::trace(visitor);
}

void PageConsoleAgent::enable(ErrorString* errorString)
{
    InspectorConsoleAgent::enable(errorString);
    m_workersWithEnabledConsole.clear();
    m_instrumentingAgents->setPageConsoleAgent(this);
}

void PageConsoleAgent::disable(ErrorString* errorString)
{
    m_instrumentingAgents->setPageConsoleAgent(nullptr);
    InspectorConsoleAgent::disable(errorString);
    m_workersWithEnabledConsole.clear();
}

void PageConsoleAgent::clearMessages(ErrorString* errorString)
{
    m_inspectorDOMAgent->releaseDanglingNodes();
    messageStorage()->clear(m_inspectedFrames->root()->document());
}

void PageConsoleAgent::workerConsoleAgentEnabled(WorkerInspectorProxy* workerInspectorProxy)
{
    m_workersWithEnabledConsole.add(workerInspectorProxy);
}

ConsoleMessageStorage* PageConsoleAgent::messageStorage()
{
    return &m_inspectedFrames->root()->host()->consoleMessageStorage();
}

void PageConsoleAgent::workerTerminated(WorkerInspectorProxy* workerInspectorProxy)
{
    WorkerInspectorProxySet::iterator it = m_workersWithEnabledConsole.find(workerInspectorProxy);
    if (it != m_workersWithEnabledConsole.end()) {
        m_workersWithEnabledConsole.remove(it);
        return;
    }

    ConsoleMessageStorage* storage = messageStorage();
    size_t messageCount = storage->size();
    for (size_t i = 0; i < messageCount; ++i) {
        ConsoleMessage* message = storage->at(i);
        if (message->workerInspectorProxy() == workerInspectorProxy) {
            message->setWorkerInspectorProxy(nullptr);
            sendConsoleMessageToFrontend(message, false);
        }
    }
}

void PageConsoleAgent::enableStackCapturingIfNeeded()
{
    if (!s_enabledAgentCount)
        ScriptController::setCaptureCallStackForUncaughtExceptions(toIsolate(m_inspectedFrames->root()), true);
    ++s_enabledAgentCount;
}

void PageConsoleAgent::disableStackCapturingIfNeeded()
{
    if (!(--s_enabledAgentCount))
        ScriptController::setCaptureCallStackForUncaughtExceptions(toIsolate(m_inspectedFrames->root()), false);
}

} // namespace blink
