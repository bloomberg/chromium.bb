/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "web/WebPagePopupImpl.h"

#include "core/dom/ContextFeatures.h"
#include "core/events/MessageEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutView.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/DOMWindowPagePopup.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/PagePopupClient.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/LayoutTestSupport.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "public/platform/WebCursorInfo.h"
#include "public/web/WebAXObject.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWidgetClient.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

class PagePopupChromeClient final : public EmptyChromeClient {
public:
    static PassOwnPtrWillBeRawPtr<PagePopupChromeClient> create(WebPagePopupImpl* popup)
    {
        return adoptPtrWillBeNoop(new PagePopupChromeClient(popup));
    }

    void setWindowRect(const IntRect& rect) override
    {
        m_popup->setWindowRect(rect);
    }

private:
    explicit PagePopupChromeClient(WebPagePopupImpl* popup)
        : m_popup(popup)
    {
        ASSERT(m_popup->widgetClient());
    }

    void closeWindowSoon() override
    {
        m_popup->closePopup();
    }

    IntRect windowRect() override
    {
        return m_popup->m_windowRectInScreen;
    }

    IntRect viewportToScreen(const IntRect& rect) const override
    {
        WebRect rectInScreen(rect);
        m_popup->widgetClient()->convertViewportToWindow(&rectInScreen);
        rectInScreen.x += m_popup->m_windowRectInScreen.x;
        rectInScreen.y += m_popup->m_windowRectInScreen.y;
        return rectInScreen;
    }

    void addMessageToConsole(LocalFrame*, MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String&, const String&) override
    {
#ifndef NDEBUG
        fprintf(stderr, "CONSOLE MESSSAGE:%u: %s\n", lineNumber, message.utf8().data());
#endif
    }

    void invalidateRect(const IntRect& paintRect) override
    {
        if (!paintRect.isEmpty())
            m_popup->widgetClient()->didInvalidateRect(paintRect);
    }

    void scheduleAnimation(Widget*) override
    {
        // Calling scheduleAnimation on m_webView so WebTestProxy will call beginFrame.
        if (LayoutTestSupport::isRunningLayoutTest())
            m_popup->m_webView->scheduleAnimation();

        if (m_popup->isAcceleratedCompositingActive()) {
            ASSERT(m_popup->m_layerTreeView);
            m_popup->m_layerTreeView->setNeedsBeginFrame();
            return;
        }
        m_popup->m_widgetClient->scheduleAnimation();
    }

    void attachCompositorAnimationTimeline(WebCompositorAnimationTimeline* timeline, LocalFrame*) override
    {
        if (m_popup->m_layerTreeView)
            m_popup->m_layerTreeView->attachCompositorAnimationTimeline(timeline);
    }

    void detachCompositorAnimationTimeline(WebCompositorAnimationTimeline* timeline, LocalFrame*) override
    {
        if (m_popup->m_layerTreeView)
            m_popup->m_layerTreeView->detachCompositorAnimationTimeline(timeline);
    }

    WebScreenInfo screenInfo() const override
    {
        return m_popup->m_webView->client() ? m_popup->m_webView->client()->screenInfo() : WebScreenInfo();
    }

    void* webView() const override
    {
        return m_popup->m_webView;
    }

    IntSize minimumWindowSize() const override
    {
        return IntSize(0, 0);
    }

    void setCursor(const Cursor& cursor, LocalFrame* localRoot) override
    {
        if (m_popup->m_webView->client())
            m_popup->m_webView->client()->didChangeCursor(WebCursorInfo(cursor));
    }

    void needTouchEvents(bool needsTouchEvents) override
    {
        m_popup->widgetClient()->hasTouchEventHandlers(needsTouchEvents);
    }

    void setHaveWheelEventHandlers(bool haveEventHandlers) override
    {
        if (m_popup->m_layerTreeView)
            return m_popup->m_layerTreeView->setHaveWheelEventHandlers(haveEventHandlers);
    }

    bool haveWheelEventHandlers() const override
    {
        if (m_popup->m_layerTreeView)
            return m_popup->m_layerTreeView->haveWheelEventHandlers();
        return false;
    }

    void setTouchAction(TouchAction touchAction) override
    {
        if (WebViewClient* client = m_popup->m_webView->client())
            client->setTouchAction(static_cast<WebTouchAction>(touchAction));
    }

    GraphicsLayerFactory* graphicsLayerFactory() const override
    {
        return m_popup->m_webView->graphicsLayerFactory();
    }

    void attachRootGraphicsLayer(GraphicsLayer* graphicsLayer, LocalFrame* localRoot) override
    {
        m_popup->setRootGraphicsLayer(graphicsLayer);
    }

