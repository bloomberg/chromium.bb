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

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "core/InspectorBackendDispatcher.h"
#include "core/InspectorFrontend.h"
#include "core/frame/FrameHost.h"
#include "core/inspector/AsyncCallTracker.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorAnimationAgent.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorCanvasAgent.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorHeapProfilerAgent.h"
#include "core/inspector/InspectorInputAgent.h"
#include "core/inspector/InspectorInspectorAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorLayerTreeAgent.h"
#include "core/inspector/InspectorMemoryAgent.h"
#include "core/inspector/InspectorOverlay.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorProfilerAgent.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InspectorTimelineAgent.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/PageConsoleAgent.h"
#include "core/inspector/PageDebuggerAgent.h"
#include "core/inspector/PageRuntimeAgent.h"
#include "core/layout/Layer.h"
#include "core/page/Page.h"
#include "platform/PlatformMouseEvent.h"

namespace blink {

InspectorController::InspectorController(Page* page, InspectorClient* inspectorClient)
    : m_instrumentingAgents(InstrumentingAgents::create())
    , m_injectedScriptManager(InjectedScriptManager::createForPage())
    , m_state(adoptPtrWillBeNoop(new InspectorCompositeState(inspectorClient)))
    , m_overlay(InspectorOverlay::create(page, inspectorClient))
    , m_cssAgent(nullptr)
    , m_resourceAgent(nullptr)
    , m_layerTreeAgent(nullptr)
    , m_animationAgent(nullptr)
    , m_inspectorClient(inspectorClient)
    , m_agents(m_instrumentingAgents.get(), m_state.get())
    , m_isUnderTest(false)
    , m_deferredAgentsInitialized(false)
{
    InjectedScriptManager* injectedScriptManager = m_injectedScriptManager.get();
    InspectorOverlay* overlay = m_overlay.get();

    m_agents.append(InspectorInspectorAgent::create(this, injectedScriptManager));

    OwnPtrWillBeRawPtr<InspectorPageAgent> pageAgentPtr(InspectorPageAgent::create(page, injectedScriptManager, inspectorClient, overlay));
    m_pageAgent = pageAgentPtr.get();
    m_agents.append(pageAgentPtr.release());

    OwnPtrWillBeRawPtr<InspectorDOMAgent> domAgentPtr(InspectorDOMAgent::create(m_pageAgent, injectedScriptManager, overlay));
    m_domAgent = domAgentPtr.get();
    m_agents.append(domAgentPtr.release());

    OwnPtrWillBeRawPtr<InspectorLayerTreeAgent> layerTreeAgentPtr(InspectorLayerTreeAgent::create(m_pageAgent));
    m_layerTreeAgent = layerTreeAgentPtr.get();
    m_agents.append(layerTreeAgentPtr.release());

    m_agents.append(InspectorTimelineAgent::create());

    PageScriptDebugServer* pageScriptDebugServer = &PageScriptDebugServer::shared();

    m_agents.append(PageRuntimeAgent::create(injectedScriptManager, inspectorClient, pageScriptDebugServer, m_pageAgent));

    OwnPtrWillBeRawPtr<PageConsoleAgent> pageConsoleAgentPtr = PageConsoleAgent::create(injectedScriptManager, m_domAgent, m_pageAgent);
    OwnPtrWillBeRawPtr<InspectorWorkerAgent> workerAgentPtr = InspectorWorkerAgent::create(pageConsoleAgentPtr.get());

    OwnPtrWillBeRawPtr<InspectorTracingAgent> tracingAgentPtr = InspectorTracingAgent::create(inspectorClient, workerAgentPtr.get(), m_pageAgent);
    m_tracingAgent = tracingAgentPtr.get();
    m_agents.append(tracingAgentPtr.release());

    m_agents.append(workerAgentPtr.release());
    m_agents.append(pageConsoleAgentPtr.release());

    ASSERT_ARG(inspectorClient, inspectorClient);
    m_injectedScriptManager->injectedScriptHost()->init(m_instrumentingAgents.get(), pageScriptDebugServer);
}

InspectorController::~InspectorController()
{
}

DEFINE_TRACE(InspectorController)
{
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_injectedScriptManager);
    visitor->trace(m_state);
    visitor->trace(m_overlay);
    visitor->trace(m_asyncCallTracker);
    visitor->trace(m_domAgent);
    visitor->trace(m_animationAgent);
    visitor->trace(m_pageAgent);
    visitor->trace(m_cssAgent);
    visitor->trace(m_resourceAgent);
    visitor->trace(m_layerTreeAgent);
    visitor->trace(m_tracingAgent);
    visitor->trace(m_inspectorBackendDispatcher);
    visitor->trace(m_agents);
}

