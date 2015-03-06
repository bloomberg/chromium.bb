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
#include "core/dom/ExceptionCode.h"
#include "core/fetch/MemoryCache.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InspectorController.h"
#include "core/layout/LayoutView.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "platform/JSONValues.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/web/WebDataSource.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "public/web/WebDeviceEmulationParams.h"
#include "public/web/WebMemoryUsageInfo.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "web/WebGraphicsContextImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/CurrentTime.h"
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
    static void ensureClientMessageLoopCreated(WebDevToolsAgentClient* client)
    {
        if (s_instance)
            return;
        OwnPtr<ClientMessageLoopAdapter> instance = adoptPtr(new ClientMessageLoopAdapter(adoptPtr(client->createClientMessageLoop())));
        s_instance = instance.get();
        PageScriptDebugServer::shared().setClientMessageLoop(instance.release());
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
            PageScriptDebugServer::shared().continueProgram();
    }

private:
    ClientMessageLoopAdapter(PassOwnPtr<WebDevToolsAgentClient::WebKitClientMessageLoop> messageLoop)
        : m_running(false)
        , m_messageLoop(messageLoop) { }


    virtual void run(LocalFrame* frame)
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

    virtual void quitNow()
    {
        m_messageLoop->quitNow();
    }

    bool m_running;
    OwnPtr<WebDevToolsAgentClient::WebKitClientMessageLoop> m_messageLoop;
    typedef HashSet<WebViewImpl*> FrozenViewsSet;
    FrozenViewsSet m_frozenViews;
    // FIXME: The ownership model for s_instance is somewhat complicated. Can we make this simpler?
    static ClientMessageLoopAdapter* s_instance;
};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::s_instance = 0;

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
            agentImpl->inspectorController()->dispatchMessageFromFrontend(m_descriptor->message());
    }

private:
    OwnPtr<WebDevToolsAgent::MessageDescriptor> m_descriptor;
};

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* webViewImpl,
    WebDevToolsAgentClient* client)
    : m_layerTreeId(0)
    , m_client(client)
    , m_webViewImpl(webViewImpl)
    , m_attached(false)
    , m_generatingEvent(false)
    , m_deviceMetricsEnabled(false)
    , m_emulateMobileEnabled(false)
    , m_originalViewportEnabled(false)
    , m_isOverlayScrollbarsEnabled(false)
    , m_originalDefaultMinimumPageScaleFactor(0)
    , m_originalDefaultMaximumPageScaleFactor(0)
    , m_touchEventEmulationEnabled(false)
{
    ASSERT(isMainThread());

    long processId = WTF::getCurrentProcessID();
    ASSERT(processId > 0);
    inspectorController()->setProcessId(processId);

    ClientMessageLoopAdapter::ensureClientMessageLoopCreated(m_client);
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl()
{
    ClientMessageLoopAdapter::inspectedViewClosed(m_webViewImpl);
    if (m_attached)
        Platform::current()->currentThread()->removeTaskObserver(this);
}

void WebDevToolsAgentImpl::attach(const WebString& hostId)
{
    if (m_attached)
        return;

    // Set the attached bit first so that sync notifications were delivered.
    m_attached = true;
    inspectorController()->connectFrontend(hostId, this);
    Platform::current()->currentThread()->addTaskObserver(this);
}

void WebDevToolsAgentImpl::reattach(const WebString& hostId, const WebString& savedState)
{
    if (m_attached)
        return;

    // Set the attached bit first so that sync notifications were delivered.
    m_attached = true;
    inspectorController()->reuseFrontend(hostId, this, savedState);
    Platform::current()->currentThread()->addTaskObserver(this);
}

void WebDevToolsAgentImpl::detach()
{
    Platform::current()->currentThread()->removeTaskObserver(this);

    // Prevent controller from sending messages to the frontend.
    InspectorController* ic = inspectorController();
    ic->disconnectFrontend();
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

    InspectorController* ic = inspectorController();
    if (!ic)
        return false;

    if (WebInputEvent::isGestureEventType(inputEvent.type) && inputEvent.type == WebInputEvent::GestureTap) {
        // Only let GestureTab in (we only need it and we know PlatformGestureEventBuilder supports it).
        PlatformGestureEvent gestureEvent = PlatformGestureEventBuilder(page->deprecatedLocalMainFrame()->view(), static_cast<const WebGestureEvent&>(inputEvent));
        return ic->handleGestureEvent(toLocalFrame(page->mainFrame()), gestureEvent);
    }
    if (WebInputEvent::isMouseEventType(inputEvent.type) && inputEvent.type != WebInputEvent::MouseEnter) {
        // PlatformMouseEventBuilder does not work with MouseEnter type, so we filter it out manually.
        PlatformMouseEvent mouseEvent = PlatformMouseEventBuilder(page->deprecatedLocalMainFrame()->view(), static_cast<const WebMouseEvent&>(inputEvent));
        return ic->handleMouseEvent(toLocalFrame(page->mainFrame()), mouseEvent);
    }
    if (WebInputEvent::isTouchEventType(inputEvent.type)) {
        PlatformTouchEvent touchEvent = PlatformTouchEventBuilder(page->deprecatedLocalMainFrame()->view(), static_cast<const WebTouchEvent&>(inputEvent));
        return ic->handleTouchEvent(toLocalFrame(page->mainFrame()), touchEvent);
    }
    if (WebInputEvent::isKeyboardEventType(inputEvent.type)) {
        PlatformKeyboardEvent keyboardEvent = PlatformKeyboardEventBuilder(static_cast<const WebKeyboardEvent&>(inputEvent));
        return ic->handleKeyboardEvent(page->deprecatedLocalMainFrame(), keyboardEvent);
    }
    return false;
}

void WebDevToolsAgentImpl::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    if (InspectorController* ic = inspectorController())
        ic->didCommitLoadForLocalFrame(frame);
}

