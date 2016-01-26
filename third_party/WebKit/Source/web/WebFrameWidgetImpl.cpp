/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "web/WebFrameWidgetImpl.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/PlainTextRange.h"
#include "core/frame/FrameView.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/ContextMenuController.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "platform/KeyboardCodes.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/web/WebWidgetClient.h"
#include "web/ContextMenuAllowedScope.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "web/WebViewFrameWidget.h"

namespace blink {

// WebFrameWidget ----------------------------------------------------------------

WebFrameWidget* WebFrameWidget::create(WebWidgetClient* client, WebLocalFrame* localRoot)
{
    // Pass the WebFrameWidget's self-reference to the caller.
    return WebFrameWidgetImpl::create(client, localRoot);
}

WebFrameWidget* WebFrameWidget::create(WebWidgetClient* client, WebView* webView, WebLocalFrame* mainFrame)
{
    return new WebViewFrameWidget(client, toWebViewImpl(*webView), toWebLocalFrameImpl(*mainFrame));
}

WebFrameWidgetImpl* WebFrameWidgetImpl::create(WebWidgetClient* client, WebLocalFrame* localRoot)
{
    // Pass the WebFrameWidgetImpl's self-reference to the caller.
#if ENABLE(OILPAN)
    return new WebFrameWidgetImpl(client, localRoot); // SelfKeepAlive is set in constructor.
#else
    return adoptRef(new WebFrameWidgetImpl(client, localRoot)).leakRef();
#endif
}

// static
WebFrameWidgetsSet& WebFrameWidgetImpl::allInstances()
{
    DEFINE_STATIC_LOCAL(WebFrameWidgetsSet, allInstances, ());
    return allInstances;
}

WebFrameWidgetImpl::WebFrameWidgetImpl(WebWidgetClient* client, WebLocalFrame* localRoot)
    : m_client(client)
    , m_localRoot(toWebLocalFrameImpl(localRoot))
    , m_layerTreeView(nullptr)
    , m_rootLayer(nullptr)
    , m_rootGraphicsLayer(nullptr)
    , m_isAcceleratedCompositingActive(false)
    , m_layerTreeViewClosed(false)
    , m_suppressNextKeypressEvent(false)
    , m_ignoreInputEvents(false)
#if ENABLE(OILPAN)
    , m_selfKeepAlive(this)
#endif
{
    ASSERT(m_localRoot->frame()->isLocalRoot());
    initializeLayerTreeView();
    m_localRoot->setFrameWidget(this);
    allInstances().add(this);
}

WebFrameWidgetImpl::~WebFrameWidgetImpl()
{
}

DEFINE_TRACE(WebFrameWidgetImpl)
{
    visitor->trace(m_localRoot);
    visitor->trace(m_mouseCaptureNode);
}

// WebWidget ------------------------------------------------------------------

void WebFrameWidgetImpl::close()
{
    WebDevToolsAgentImpl::webFrameWidgetImplClosed(this);
    ASSERT(allInstances().contains(this));
    allInstances().remove(this);

    m_localRoot->setFrameWidget(nullptr);
    m_localRoot = nullptr;
    // Reset the delegate to prevent notifications being sent as we're being
    // deleted.
    m_client = nullptr;

    m_layerTreeView = nullptr;
    m_rootLayer = nullptr;
    m_rootGraphicsLayer = nullptr;

#if ENABLE(OILPAN)
    m_selfKeepAlive.clear();
#else
    deref(); // Balances ref() acquired in WebFrameWidget::create
#endif
}

WebSize WebFrameWidgetImpl::size()
{
    return m_size;
}

void WebFrameWidgetImpl::willStartLiveResize()
{
    if (m_localRoot->frameView())
        m_localRoot->frameView()->willStartLiveResize();
}

void WebFrameWidgetImpl::resize(const WebSize& newSize)
{
    if (m_size == newSize)
        return;

    FrameView* view = m_localRoot->frameView();
    if (!view)
        return;

    m_size = newSize;

    updateMainFrameLayoutSize();

    view->resize(m_size);

    // FIXME: In WebViewImpl this layout was a precursor to setting the minimum scale limit.
    // It is not clear if this is necessary for frame-level widget resize.
    if (view->needsLayout())
        view->layout();

    // FIXME: Investigate whether this is needed; comment from eseidel suggests that this function
    // is flawed.
    sendResizeEventAndRepaint();
}

void WebFrameWidgetImpl::sendResizeEventAndRepaint()
{
    // FIXME: This is wrong. The FrameView is responsible sending a resizeEvent
    // as part of layout. Layout is also responsible for sending invalidations
    // to the embedder. This method and all callers may be wrong. -- eseidel.
    if (m_localRoot->frameView()) {
        // Enqueues the resize event.
        m_localRoot->frame()->document()->enqueueResizeEvent();
    }

    if (m_client) {
        if (isAcceleratedCompositingActive()) {
            updateLayerTreeViewport();
        } else {
            WebRect damagedRect(0, 0, m_size.width, m_size.height);
            m_client->didInvalidateRect(damagedRect);
        }
    }
}

void WebFrameWidgetImpl::resizeVisualViewport(const WebSize& newSize)
{
    // FIXME: Implement visual viewport for out-of-process iframes.
}

void WebFrameWidgetImpl::updateMainFrameLayoutSize()
{
    if (!m_localRoot)
        return;

    RefPtrWillBeRawPtr<FrameView> view = m_localRoot->frameView();
    if (!view)
        return;

    WebSize layoutSize = m_size;

    view->setLayoutSize(layoutSize);
}

void WebFrameWidgetImpl::setIgnoreInputEvents(bool newValue)
{
    ASSERT(m_ignoreInputEvents != newValue);
    m_ignoreInputEvents = newValue;
}

void WebFrameWidgetImpl::willEndLiveResize()
{
    if (m_localRoot->frameView())
        m_localRoot->frameView()->willEndLiveResize();
}

void WebFrameWidgetImpl::didEnterFullScreen()
{
    // FIXME: Implement full screen for out-of-process iframes.
}

void WebFrameWidgetImpl::didExitFullScreen()
{
    // FIXME: Implement full screen for out-of-process iframes.
}

void WebFrameWidgetImpl::beginFrame(double lastFrameTimeMonotonic)
{
    TRACE_EVENT1("blink", "WebFrameWidgetImpl::beginFrame", "frameTime", lastFrameTimeMonotonic);
    ASSERT(lastFrameTimeMonotonic);
    PageWidgetDelegate::animate(*page(), lastFrameTimeMonotonic);
}

void WebFrameWidgetImpl::updateAllLifecyclePhases()
{
    TRACE_EVENT0("blink", "WebFrameWidgetImpl::updateAllLifecyclePhases");
    if (!m_localRoot)
        return;

    PageWidgetDelegate::updateAllLifecyclePhases(*page(), *m_localRoot->frame());
    updateLayerTreeBackgroundColor();
}

void WebFrameWidgetImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    // Out-of-process iframes require compositing.
    ASSERT_NOT_REACHED();
}


