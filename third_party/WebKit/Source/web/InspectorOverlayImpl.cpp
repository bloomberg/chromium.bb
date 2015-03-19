/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "web/InspectorOverlayImpl.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8InspectorOverlayHost.h"
#include "core/dom/Node.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectorOverlayHost.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/Chrome.h"
#include "core/page/EventHandler.h"
#include "core/page/Page.h"
#include "platform/JSONValues.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/graphics/GraphicsContext.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "web/WebGraphicsContextImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include <v8.h>

namespace OverlayZOrders {
// Use 99 as a big z-order number so that highlight is above other overlays.
static const int highlight = 99;
}

namespace blink {

namespace {

class InspectorOverlayChromeClient final: public EmptyChromeClient {
public:
    InspectorOverlayChromeClient(ChromeClient& client, InspectorOverlayImpl* overlay)
        : m_client(client)
        , m_overlay(overlay)
    { }

    virtual void setCursor(const Cursor& cursor) override
    {
        m_client.setCursor(cursor);
    }

    virtual void setToolTip(const String& tooltip, TextDirection direction) override
    {
        m_client.setToolTip(tooltip, direction);
    }

    virtual void invalidateRect(const IntRect&) override
    {
        m_overlay->invalidate();
    }

private:
    ChromeClient& m_client;
    InspectorOverlayImpl* m_overlay;
};

} // anonymous namespace

InspectorOverlayImpl::InspectorOverlayImpl(WebViewImpl* webViewImpl)
    : m_webViewImpl(webViewImpl)
    , m_inspectModeEnabled(false)
    , m_overlayHost(InspectorOverlayHost::create())
    , m_drawViewSize(false)
    , m_drawViewSizeWithGrid(false)
    , m_omitTooltip(false)
    , m_timer(this, &InspectorOverlayImpl::onTimer)
    , m_suspendCount(0)
    , m_updating(false)
{
    m_overlayHost->setListener(this);
}

InspectorOverlayImpl::~InspectorOverlayImpl()
{
    ASSERT(!m_overlayPage);
}

DEFINE_TRACE(InspectorOverlayImpl)
{
    visitor->trace(m_highlightNode);
    visitor->trace(m_eventTargetNode);
    visitor->trace(m_overlayPage);
    visitor->trace(m_overlayHost);
    visitor->trace(m_listener);
    InspectorOverlay::trace(visitor);
}

void InspectorOverlayImpl::paintPageOverlay(WebGraphicsContext* context, const WebSize& webViewSize)
{
    if (isEmpty())
        return;

    GraphicsContext& graphicsContext = toWebGraphicsContextImpl(context)->graphicsContext();
    FrameView* view = overlayMainFrame()->view();
    ASSERT(!view->needsLayout());
    view->paint(&graphicsContext, IntRect(0, 0, view->width(), view->height()));
}

void InspectorOverlayImpl::invalidate()
{
    // Don't invalidate during an update, because that will lead to Document::scheduleRenderTreeUpdate
    // being called within Document::updateRenderTree which violates document lifecycle expectations.
    if (m_updating)
        return;

    m_webViewImpl->addPageOverlay(this, OverlayZOrders::highlight);
}

bool InspectorOverlayImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    if (isEmpty())
        return false;

