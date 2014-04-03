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
#include "WebDevToolsAgentImpl.h"

#include "InspectorBackendDispatcher.h"
#include "InspectorFrontend.h"
#include "InspectorProtocolVersion.h"
#include "RuntimeEnabledFeatures.h"
#include "WebDataSource.h"
#include "WebDevToolsAgentClient.h"
#include "WebDeviceEmulationParams.h"
#include "WebFrameImpl.h"
#include "WebInputEventConversion.h"
#include "WebMemoryUsageInfo.h"
#include "WebSettings.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "bindings/v8/PageScriptDebugServer.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExceptionCode.h"
#include "core/fetch/MemoryCache.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InspectorController.h"
#include "core/page/Page.h"
#include "core/rendering/RenderView.h"
#include "platform/JSONValues.h"
#include "platform/graphics/GraphicsContext.h"
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
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

using namespace WebCore;
using namespace std;

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
    ClientMessageLoopAdapter(PassOwnPtr<blink::WebDevToolsAgentClient::WebKitClientMessageLoop> messageLoop)
        : m_running(false)
        , m_messageLoop(messageLoop) { }


    virtual void run(Page* page)
    {
        if (m_running)
            return;
        m_running = true;

        // 0. Flush pending frontend messages.
        WebViewImpl* viewImpl = WebViewImpl::fromPage(page);
        WebDevToolsAgentImpl* agent = static_cast<WebDevToolsAgentImpl*>(viewImpl->devToolsAgent());
        agent->flushPendingFrontendMessages();

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

        // 6. All views have been resumed, clear the set.
        m_frozenViews.clear();

        m_running = false;
    }

    virtual void quitNow()
    {
        m_messageLoop->quitNow();
    }

    bool m_running;
    OwnPtr<blink::WebDevToolsAgentClient::WebKitClientMessageLoop> m_messageLoop;
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
        if (WebDevToolsAgent* webagent = m_descriptor->agent())
            webagent->dispatchOnInspectorBackend(m_descriptor->message());
    }