void WebFrameWidgetImpl::updateLayerTreeViewport()
{
    if (!page() || !m_layerTreeView)
        return;

    // FIXME: We need access to page scale information from the WebView.
    m_layerTreeView->setPageScaleFactorAndLimits(1, 1, 1);
}

void WebFrameWidgetImpl::updateLayerTreeBackgroundColor()
{
    if (!m_layerTreeView)
        return;

    m_layerTreeView->setBackgroundColor(alphaChannel(view()->backgroundColorOverride()) ? view()->backgroundColorOverride() : view()->backgroundColor());
}

void WebFrameWidgetImpl::updateLayerTreeDeviceScaleFactor()
{
    ASSERT(page());
    ASSERT(m_layerTreeView);

    float deviceScaleFactor = page()->deviceScaleFactor();
    m_layerTreeView->setDeviceScaleFactor(deviceScaleFactor);
}

bool WebFrameWidgetImpl::isTransparent() const
{
    // FIXME: This might need to proxy to the WebView's isTransparent().
    return false;
}

void WebFrameWidgetImpl::layoutAndPaintAsync(WebLayoutAndPaintAsyncCallback* callback)
{
    m_layerTreeView->layoutAndPaintAsync(callback);
}

void WebFrameWidgetImpl::compositeAndReadbackAsync(WebCompositeAndReadbackAsyncCallback* callback)
{
    m_layerTreeView->compositeAndReadbackAsync(callback);
}

void WebFrameWidgetImpl::themeChanged()
{
    FrameView* view = m_localRoot->frameView();

    WebRect damagedRect(0, 0, m_size.width, m_size.height);
    view->invalidateRect(damagedRect);
}

const WebInputEvent* WebFrameWidgetImpl::m_currentInputEvent = nullptr;