    if (WebInputEvent::isGestureEventType(inputEvent.type) && inputEvent.type == WebInputEvent::GestureTap) {
        // Only let GestureTab in (we only need it and we know PlatformGestureEventBuilder supports it).
        PlatformGestureEvent gestureEvent = PlatformGestureEventBuilder(m_webViewImpl->mainFrameImpl()->frameView(), static_cast<const WebGestureEvent&>(inputEvent));
        overlayMainFrame()->eventHandler().handleGestureEvent(gestureEvent);
    }
    if (WebInputEvent::isMouseEventType(inputEvent.type) && inputEvent.type != WebInputEvent::MouseEnter) {
        // PlatformMouseEventBuilder does not work with MouseEnter type, so we filter it out manually.
        PlatformMouseEvent mouseEvent = PlatformMouseEventBuilder(m_webViewImpl->mainFrameImpl()->frameView(), static_cast<const WebMouseEvent&>(inputEvent));
        if (mouseEvent.type() == PlatformEvent::MouseMoved)
            overlayMainFrame()->eventHandler().handleMouseMoveEvent(mouseEvent);
        if (mouseEvent.type() == PlatformEvent::MousePressed)
            overlayMainFrame()->eventHandler().handleMousePressEvent(mouseEvent);
        if (mouseEvent.type() == PlatformEvent::MouseReleased)
            overlayMainFrame()->eventHandler().handleMouseReleaseEvent(mouseEvent);
    }
    if (WebInputEvent::isTouchEventType(inputEvent.type)) {
        PlatformTouchEvent touchEvent = PlatformTouchEventBuilder(m_webViewImpl->mainFrameImpl()->frameView(), static_cast<const WebTouchEvent&>(inputEvent));
        overlayMainFrame()->eventHandler().handleTouchEvent(touchEvent);
    }
    if (WebInputEvent::isKeyboardEventType(inputEvent.type)) {
        PlatformKeyboardEvent keyboardEvent = PlatformKeyboardEventBuilder(static_cast<const WebKeyboardEvent&>(inputEvent));
        overlayMainFrame()->eventHandler().keyEvent(keyboardEvent);
    }

    // Overlay should not consume events.
    return false;
}

void InspectorOverlayImpl::setPausedInDebuggerMessage(const String* message)
{
    m_pausedInDebuggerMessage = message ? *message : String();
    update();
}

void InspectorOverlayImpl::setInspectModeEnabled(bool enabled)
{
    m_inspectModeEnabled = enabled;
    update();
}

void InspectorOverlayImpl::hideHighlight()
{
    m_highlightNode.clear();
    m_eventTargetNode.clear();
    m_highlightQuad.clear();
    update();
}

void InspectorOverlayImpl::highlightNode(Node* node, Node* eventTarget, const InspectorHighlightConfig& highlightConfig, bool omitTooltip)
{
    m_nodeHighlightConfig = highlightConfig;
    m_highlightNode = node;
    m_eventTargetNode = eventTarget;
    m_omitTooltip = omitTooltip;
    update();
}

void InspectorOverlayImpl::highlightQuad(PassOwnPtr<FloatQuad> quad, const InspectorHighlightConfig& highlightConfig)
{
    m_quadHighlightConfig = highlightConfig;
    m_highlightQuad = quad;
    m_omitTooltip = false;
    update();
}

void InspectorOverlayImpl::showAndHideViewSize(bool showGrid)
{
    m_drawViewSize = true;
    m_drawViewSizeWithGrid = showGrid;
    update();
    m_timer.startOneShot(1, FROM_HERE);
}

bool InspectorOverlayImpl::isEmpty()
{
    if (m_suspendCount)
        return true;
    bool hasAlwaysVisibleElements = m_highlightNode || m_eventTargetNode || m_highlightQuad  || m_drawViewSize;
    bool hasInvisibleInInspectModeElements = !m_pausedInDebuggerMessage.isNull();
    return !(hasAlwaysVisibleElements || (hasInvisibleInInspectModeElements && !m_inspectModeEnabled));
}

void InspectorOverlayImpl::update()
{
    TemporaryChange<bool> scoped(m_updating, true);

    if (isEmpty()) {
        m_webViewImpl->removePageOverlay(this);
        return;
    }

    FrameView* view = m_webViewImpl->mainFrameImpl()->frameView();
    if (!view)
        return;

    // Include scrollbars to avoid masking them by the gutter.
    IntSize size = view->unscaledVisibleContentSize(IncludeScrollbars);
    toLocalFrame(overlayPage()->mainFrame())->view()->resize(size);

    // Clear canvas and paint things.
    IntRect viewRect = view->visibleContentRect();
    reset(size, viewRect.x(), viewRect.y());

    drawNodeHighlight();
    drawQuadHighlight();
    if (!m_inspectModeEnabled)
        drawPausedInDebuggerMessage();
    drawViewSize();

    toLocalFrame(overlayPage()->mainFrame())->view()->updateLayoutAndStyleForPainting();

    m_webViewImpl->addPageOverlay(this, OverlayZOrders::highlight);
}

