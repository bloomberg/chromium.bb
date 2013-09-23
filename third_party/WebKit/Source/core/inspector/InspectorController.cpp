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

#include "config.h"
#include "core/inspector/InspectorController.h"

#include "InspectorBackendDispatcher.h"
#include "InspectorFrontend.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorAgent.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorCanvasAgent.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorDOMStorageAgent.h"
#include "core/inspector/InspectorDatabaseAgent.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorFileSystemAgent.h"
#include "core/inspector/InspectorFrontendClient.h"
#include "core/inspector/InspectorHeapProfilerAgent.h"
#include "core/inspector/InspectorIndexedDBAgent.h"
#include "core/inspector/InspectorInputAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorLayerTreeAgent.h"
#include "core/inspector/InspectorMemoryAgent.h"
#include "core/inspector/InspectorOverlay.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorProfilerAgent.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InspectorTimelineAgent.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/PageConsoleAgent.h"
#include "core/inspector/PageDebuggerAgent.h"
#include "core/inspector/PageRuntimeAgent.h"
#include "core/page/Page.h"
#include "core/platform/PlatformMouseEvent.h"

namespace WebCore {

InspectorController::InspectorController(Page* page, InspectorClient* inspectorClient)
    : m_instrumentingAgents(InstrumentingAgents::create())
    , m_injectedScriptManager(InjectedScriptManager::createForPage())
    , m_state(adoptPtr(new InspectorCompositeState(inspectorClient)))
    , m_overlay(InspectorOverlay::create(page, inspectorClient))
    , m_page(page)
    , m_inspectorClient(inspectorClient)
    , m_isUnderTest(false)
{
    m_agents.append(InspectorAgent::create(page, m_injectedScriptManager.get(), m_instrumentingAgents.get(), m_state.get()));

    OwnPtr<InspectorPageAgent> pageAgentPtr(InspectorPageAgent::create(m_instrumentingAgents.get(), page, m_state.get(), m_injectedScriptManager.get(), inspectorClient, m_overlay.get()));
    InspectorPageAgent* pageAgent = pageAgentPtr.get();
    m_agents.append(pageAgentPtr.release());

    OwnPtr<InspectorDOMAgent> domAgentPtr(InspectorDOMAgent::create(m_instrumentingAgents.get(), pageAgent, m_state.get(), m_injectedScriptManager.get(), m_overlay.get(), inspectorClient));
    InspectorDOMAgent* domAgent = domAgentPtr.get();
    m_agents.append(domAgentPtr.release());

    OwnPtr<InspectorResourceAgent> resourceAgentPtr(InspectorResourceAgent::create(m_instrumentingAgents.get(), pageAgent, inspectorClient, m_state.get(), m_overlay.get()));
    InspectorResourceAgent* resourceAgent = resourceAgentPtr.get();
    m_agents.append(resourceAgentPtr.release());

    m_agents.append(InspectorCSSAgent::create(m_instrumentingAgents.get(), m_state.get(), domAgent, pageAgent, resourceAgent));

    m_agents.append(InspectorDatabaseAgent::create(m_instrumentingAgents.get(), m_state.get()));

    m_agents.append(InspectorIndexedDBAgent::create(m_instrumentingAgents.get(), m_state.get(), m_injectedScriptManager.get(), pageAgent));

    m_agents.append(InspectorFileSystemAgent::create(m_instrumentingAgents.get(), pageAgent, m_state.get()));

    m_agents.append(InspectorDOMStorageAgent::create(m_instrumentingAgents.get(), pageAgent, m_state.get()));

    OwnPtr<InspectorMemoryAgent> memoryAgentPtr(InspectorMemoryAgent::create(m_instrumentingAgents.get(), m_state.get()));
    m_memoryAgent = memoryAgentPtr.get();
    m_agents.append(memoryAgentPtr.release());

    OwnPtr<InspectorTimelineAgent> timelineAgentPtr(InspectorTimelineAgent::create(m_instrumentingAgents.get(), pageAgent, m_memoryAgent, domAgent, m_state.get(),
        InspectorTimelineAgent::PageInspector, inspectorClient));
    m_timelineAgent = timelineAgentPtr.get();
    m_agents.append(timelineAgentPtr.release());

    m_agents.append(InspectorApplicationCacheAgent::create(m_instrumentingAgents.get(), m_state.get(), pageAgent));

    PageScriptDebugServer* pageScriptDebugServer = &PageScriptDebugServer::shared();

    m_agents.append(PageRuntimeAgent::create(m_instrumentingAgents.get(), m_state.get(), m_injectedScriptManager.get(), pageScriptDebugServer, page, pageAgent));

    OwnPtr<InspectorConsoleAgent> consoleAgentPtr(PageConsoleAgent::create(m_instrumentingAgents.get(), m_state.get(), m_injectedScriptManager.get(), domAgent, m_timelineAgent));
    InspectorConsoleAgent* consoleAgent = consoleAgentPtr.get();
    m_agents.append(consoleAgentPtr.release());

    OwnPtr<InspectorDebuggerAgent> debuggerAgentPtr(PageDebuggerAgent::create(m_instrumentingAgents.get(), m_state.get(), pageScriptDebugServer, pageAgent, m_injectedScriptManager.get(), m_overlay.get()));
    InspectorDebuggerAgent* debuggerAgent = debuggerAgentPtr.get();
    m_agents.append(debuggerAgentPtr.release());

    m_agents.append(InspectorDOMDebuggerAgent::create(m_instrumentingAgents.get(), m_state.get(), domAgent, debuggerAgent));

    m_agents.append(InspectorProfilerAgent::create(m_instrumentingAgents.get(), consoleAgent, m_state.get(), m_injectedScriptManager.get()));

    m_agents.append(InspectorHeapProfilerAgent::create(m_instrumentingAgents.get(), m_state.get(), m_injectedScriptManager.get()));


    m_agents.append(InspectorWorkerAgent::create(m_instrumentingAgents.get(), m_state.get()));

    m_agents.append(InspectorCanvasAgent::create(m_instrumentingAgents.get(), m_state.get(), pageAgent, m_injectedScriptManager.get()));

    m_agents.append(InspectorInputAgent::create(m_instrumentingAgents.get(), m_state.get(), page, inspectorClient));

    m_agents.append(InspectorLayerTreeAgent::create(m_instrumentingAgents.get(), m_state.get(), domAgent, page));

    ASSERT_ARG(inspectorClient, inspectorClient);
    m_injectedScriptManager->injectedScriptHost()->init(m_instrumentingAgents.get(), pageScriptDebugServer);
}

InspectorController::~InspectorController()
{
    m_instrumentingAgents->reset();
    m_agents.discardAgents();
    ASSERT(!m_inspectorClient);
}

PassOwnPtr<InspectorController> InspectorController::create(Page* page, InspectorClient* client)
{
    return adoptPtr(new InspectorController(page, client));
}

void InspectorController::inspectedPageDestroyed()
{
    disconnectFrontend();
    m_injectedScriptManager->disconnect();
    m_inspectorClient = 0;
    m_page = 0;
}

void InspectorController::setInspectorFrontendClient(PassOwnPtr<InspectorFrontendClient> inspectorFrontendClient)
{
    m_inspectorFrontendClient = inspectorFrontendClient;
}

void InspectorController::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    // If the page is supposed to serve as InspectorFrontend notify inspector frontend
    // client that it's cleared so that the client can expose inspector bindings.
    if (m_inspectorFrontendClient && frame == m_page->mainFrame())
        m_inspectorFrontendClient->windowObjectCleared();
}