WebInputEventResult WebFrameWidgetImpl::handleInputEvent(const WebInputEvent& inputEvent)
{

    TRACE_EVENT1("input", "WebFrameWidgetImpl::handleInputEvent", "type", inputTypeToName(inputEvent.type));

    // Report the event to be NOT processed by WebKit, so that the browser can handle it appropriately.
    if (m_ignoreInputEvents)
        return WebInputEventResult::NotHandled;

    // FIXME: pass event to m_localRoot's WebDevToolsAgentImpl once available.

    TemporaryChange<const WebInputEvent*> currentEventChange(m_currentInputEvent, &inputEvent);

    if (m_mouseCaptureNode && WebInputEvent::isMouseEventType(inputEvent.type)) {
        TRACE_EVENT1("input", "captured mouse event", "type", inputEvent.type);
        // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
        RefPtrWillBeRawPtr<Node> node = m_mouseCaptureNode;

        // Not all platforms call mouseCaptureLost() directly.
        if (inputEvent.type == WebInputEvent::MouseUp)
            mouseCaptureLost();

        OwnPtr<UserGestureIndicator> gestureIndicator;

        AtomicString eventType;
        switch (inputEvent.type) {
        case WebInputEvent::MouseMove:
            eventType = EventTypeNames::mousemove;
            break;
        case WebInputEvent::MouseLeave:
            eventType = EventTypeNames::mouseout;
            break;
        case WebInputEvent::MouseDown:
            eventType = EventTypeNames::mousedown;
            gestureIndicator = adoptPtr(new UserGestureIndicator(DefinitelyProcessingNewUserGesture));
            m_mouseCaptureGestureToken = gestureIndicator->currentToken();
            break;
        case WebInputEvent::MouseUp:
            eventType = EventTypeNames::mouseup;
            gestureIndicator = adoptPtr(new UserGestureIndicator(m_mouseCaptureGestureToken.release()));
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        node->dispatchMouseEvent(
            PlatformMouseEventBuilder(m_localRoot->frameView(), static_cast<const WebMouseEvent&>(inputEvent)),
            eventType, static_cast<const WebMouseEvent&>(inputEvent).clickCount);
        return WebInputEventResult::HandledSystem;
    }

    return PageWidgetDelegate::handleInputEvent(*this, inputEvent, m_localRoot->frame());
}

void WebFrameWidgetImpl::setCursorVisibilityState(bool isVisible)
{
    page()->setIsCursorVisible(isVisible);
}

bool WebFrameWidgetImpl::hasTouchEventHandlersAt(const WebPoint& point)
{
    // FIXME: Implement this. Note that the point must be divided by pageScaleFactor.
    return true;
}

void WebFrameWidgetImpl::scheduleAnimation()
{
    if (m_layerTreeView) {
        m_layerTreeView->setNeedsBeginFrame();
        return;
    }
    if (m_client)
        m_client->scheduleAnimation();
}

void WebFrameWidgetImpl::applyViewportDeltas(
    const WebFloatSize& visualViewportDelta,
    const WebFloatSize& mainFrameDelta,
    const WebFloatSize& elasticOverscrollDelta,
    float pageScaleDelta,
    float topControlsDelta)
{
    // FIXME: To be implemented.
}

void WebFrameWidgetImpl::mouseCaptureLost()
{
    TRACE_EVENT_ASYNC_END0("input", "capturing mouse", this);
    m_mouseCaptureNode = nullptr;
}

void WebFrameWidgetImpl::setFocus(bool enable)
{
    page()->focusController().setFocused(enable);
    if (enable) {
        page()->focusController().setActive(true);
        RefPtrWillBeRawPtr<LocalFrame> focusedFrame = page()->focusController().focusedFrame();
        if (focusedFrame) {
            Element* element = focusedFrame->document()->focusedElement();
            if (element && focusedFrame->selection().selection().isNone()) {
                // If the selection was cleared while the WebView was not
                // focused, then the focus element shows with a focus ring but
                // no caret and does respond to keyboard inputs.
                if (element->isTextFormControl()) {
                    element->updateFocusAppearance(SelectionBehaviorOnFocus::Restore);
                } else if (element->isContentEditable()) {
                    // updateFocusAppearance() selects all the text of
                    // contentseditable DIVs. So we set the selection explicitly
                    // instead. Note that this has the side effect of moving the
                    // caret back to the beginning of the text.
                    Position position(element, 0);
                    focusedFrame->selection().setSelection(VisibleSelection(position, SelDefaultAffinity));
                }
            }
        }
    }
}

bool WebFrameWidgetImpl::setComposition(
    const WebString& text,
    const WebVector<WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd)
{
    // FIXME: To be implemented.
    return false;
}

bool WebFrameWidgetImpl::confirmComposition()
{
    // FIXME: To be implemented.
    return false;
}

bool WebFrameWidgetImpl::confirmComposition(ConfirmCompositionBehavior selectionBehavior)
{
    // FIXME: To be implemented.
    return false;
}

bool WebFrameWidgetImpl::confirmComposition(const WebString& text)
{
    // FIXME: To be implemented.
    return false;
}

bool WebFrameWidgetImpl::compositionRange(size_t* location, size_t* length)
{
    // FIXME: To be implemented.
    return false;
}

WebTextInputInfo WebFrameWidgetImpl::textInputInfo()
{
    return view()->textInputInfo();
}

WebTextInputType WebFrameWidgetImpl::textInputType()
{
    return view()->textInputType();
}

WebColor WebFrameWidgetImpl::backgroundColor() const
{
    if (isTransparent())
        return Color::transparent;
    if (!m_localRoot->frameView())
        return view()->backgroundColor();
    FrameView* view = m_localRoot->frameView();
    return view->documentBackgroundColor().rgb();
}

bool WebFrameWidgetImpl::selectionBounds(WebRect& anchor, WebRect& focus) const
{
    const Frame* frame = focusedCoreFrame();
    if (!frame || !frame->isLocalFrame())
        return false;

    const LocalFrame* localFrame = toLocalFrame(frame);
    if (!localFrame)
        return false;
    FrameSelection& selection = localFrame->selection();

    if (selection.isCaret()) {
        anchor = focus = selection.absoluteCaretBounds();
    } else {
        const EphemeralRange selectedRange = selection.selection().toNormalizedEphemeralRange();
        if (selectedRange.isNull())
            return false;
        anchor = localFrame->editor().firstRectForRange(EphemeralRange(selectedRange.startPosition()));
        focus = localFrame->editor().firstRectForRange(EphemeralRange(selectedRange.endPosition()));
    }

    // FIXME: This doesn't apply page scale. This should probably be contents to viewport. crbug.com/459293.
    IntRect scaledAnchor(localFrame->view()->contentsToRootFrame(anchor));
    IntRect scaledFocus(localFrame->view()->contentsToRootFrame(focus));

    anchor = scaledAnchor;
    focus = scaledFocus;

    if (!selection.selection().isBaseFirst())
        std::swap(anchor, focus);
    return true;
}

bool WebFrameWidgetImpl::selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const
{
    if (!focusedCoreFrame()->isLocalFrame())
        return false;
    const LocalFrame* frame = toLocalFrame(focusedCoreFrame());
    if (!frame)
        return false;
    FrameSelection& selection = frame->selection();
    if (selection.selection().toNormalizedEphemeralRange().isNull())
        return false;
    start = toWebTextDirection(primaryDirectionOf(*selection.start().anchorNode()));
    end = toWebTextDirection(primaryDirectionOf(*selection.end().anchorNode()));
    return true;
}

bool WebFrameWidgetImpl::isSelectionAnchorFirst() const
{
    if (!focusedCoreFrame()->isLocalFrame())
        return false;
    if (const LocalFrame* frame = toLocalFrame(focusedCoreFrame()))
        return frame->selection().selection().isBaseFirst();
    return false;
}

bool WebFrameWidgetImpl::caretOrSelectionRange(size_t* location, size_t* length)
{
    if (!focusedCoreFrame()->isLocalFrame())
        return false;
    const LocalFrame* focused = toLocalFrame(focusedCoreFrame());
    if (!focused)
        return false;

    PlainTextRange selectionOffsets = focused->inputMethodController().getSelectionOffsets();
    if (selectionOffsets.isNull())
        return false;

    *location = selectionOffsets.start();
    *length = selectionOffsets.length();
    return true;
}

void WebFrameWidgetImpl::setTextDirection(WebTextDirection direction)
{
    // The Editor::setBaseWritingDirection() function checks if we can change
    // the text direction of the selected node and updates its DOM "dir"
    // attribute and its CSS "direction" property.
    // So, we just call the function as Safari does.
    if (!focusedCoreFrame()->isLocalFrame())
        return;
    const LocalFrame* focused = toLocalFrame(focusedCoreFrame());
    if (!focused)
        return;

    Editor& editor = focused->editor();
    if (!editor.canEdit())
        return;

    switch (direction) {
    case WebTextDirectionDefault:
        editor.setBaseWritingDirection(NaturalWritingDirection);
        break;

    case WebTextDirectionLeftToRight:
        editor.setBaseWritingDirection(LeftToRightWritingDirection);
        break;

    case WebTextDirectionRightToLeft:
        editor.setBaseWritingDirection(RightToLeftWritingDirection);
        break;

    default:
        notImplemented();
        break;
    }
}

bool WebFrameWidgetImpl::isAcceleratedCompositingActive() const
{
    return m_isAcceleratedCompositingActive;
}

void WebFrameWidgetImpl::willCloseLayerTreeView()
{
    if (m_layerTreeView)
        page()->willCloseLayerTreeView(*m_layerTreeView);

    setIsAcceleratedCompositingActive(false);
    m_layerTreeView = nullptr;
    m_layerTreeViewClosed = true;
}

void WebFrameWidgetImpl::didChangeWindowResizerRect()
{
    if (m_localRoot->frameView())
        m_localRoot->frameView()->windowResizerRectChanged();
}

void WebFrameWidgetImpl::handleMouseLeave(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    // FIXME: WebWidget doesn't have the method below.
    // m_client->setMouseOverURL(WebURL());
    PageWidgetEventHandler::handleMouseLeave(mainFrame, event);
}

void WebFrameWidgetImpl::handleMouseDown(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    // Take capture on a mouse down on a plugin so we can send it mouse events.
    // If the hit node is a plugin but a scrollbar is over it don't start mouse
    // capture because it will interfere with the scrollbar receiving events.
    IntPoint point(event.x, event.y);
    if (event.button == WebMouseEvent::ButtonLeft) {
        point = m_localRoot->frameView()->rootFrameToContents(point);
        HitTestResult result(m_localRoot->frame()->eventHandler().hitTestResultAtPoint(point));
        result.setToShadowHostIfInUserAgentShadowRoot();
        Node* hitNode = result.innerNode();

        if (!result.scrollbar() && hitNode && hitNode->layoutObject() && hitNode->layoutObject()->isEmbeddedObject()) {
            m_mouseCaptureNode = hitNode;
            TRACE_EVENT_ASYNC_BEGIN0("input", "capturing mouse", this);
        }
    }

    PageWidgetEventHandler::handleMouseDown(mainFrame, event);

    if (event.button == WebMouseEvent::ButtonLeft && m_mouseCaptureNode)
        m_mouseCaptureGestureToken = mainFrame.eventHandler().takeLastMouseDownGestureToken();

    // Dispatch the contextmenu event regardless of if the click was swallowed.
    if (!page()->settings().showContextMenuOnMouseUp()) {
#if OS(MACOSX)
        if (event.button == WebMouseEvent::ButtonRight || (event.button == WebMouseEvent::ButtonLeft && event.modifiers & WebMouseEvent::ControlKey))
            mouseContextMenu(event);
#else
        if (event.button == WebMouseEvent::ButtonRight)
            mouseContextMenu(event);
#endif
    }
}

void WebFrameWidgetImpl::mouseContextMenu(const WebMouseEvent& event)
{
    page()->contextMenuController().clearContextMenu();

    PlatformMouseEventBuilder pme(m_localRoot->frameView(), event);

    // Find the right target frame. See issue 1186900.
    HitTestResult result = hitTestResultForRootFramePos(pme.position());
    Frame* targetFrame;
    if (result.innerNodeOrImageMapImage())
        targetFrame = result.innerNodeOrImageMapImage()->document().frame();
    else
        targetFrame = page()->focusController().focusedOrMainFrame();

    // This will need to be changed to a nullptr check when focus control
    // is refactored, at which point focusedOrMainFrame will never return a
    // RemoteFrame.
    // See https://crbug.com/341918.
    if (!targetFrame->isLocalFrame())
        return;

    LocalFrame* targetLocalFrame = toLocalFrame(targetFrame);

#if OS(WIN)
    targetLocalFrame->view()->setCursor(pointerCursor());
#endif

    {
        ContextMenuAllowedScope scope;
        targetLocalFrame->eventHandler().sendContextMenuEvent(pme, nullptr);
    }
    // Actually showing the context menu is handled by the ContextMenuClient
    // implementation...
}

void WebFrameWidgetImpl::handleMouseUp(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    PageWidgetEventHandler::handleMouseUp(mainFrame, event);

    if (page()->settings().showContextMenuOnMouseUp()) {
        // Dispatch the contextmenu event regardless of if the click was swallowed.
        // On Mac/Linux, we handle it on mouse down, not up.
        if (event.button == WebMouseEvent::ButtonRight)
            mouseContextMenu(event);
    }
}

WebInputEventResult WebFrameWidgetImpl::handleMouseWheel(LocalFrame& mainFrame, const WebMouseWheelEvent& event)
{
    return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
}

WebInputEventResult WebFrameWidgetImpl::handleGestureEvent(const WebGestureEvent& event)
{
    WebInputEventResult eventResult = WebInputEventResult::NotHandled;
    bool eventCancelled = false;
    switch (event.type) {
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureShowPress:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureDoubleTap:
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap:
        break;
    case WebInputEvent::GestureFlingStart:
    case WebInputEvent::GestureFlingCancel:
        m_client->didHandleGestureEvent(event, eventCancelled);
        return WebInputEventResult::NotHandled;
    default:
        ASSERT_NOT_REACHED();
    }
    LocalFrame* frame = m_localRoot->frame();
    eventResult = frame->eventHandler().handleGestureEvent(PlatformGestureEventBuilder(frame->view(), event));
    m_client->didHandleGestureEvent(event, eventCancelled);
    return eventResult;
}

WebInputEventResult WebFrameWidgetImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    ASSERT((event.type == WebInputEvent::RawKeyDown)
        || (event.type == WebInputEvent::KeyDown)
        || (event.type == WebInputEvent::KeyUp));

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.
    // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
    // Webkit. A keyDown event is typically associated with a keyPress(char)
    // event and a keyUp event. We reset this flag here as this is a new keyDown
    // event.
    m_suppressNextKeypressEvent = false;

    RefPtrWillBeRawPtr<Frame> focusedFrame = focusedCoreFrame();
    if (focusedFrame && focusedFrame->isRemoteFrame()) {
        WebRemoteFrameImpl* webFrame = WebRemoteFrameImpl::fromFrame(*toRemoteFrame(focusedFrame.get()));
        webFrame->client()->forwardInputEvent(&event);
        return WebInputEventResult::HandledSystem;
    }

    if (!focusedFrame || !focusedFrame->isLocalFrame())
        return WebInputEventResult::NotHandled;

    RefPtrWillBeRawPtr<LocalFrame> frame = toLocalFrame(focusedFrame.get());

    PlatformKeyboardEventBuilder evt(event);

    WebInputEventResult result = frame->eventHandler().keyEvent(evt);
    if (result != WebInputEventResult::NotHandled) {
        if (WebInputEvent::RawKeyDown == event.type) {
            // Suppress the next keypress event unless the focused node is a plugin node.
            // (Flash needs these keypress events to handle non-US keyboards.)
            Element* element = focusedElement();
            if (!element || !element->layoutObject() || !element->layoutObject()->isEmbeddedObject())
                m_suppressNextKeypressEvent = true;
        }
        return result;
    }

#if !OS(MACOSX)
    const WebInputEvent::Type contextMenuTriggeringEventType =
#if OS(WIN)
        WebInputEvent::KeyUp;
#else
        WebInputEvent::RawKeyDown;
#endif

    bool isUnmodifiedMenuKey = !(event.modifiers & WebInputEvent::InputModifiers) && event.windowsKeyCode == VKEY_APPS;
    bool isShiftF10 = event.modifiers == WebInputEvent::ShiftKey && event.windowsKeyCode == VKEY_F10;
    if ((isUnmodifiedMenuKey || isShiftF10) && event.type == contextMenuTriggeringEventType) {
        view()->sendContextMenuEvent(event);
        return WebInputEventResult::HandledSystem;
    }
#endif // !OS(MACOSX)

    return keyEventDefault(event);
}