PassOwnPtrWillBeRawPtr<InspectorController> InspectorController::create(Page* page, InspectorClient* client)
{
    return adoptPtrWillBeNoop(new InspectorController(page, client));
}

void InspectorController::setTextAutosizingEnabled(bool enabled)
{
    m_pageAgent->setTextAutosizingEnabled(enabled);
}

void InspectorController::setDeviceScaleAdjustment(float deviceScaleAdjustment)
{
    m_pageAgent->setDeviceScaleAdjustment(deviceScaleAdjustment);
}

void InspectorController::setPreferCompositingToLCDTextEnabled(bool enabled)
{
    m_pageAgent->setPreferCompositingToLCDTextEnabled(enabled);
}

void InspectorController::initializeDeferredAgents()
{
    if (m_deferredAgentsInitialized)
        return;
    m_deferredAgentsInitialized = true;

    InjectedScriptManager* injectedScriptManager = m_injectedScriptManager.get();
    InspectorOverlay* overlay = m_overlay.get();

    OwnPtrWillBeRawPtr<InspectorResourceAgent> resourceAgentPtr(InspectorResourceAgent::create(m_pageAgent));
    m_resourceAgent = resourceAgentPtr.get();
    m_agents.append(resourceAgentPtr.release());

    OwnPtrWillBeRawPtr<InspectorCSSAgent> cssAgentPtr(InspectorCSSAgent::create(m_domAgent, m_pageAgent, m_resourceAgent));
    m_cssAgent = cssAgentPtr.get();
    m_agents.append(cssAgentPtr.release());

    OwnPtrWillBeRawPtr<InspectorAnimationAgent> animationAgentPtr(InspectorAnimationAgent::create(m_pageAgent, m_domAgent));
    m_animationAgent = animationAgentPtr.get();
    m_agents.append(animationAgentPtr.release());

    m_agents.append(InspectorMemoryAgent::create());

    m_agents.append(InspectorApplicationCacheAgent::create(m_pageAgent));

    PageScriptDebugServer* pageScriptDebugServer = &PageScriptDebugServer::shared();

    OwnPtrWillBeRawPtr<InspectorDebuggerAgent> debuggerAgentPtr(PageDebuggerAgent::create(pageScriptDebugServer, m_pageAgent, injectedScriptManager, overlay));
    InspectorDebuggerAgent* debuggerAgent = debuggerAgentPtr.get();
    m_agents.append(debuggerAgentPtr.release());
    m_asyncCallTracker = adoptPtrWillBeNoop(new AsyncCallTracker(debuggerAgent, m_instrumentingAgents.get()));

    m_agents.append(InspectorDOMDebuggerAgent::create(m_domAgent, debuggerAgent));

    m_agents.append(InspectorProfilerAgent::create(injectedScriptManager, overlay));

    m_agents.append(InspectorHeapProfilerAgent::create(injectedScriptManager));

    m_agents.append(InspectorCanvasAgent::create(m_pageAgent, injectedScriptManager));

    m_agents.append(InspectorInputAgent::create(m_pageAgent, m_inspectorClient));

    m_pageAgent->setDeferredAgents(debuggerAgent, m_cssAgent);
}

void InspectorController::willBeDestroyed()
{
#if ENABLE(ASSERT)
    Frame* frame = m_pageAgent->frameHost()->page().mainFrame();
    ASSERT(frame);
    if (frame->isLocalFrame())
        ASSERT(m_pageAgent->inspectedFrame()->view());
#endif

    disconnectFrontend();
    m_injectedScriptManager->disconnect();
    m_inspectorClient = 0;
    m_instrumentingAgents->reset();
    m_agents.discardAgents();
}

void InspectorController::registerModuleAgent(PassOwnPtrWillBeRawPtr<InspectorAgent> agent)
{
    m_agents.append(agent);
}

void InspectorController::connectFrontend(const String& hostId, InspectorFrontendChannel* frontendChannel)
{
    ASSERT(frontendChannel);
    m_hostId = hostId;

    initializeDeferredAgents();
    m_resourceAgent->setHostId(hostId);

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

    // Release overlay page resources
    m_overlay->freePage();
    InspectorInstrumentation::frontendDeleted();
    InspectorInstrumentation::unregisterInstrumentingAgents(m_instrumentingAgents.get());
    m_hostId = "";
}