void InspectorController::connectFrontend(InspectorFrontendChannel* frontendChannel)
{
    ASSERT(frontendChannel);

    m_inspectorFrontend = adoptPtr(new InspectorFrontend(frontendChannel));
    // We can reconnect to existing front-end -> unmute state.
    m_state->unmute();

    m_agents.setFrontend(m_inspectorFrontend.get());

    InspectorInstrumentation::registerInstrumentingAgents(m_instrumentingAgents.get());
    InspectorInstrumentation::frontendCreated();

    ASSERT(m_inspectorClient);
    m_inspectorBackendDispatcher = InspectorBackendDispatcher::create(frontendChannel);

    m_agents.registerInDispatcher(m_inspectorBackendDispatcher.get());
}

void InspectorController::disconnectFrontend()
{
    if (!m_inspectorFrontend)
        return;
    m_inspectorBackendDispatcher->clearFrontend();
    m_inspectorBackendDispatcher.clear();

    // Destroying agents would change the state, but we don't want that.
    // Pre-disconnect state will be used to restore inspector agents.
    m_state->mute();

    m_agents.clearFrontend();

    m_inspectorFrontend.clear();

    // relese overlay page resources
    m_overlay->freePage();
    InspectorInstrumentation::frontendDeleted();
    InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());
}

void InspectorController::reconnectFrontend()
{
    if (!m_inspectorFrontend)
        return;
    InspectorFrontendChannel* frontendChannel = m_inspectorFrontend->inspector()->getInspectorFrontendChannel();
    disconnectFrontend();
    connectFrontend(frontendChannel);
}

void InspectorController::reuseFrontend(InspectorFrontendChannel* frontendChannel, const String& inspectorStateCookie)
{
    ASSERT(!m_inspectorFrontend);
    connectFrontend(frontendChannel);
    m_state->loadFromCookie(inspectorStateCookie);
    m_agents.restore();
}

void InspectorController::setProcessId(long processId)
{
    IdentifiersFactory::setProcessId(processId);
}