static PassRefPtr<JSONObject> buildObjectForSize(const IntSize& size)
{
    RefPtr<JSONObject> result = JSONObject::create();
    result->setNumber("width", size.width());
    result->setNumber("height", size.height());
    return result.release();
}

void InspectorOverlayImpl::drawNodeHighlight()
{
    if (!m_highlightNode)
        return;

    bool appendElementInfo = m_highlightNode->isElementNode() && !m_omitTooltip && m_nodeHighlightConfig.showInfo && m_highlightNode->layoutObject() && m_highlightNode->document().frame();
    OwnPtrWillBeRawPtr<InspectorHighlight> highlight = InspectorHighlight::create(m_highlightNode.get(), m_nodeHighlightConfig, appendElementInfo);
    if (m_eventTargetNode)
        highlight->appendEventTargetQuads(m_eventTargetNode.get(), m_nodeHighlightConfig);
    evaluateInOverlay("drawHighlight", highlight->asJSONObject());
}

void InspectorOverlayImpl::drawQuadHighlight()
{
    if (!m_highlightQuad)
        return;

    OwnPtrWillBeRawPtr<InspectorHighlight> highlight = InspectorHighlight::create();
    highlight->appendQuad(*m_highlightQuad, m_quadHighlightConfig.content, m_quadHighlightConfig.contentOutline);
    evaluateInOverlay("drawHighlight", highlight->asJSONObject());
}

void InspectorOverlayImpl::drawPausedInDebuggerMessage()
{
    if (!m_pausedInDebuggerMessage.isNull())
        evaluateInOverlay("drawPausedInDebuggerMessage", m_pausedInDebuggerMessage);
}

void InspectorOverlayImpl::drawViewSize()
{
    if (m_drawViewSize)
        evaluateInOverlay("drawViewSize", m_drawViewSizeWithGrid ? "true" : "false");
}

Page* InspectorOverlayImpl::overlayPage()
{
    if (m_overlayPage)
        return m_overlayPage.get();

    ScriptForbiddenScope::AllowUserAgentScript allowScript;

    static FrameLoaderClient* dummyFrameLoaderClient =  new EmptyFrameLoaderClient;
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    ASSERT(!m_overlayChromeClient);
    m_overlayChromeClient = adoptPtr(new InspectorOverlayChromeClient(m_webViewImpl->page()->chrome().client(), this));
    pageClients.chromeClient = m_overlayChromeClient.get();
    m_overlayPage = adoptPtrWillBeNoop(new Page(pageClients));

    Settings& settings = m_webViewImpl->page()->settings();
    Settings& overlaySettings = m_overlayPage->settings();

    overlaySettings.genericFontFamilySettings().updateStandard(settings.genericFontFamilySettings().standard());
    overlaySettings.genericFontFamilySettings().updateSerif(settings.genericFontFamilySettings().serif());
    overlaySettings.genericFontFamilySettings().updateSansSerif(settings.genericFontFamilySettings().sansSerif());
    overlaySettings.genericFontFamilySettings().updateCursive(settings.genericFontFamilySettings().cursive());
    overlaySettings.genericFontFamilySettings().updateFantasy(settings.genericFontFamilySettings().fantasy());
    overlaySettings.genericFontFamilySettings().updatePictograph(settings.genericFontFamilySettings().pictograph());
    overlaySettings.setMinimumFontSize(settings.minimumFontSize());
    overlaySettings.setMinimumLogicalFontSize(settings.minimumLogicalFontSize());
    overlaySettings.setScriptEnabled(true);
    overlaySettings.setPluginsEnabled(false);
    overlaySettings.setLoadsImagesAutomatically(true);
    // FIXME: http://crbug.com/363843. Inspector should probably create its
    // own graphics layers and attach them to the tree rather than going
    // through some non-composited paint function.
    overlaySettings.setAcceleratedCompositingEnabled(false);

    RefPtrWillBeRawPtr<LocalFrame> frame = LocalFrame::create(dummyFrameLoaderClient, &m_overlayPage->frameHost(), 0);
    frame->setView(FrameView::create(frame.get()));
    frame->init();
    FrameLoader& loader = frame->loader();
    frame->view()->setCanHaveScrollbars(false);
    frame->view()->setTransparent(true);

    const blink::WebData& overlayPageHTMLResource = blink::Platform::current()->loadResource("InspectorOverlayPage.html");
    RefPtr<SharedBuffer> data = SharedBuffer::create(overlayPageHTMLResource.data(), overlayPageHTMLResource.size());
    loader.load(FrameLoadRequest(0, blankURL(), SubstituteData(data, "text/html", "UTF-8", KURL(), ForceSynchronousLoad)));
    v8::Isolate* isolate = toIsolate(frame.get());
    ScriptState* scriptState = ScriptState::forMainWorld(frame.get());
    ASSERT(scriptState->contextIsValid());
    ScriptState::Scope scope(scriptState);
    v8::Local<v8::Object> global = scriptState->context()->Global();
    v8::Local<v8::Value> overlayHostObj = toV8(m_overlayHost.get(), global, isolate);
    global->Set(v8::String::NewFromUtf8(isolate, "InspectorOverlayHost"), overlayHostObj);

#if OS(WIN)
    evaluateInOverlay("setPlatform", "windows");
#elif OS(MACOSX)
    evaluateInOverlay("setPlatform", "mac");
#elif OS(POSIX)
    evaluateInOverlay("setPlatform", "linux");
#endif

    return m_overlayPage.get();
}