    void postAccessibilityNotification(AXObject* obj, AXObjectCache::AXNotification notification) override
    {
        WebLocalFrameImpl* frame = WebLocalFrameImpl::fromFrame(m_popup->m_popupClient->ownerElement().document().frame());
        if (obj && frame && frame->client())
            frame->client()->postAccessibilityEvent(WebAXObject(obj), static_cast<WebAXEvent>(notification));
    }

    void setToolTip(const String& tooltipText, TextDirection dir) override
    {
        if (m_popup->widgetClient())
            m_popup->widgetClient()->setToolTipText(tooltipText, toWebTextDirection(dir));
    }

    WebPagePopupImpl* m_popup;
};

class PagePopupFeaturesClient : public ContextFeaturesClient {
    bool isEnabled(Document*, ContextFeatures::FeatureType, bool) override;
};

bool PagePopupFeaturesClient::isEnabled(Document*, ContextFeatures::FeatureType type, bool defaultValue)
{
    if (type == ContextFeatures::PagePopup)
        return true;
    return defaultValue;
}

// WebPagePopupImpl ----------------------------------------------------------------

WebPagePopupImpl::WebPagePopupImpl(WebWidgetClient* client)
    : m_widgetClient(client)
    , m_closing(false)
    , m_layerTreeView(0)
    , m_rootLayer(0)
    , m_rootGraphicsLayer(0)
    , m_isAcceleratedCompositingActive(false)
{
    ASSERT(client);
}

WebPagePopupImpl::~WebPagePopupImpl()
{
    ASSERT(!m_page);
}

bool WebPagePopupImpl::initialize(WebViewImpl* webView, PagePopupClient* popupClient)
{
    ASSERT(webView);
    ASSERT(popupClient);
    m_webView = webView;
    m_popupClient = popupClient;

    if (!m_widgetClient || !initializePage())
        return false;
    m_widgetClient->show(WebNavigationPolicy());
    setFocus(true);

    return true;
}

bool WebPagePopupImpl::initializePage()
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_chromeClient = PagePopupChromeClient::create(this);
    pageClients.chromeClient = m_chromeClient.get();

    m_page = Page::create(pageClients);
    m_page->settings().setScriptEnabled(true);
    m_page->settings().setAllowScriptsToCloseWindows(true);
    m_page->settings().setDeviceSupportsTouch(m_webView->page()->settings().deviceSupportsTouch());
    // FIXME: Should we support enabling a11y while a popup is shown?
    m_page->settings().setAccessibilityEnabled(m_webView->page()->settings().accessibilityEnabled());
    m_page->settings().setScrollAnimatorEnabled(m_webView->page()->settings().scrollAnimatorEnabled());

    provideContextFeaturesTo(*m_page, adoptPtr(new PagePopupFeaturesClient()));
    DEFINE_STATIC_LOCAL(OwnPtrWillBePersistent<FrameLoaderClient>, emptyFrameLoaderClient, (EmptyFrameLoaderClient::create()));
    RefPtrWillBeRawPtr<LocalFrame> frame = LocalFrame::create(emptyFrameLoaderClient.get(), &m_page->frameHost(), 0);
    frame->setPagePopupOwner(m_popupClient->ownerElement());
    frame->setView(FrameView::create(frame.get()));
    frame->init();
    frame->view()->setParentVisible(true);
    frame->view()->setSelfVisible(true);
    frame->view()->setTransparent(false);
    if (AXObjectCache* cache = m_popupClient->ownerElement().document().existingAXObjectCache())
        cache->childrenChanged(&m_popupClient->ownerElement());

    ASSERT(frame->localDOMWindow());
    DOMWindowPagePopup::install(*frame->localDOMWindow(), *this, m_popupClient);
    ASSERT(m_popupClient->ownerElement().document().existingAXObjectCache() == frame->document()->existingAXObjectCache());

    RefPtr<SharedBuffer> data = SharedBuffer::create();
    m_popupClient->writeDocument(data.get());
    frame->loader().load(FrameLoadRequest(0, blankURL(), SubstituteData(data, "text/html", "UTF-8", KURL(), ForceSynchronousLoad)));
    frame->setPageZoomFactor(m_popupClient->zoomFactor());
    return true;
}

void WebPagePopupImpl::postMessage(const String& message)
{
    if (!m_page)
        return;
    ScriptForbiddenScope::AllowUserAgentScript allowScript;
    if (LocalDOMWindow* window = toLocalFrame(m_page->mainFrame())->localDOMWindow())
        window->dispatchEvent(MessageEvent::create(message));
}

