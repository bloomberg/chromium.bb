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
#include "core/events/EventTarget.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/frame/FrameHost.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorConsoleAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorProfilerAgent.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/page/Page.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "core/workers/WorkerGlobalScope.h"

namespace blink {

namespace {

PersistentHeapHashSet<WeakMember<InstrumentingAgents>>& instrumentingAgentsSet()
{
    DEFINE_STATIC_LOCAL(PersistentHeapHashSet<WeakMember<InstrumentingAgents>>, instrumentingAgentsSet, ());
    return instrumentingAgentsSet;
}

}

namespace InspectorInstrumentation {

AsyncTask::AsyncTask(ExecutionContext* context, void* task) : AsyncTask(context, task, true)
{
}

AsyncTask::AsyncTask(ExecutionContext* context, void* task, bool enabled)
    : m_instrumentingAgents(enabled ? instrumentingAgentsFor(context) : nullptr)
    , m_task(task)
{
    if (m_instrumentingAgents && m_instrumentingAgents->inspectorDebuggerAgent())
        m_instrumentingAgents->inspectorDebuggerAgent()->asyncTaskStarted(m_task);
}

AsyncTask::~AsyncTask()
{
    if (m_instrumentingAgents && m_instrumentingAgents->inspectorDebuggerAgent())
        m_instrumentingAgents->inspectorDebuggerAgent()->asyncTaskFinished(m_task);
}

NativeBreakpoint::NativeBreakpoint(ExecutionContext* context, const String& name, bool sync)
    : m_instrumentingAgents(instrumentingAgentsFor(context))
    , m_sync(sync)
{
    if (m_instrumentingAgents && m_instrumentingAgents->inspectorDOMDebuggerAgent())
        m_instrumentingAgents->inspectorDOMDebuggerAgent()->allowNativeBreakpoint(name, nullptr, m_sync);
}

NativeBreakpoint::NativeBreakpoint(ExecutionContext* context, EventTarget* eventTarget, Event* event)
    : m_instrumentingAgents(instrumentingAgentsFor(context))
    , m_sync(false)
{
    if (m_instrumentingAgents && m_instrumentingAgents->inspectorDOMDebuggerAgent()) {
        Node* node = eventTarget->toNode();
        String targetName = node ? node->nodeName() : eventTarget->interfaceName();
        m_instrumentingAgents->inspectorDOMDebuggerAgent()->allowNativeBreakpoint(event->type(), &targetName, m_sync);
    }
}

NativeBreakpoint::~NativeBreakpoint()
{
    if (!m_sync && m_instrumentingAgents && m_instrumentingAgents->inspectorDOMDebuggerAgent())
        m_instrumentingAgents->inspectorDOMDebuggerAgent()->cancelNativeBreakpoint();
}

int FrontendCounter::s_frontendCounter = 0;

// Keep in sync with kDevToolsRequestInitiator defined in devtools_network_controller.cc
const char kInspectorEmulateNetworkConditionsClientId[] = "X-DevTools-Emulate-Network-Conditions-Client-Id";
}

InspectorInstrumentationCookie::InspectorInstrumentationCookie()
    : m_instrumentingAgents(nullptr)
{
}

InspectorInstrumentationCookie::InspectorInstrumentationCookie(InstrumentingAgents* agents)
    : m_instrumentingAgents(agents)
{
}

InspectorInstrumentationCookie::InspectorInstrumentationCookie(const InspectorInstrumentationCookie& other)
    : m_instrumentingAgents(other.m_instrumentingAgents)
{
}

InspectorInstrumentationCookie& InspectorInstrumentationCookie::operator=(const InspectorInstrumentationCookie& other)
{
    if (this != &other)
        m_instrumentingAgents = other.m_instrumentingAgents;
    return *this;
}

InspectorInstrumentationCookie::~InspectorInstrumentationCookie()
{
}

namespace InspectorInstrumentation {

bool isDebuggerPaused(LocalFrame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsFor(frame)) {
        if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents->inspectorDebuggerAgent())
            return debuggerAgent->isPaused();
    }
    return false;
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
    for (InstrumentingAgents* instrumentingAgents: instrumentingAgentsSet()) {
        if (InspectorResourceAgent* inspectorResourceAgent = instrumentingAgents->inspectorResourceAgent())
            inspectorResourceAgent->removedResourceFromMemoryCache(cachedResource);
    }
}

bool collectingHTMLParseErrors(Document* document)
{
    ASSERT(isMainThread());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsFor(document))
        return instrumentingAgentsSet().contains(instrumentingAgents);
    return false;
}

bool consoleAgentEnabled(ExecutionContext* executionContext)
{
    InstrumentingAgents* instrumentingAgents = instrumentingAgentsFor(executionContext);
    InspectorConsoleAgent* consoleAgent = instrumentingAgents ? instrumentingAgents->inspectorConsoleAgent() : 0;
    return consoleAgent && consoleAgent->enabled();
}

void registerInstrumentingAgents(InstrumentingAgents* instrumentingAgents)
{
    ASSERT(isMainThread());
    instrumentingAgentsSet().add(instrumentingAgents);
}

void unregisterInstrumentingAgents(InstrumentingAgents* instrumentingAgents)
{
    ASSERT(isMainThread());
    ASSERT(instrumentingAgentsSet().contains(instrumentingAgents));
    instrumentingAgentsSet().remove(instrumentingAgents);
}

InstrumentingAgents* instrumentingAgentsFor(WorkerGlobalScope* workerGlobalScope)
{
    if (!workerGlobalScope)
        return nullptr;
    if (WorkerInspectorController* controller = workerGlobalScope->workerInspectorController())
        return controller->instrumentingAgents();
    return nullptr;
}

InstrumentingAgents* instrumentingAgentsForNonDocumentContext(ExecutionContext* context)
{
    if (context->isWorkerGlobalScope())
        return instrumentingAgentsFor(toWorkerGlobalScope(context));
    if (context->isWorkletGlobalScope())
        return instrumentingAgentsFor(toMainThreadWorkletGlobalScope(context)->frame());
    return nullptr;
}

} // namespace InspectorInstrumentation

} // namespace blink