private:
    OwnPtr<WebDevToolsAgent::MessageDescriptor> m_descriptor;
};

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* webViewImpl,
    WebDevToolsAgentClient* client)
    : m_hostId(client->hostIdentifier())
    , m_client(client)
    , m_webViewImpl(webViewImpl)
    , m_attached(false)
    , m_generatingEvent(false)
    , m_deviceMetricsEnabled(false)
    , m_emulateViewportEnabled(false)
    , m_originalViewportEnabled(false)
    , m_isOverlayScrollbarsEnabled(false)
{
    ASSERT(m_hostId > 0);
    ClientMessageLoopAdapter::ensureClientMessageLoopCreated(m_client);
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl()
{
    ClientMessageLoopAdapter::inspectedViewClosed(m_webViewImpl);
    if (m_attached)
        blink::Platform::current()->currentThread()->removeTaskObserver(this);
}

void WebDevToolsAgentImpl::attach()
{
    if (m_attached)
        return;

    inspectorController()->connectFrontend(this);
    inspectorController()->webViewResized(m_webViewImpl->size());
    blink::Platform::current()->currentThread()->addTaskObserver(this);
    m_attached = true;
}

void WebDevToolsAgentImpl::reattach(const WebString& savedState)
{
    if (m_attached)
        return;

    inspectorController()->reuseFrontend(this, savedState);
    blink::Platform::current()->currentThread()->addTaskObserver(this);
    m_attached = true;
}

void WebDevToolsAgentImpl::detach()
{
    blink::Platform::current()->currentThread()->removeTaskObserver(this);

    // Prevent controller from sending messages to the frontend.
    InspectorController* ic = inspectorController();
    ic->disconnectFrontend();
    ic->hideHighlight();
    m_attached = false;
}

void WebDevToolsAgentImpl::didNavigate()
{
    ClientMessageLoopAdapter::didNavigate();
}

void WebDevToolsAgentImpl::didBeginFrame(int frameId)
{
    if (InspectorController* ic = inspectorController())
        ic->didBeginFrame(frameId);
}

void WebDevToolsAgentImpl::didCancelFrame()
{
    if (InspectorController* ic = inspectorController())
        ic->didCancelFrame();
}

void WebDevToolsAgentImpl::willComposite()
{
    if (InspectorController* ic = inspectorController())
        ic->willComposite();
}

void WebDevToolsAgentImpl::didComposite()
{
    if (InspectorController* ic = inspectorController())
        ic->didComposite();
}

void WebDevToolsAgentImpl::didCreateScriptContext(WebFrameImpl* webframe, int worldId)
{
    // Skip non main world contexts.
    if (worldId)
        return;
    if (WebCore::LocalFrame* frame = webframe->frame())
        frame->script().setContextDebugId(m_hostId);
}

void WebDevToolsAgentImpl::webViewResized(const WebSize& size)
{
    if (InspectorController* ic = inspectorController())
        ic->webViewResized(IntSize());
}

bool WebDevToolsAgentImpl::handleInputEvent(WebCore::Page* page, const WebInputEvent& inputEvent)
{
    if (!m_attached && !m_generatingEvent)
        return false;

    InspectorController* ic = inspectorController();
    if (!ic)
        return false;

    if (WebInputEvent::isGestureEventType(inputEvent.type) && inputEvent.type == WebInputEvent::GestureTap) {
        // Only let GestureTab in (we only need it and we know PlatformGestureEventBuilder supports it).
        PlatformGestureEvent gestureEvent = PlatformGestureEventBuilder(page->mainFrame()->view(), *static_cast<const WebGestureEvent*>(&inputEvent));
        return ic->handleGestureEvent(page->mainFrame(), gestureEvent);
    }
    if (WebInputEvent::isMouseEventType(inputEvent.type) && inputEvent.type != WebInputEvent::MouseEnter) {
        // PlatformMouseEventBuilder does not work with MouseEnter type, so we filter it out manually.
        PlatformMouseEvent mouseEvent = PlatformMouseEventBuilder(page->mainFrame()->view(), *static_cast<const WebMouseEvent*>(&inputEvent));
        return ic->handleMouseEvent(page->mainFrame(), mouseEvent);
    }
    if (WebInputEvent::isTouchEventType(inputEvent.type)) {
        PlatformTouchEvent touchEvent = PlatformTouchEventBuilder(page->mainFrame()->view(), *static_cast<const WebTouchEvent*>(&inputEvent));
        return ic->handleTouchEvent(page->mainFrame(), touchEvent);
    }
    if (WebInputEvent::isKeyboardEventType(inputEvent.type)) {
        PlatformKeyboardEvent keyboardEvent = PlatformKeyboardEventBuilder(*static_cast<const WebKeyboardEvent*>(&inputEvent));
        return ic->handleKeyboardEvent(page->mainFrame(), keyboardEvent);
    }
    return false;
}

void WebDevToolsAgentImpl::overrideDeviceMetrics(int width, int height, float deviceScaleFactor, bool emulateViewport, bool fitWindow)
{
    if (!width && !height && !deviceScaleFactor) {
        if (m_deviceMetricsEnabled) {
            m_deviceMetricsEnabled = false;
            m_webViewImpl->setBackgroundColorOverride(Color::transparent);
            disableViewportEmulation();
            m_client->disableDeviceEmulation();
        }
    } else {
        if (!m_deviceMetricsEnabled) {
            m_deviceMetricsEnabled = true;
            m_webViewImpl->setBackgroundColorOverride(Color::darkGray);
        }
        if (emulateViewport)
            enableViewportEmulation();
        else
            disableViewportEmulation();

        WebDeviceEmulationParams params;
        params.screenPosition = emulateViewport ? WebDeviceEmulationParams::Mobile : WebDeviceEmulationParams::Desktop;
        params.deviceScaleFactor = deviceScaleFactor;
        params.viewSize = WebSize(width, height);
        params.fitToView = fitWindow;
        params.viewInsets = WebSize(10, 10);
        m_client->enableDeviceEmulation(params);
    }
}

void WebDevToolsAgentImpl::enableViewportEmulation()
{
    if (m_emulateViewportEnabled)
        return;
    m_emulateViewportEnabled = true;
    m_isOverlayScrollbarsEnabled = RuntimeEnabledFeatures::overlayScrollbarsEnabled();
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
    m_originalViewportEnabled = RuntimeEnabledFeatures::cssViewportEnabled();
    RuntimeEnabledFeatures::setCSSViewportEnabled(true);
    m_webViewImpl->settings()->setViewportEnabled(true);
    m_webViewImpl->settings()->setViewportMetaEnabled(true);
    m_webViewImpl->setIgnoreViewportTagScaleLimits(true);
    m_webViewImpl->setPageScaleFactorLimits(-1, -1);
    m_webViewImpl->setZoomFactorOverride(1);
}

void WebDevToolsAgentImpl::disableViewportEmulation()
{
    if (!m_emulateViewportEnabled)
        return;
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(m_isOverlayScrollbarsEnabled);
    RuntimeEnabledFeatures::setCSSViewportEnabled(m_originalViewportEnabled);
    m_webViewImpl->settings()->setViewportEnabled(false);
    m_webViewImpl->settings()->setViewportMetaEnabled(false);
    m_webViewImpl->setIgnoreViewportTagScaleLimits(false);
    m_webViewImpl->setPageScaleFactorLimits(1, 1);
    m_webViewImpl->setZoomFactorOverride(0);
    m_emulateViewportEnabled = false;
}

void WebDevToolsAgentImpl::getAllocatedObjects(HashSet<const void*>& set)
{
    class CountingVisitor : public WebDevToolsAgentClient::AllocatedObjectVisitor {
    public:
        CountingVisitor() : m_totalObjectsCount(0)
        {
        }

        virtual bool visitObject(const void* ptr)
        {
            ++m_totalObjectsCount;
            return true;
        }
        size_t totalObjectsCount() const
        {
            return m_totalObjectsCount;
        }

    private:
        size_t m_totalObjectsCount;
    };

    CountingVisitor counter;
    m_client->visitAllocatedObjects(&counter);

    class PointerCollector : public WebDevToolsAgentClient::AllocatedObjectVisitor {
    public:
        explicit PointerCollector(size_t maxObjectsCount)
            : m_maxObjectsCount(maxObjectsCount)
            , m_index(0)
            , m_success(true)
            , m_pointers(new const void*[maxObjectsCount])
        {
        }
        virtual ~PointerCollector()
        {
            delete[] m_pointers;
        }
        virtual bool visitObject(const void* ptr)
        {
            if (m_index == m_maxObjectsCount) {
                m_success = false;
                return false;
            }
            m_pointers[m_index++] = ptr;
            return true;
        }

        bool success() const { return m_success; }

        void copyTo(HashSet<const void*>& set)
        {
            for (size_t i = 0; i < m_index; i++)
                set.add(m_pointers[i]);
        }

    private:
        const size_t m_maxObjectsCount;
        size_t m_index;
        bool m_success;
        const void** m_pointers;
    };

    // Double size to allow room for all objects that may have been allocated
    // since we counted them.
    size_t estimatedMaxObjectsCount = counter.totalObjectsCount() * 2;
    while (true) {
        PointerCollector collector(estimatedMaxObjectsCount);
        m_client->visitAllocatedObjects(&collector);
        if (collector.success()) {
            collector.copyTo(set);
            break;
        }
        estimatedMaxObjectsCount *= 2;
    }
}

void WebDevToolsAgentImpl::dumpUncountedAllocatedObjects(const HashMap<const void*, size_t>& map)
{
    class InstrumentedObjectSizeProvider : public WebDevToolsAgentClient::InstrumentedObjectSizeProvider {
    public:
        InstrumentedObjectSizeProvider(const HashMap<const void*, size_t>& map) : m_map(map) { }
        virtual size_t objectSize(const void* ptr) const
        {
            HashMap<const void*, size_t>::const_iterator i = m_map.find(ptr);
            return i == m_map.end() ? 0 : i->value;
        }

    private:
        const HashMap<const void*, size_t>& m_map;
    };

    InstrumentedObjectSizeProvider provider(map);
    m_client->dumpUncountedAllocatedObjects(&provider);
}

void WebDevToolsAgentImpl::setTraceEventCallback(const String& categoryFilter, TraceEventCallback callback)
{
    m_client->setTraceEventCallback(categoryFilter, callback);
}

void WebDevToolsAgentImpl::resetTraceEventCallback()
{
    m_client->resetTraceEventCallback();
}

void WebDevToolsAgentImpl::startGPUEventsRecording()
{
    m_client->startGPUEventsRecording();
}

void WebDevToolsAgentImpl::stopGPUEventsRecording()
{
    m_client->stopGPUEventsRecording();
}

void WebDevToolsAgentImpl::processGPUEvent(const GPUEvent& event)
{
    if (InspectorController* ic = inspectorController())
        ic->processGPUEvent(event.timestamp, event.phase, event.foreign, event.usedGPUMemoryBytes, event.limitGPUMemoryBytes);
}

void WebDevToolsAgentImpl::dispatchKeyEvent(const PlatformKeyboardEvent& event)
{
    m_generatingEvent = true;
    WebKeyboardEvent webEvent = WebKeyboardEventBuilder(event);
    if (!webEvent.keyIdentifier[0] && webEvent.type != WebInputEvent::Char)
        webEvent.setKeyIdentifierFromWindowsKeyCode();
    m_webViewImpl->handleInputEvent(webEvent);
    m_generatingEvent = false;
}

void WebDevToolsAgentImpl::dispatchMouseEvent(const PlatformMouseEvent& event)
{
    m_generatingEvent = true;
    WebMouseEvent webEvent = WebMouseEventBuilder(m_webViewImpl->mainFrameImpl()->frameView(), event);
    m_webViewImpl->handleInputEvent(webEvent);
    m_generatingEvent = false;
}

void WebDevToolsAgentImpl::dispatchOnInspectorBackend(const WebString& message)
{
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
        return page->mainFrame();
    return 0;
}

// WebPageOverlay
void WebDevToolsAgentImpl::paintPageOverlay(WebCanvas* canvas)
{
    InspectorController* ic = inspectorController();
    if (ic) {
        GraphicsContext context(canvas);
        context.setCertainlyOpaque(false);
        ic->drawHighlight(context);
    }
}

void WebDevToolsAgentImpl::highlight()
{
    m_webViewImpl->addPageOverlay(this, OverlayZOrders::highlight);
}

void WebDevToolsAgentImpl::hideHighlight()
{
    m_webViewImpl->removePageOverlay(this);
}

void WebDevToolsAgentImpl::sendMessageToFrontend(PassRefPtr<WebCore::JSONObject> message)
{
    m_frontendMessageQueue.append(message);
}

void WebDevToolsAgentImpl::flush()
{
    flushPendingFrontendMessages();
}

void WebDevToolsAgentImpl::updateInspectorStateCookie(const String& state)
{
    m_client->saveAgentRuntimeState(state);
}

void WebDevToolsAgentImpl::clearBrowserCache()
{
    m_client->clearBrowserCache();
}

void WebDevToolsAgentImpl::clearBrowserCookies()
{
    m_client->clearBrowserCookies();
}

void WebDevToolsAgentImpl::setProcessId(long processId)
{
    inspectorController()->setProcessId(processId);
}

void WebDevToolsAgentImpl::setLayerTreeId(int layerTreeId)
{
    inspectorController()->setLayerTreeId(layerTreeId);
}

void WebDevToolsAgentImpl::evaluateInWebInspector(long callId, const WebString& script)
{
    InspectorController* ic = inspectorController();
    ic->evaluateForTestInFrontend(callId, script);
}

void WebDevToolsAgentImpl::flushPendingFrontendMessages()
{
    InspectorController* ic = inspectorController();
    ic->flushPendingFrontendMessages();

    for (size_t i = 0; i < m_frontendMessageQueue.size(); ++i)
        m_client->sendMessageToInspectorFrontend(m_frontendMessageQueue[i]->toJSONString());
    m_frontendMessageQueue.clear();
}

void WebDevToolsAgentImpl::willProcessTask()
{
    if (!m_attached)
        return;
    if (InspectorController* ic = inspectorController())
        ic->willProcessTask();
}

void WebDevToolsAgentImpl::didProcessTask()
{
    if (!m_attached)
        return;
    if (InspectorController* ic = inspectorController())
        ic->didProcessTask();
    flushPendingFrontendMessages();
}

WebString WebDevToolsAgent::inspectorProtocolVersion()
{
    return WebCore::inspectorProtocolVersion();
}

bool WebDevToolsAgent::supportsInspectorProtocolVersion(const WebString& version)
{
    return WebCore::supportsInspectorProtocolVersion(version);
}

void WebDevToolsAgent::interruptAndDispatch(MessageDescriptor* rawDescriptor)
{
    // rawDescriptor can't be a PassOwnPtr because interruptAndDispatch is a WebKit API function.
    OwnPtr<MessageDescriptor> descriptor = adoptPtr(rawDescriptor);
    OwnPtr<DebuggerTask> task = adoptPtr(new DebuggerTask(descriptor.release()));
    PageScriptDebugServer::interruptAndRun(task.release(), v8::Isolate::GetCurrent());
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
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kDebugger_setBreakpointsActiveCmd)
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kProfiler_startCmd)
        || commandName == InspectorBackendDispatcher::commandName(InspectorBackendDispatcher::kProfiler_stopCmd);
}

void WebDevToolsAgent::processPendingMessages()
{
    PageScriptDebugServer::shared().runPendingTasks();
}

} // namespace blink