void WebPagePopupImpl::destroyPage()
{
    if (!m_page)
        return;

    m_page->willBeDestroyed();
    m_page.clear();
}

AXObject* WebPagePopupImpl::rootAXObject()
{
    if (!m_page || !m_page->mainFrame())
        return 0;
    Document* document = toLocalFrame(m_page->mainFrame())->document();
    if (!document)
        return 0;
    AXObjectCache* cache = document->axObjectCache();
    ASSERT(cache);
    return toAXObjectCacheImpl(cache)->getOrCreate(document->layoutView());
}

void WebPagePopupImpl::setWindowRect(const IntRect& rectInScreen)
{
    m_windowRectInScreen = rectInScreen;
    widgetClient()->setWindowRect(m_windowRectInScreen);
}

void WebPagePopupImpl::setRootGraphicsLayer(GraphicsLayer* layer)
{
    m_rootGraphicsLayer = layer;
    m_rootLayer = layer ? layer->platformLayer() : 0;

    setIsAcceleratedCompositingActive(layer);
    if (m_layerTreeView) {
        if (m_rootLayer) {
            m_layerTreeView->setRootLayer(*m_rootLayer);
        } else {
            m_layerTreeView->clearRootLayer();
        }
    }
}

void WebPagePopupImpl::setIsAcceleratedCompositingActive(bool enter)
{
    if (m_isAcceleratedCompositingActive == enter)
        return;

    if (!enter) {
        m_isAcceleratedCompositingActive = false;
    } else if (m_layerTreeView) {
        m_isAcceleratedCompositingActive = true;
    } else {
        TRACE_EVENT0("blink", "WebPagePopupImpl::setIsAcceleratedCompositingActive(true)");

        m_widgetClient->initializeLayerTreeView();
        m_layerTreeView = m_widgetClient->layerTreeView();
        if (m_layerTreeView) {
            m_layerTreeView->setVisible(true);
            m_isAcceleratedCompositingActive = true;
            m_page->layerTreeViewInitialized(*m_layerTreeView);
        } else {
            m_isAcceleratedCompositingActive = false;
        }
    }
}

void WebPagePopupImpl::beginFrame(double lastFrameTimeMonotonic)
{
    if (!m_page)
        return;
    // FIXME: This should use lastFrameTimeMonotonic but doing so
    // breaks tests.
    PageWidgetDelegate::animate(*m_page, monotonicallyIncreasingTime());
}

void WebPagePopupImpl::willCloseLayerTreeView()
{
    if (m_page && m_layerTreeView)
        m_page->willCloseLayerTreeView(*m_layerTreeView);

    setIsAcceleratedCompositingActive(false);
    m_layerTreeView = 0;
}

void WebPagePopupImpl::updateAllLifecyclePhases()
{
    if (!m_page)
        return;
    PageWidgetDelegate::updateAllLifecyclePhases(*m_page, *m_page->deprecatedLocalMainFrame());
}

void WebPagePopupImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    if (!m_closing)
        PageWidgetDelegate::paint(*m_page, canvas, rect, *m_page->deprecatedLocalMainFrame());
}

void WebPagePopupImpl::resize(const WebSize& newSizeInViewport)
{
    WebRect newSize(0, 0, newSizeInViewport.width, newSizeInViewport.height);
    widgetClient()->convertViewportToWindow(&newSize);

    setWindowRect(WebRect(m_windowRectInScreen.x, m_windowRectInScreen.y, newSize.width, newSize.height));
    if (m_page) {
        toLocalFrame(m_page->mainFrame())->view()->resize(newSizeInViewport);
        m_page->frameHost().visualViewport().setSize(newSizeInViewport);
    }

    m_widgetClient->didInvalidateRect(WebRect(0, 0, newSize.width, newSize.height));
}

WebInputEventResult WebPagePopupImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    return handleKeyEvent(PlatformKeyboardEventBuilder(event));
}

WebInputEventResult WebPagePopupImpl::handleCharEvent(const WebKeyboardEvent& event)
{
    return handleKeyEvent(PlatformKeyboardEventBuilder(event));
}

WebInputEventResult WebPagePopupImpl::handleGestureEvent(const WebGestureEvent& event)
{
    if (m_closing || !m_page || !m_page->mainFrame() || !toLocalFrame(m_page->mainFrame())->view())
        return WebInputEventResult::NotHandled;
    if (event.type == WebInputEvent::GestureTap && !isViewportPointInWindow(event.x, event.y)) {
        cancel();
        return WebInputEventResult::NotHandled;
    }
    LocalFrame& frame = *toLocalFrame(m_page->mainFrame());
    return frame.eventHandler().handleGestureEvent(PlatformGestureEventBuilder(frame.view(), event));
}

