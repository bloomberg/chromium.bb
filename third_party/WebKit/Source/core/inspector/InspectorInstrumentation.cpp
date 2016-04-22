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

#include "core/inspector/InspectorInstrumentation.h"

#include "bindings/core/v8/ScriptCallStack.h"
#include "core/InstrumentingAgents.h"
#include "core/events/EventTarget.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/frame/FrameHost.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorConsoleAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorProfilerAgent.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InspectorSession.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/page/Page.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "core/workers/WorkerGlobalScope.h"

namespace blink {

namespace {

PersistentHeapHashSet<WeakMember<InstrumentingSessions>>& instrumentingSessionsSet()
{
    DEFINE_STATIC_LOCAL(PersistentHeapHashSet<WeakMember<InstrumentingSessions>>, instrumentingSessionsSet, ());
    return instrumentingSessionsSet;
}

}

namespace InspectorInstrumentation {

AsyncTask::AsyncTask(ExecutionContext* context, void* task) : AsyncTask(context, task, true)
{
}

AsyncTask::AsyncTask(ExecutionContext* context, void* task, bool enabled)
    : m_instrumentingSessions(enabled ? instrumentingSessionsFor(context) : nullptr)
    , m_task(task)
{
    if (!m_instrumentingSessions || m_instrumentingSessions->isEmpty())
        return;
    for (InspectorSession* session : *m_instrumentingSessions) {
        if (session->instrumentingAgents()->inspectorDebuggerAgent())
            session->instrumentingAgents()->inspectorDebuggerAgent()->asyncTaskStarted(m_task);
    }
}

AsyncTask::~AsyncTask()
{
    if (!m_instrumentingSessions || m_instrumentingSessions->isEmpty())
        return;
    for (InspectorSession* session : *m_instrumentingSessions) {
        if (session->instrumentingAgents()->inspectorDebuggerAgent())
            session->instrumentingAgents()->inspectorDebuggerAgent()->asyncTaskFinished(m_task);
    }
}

NativeBreakpoint::NativeBreakpoint(ExecutionContext* context, const String& name, bool sync)
    : m_instrumentingSessions(instrumentingSessionsFor(context))
    , m_sync(sync)
{
    if (!m_instrumentingSessions || m_instrumentingSessions->isEmpty())
        return;
    for (InspectorSession* session : *m_instrumentingSessions) {
        if (session->instrumentingAgents()->inspectorDOMDebuggerAgent())
            session->instrumentingAgents()->inspectorDOMDebuggerAgent()->allowNativeBreakpoint(name, nullptr, m_sync);
    }
}

NativeBreakpoint::NativeBreakpoint(ExecutionContext* context, EventTarget* eventTarget, Event* event)
    : m_instrumentingSessions(instrumentingSessionsFor(context))
    , m_sync(false)
{
    if (!m_instrumentingSessions || m_instrumentingSessions->isEmpty())
        return;
    Node* node = eventTarget->toNode();
    String targetName = node ? node->nodeName() : eventTarget->interfaceName();
    for (InspectorSession* session : *m_instrumentingSessions) {
        if (session->instrumentingAgents()->inspectorDOMDebuggerAgent())
            session->instrumentingAgents()->inspectorDOMDebuggerAgent()->allowNativeBreakpoint(event->type(), &targetName, m_sync);
    }
}

NativeBreakpoint::~NativeBreakpoint()
{
    if (m_sync || !m_instrumentingSessions || m_instrumentingSessions->isEmpty())
        return;
    for (InspectorSession* session : *m_instrumentingSessions) {
        if (session->instrumentingAgents()->inspectorDOMDebuggerAgent())
            session->instrumentingAgents()->inspectorDOMDebuggerAgent()->cancelNativeBreakpoint();
    }
}

int FrontendCounter::s_frontendCounter = 0;

// Keep in sync with kDevToolsRequestInitiator defined in devtools_network_controller.cc
const char kInspectorEmulateNetworkConditionsClientId[] = "X-DevTools-Emulate-Network-Conditions-Client-Id";
}

InspectorInstrumentationCookie::InspectorInstrumentationCookie()
    : m_instrumentingSessions(nullptr)
{
}

InspectorInstrumentationCookie::InspectorInstrumentationCookie(InstrumentingSessions* sessions)
    : m_instrumentingSessions(sessions)
{
}

InspectorInstrumentationCookie::InspectorInstrumentationCookie(const InspectorInstrumentationCookie& other)
    : m_instrumentingSessions(other.m_instrumentingSessions)
{
}

InspectorInstrumentationCookie& InspectorInstrumentationCookie::operator=(const InspectorInstrumentationCookie& other)
{
    if (this != &other)
        m_instrumentingSessions = other.m_instrumentingSessions;
    return *this;
}

InspectorInstrumentationCookie::~InspectorInstrumentationCookie()
{
}

namespace InspectorInstrumentation {

bool isDebuggerPaused(LocalFrame*)
{
    return MainThreadDebugger::instance()->debugger()->isPaused();
}

void didReceiveResourceResponseButCanceled(LocalFrame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponse(frame, identifier, loader, r, 0);
}

void continueAfterXFrameOptionsDenied(LocalFrame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceled(frame, loader, identifier, r);
}

void continueWithPolicyIgnore(LocalFrame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceled(frame, loader, identifier, r);
}

void removedResourceFromMemoryCache(Resource* cachedResource)
{
    ASSERT(isMainThread());
    for (InstrumentingSessions* instrumentingSessions: instrumentingSessionsSet()) {
        if (instrumentingSessions->isEmpty())
            continue;
        for (InspectorSession* session : *instrumentingSessions) {
            if (InspectorResourceAgent* inspectorResourceAgent = session->instrumentingAgents()->inspectorResourceAgent())
                inspectorResourceAgent->removedResourceFromMemoryCache(cachedResource);
        }
    }
}

bool collectingHTMLParseErrors(Document* document)
{
    ASSERT(isMainThread());
    if (InstrumentingSessions* instrumentingSessions = instrumentingSessionsFor(document))
        return instrumentingSessionsSet().contains(instrumentingSessions);
    return false;
}

bool consoleAgentEnabled(ExecutionContext* executionContext)
{
    InstrumentingSessions* instrumentingSessions = instrumentingSessionsFor(executionContext);
    if (!instrumentingSessions || instrumentingSessions->isEmpty())
        return false;
    for (InspectorSession* session : *instrumentingSessions) {
        if (InspectorConsoleAgent* consoleAgent = session->instrumentingAgents()->inspectorConsoleAgent()) {
            if (consoleAgent->enabled())
                return true;
        }
    }
    return false;
}

void registerInstrumentingSessions(InstrumentingSessions* instrumentingSessions)
{
    ASSERT(isMainThread());
    instrumentingSessionsSet().add(instrumentingSessions);
}

void unregisterInstrumentingSessions(InstrumentingSessions* instrumentingSessions)
{
    ASSERT(isMainThread());
    ASSERT(instrumentingSessionsSet().contains(instrumentingSessions));
    instrumentingSessionsSet().remove(instrumentingSessions);
}

InstrumentingSessions* instrumentingSessionsFor(WorkerGlobalScope* workerGlobalScope)
{
    if (!workerGlobalScope)
        return nullptr;
    if (WorkerInspectorController* controller = workerGlobalScope->workerInspectorController())
        return controller->instrumentingSessions();
    return nullptr;
}

InstrumentingSessions* instrumentingSessionsForNonDocumentContext(ExecutionContext* context)
{
    if (context->isWorkerGlobalScope())
        return instrumentingSessionsFor(toWorkerGlobalScope(context));
    if (context->isWorkletGlobalScope())
        return instrumentingSessionsFor(toMainThreadWorkletGlobalScope(context)->frame());
    return nullptr;
}

} // namespace InspectorInstrumentation

} // namespace blink