void InspectorController::setLayerTreeId(int id)
{
    m_timelineAgent->setLayerTreeId(id);
}

void InspectorController::webViewResized(const IntSize& size)
{
    if (InspectorPageAgent* pageAgent = m_instrumentingAgents->inspectorPageAgent())
        pageAgent->webViewResized(size);
}

bool InspectorController::isUnderTest()
{
    return m_isUnderTest;
}

void InspectorController::evaluateForTestInFrontend(long callId, const String& script)
{
    m_isUnderTest = true;
    if (InspectorAgent* inspectorAgent = m_instrumentingAgents->inspectorAgent())
        inspectorAgent->evaluateForTestInFrontend(callId, script);
}

void InspectorController::drawHighlight(GraphicsContext& context) const
{
    m_overlay->paint(context);
}

void InspectorController::getHighlight(Highlight* highlight) const
{
    m_overlay->getHighlight(highlight);
}

void InspectorController::inspect(Node* node)
{
    if (!node)
        return;
    Document* document = node->ownerDocument();
    if (!document)
        return;
    Frame* frame = document->frame();
    if (!frame)
        return;

    if (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE)
        node = node->parentNode();

    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(mainWorldScriptState(frame));
    if (injectedScript.hasNoValue())
        return;
    injectedScript.inspectNode(node);
}

Page* InspectorController::inspectedPage() const
{
    return m_page;
}

void InspectorController::setInjectedScriptForOrigin(const String& origin, const String& source)
{
    if (InspectorAgent* inspectorAgent = m_instrumentingAgents->inspectorAgent())
        inspectorAgent->setInjectedScriptForOrigin(origin, source);
}

void InspectorController::dispatchMessageFromFrontend(const String& message)
{
    if (m_inspectorBackendDispatcher)
        m_inspectorBackendDispatcher->dispatch(message);
}

void InspectorController::hideHighlight()
{
    m_overlay->hideHighlight();
}

Node* InspectorController::highlightedNode() const
{
    return m_overlay->highlightedNode();
}

bool InspectorController::handleGestureEvent(Frame* frame, const PlatformGestureEvent& event)
{
    // Overlay should not consume events.
    m_overlay->handleGestureEvent(event);
    if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
        return domAgent->handleGestureEvent(frame, event);
    return false;
}

bool InspectorController::handleMouseEvent(Frame* frame, const PlatformMouseEvent& event)
{
    // Overlay should not consume events.
    m_overlay->handleMouseEvent(event);

    if (event.type() == PlatformEvent::MouseMoved) {
        if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
            domAgent->handleMouseMove(frame, event);
        return false;
    }
    if (event.type() == PlatformEvent::MousePressed) {
        if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
            return domAgent->handleMousePress();
    }
    return false;
}

bool InspectorController::handleTouchEvent(Frame* frame, const PlatformTouchEvent& event)
{
    // Overlay should not consume events.
    m_overlay->handleTouchEvent(event);
    if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
        return domAgent->handleTouchEvent(frame, event);
    return false;
}

void InspectorController::resume()
{
    if (InspectorDebuggerAgent* debuggerAgent = m_instrumentingAgents->inspectorDebuggerAgent()) {
        ErrorString error;
        debuggerAgent->resume(&error);
    }
}

void InspectorController::setResourcesDataSizeLimitsFromInternals(int maximumResourcesContentSize, int maximumSingleResourceContentSize)
{
    if (InspectorResourceAgent* resourceAgent = m_instrumentingAgents->inspectorResourceAgent())
        resourceAgent->setResourcesDataSizeLimitsFromInternals(maximumResourcesContentSize, maximumSingleResourceContentSize);
}

void InspectorController::willProcessTask()
{
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->willProcessTask();
    if (InspectorProfilerAgent* profilerAgent = m_instrumentingAgents->inspectorProfilerAgent())
        profilerAgent->willProcessTask();
}

void InspectorController::didProcessTask()
{
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didProcessTask();
    if (InspectorProfilerAgent* profilerAgent = m_instrumentingAgents->inspectorProfilerAgent())
        profilerAgent->didProcessTask();
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = m_instrumentingAgents->inspectorDOMDebuggerAgent())
        domDebuggerAgent->didProcessTask();
}

void InspectorController::didBeginFrame()
{
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didBeginFrame();
    if (InspectorCanvasAgent* canvasAgent = m_instrumentingAgents->inspectorCanvasAgent())
        canvasAgent->didBeginFrame();
}

void InspectorController::didCancelFrame()
{
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didCancelFrame();
}

void InspectorController::willComposite()
{
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->willComposite();
}

void InspectorController::didComposite()
{
    if (InspectorTimelineAgent* timelineAgent = m_instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didComposite();
}

} // namespace WebCore