void InspectorController::reconnectFrontend()
{
    if (!m_inspectorFrontend)
        return;
    InspectorFrontendChannel* frontendChannel = m_inspectorFrontend->channel();
    String hostId = m_hostId;
    disconnectFrontend();
    connectFrontend(hostId, frontendChannel);
}

void InspectorController::reuseFrontend(const String& hostId, InspectorFrontendChannel* frontendChannel, const String& inspectorStateCookie)
{
    ASSERT(!m_inspectorFrontend);
    connectFrontend(hostId, frontendChannel);
    m_state->loadFromCookie(inspectorStateCookie);
    m_agents.restore();
}

void InspectorController::setProcessId(long processId)
{
    IdentifiersFactory::setProcessId(processId);
}

void InspectorController::setLayerTreeId(int id)
{
    m_tracingAgent->setLayerTreeId(id);
}

bool InspectorController::isUnderTest()
{
    return m_isUnderTest;
}

void InspectorController::evaluateForTestInFrontend(long callId, const String& script)
{
    m_isUnderTest = true;
    if (InspectorInspectorAgent* inspectorAgent = m_instrumentingAgents->inspectorInspectorAgent())
        inspectorAgent->evaluateForTestInFrontend(callId, script);
}

void InspectorController::drawHighlight(GraphicsContext& context) const
{
    m_overlay->paint(context);
}

void InspectorController::inspect(Node* node)
{
    m_domAgent->inspect(node);
}

void InspectorController::dispatchMessageFromFrontend(const String& message)
{
    if (m_inspectorBackendDispatcher)
        m_inspectorBackendDispatcher->dispatch(message);
}

bool InspectorController::handleGestureEvent(LocalFrame* frame, const PlatformGestureEvent& event)
{
    // Overlay should not consume events.
    m_overlay->handleGestureEvent(event);
    if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
        return domAgent->handleGestureEvent(frame, event);
    return false;
}

bool InspectorController::handleMouseEvent(LocalFrame* frame, const PlatformMouseEvent& event)
{
    // Overlay should not consume events.
    m_overlay->handleMouseEvent(event);

    if (event.type() == PlatformEvent::MouseMoved) {
        if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
            return domAgent->handleMouseMove(frame, event);
        return false;
    }
    if (event.type() == PlatformEvent::MousePressed) {
        if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
            return domAgent->handleMousePress();
    }
    return false;
}

bool InspectorController::handleTouchEvent(LocalFrame* frame, const PlatformTouchEvent& event)
{
    // Overlay should not consume events.
    m_overlay->handleTouchEvent(event);
    if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
        return domAgent->handleTouchEvent(frame, event);
    return false;
}

bool InspectorController::handleKeyboardEvent(LocalFrame* frame, const PlatformKeyboardEvent& event)
{
    // Overlay should not consume events.
    m_overlay->handleKeyboardEvent(event);
    return false;
}

void InspectorController::pageScaleFactorChanged()
{
    m_pageAgent->pageScaleFactorChanged();
}

bool InspectorController::deviceEmulationEnabled()
{
    return m_pageAgent->deviceMetricsOverrideEnabled();
}

bool InspectorController::screencastEnabled()
{
    return m_pageAgent->screencastEnabled();
}

void InspectorController::willProcessTask()
{
    if (InspectorProfilerAgent* profilerAgent = m_instrumentingAgents->inspectorProfilerAgent())
        profilerAgent->willProcessTask();
}

void InspectorController::didProcessTask()
{
    if (InspectorProfilerAgent* profilerAgent = m_instrumentingAgents->inspectorProfilerAgent())
        profilerAgent->didProcessTask();
    if (InspectorCanvasAgent* canvasAgent = m_instrumentingAgents->inspectorCanvasAgent())
        canvasAgent->didProcessTask();
}

void InspectorController::flushPendingProtocolNotifications()
{
    m_agents.flushPendingProtocolNotifications();
}

void InspectorController::didCommitLoadForMainFrame()
{
    m_agents.didCommitLoadForMainFrame();
}

void InspectorController::scriptsEnabled(bool  enabled)
{
    if (InspectorPageAgent* pageAgent = m_instrumentingAgents->inspectorPageAgent())
        pageAgent->scriptsEnabled(enabled);
}

void InspectorController::willAddPageOverlay(const GraphicsLayer* layer)
{
    if (m_layerTreeAgent)
        m_layerTreeAgent->willAddPageOverlay(layer);
}

void InspectorController::didRemovePageOverlay(const GraphicsLayer* layer)
{
    if (m_layerTreeAgent)
        m_layerTreeAgent->didRemovePageOverlay(layer);
}

} // namespace blink