WebInputEventResult WebFrameWidgetImpl::handleCharEvent(const WebKeyboardEvent& event)
{
    ASSERT(event.type == WebInputEvent::Char);

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
    // handled by Webkit. A keyDown event is typically associated with a
    // keyPress(char) event and a keyUp event. We reset this flag here as it
    // only applies to the current keyPress event.
    bool suppress = m_suppressNextKeypressEvent;
    m_suppressNextKeypressEvent = false;

    LocalFrame* frame = toLocalFrame(focusedCoreFrame());
    if (!frame)
        return suppress ? WebInputEventResult::HandledSuppressed : WebInputEventResult::NotHandled;

    EventHandler& handler = frame->eventHandler();

    PlatformKeyboardEventBuilder evt(event);
    if (!evt.isCharacterKey())
        return WebInputEventResult::HandledSuppressed;

    // Accesskeys are triggered by char events and can't be suppressed.
    // It is unclear whether a keypress should be dispatched as well
    // crbug.com/563507
    if (handler.handleAccessKey(evt))
        return WebInputEventResult::HandledSystem;

    // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
    // the eventHandler::keyEvent. We mimic this behavior on all platforms since
    // for now we are converting other platform's key events to windows key
    // events.
    if (evt.isSystemKey())
        return WebInputEventResult::NotHandled;

    if (suppress)
        return WebInputEventResult::HandledSuppressed;

    WebInputEventResult result = handler.keyEvent(evt);
    if (result != WebInputEventResult::NotHandled)
        return result;

    return keyEventDefault(event);
}