LocalFrame* InspectorOverlayImpl::overlayMainFrame()
{
    return toLocalFrame(overlayPage()->mainFrame());
}

void InspectorOverlayImpl::reset(const IntSize& viewportSize, int scrollX, int scrollY)
{
    RefPtr<JSONObject> resetData = JSONObject::create();
    resetData->setNumber("pageScaleFactor", m_webViewImpl->page()->settings().pinchVirtualViewportEnabled() ? 1 : m_webViewImpl->page()->pageScaleFactor());
    resetData->setNumber("deviceScaleFactor", m_webViewImpl->page()->deviceScaleFactor());
    resetData->setObject("viewportSize", buildObjectForSize(viewportSize));
    resetData->setNumber("pageZoomFactor", m_webViewImpl->mainFrameImpl()->frame()->pageZoomFactor());
    resetData->setNumber("scrollX", scrollX);
    resetData->setNumber("scrollY", scrollY);
    evaluateInOverlay("reset", resetData.release());
}

void InspectorOverlayImpl::evaluateInOverlay(const String& method, const String& argument)
{
    ScriptForbiddenScope::AllowUserAgentScript allowScript;
    RefPtr<JSONArray> command = JSONArray::create();
    command->pushString(method);
    command->pushString(argument);
    toLocalFrame(overlayPage()->mainFrame())->script().executeScriptInMainWorld("dispatch(" + command->toJSONString() + ")", ScriptController::ExecuteScriptWhenScriptsDisabled);
}

void InspectorOverlayImpl::evaluateInOverlay(const String& method, PassRefPtr<JSONValue> argument)
{
    ScriptForbiddenScope::AllowUserAgentScript allowScript;
    RefPtr<JSONArray> command = JSONArray::create();
    command->pushString(method);
    command->pushValue(argument);
    toLocalFrame(overlayPage()->mainFrame())->script().executeScriptInMainWorld("dispatch(" + command->toJSONString() + ")", ScriptController::ExecuteScriptWhenScriptsDisabled);
}

void InspectorOverlayImpl::onTimer(Timer<InspectorOverlayImpl>*)
{
    m_drawViewSize = false;
    update();
}

void InspectorOverlayImpl::clear()
{
    if (m_overlayPage) {
        m_overlayPage->willBeDestroyed();
        m_overlayPage.clear();
        m_overlayChromeClient.clear();
    }
    m_drawViewSize = false;
    m_pausedInDebuggerMessage = String();
    m_inspectModeEnabled = false;
    m_timer.stop();
    hideHighlight();
}

void InspectorOverlayImpl::overlayResumed()
{
    if (m_listener)
        m_listener->overlayResumed();
}

void InspectorOverlayImpl::overlaySteppedOver()
{
    if (m_listener)
        m_listener->overlaySteppedOver();
}

void InspectorOverlayImpl::suspendUpdates()
{
    if (!m_suspendCount++)
        clear();
}

void InspectorOverlayImpl::resumeUpdates()
{
    --m_suspendCount;
}

} // namespace blink