void WebPagePopupImpl::handleMouseDown(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    if (isViewportPointInWindow(event.x, event.y))
        PageWidgetEventHandler::handleMouseDown(mainFrame, event);
    else
        cancel();
}

WebInputEventResult WebPagePopupImpl::handleMouseWheel(LocalFrame& mainFrame, const WebMouseWheelEvent& event)
{
    if (isViewportPointInWindow(event.x, event.y))
        return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
    cancel();
    return WebInputEventResult::NotHandled;
}

bool WebPagePopupImpl::isViewportPointInWindow(int x, int y)
{
    WebRect pointInWindow(x, y, 0, 0);
    widgetClient()->convertViewportToWindow(&pointInWindow);
    return IntRect(0, 0, m_windowRectInScreen.width, m_windowRectInScreen.height).contains(IntPoint(pointInWindow.x, pointInWindow.y));
}

WebInputEventResult WebPagePopupImpl::handleInputEvent(const WebInputEvent& event)
{
    if (m_closing)
        return WebInputEventResult::NotHandled;
    return PageWidgetDelegate::handleInputEvent(*this, event, m_page->deprecatedLocalMainFrame());
}

WebInputEventResult WebPagePopupImpl::handleKeyEvent(const PlatformKeyboardEvent& event)
{
    if (m_closing || !m_page->mainFrame() || !toLocalFrame(m_page->mainFrame())->view())
        return WebInputEventResult::NotHandled;
    return toLocalFrame(m_page->mainFrame())->eventHandler().keyEvent(event);
}

void WebPagePopupImpl::setFocus(bool enable)
{
    if (!m_page)
        return;
    m_page->focusController().setFocused(enable);
    if (enable)
        m_page->focusController().setActive(true);
}

void WebPagePopupImpl::close()
{
    m_closing = true;
    // In case closePopup() was not called.
    if (m_page) {
        destroyPage();
        m_popupClient->didClosePopup();
        m_webView->cleanupPagePopup();
    }
    m_widgetClient = 0;
    deref();
}

void WebPagePopupImpl::closePopup()
{
    // This function can be called in EventDispatchForbiddenScope for the main
    // document, and the following operations dispatch some events.  It's safe
    // because web authors can't listen the events.
    EventDispatchForbiddenScope::AllowUserAgentEvents allowEvents;

    if (m_page) {
        toLocalFrame(m_page->mainFrame())->loader().stopAllLoaders();
        ASSERT(m_page->deprecatedLocalMainFrame()->localDOMWindow());
        DOMWindowPagePopup::uninstall(*m_page->deprecatedLocalMainFrame()->localDOMWindow());
    }
    m_closing = true;

    destroyPage();

    // m_widgetClient might be 0 because this widget might be already closed.
    if (m_widgetClient) {
        // closeWidgetSoon() will call this->close() later.
        m_widgetClient->closeWidgetSoon();
    }

    m_popupClient->didClosePopup();
}

LocalDOMWindow* WebPagePopupImpl::window()
{
    return m_page->deprecatedLocalMainFrame()->localDOMWindow();
}

void WebPagePopupImpl::layoutAndPaintAsync(WebLayoutAndPaintAsyncCallback* callback)
{
    m_layerTreeView->layoutAndPaintAsync(callback);
}

void WebPagePopupImpl::compositeAndReadbackAsync(WebCompositeAndReadbackAsyncCallback* callback)
{
    ASSERT(isAcceleratedCompositingActive());
    m_layerTreeView->compositeAndReadbackAsync(callback);
}

WebPoint WebPagePopupImpl::positionRelativeToOwner()
{
    WebRect windowRect = m_webView->client()->rootWindowRect();
    return WebPoint(m_windowRectInScreen.x - windowRect.x, m_windowRectInScreen.y - windowRect.y);
}

void WebPagePopupImpl::cancel()
{
    if (m_popupClient)
        m_popupClient->closePopup();
}

// WebPagePopup ----------------------------------------------------------------

WebPagePopup* WebPagePopup::create(WebWidgetClient* client)
{
    if (!client)
        CRASH();
    // A WebPagePopupImpl instance usually has two references.
    //  - One owned by the instance itself. It represents the visible widget.
    //  - One owned by a WebViewImpl. It's released when the WebViewImpl ask the
    //    WebPagePopupImpl to close.
    // We need them because the closing operation is asynchronous and the widget
    // can be closed while the WebViewImpl is unaware of it.
    return adoptRef(new WebPagePopupImpl(client)).leakRef();
}

} // namespace blink