WebInputEventResult WebFrameWidgetImpl::keyEventDefault(const WebKeyboardEvent& event)
{
    LocalFrame* frame = toLocalFrame(focusedCoreFrame());
    if (!frame)
        return WebInputEventResult::NotHandled;

    switch (event.type) {
    case WebInputEvent::Char:
        if (event.windowsKeyCode == VKEY_SPACE) {
            int keyCode = ((event.modifiers & WebInputEvent::ShiftKey) ? VKEY_PRIOR : VKEY_NEXT);
            return scrollViewWithKeyboard(keyCode, event.modifiers);
        }
        break;
    case WebInputEvent::RawKeyDown:
        if (event.modifiers == WebInputEvent::ControlKey) {
            switch (event.windowsKeyCode) {
#if !OS(MACOSX)
            case 'A':
                WebFrame::fromFrame(focusedCoreFrame())->executeCommand(WebString::fromUTF8("SelectAll"));
                return WebInputEventResult::HandledSystem;
            case VKEY_INSERT:
            case 'C':
                WebFrame::fromFrame(focusedCoreFrame())->executeCommand(WebString::fromUTF8("Copy"));
                return WebInputEventResult::HandledSystem;
#endif
            // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
            // key combinations which affect scrolling. Safari is buggy in the
            // sense that it scrolls the page for all Ctrl+scrolling key
            // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
            case VKEY_HOME:
            case VKEY_END:
                break;
            default:
                return WebInputEventResult::NotHandled;
            }
        }
        if (!event.isSystemKey && !(event.modifiers & WebInputEvent::ShiftKey))
            return scrollViewWithKeyboard(event.windowsKeyCode, event.modifiers);
        break;
    default:
        break;
    }
    return WebInputEventResult::NotHandled;
}