void WebDevToolsAgentImpl::setDeviceMetricsOverride(int width, int height, float deviceScaleFactor, bool mobile, bool fitWindow, float scale, float offsetX, float offsetY)
{
    if (!m_deviceMetricsEnabled) {
        m_deviceMetricsEnabled = true;
        m_webViewImpl->setBackgroundColorOverride(Color::darkGray);
    }
    if (mobile)
        enableMobileEmulation();
    else
        disableMobileEmulation();

    WebDeviceEmulationParams params;
    params.screenPosition = mobile ? WebDeviceEmulationParams::Mobile : WebDeviceEmulationParams::Desktop;
    params.deviceScaleFactor = deviceScaleFactor;
    params.viewSize = WebSize(width, height);
    params.fitToView = fitWindow;
    params.scale = scale;
    params.offset = WebFloatPoint(offsetX, offsetY);
    m_client->enableDeviceEmulation(params);
}

void WebDevToolsAgentImpl::clearDeviceMetricsOverride()
{
    if (m_deviceMetricsEnabled) {
        m_deviceMetricsEnabled = false;
        m_webViewImpl->setBackgroundColorOverride(Color::transparent);
        disableMobileEmulation();
        m_client->disableDeviceEmulation();
    }
}

void WebDevToolsAgentImpl::setTouchEventEmulationEnabled(bool enabled)
{
    m_touchEventEmulationEnabled = enabled;
}

void WebDevToolsAgentImpl::enableMobileEmulation()
{
    if (m_emulateMobileEnabled)
        return;
    m_emulateMobileEnabled = true;
    m_isOverlayScrollbarsEnabled = RuntimeEnabledFeatures::overlayScrollbarsEnabled();
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
    m_originalViewportEnabled = RuntimeEnabledFeatures::cssViewportEnabled();
    RuntimeEnabledFeatures::setCSSViewportEnabled(true);
    m_webViewImpl->enableViewport();
    m_webViewImpl->settings()->setViewportMetaEnabled(true);
    m_webViewImpl->settings()->setShrinksViewportContentToFit(true);
    m_webViewImpl->setZoomFactorOverride(1);

    m_originalDefaultMinimumPageScaleFactor = m_webViewImpl->defaultMinimumPageScaleFactor();
    m_originalDefaultMaximumPageScaleFactor = m_webViewImpl->defaultMaximumPageScaleFactor();
    m_webViewImpl->setDefaultPageScaleLimits(0.25f, 5);
}

