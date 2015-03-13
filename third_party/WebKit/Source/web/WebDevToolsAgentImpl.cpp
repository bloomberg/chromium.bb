/*
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
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
#include "web/WebDevToolsAgentImpl.h"

#include "bindings/core/v8/PageScriptDebugServer.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/InspectorBackendDispatcher.h"
#include "core/InspectorFrontend.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/AsyncCallTracker.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorAnimationAgent.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorCanvasAgent.h"
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
#include "core/layout/LayoutView.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "modules/accessibility/InspectorAccessibilityAgent.h"
#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"
#include "modules/filesystem/InspectorFileSystemAgent.h"
#include "modules/indexeddb/InspectorIndexedDBAgent.h"
#include "modules/storage/InspectorDOMStorageAgent.h"
#include "modules/webdatabase/InspectorDatabaseAgent.h"
#include "platform/JSONValues.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "public/web/WebDeviceEmulationParams.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "web/DevToolsEmulator.h"
#include "web/WebGraphicsContextImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/MathExtras.h"
#include "wtf/Noncopyable.h"
#include "wtf/ProcessID.h"
#include "wtf/text/WTFString.h"

namespace OverlayZOrders {
// Use 99 as a big z-order number so that highlight is above other overlays.
static const int highlight = 99;
}

namespace blink {

class ClientMessageLoopAdapter : public PageScriptDebugServer::ClientMessageLoop {
public:
    ~ClientMessageLoopAdapter() override
    {
        s_instance = nullptr;
    }

    static void ensurePageScriptDebugServerCreated(WebDevToolsAgentClient* client)
    {
        if (s_instance)
            return;
        OwnPtr<ClientMessageLoopAdapter> instance = adoptPtr(new ClientMessageLoopAdapter(adoptPtr(client->createClientMessageLoop())));
        s_instance = instance.get();
        v8::Isolate* isolate = V8PerIsolateData::mainThreadIsolate();
        V8PerIsolateData* data = V8PerIsolateData::from(isolate);
        data->setScriptDebugServer(adoptPtrWillBeNoop(new PageScriptDebugServer(instance.release(), isolate)));
    }

    static void inspectedViewClosed(WebViewImpl* view)
    {
        if (s_instance)
            s_instance->m_frozenViews.remove(view);
    }

    static void didNavigate()
    {
        // Release render thread if necessary.
        if (s_instance && s_instance->m_running)
            PageScriptDebugServer::instance()->continueProgram();
    }

private:
    ClientMessageLoopAdapter(PassOwnPtr<WebDevToolsAgentClient::WebKitClientMessageLoop> messageLoop)
        : m_running(false)
        , m_messageLoop(messageLoop) { }

    void run(LocalFrame* frame) override
    {
        if (m_running)
            return;
        m_running = true;

        // 0. Flush pending frontend messages.
        WebViewImpl* viewImpl = WebViewImpl::fromPage(frame->page());
        WebDevToolsAgentImpl* agent = static_cast<WebDevToolsAgentImpl*>(viewImpl->devToolsAgent());
        agent->flushPendingProtocolNotifications();

        Vector<WebViewImpl*> views;

        // 1. Disable input events.
        const HashSet<Page*>& pages = Page::ordinaryPages();
        HashSet<Page*>::const_iterator end = pages.end();
        for (HashSet<Page*>::const_iterator it =  pages.begin(); it != end; ++it) {
            WebViewImpl* view = WebViewImpl::fromPage(*it);
            if (!view)
                continue;
            m_frozenViews.add(view);
            views.append(view);
            view->setIgnoreInputEvents(true);
        }
        // Notify embedder about pausing.
        agent->client()->willEnterDebugLoop();

        // 2. Disable active objects
        WebView::willEnterModalLoop();

        // 3. Process messages until quitNow is called.
        m_messageLoop->run();

        // 4. Resume active objects
        WebView::didExitModalLoop();

        // 5. Resume input events.
        for (Vector<WebViewImpl*>::iterator it = views.begin(); it != views.end(); ++it) {
            if (m_frozenViews.contains(*it)) {
                // The view was not closed during the dispatch.
                (*it)->setIgnoreInputEvents(false);
            }
        }
        agent->client()->didExitDebugLoop();

        // 6. All views have been resumed, clear the set.
        m_frozenViews.clear();

        m_running = false;
    }

    void quitNow() override
    {
        m_messageLoop->quitNow();
    }

    bool m_running;
    OwnPtr<WebDevToolsAgentClient::WebKitClientMessageLoop> m_messageLoop;
    typedef HashSet<WebViewImpl*> FrozenViewsSet;
    FrozenViewsSet m_frozenViews;
    static ClientMessageLoopAdapter* s_instance;
};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::s_instance = nullptr;

class DebuggerTask : public PageScriptDebugServer::Task {
public:
    DebuggerTask(PassOwnPtr<WebDevToolsAgent::MessageDescriptor> descriptor)
        : m_descriptor(descriptor)
    {
    }

    virtual ~DebuggerTask() { }
    virtual void run()
    {
        WebDevToolsAgent* webagent = m_descriptor->agent();
        if (!webagent)
            return;

        WebDevToolsAgentImpl* agentImpl = static_cast<WebDevToolsAgentImpl*>(webagent);
        if (agentImpl->m_attached)
            agentImpl->dispatchMessageFromFrontend(m_descriptor->message());
    }

private:
    OwnPtr<WebDevToolsAgent::MessageDescriptor> m_descriptor;
};

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* webViewImpl,
    WebDevToolsAgentClient* client)
    : m_client(client)
    , m_webViewImpl(webViewImpl)
    , m_attached(false)
#if ENABLE(ASSERT)
    , m_hasBeenDisposed(false)
#endif
    , m_instrumentingAgents(webViewImpl->page()->instrumentingAgents())
    , m_injectedScriptManager(InjectedScriptManager::createForPage())
    , m_state(adoptPtrWillBeNoop(new InspectorCompositeState(this)))
    , m_overlay(InspectorOverlay::create(webViewImpl->page(), this))
    , m_cssAgent(nullptr)
    , m_resourceAgent(nullptr)
    , m_layerTreeAgent(nullptr)
    , m_agents(m_instrumentingAgents.get(), m_state.get())
    , m_deferredAgentsInitialized(false)
    , m_generatingEvent(false)
    , m_touchEventEmulationEnabled(false)
{
    ASSERT(isMainThread());

    long processId = WTF::getCurrentProcessID();
    ASSERT(processId > 0);
    IdentifiersFactory::setProcessId(processId);
    Page* page = m_webViewImpl->page();

    InjectedScriptManager* injectedScriptManager = m_injectedScriptManager.get();
    InspectorOverlay* overlay = m_overlay.get();

    m_agents.append(InspectorInspectorAgent::create(injectedScriptManager));

    OwnPtrWillBeRawPtr<InspectorPageAgent> pageAgentPtr(InspectorPageAgent::create(page, injectedScriptManager, this, overlay));
    m_pageAgent = pageAgentPtr.get();
    m_agents.append(pageAgentPtr.release());

    OwnPtrWillBeRawPtr<InspectorDOMAgent> domAgentPtr(InspectorDOMAgent::create(m_pageAgent, injectedScriptManager, overlay));
    m_domAgent = domAgentPtr.get();
    m_agents.append(domAgentPtr.release());

    OwnPtrWillBeRawPtr<InspectorLayerTreeAgent> layerTreeAgentPtr(InspectorLayerTreeAgent::create(m_pageAgent));
    m_layerTreeAgent = layerTreeAgentPtr.get();
    m_agents.append(layerTreeAgentPtr.release());

    m_agents.append(InspectorTimelineAgent::create());

    ClientMessageLoopAdapter::ensurePageScriptDebugServerCreated(m_client);
    PageScriptDebugServer* scriptDebugServer = PageScriptDebugServer::instance();

    m_agents.append(PageRuntimeAgent::create(injectedScriptManager, this, scriptDebugServer, m_pageAgent));

    OwnPtrWillBeRawPtr<PageConsoleAgent> pageConsoleAgentPtr = PageConsoleAgent::create(injectedScriptManager, m_domAgent, m_pageAgent);
    OwnPtrWillBeRawPtr<InspectorWorkerAgent> workerAgentPtr = InspectorWorkerAgent::create(pageConsoleAgentPtr.get());

    OwnPtrWillBeRawPtr<InspectorTracingAgent> tracingAgentPtr = InspectorTracingAgent::create(this, workerAgentPtr.get(), m_pageAgent);
    m_tracingAgent = tracingAgentPtr.get();
    m_agents.append(tracingAgentPtr.release());

    m_agents.append(workerAgentPtr.release());
    m_agents.append(pageConsoleAgentPtr.release());

    m_injectedScriptManager->injectedScriptHost()->init(m_instrumentingAgents.get(), scriptDebugServer);

    m_agents.append(InspectorDatabaseAgent::create(page));
    m_agents.append(DeviceOrientationInspectorAgent::create(page));
    m_agents.append(InspectorFileSystemAgent::create(page));
    m_agents.append(InspectorIndexedDBAgent::create(page));
    m_agents.append(InspectorAccessibilityAgent::create(page));
    m_agents.append(InspectorDOMStorageAgent::create(page));

    m_webViewImpl->settingsImpl()->setWebDevToolsAgentImpl(this);
    m_webViewImpl->devToolsEmulator()->setDevToolsAgent(this);
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl()
{
    ASSERT(m_hasBeenDisposed);
}

void WebDevToolsAgentImpl::dispose()
{
    // Explicitly dispose of the agent before destructing to ensure
    // same behavior (and correctness) with and without Oilpan.
    ClientMessageLoopAdapter::inspectedViewClosed(m_webViewImpl);
    m_webViewImpl->settingsImpl()->setWebDevToolsAgentImpl(nullptr);
    m_webViewImpl->devToolsEmulator()->setDevToolsAgent(nullptr);
    if (m_attached) {
        ASSERT(isMainThread());
        Platform::current()->mainThread()->removeTaskObserver(this);
    }
#if ENABLE(ASSERT)
    ASSERT(!m_hasBeenDisposed);
    m_hasBeenDisposed = true;
#endif
}

DEFINE_TRACE(WebDevToolsAgentImpl)
{
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_injectedScriptManager);
    visitor->trace(m_state);
    visitor->trace(m_overlay);
    visitor->trace(m_asyncCallTracker);
    visitor->trace(m_domAgent);
    visitor->trace(m_pageAgent);
    visitor->trace(m_cssAgent);
    visitor->trace(m_resourceAgent);
    visitor->trace(m_layerTreeAgent);
    visitor->trace(m_tracingAgent);
    visitor->trace(m_inspectorBackendDispatcher);
    visitor->trace(m_agents);
}

void WebDevToolsAgentImpl::willBeDestroyed()
{
#if ENABLE(ASSERT)
    Frame* frame = m_webViewImpl->page()->mainFrame();
    ASSERT(frame);
    if (frame->isLocalFrame())
        ASSERT(m_pageAgent->inspectedFrame()->view());
#endif

    detach();
    m_injectedScriptManager->disconnect();
    m_agents.discardAgents();
}

void WebDevToolsAgentImpl::initializeDeferredAgents()
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

    m_agents.append(InspectorAnimationAgent::create(m_pageAgent, m_domAgent));

    m_agents.append(InspectorMemoryAgent::create());

    m_agents.append(InspectorApplicationCacheAgent::create(m_pageAgent));

    OwnPtrWillBeRawPtr<InspectorDebuggerAgent> debuggerAgentPtr(PageDebuggerAgent::create(PageScriptDebugServer::instance(), m_pageAgent, injectedScriptManager, overlay));
    InspectorDebuggerAgent* debuggerAgent = debuggerAgentPtr.get();
    m_agents.append(debuggerAgentPtr.release());
    m_asyncCallTracker = adoptPtrWillBeNoop(new AsyncCallTracker(debuggerAgent, m_instrumentingAgents.get()));

    m_agents.append(InspectorDOMDebuggerAgent::create(m_domAgent, debuggerAgent));

    m_agents.append(InspectorInputAgent::create(m_pageAgent, this));

    m_agents.append(InspectorProfilerAgent::create(injectedScriptManager, overlay));

    m_agents.append(InspectorHeapProfilerAgent::create(injectedScriptManager));

    m_agents.append(InspectorCanvasAgent::create(m_pageAgent, injectedScriptManager));

    m_pageAgent->setDeferredAgents(debuggerAgent, m_cssAgent);
}

void WebDevToolsAgentImpl::attach(const WebString& hostId)
{
    ASSERT(isMainThread());
    if (m_attached)
        return;

    // Set the attached bit first so that sync notifications were delivered.
    m_attached = true;

    initializeDeferredAgents();
    m_resourceAgent->setHostId(hostId);

    m_inspectorFrontend = adoptPtr(new InspectorFrontend(this));
    // We can reconnect to existing front-end -> unmute state.
    m_state->unmute();
    m_agents.setFrontend(m_inspectorFrontend.get());

    InspectorInstrumentation::registerInstrumentingAgents(m_instrumentingAgents.get());
    InspectorInstrumentation::frontendCreated();

    m_inspectorBackendDispatcher = InspectorBackendDispatcher::create(this);
    m_agents.registerInDispatcher(m_inspectorBackendDispatcher.get());

    Platform::current()->mainThread()->addTaskObserver(this);
}

void WebDevToolsAgentImpl::reattach(const WebString& hostId, const WebString& savedState)
{
    if (m_attached)
        return;

    attach(hostId);
    m_state->loadFromCookie(savedState);
    m_agents.restore();
}

void WebDevToolsAgentImpl::detach()
{
    ASSERT(isMainThread());
    if (!m_attached)
        return;

    Platform::current()->mainThread()->removeTaskObserver(this);

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

    m_attached = false;
}

void WebDevToolsAgentImpl::continueProgram()
{
    ClientMessageLoopAdapter::didNavigate();
}

bool WebDevToolsAgentImpl::handleInputEvent(Page* page, const WebInputEvent& inputEvent)
{
    if (!m_attached && !m_generatingEvent)
        return false;

    // FIXME: This workaround is required for touch emulation on Mac, where
    // compositor-side pinch handling is not enabled. See http://crbug.com/138003.
    bool isPinch = inputEvent.type == WebInputEvent::GesturePinchBegin || inputEvent.type == WebInputEvent::GesturePinchUpdate || inputEvent.type == WebInputEvent::GesturePinchEnd;
    if (isPinch && m_touchEventEmulationEnabled) {
        FrameView* frameView = page->deprecatedLocalMainFrame()->view();
        PlatformGestureEventBuilder gestureEvent(frameView, static_cast<const WebGestureEvent&>(inputEvent));
        float pageScaleFactor = page->pageScaleFactor();
        if (gestureEvent.type() == PlatformEvent::GesturePinchBegin) {
            m_lastPinchAnchorCss = adoptPtr(new IntPoint(frameView->scrollPosition() + gestureEvent.position()));
            m_lastPinchAnchorDip = adoptPtr(new IntPoint(gestureEvent.position()));
            m_lastPinchAnchorDip->scale(pageScaleFactor, pageScaleFactor);
        }
        if (gestureEvent.type() == PlatformEvent::GesturePinchUpdate && m_lastPinchAnchorCss) {
            float newPageScaleFactor = pageScaleFactor * gestureEvent.scale();
            IntPoint anchorCss(*m_lastPinchAnchorDip.get());
            anchorCss.scale(1.f / newPageScaleFactor, 1.f / newPageScaleFactor);
            m_webViewImpl->setPageScaleFactor(newPageScaleFactor);
            m_webViewImpl->setMainFrameScrollOffset(*m_lastPinchAnchorCss.get() - toIntSize(anchorCss));
        }
        if (gestureEvent.type() == PlatformEvent::GesturePinchEnd) {
            m_lastPinchAnchorCss.clear();
            m_lastPinchAnchorDip.clear();
        }
        return true;
    }

    if (WebInputEvent::isGestureEventType(inputEvent.type) && inputEvent.type == WebInputEvent::GestureTap) {
        // Only let GestureTab in (we only need it and we know PlatformGestureEventBuilder supports it).
        PlatformGestureEvent gestureEvent = PlatformGestureEventBuilder(page->deprecatedLocalMainFrame()->view(), static_cast<const WebGestureEvent&>(inputEvent));
        return handleGestureEvent(toLocalFrame(page->mainFrame()), gestureEvent);
    }
    if (WebInputEvent::isMouseEventType(inputEvent.type) && inputEvent.type != WebInputEvent::MouseEnter) {
        // PlatformMouseEventBuilder does not work with MouseEnter type, so we filter it out manually.
        PlatformMouseEvent mouseEvent = PlatformMouseEventBuilder(page->deprecatedLocalMainFrame()->view(), static_cast<const WebMouseEvent&>(inputEvent));
        return handleMouseEvent(toLocalFrame(page->mainFrame()), mouseEvent);
    }
    if (WebInputEvent::isTouchEventType(inputEvent.type)) {
        PlatformTouchEvent touchEvent = PlatformTouchEventBuilder(page->deprecatedLocalMainFrame()->view(), static_cast<const WebTouchEvent&>(inputEvent));
        return handleTouchEvent(toLocalFrame(page->mainFrame()), touchEvent);
    }
    if (WebInputEvent::isKeyboardEventType(inputEvent.type)) {
        PlatformKeyboardEvent keyboardEvent = PlatformKeyboardEventBuilder(static_cast<const WebKeyboardEvent&>(inputEvent));
        return handleKeyboardEvent(page->deprecatedLocalMainFrame(), keyboardEvent);
    }
    return false;
}

bool WebDevToolsAgentImpl::handleGestureEvent(LocalFrame* frame, const PlatformGestureEvent& event)
{
    // Overlay should not consume events.
    m_overlay->handleGestureEvent(event);
    if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
        return domAgent->handleGestureEvent(frame, event);
    return false;
}

bool WebDevToolsAgentImpl::handleMouseEvent(LocalFrame* frame, const PlatformMouseEvent& event)
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

bool WebDevToolsAgentImpl::handleTouchEvent(LocalFrame* frame, const PlatformTouchEvent& event)
{
    // Overlay should not consume events.
    m_overlay->handleTouchEvent(event);
    if (InspectorDOMAgent* domAgent = m_instrumentingAgents->inspectorDOMAgent())
        return domAgent->handleTouchEvent(frame, event);
    return false;
}

bool WebDevToolsAgentImpl::handleKeyboardEvent(LocalFrame* frame, const PlatformKeyboardEvent& event)
{
    // Overlay should not consume events.
    m_overlay->handleKeyboardEvent(event);
    return false;
}

void WebDevToolsAgentImpl::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    m_agents.didCommitLoadForLocalFrame(frame);
}

void WebDevToolsAgentImpl::pageScaleFactorChanged()
{
    m_pageAgent->pageScaleFactorChanged();
}

bool WebDevToolsAgentImpl::screencastEnabled()
{
    return m_pageAgent->screencastEnabled();
}

void WebDevToolsAgentImpl::willAddPageOverlay(const GraphicsLayer* layer)
{
    m_layerTreeAgent->willAddPageOverlay(layer);
}

void WebDevToolsAgentImpl::didRemovePageOverlay(const GraphicsLayer* layer)
{
    m_layerTreeAgent->didRemovePageOverlay(layer);
}

void WebDevToolsAgentImpl::setScriptEnabled(bool enabled)
{
    m_pageAgent->setScriptEnabled(enabled);
}

void WebDevToolsAgentImpl::setDeviceMetricsOverride(int width, int height, float deviceScaleFactor, bool mobile, bool fitWindow, float scale, float offsetX, float offsetY)
{
    m_webViewImpl->devToolsEmulator()->setDeviceMetricsOverride(width, height, deviceScaleFactor, mobile, fitWindow, scale, offsetX, offsetY);
}

void WebDevToolsAgentImpl::clearDeviceMetricsOverride()
{
    m_webViewImpl->devToolsEmulator()->clearDeviceMetricsOverride();
}

void WebDevToolsAgentImpl::setTouchEventEmulationEnabled(bool enabled)
{
    m_touchEventEmulationEnabled = enabled;
}

void WebDevToolsAgentImpl::enableTracing(const String& categoryFilter)
{
    m_client->enableTracing(categoryFilter);
}

void WebDevToolsAgentImpl::disableTracing()
{
    m_client->disableTracing();
}

void WebDevToolsAgentImpl::dispatchKeyEvent(const PlatformKeyboardEvent& event)
{
    if (!m_webViewImpl->page()->focusController().isFocused())
        m_webViewImpl->setFocus(true);

    m_generatingEvent = true;
    WebKeyboardEvent webEvent = WebKeyboardEventBuilder(event);
    if (!webEvent.keyIdentifier[0] && webEvent.type != WebInputEvent::Char)
        webEvent.setKeyIdentifierFromWindowsKeyCode();
    m_webViewImpl->handleInputEvent(webEvent);
    m_generatingEvent = false;
}

void WebDevToolsAgentImpl::dispatchMouseEvent(const PlatformMouseEvent& event)
{
    if (!m_webViewImpl->page()->focusController().isFocused())
        m_webViewImpl->setFocus(true);

    m_generatingEvent = true;
    WebMouseEvent webEvent = WebMouseEventBuilder(m_webViewImpl->mainFrameImpl()->frameView(), event);
    m_webViewImpl->handleInputEvent(webEvent);
    m_generatingEvent = false;
}

void WebDevToolsAgentImpl::dispatchOnInspectorBackend(const WebString& message)
{
    if (!m_attached)
        return;
    if (WebDevToolsAgent::shouldInterruptForMessage(message))
        PageScriptDebugServer::instance()->runPendingTasks();
    else
        dispatchMessageFromFrontend(message);
}

void WebDevToolsAgentImpl::dispatchMessageFromFrontend(const String& message)
{
    if (m_inspectorBackendDispatcher)
        m_inspectorBackendDispatcher->dispatch(message);
}

void WebDevToolsAgentImpl::inspectElementAt(const WebPoint& point)
{
    Page* page = m_webViewImpl->page();
    if (!page)
        return;

    HitTestRequest::HitTestRequestType hitType = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::AllowChildFrameContent;
    HitTestRequest request(hitType);
    WebMouseEvent dummyEvent;
    dummyEvent.type = WebInputEvent::MouseDown;
    dummyEvent.x = point.x;
    dummyEvent.y = point.y;
    IntPoint transformedPoint = PlatformMouseEventBuilder(page->deprecatedLocalMainFrame()->view(), dummyEvent).position();
    HitTestResult result(page->deprecatedLocalMainFrame()->view()->windowToContents(transformedPoint));
    page->deprecatedLocalMainFrame()->contentRenderer()->hitTest(request, result);
    Node* node = result.innerNode();
    if (!node && page->deprecatedLocalMainFrame()->document())
        node = page->deprecatedLocalMainFrame()->document()->documentElement();
    m_domAgent->inspect(node);
}

void WebDevToolsAgentImpl::paintPageOverlay(WebGraphicsContext* context, const WebSize& webViewSize)
{
    m_overlay->paint(toWebGraphicsContextImpl(context)->graphicsContext());
}

void WebDevToolsAgentImpl::highlight()
{
    m_webViewImpl->addPageOverlay(this, OverlayZOrders::highlight);
}

void WebDevToolsAgentImpl::hideHighlight()
{
    m_webViewImpl->removePageOverlay(this);
}

void WebDevToolsAgentImpl::resetScrollAndPageScaleFactor()
{
    m_webViewImpl->resetScrollAndScaleState();
}

float WebDevToolsAgentImpl::minimumPageScaleFactor()
{
    return m_webViewImpl->minimumPageScaleFactor();
}

float WebDevToolsAgentImpl::maximumPageScaleFactor()
{
    return m_webViewImpl->maximumPageScaleFactor();
}

void WebDevToolsAgentImpl::setPageScaleFactor(float pageScaleFactor)
{
    m_webViewImpl->setPageScaleFactor(pageScaleFactor);
}

bool WebDevToolsAgentImpl::overridesShowPaintRects()
{
    return m_webViewImpl->isAcceleratedCompositingActive();
}

void WebDevToolsAgentImpl::setShowPaintRects(bool show)
{
    m_webViewImpl->setShowPaintRects(show);
}

void WebDevToolsAgentImpl::setShowDebugBorders(bool show)
{
    m_webViewImpl->setShowDebugBorders(show);
}

void WebDevToolsAgentImpl::setShowFPSCounter(bool show)
{
    m_webViewImpl->setShowFPSCounter(show);
}

void WebDevToolsAgentImpl::setContinuousPaintingEnabled(bool enabled)
{
    m_webViewImpl->setContinuousPaintingEnabled(enabled);
}

void WebDevToolsAgentImpl::setShowScrollBottleneckRects(bool show)
{
    m_webViewImpl->setShowScrollBottleneckRects(show);
}

void WebDevToolsAgentImpl::sendProtocolResponse(int callId, PassRefPtr<JSONObject> message)
{
    if (!m_attached)
        return;
    flushPendingProtocolNotifications();
    m_client->sendProtocolMessage(callId, message->toJSONString(), m_stateCookie);
    m_stateCookie = String();
}

void WebDevToolsAgentImpl::sendProtocolNotification(PassRefPtr<JSONObject> message)
{
    if (!m_attached)
        return;
    m_notificationQueue.append(message);
}

void WebDevToolsAgentImpl::flush()
{
    flushPendingProtocolNotifications();
}

void WebDevToolsAgentImpl::updateInspectorStateCookie(const String& state)
{
    m_stateCookie = state;
}

void WebDevToolsAgentImpl::resumeStartup()
{
    m_client->resumeStartup();
}

void WebDevToolsAgentImpl::setLayerTreeId(int layerTreeId)
{
    m_tracingAgent->setLayerTreeId(layerTreeId);
}

void WebDevToolsAgentImpl::evaluateInWebInspector(long callId, const WebString& script)
{
    if (InspectorInspectorAgent* inspectorAgent = m_instrumentingAgents->inspectorInspectorAgent())
        inspectorAgent->evaluateForTestInFrontend(callId, script);
}

void WebDevToolsAgentImpl::flushPendingProtocolNotifications()
{
    if (!m_attached)
        return;

    m_agents.flushPendingProtocolNotifications();
    for (size_t i = 0; i < m_notificationQueue.size(); ++i)
        m_client->sendProtocolMessage(0, m_notificationQueue[i]->toJSONString(), WebString());
    m_notificationQueue.clear();
}

void WebDevToolsAgentImpl::willProcessTask()
{
    if (!m_attached)
        return;
    if (InspectorProfilerAgent* profilerAgent = m_instrumentingAgents->inspectorProfilerAgent())
        profilerAgent->willProcessTask();
    TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Program");
}

void WebDevToolsAgentImpl::didProcessTask()
{
    if (!m_attached)
        return;
    if (InspectorProfilerAgent* profilerAgent = m_instrumentingAgents->inspectorProfilerAgent())
        profilerAgent->didProcessTask();
    if (InspectorCanvasAgent* canvasAgent = m_instrumentingAgents->inspectorCanvasAgent())
        canvasAgent->didProcessTask();
    TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Program");
    flushPendingProtocolNotifications();
}

void WebDevToolsAgent::interruptAndDispatch(MessageDescriptor* rawDescriptor)
{
    // rawDescriptor can't be a PassOwnPtr because interruptAndDispatch is a WebKit API function.
    OwnPtr<MessageDescriptor> descriptor = adoptPtr(rawDescriptor);
    OwnPtr<DebuggerTask> task = adoptPtr(new DebuggerTask(descriptor.release()));
    PageScriptDebugServer::interruptMainThreadAndRun(task.release());
}

bool WebDevToolsAgent::shouldInterruptForMessage(const WebString& message)
{
    String commandName;
    if (!InspectorBackendDispatcher::getCommandName(message, &commandName))
        return false;
    return commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_pauseCmd)
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_setBreakpointCmd)
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_setBreakpointByUrlCmd)
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_removeBreakpointCmd)
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_setBreakpointsActiveCmd);
}

} // namespace blink