WebInputEventResult WebFrameWidgetImpl::scrollViewWithKeyboard(int keyCode, int modifiers)
{
    ScrollDirection scrollDirection;
    ScrollGranularity scrollGranularity;
#if OS(MACOSX)
    // Control-Up/Down should be PageUp/Down on Mac.
    if (modifiers & WebMouseEvent::ControlKey) {
        if (keyCode == VKEY_UP)
            keyCode = VKEY_PRIOR;
        else if (keyCode == VKEY_DOWN)
            keyCode = VKEY_NEXT;
    }
#endif
    if (!mapKeyCodeForScroll(keyCode, &scrollDirection, &scrollGranularity))
        return WebInputEventResult::NotHandled;

    LocalFrame* frame = toLocalFrame(focusedCoreFrame());
    if (frame && frame->eventHandler().bubblingScroll(scrollDirection, scrollGranularity))
        return WebInputEventResult::HandledSystem;
    return WebInputEventResult::NotHandled;
}

bool WebFrameWidgetImpl::mapKeyCodeForScroll(
    int keyCode,
    ScrollDirection* scrollDirection,
    ScrollGranularity* scrollGranularity)
{
    switch (keyCode) {
    case VKEY_LEFT:
        *scrollDirection = ScrollLeftIgnoringWritingMode;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_RIGHT:
        *scrollDirection = ScrollRightIgnoringWritingMode;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_UP:
        *scrollDirection = ScrollUpIgnoringWritingMode;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_DOWN:
        *scrollDirection = ScrollDownIgnoringWritingMode;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_HOME:
        *scrollDirection = ScrollUpIgnoringWritingMode;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_END:
        *scrollDirection = ScrollDownIgnoringWritingMode;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_PRIOR: // page up
        *scrollDirection = ScrollUpIgnoringWritingMode;
        *scrollGranularity = ScrollByPage;
        break;
    case VKEY_NEXT: // page down
        *scrollDirection = ScrollDownIgnoringWritingMode;
        *scrollGranularity = ScrollByPage;
        break;
    default:
        return false;
    }

    return true;
}