void WebDevToolsAgentImpl::disableMobileEmulation()
{
    if (!m_emulateMobileEnabled)
        return;
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(m_isOverlayScrollbarsEnabled);
    RuntimeEnabledFeatures::setCSSViewportEnabled(m_originalViewportEnabled);
    m_webViewImpl->disableViewport();
    m_webViewImpl->settings()->setViewportMetaEnabled(false);
    m_webViewImpl->settings()->setShrinksViewportContentToFit(false);
    m_webViewImpl->setZoomFactorOverride(0);
    m_emulateMobileEnabled = false;
    m_webViewImpl->setDefaultPageScaleLimits(
        m_originalDefaultMinimumPageScaleFactor,
        m_originalDefaultMaximumPageScaleFactor);
}

void WebDevToolsAgentImpl::enableTracing(const String& categoryFilter)
{
    m_client->enableTracing(categoryFilter);
}

void WebDevToolsAgentImpl::disableTracing()
{
    m_client->disableTracing();
}

void WebDevToolsAgentImpl::processGPUEvent(const GPUEvent& event)
{
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
        PageScriptDebugServer::shared().runPendingTasks();
    else
        inspectorController()->dispatchMessageFromFrontend(message);
}

void WebDevToolsAgentImpl::inspectElementAt(const WebPoint& point)
{
    m_webViewImpl->inspectElementAt(point);
}

InspectorController* WebDevToolsAgentImpl::inspectorController()
{
    if (Page* page = m_webViewImpl->page())
        return &page->inspectorController();
    return 0;
}

LocalFrame* WebDevToolsAgentImpl::mainFrame()
{
    if (Page* page = m_webViewImpl->page())
        return page->deprecatedLocalMainFrame();
    return 0;
}

// WebPageOverlay
void WebDevToolsAgentImpl::paintPageOverlay(WebGraphicsContext* context, const WebSize& webViewSize)
{
    if (InspectorController* ic = inspectorController())
        ic->drawHighlight(toWebGraphicsContextImpl(context)->graphicsContext());
}

void WebDevToolsAgentImpl::highlight()
{
    m_webViewImpl->addPageOverlay(this, OverlayZOrders::highlight);
}

void WebDevToolsAgentImpl::hideHighlight()
{
    m_webViewImpl->removePageOverlay(this);
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
    m_layerTreeId = layerTreeId;
    inspectorController()->setLayerTreeId(layerTreeId);
}

void WebDevToolsAgentImpl::evaluateInWebInspector(long callId, const WebString& script)
{
    InspectorController* ic = inspectorController();
    ic->evaluateForTestInFrontend(callId, script);
}

void WebDevToolsAgentImpl::flushPendingProtocolNotifications()
{
    if (!m_attached)
        return;
    InspectorController* ic = inspectorController();
    ic->flushPendingProtocolNotifications();

    for (size_t i = 0; i < m_notificationQueue.size(); ++i)
        m_client->sendProtocolMessage(0, m_notificationQueue[i]->toJSONString(), WebString());
    m_notificationQueue.clear();
}

void WebDevToolsAgentImpl::willProcessTask()
{
    if (!m_attached)
        return;
    if (InspectorController* ic = inspectorController())
        ic->willProcessTask();
    TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Program");
}

void WebDevToolsAgentImpl::didProcessTask()
{
    if (!m_attached)
        return;
    if (InspectorController* ic = inspectorController())
        ic->didProcessTask();
    TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Program");
    flushPendingProtocolNotifications();
}

void WebDevToolsAgent::interruptAndDispatch(MessageDescriptor* rawDescriptor)
{
    // rawDescriptor can't be a PassOwnPtr because interruptAndDispatch is a WebKit API function.
    OwnPtr<MessageDescriptor> descriptor = adoptPtr(rawDescriptor);
    OwnPtr<DebuggerTask> task = adoptPtr(new DebuggerTask(descriptor.release()));
    PageScriptDebugServer::interruptAndRun(task.release());
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