Frame* WebFrameWidgetImpl::focusedCoreFrame() const
{
    return page() ? page()->focusController().focusedOrMainFrame() : nullptr;
}

Element* WebFrameWidgetImpl::focusedElement() const
{
    LocalFrame* frame = page()->focusController().focusedFrame();
    if (!frame)
        return nullptr;

    Document* document = frame->document();
    if (!document)
        return nullptr;

    return document->focusedElement();
}

void WebFrameWidgetImpl::initializeLayerTreeView()
{
    if (m_client) {
        m_client->initializeLayerTreeView();
        m_layerTreeView = m_client->layerTreeView();
    }

    if (WebDevToolsAgentImpl* devTools = m_localRoot->devToolsAgentImpl())
        devTools->layerTreeViewChanged(m_layerTreeView);

    page()->settings().setAcceleratedCompositingEnabled(m_layerTreeView);
    if (m_layerTreeView)
        page()->layerTreeViewInitialized(*m_layerTreeView);

    // FIXME: only unittests, click to play, Android priting, and printing (for headers and footers)
    // make this assert necessary. We should make them not hit this code and then delete allowsBrokenNullLayerTreeView.
    ASSERT(m_layerTreeView || !m_client || m_client->allowsBrokenNullLayerTreeView());
}

void WebFrameWidgetImpl::setIsAcceleratedCompositingActive(bool active)
{
    // In the middle of shutting down; don't try to spin back up a compositor.
    // FIXME: compositing startup/shutdown should be refactored so that it
    // turns on explicitly rather than lazily, which causes this awkwardness.
    if (m_layerTreeViewClosed)
        return;

    ASSERT(!active || m_layerTreeView);

    if (m_isAcceleratedCompositingActive == active)
        return;

    if (!m_client)
        return;

    if (active) {
        TRACE_EVENT0("blink", "WebViewImpl::setIsAcceleratedCompositingActive(true)");
        m_layerTreeView->setRootLayer(*m_rootLayer);

        m_layerTreeView->setVisible(page()->isPageVisible());
        updateLayerTreeDeviceScaleFactor();
        updateLayerTreeBackgroundColor();
        m_layerTreeView->setHasTransparentBackground(isTransparent());
        updateLayerTreeViewport();
        m_isAcceleratedCompositingActive = true;
    }
}

PaintLayerCompositor* WebFrameWidgetImpl::compositor() const
{
    LocalFrame* frame = m_localRoot->frame();
    if (!frame || !frame->document() || !frame->document()->layoutView())
        return nullptr;

    return frame->document()->layoutView()->compositor();
}

void WebFrameWidgetImpl::setRootGraphicsLayer(GraphicsLayer* layer)
{
    m_rootGraphicsLayer = layer;
    m_rootLayer = layer ? layer->platformLayer() : nullptr;

    setIsAcceleratedCompositingActive(layer);

    if (!m_layerTreeView)
        return;

    if (m_rootLayer)
        m_layerTreeView->setRootLayer(*m_rootLayer);
    else
        m_layerTreeView->clearRootLayer();
}

void WebFrameWidgetImpl::attachCompositorAnimationTimeline(WebCompositorAnimationTimeline* compositorTimeline)
{
    if (m_layerTreeView)
        m_layerTreeView->attachCompositorAnimationTimeline(compositorTimeline);

}

void WebFrameWidgetImpl::detachCompositorAnimationTimeline(WebCompositorAnimationTimeline* compositorTimeline)
{
    if (m_layerTreeView)
        m_layerTreeView->detachCompositorAnimationTimeline(compositorTimeline);
}

void WebFrameWidgetImpl::setVisibilityState(WebPageVisibilityState visibilityState, bool isInitialState)
{
    if (!page())
        return;

    // FIXME: This is not correct, since Show and Hide messages for a frame's Widget do not necessarily
    // correspond to Page visibility, but is necessary until we properly sort out OOPIF visibility.
    page()->setVisibilityState(static_cast<PageVisibilityState>(visibilityState), isInitialState);

    m_localRoot->frame()->frameScheduler()->setPageVisible(visibilityState == WebPageVisibilityStateVisible);

    if (m_layerTreeView) {
        bool visible = visibilityState == WebPageVisibilityStateVisible;
        m_layerTreeView->setVisible(visible);
    }
}

HitTestResult WebFrameWidgetImpl::hitTestResultForRootFramePos(const IntPoint& posInRootFrame)
{
    IntPoint docPoint(m_localRoot->frame()->view()->rootFrameToContents(posInRootFrame));
    HitTestResult result = m_localRoot->frame()->eventHandler().hitTestResultAtPoint(docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
    result.setToShadowHostIfInUserAgentShadowRoot();
    return result;
}

} // namespace blink
