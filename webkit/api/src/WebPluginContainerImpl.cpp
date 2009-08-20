/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebPluginContainerImpl.h"

#include "TemporaryGlue.h"
#include "WebCursorInfo.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebPlugin.h"
#include "WebRect.h"
#include "WebURLError.h"
#include "WebVector.h"
#include "WrappedResourceResponse.h"

#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "Page.h"
#include "ScrollView.h"

#if WEBKIT_USING_SKIA
#include "PlatformContextSkia.h"
#endif

// FIXME: Remove this dependency on glue!
#include "webkit/glue/stacking_order_iterator.h"

using namespace WebCore;

namespace WebKit {

// Public methods --------------------------------------------------------------

void WebPluginContainerImpl::setFrameRect(const IntRect& frameRect)
{
    Widget::setFrameRect(frameRect);

    if (!parent() || !isVisible())
        return;

    IntRect windowRect, clipRect;
    Vector<IntRect> cutOutRects;
    calculateGeometry(frameRect, windowRect, clipRect, cutOutRects);

    m_webPlugin->updateGeometry(windowRect, clipRect, cutOutRects);
}

void WebPluginContainerImpl::paint(GraphicsContext* gc, const IntRect& damageRect)
{
    if (gc->paintingDisabled())
        return;

    if (!parent())
        return;

    // Don't paint anything if the plugin doesn't intersect the damage rect.
    if (!frameRect().intersects(damageRect))
        return;

    gc->save();

    ASSERT(parent()->isFrameView());
    ScrollView* view = parent();

    // The plugin is positioned in window coordinates, so it needs to be painted
    // in window coordinates.
    IntPoint origin = view->windowToContents(IntPoint(0, 0));
    gc->translate(static_cast<float>(origin.x()), static_cast<float>(origin.y()));

#if WEBKIT_USING_SKIA
    WebCanvas* canvas = gc->platformContext()->canvas();
#elif WEBKIT_USING_CG
    WebCanvas* canvas = gc->platformContext();
#endif

    IntRect windowRect =
        IntRect(view->contentsToWindow(damageRect.location()), damageRect.size());
    m_webPlugin->paint(canvas, windowRect);

    gc->restore();
}

void WebPluginContainerImpl::invalidateRect(const IntRect& rect)
{
    if (!parent())
        return;

    IntRect damageRect = convertToContainingWindow(rect);

    // Get our clip rect and intersect with it to ensure we don't invalidate
    // too much.
    IntRect clipRect = parent()->windowClipRect();
    damageRect.intersect(clipRect);

    parent()->hostWindow()->repaint(damageRect, true);
}

void WebPluginContainerImpl::setFocus()
{
    Widget::setFocus();
    m_webPlugin->updateFocus(true);
}

void WebPluginContainerImpl::show()
{
    setSelfVisible(true);
    m_webPlugin->updateVisibility(true);

    Widget::show();
}

void WebPluginContainerImpl::hide()
{
    setSelfVisible(false);
    m_webPlugin->updateVisibility(false);

    Widget::hide();
}

void WebPluginContainerImpl::handleEvent(Event* event)
{
    if (!m_webPlugin->acceptsInputEvents())
        return;

    // The events we pass are defined at:
    //    http://devedge-temp.mozilla.org/library/manuals/2002/plugin/1.0/structures5.html#1000000
    // Don't take the documentation as truth, however.  There are many cases
    // where mozilla behaves differently than the spec.
    if (event->isMouseEvent())
        handleMouseEvent(static_cast<MouseEvent*>(event));
    else if (event->isKeyboardEvent())
        handleKeyboardEvent(static_cast<KeyboardEvent*>(event));
}

void WebPluginContainerImpl::frameRectsChanged()
{
    Widget::frameRectsChanged();

    // This is a hack to tickle re-positioning of the plugin in the case where
    // our parent view was scrolled.
    setFrameRect(frameRect());
}

void WebPluginContainerImpl::setParentVisible(bool parentVisible)
{
    // We override this function to make sure that geometry updates are sent
    // over to the plugin. For e.g. when a plugin is instantiated it does not
    // have a valid parent. As a result the first geometry update from webkit
    // is ignored. This function is called when the plugin eventually gets a
    // parent.

    if (isParentVisible() == parentVisible)
        return;  // No change.

    Widget::setParentVisible(parentVisible);
    if (!isSelfVisible())
        return;  // This widget has explicitely been marked as not visible.

    m_webPlugin->updateVisibility(isVisible());
}

void WebPluginContainerImpl::setParent(ScrollView* view)
{
    // We override this function so that if the plugin is windowed, we can call
    // NPP_SetWindow at the first possible moment.  This ensures that
    // NPP_SetWindow is called before the manual load data is sent to a plugin.
    // If this order is reversed, Flash won't load videos.

    Widget::setParent(view);
    if (view)
        setFrameRect(frameRect());
}

void WebPluginContainerImpl::invalidate()
{
    Widget::invalidate();
}

void WebPluginContainerImpl::invalidateRect(const WebRect& rect)
{
    invalidateRect(static_cast<IntRect>(rect));
}

NPObject* WebPluginContainerImpl::scriptableObjectForElement()
{
    return m_element->getNPObject();
}

void WebPluginContainerImpl::didReceiveResponse(const ResourceResponse& response)
{
    // Make sure that the plugin receives window geometry before data, or else
    // plugins misbehave.
    frameRectsChanged();

    WrappedResourceResponse urlResponse(response);
    m_webPlugin->didReceiveResponse(urlResponse);
}

void WebPluginContainerImpl::didReceiveData(const char *data, int dataLength)
{
    m_webPlugin->didReceiveData(data, dataLength);
}

void WebPluginContainerImpl::didFinishLoading()
{
    m_webPlugin->didFinishLoading();
}

void WebPluginContainerImpl::didFailLoading(const ResourceError& error)
{
    m_webPlugin->didFailLoading(error);
}

NPObject* WebPluginContainerImpl::scriptableObject()
{
    return m_webPlugin->scriptableObject();
}

// Private methods -------------------------------------------------------------

WebPluginContainerImpl::~WebPluginContainerImpl()
{
    m_webPlugin->destroy();
}

void WebPluginContainerImpl::handleMouseEvent(MouseEvent* event)
{
    ASSERT(parent()->isFrameView());

    // We cache the parent FrameView here as the plugin widget could be deleted
    // in the call to HandleEvent. See http://b/issue?id=1362948
    FrameView* parentView = static_cast<FrameView*>(parent());

    WebMouseEventBuilder webEvent(parentView, *event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    if (event->type() == eventNames().mousedownEvent) {
        // Ensure that the frame containing the plugin has focus.
        Frame* containingFrame = parentView->frame();
        if (Page* currentPage = containingFrame->page())
            currentPage->focusController()->setFocusedFrame(containingFrame);
        // Give focus to our containing HTMLPluginElement.
        containingFrame->document()->setFocusedNode(m_element);
    }

    // TODO(pkasting): http://b/1119691 This conditional seems exactly
    // backwards, but it matches Safari's code, and if I reverse it, giving
    // focus to a transparent (windowless) plugin fails.
    WebCursorInfo cursorInfo;
    if (!m_webPlugin->handleInputEvent(webEvent, cursorInfo))
        event->setDefaultHandled();

    // A windowless plugin can change the cursor in response to a mouse move
    // event.  We need to reflect the changed cursor in the frame view as the
    // mouse is moved in the boundaries of the windowless plugin.
    TemporaryGlue::setCursorForPlugin(cursorInfo, parentView->frame());
}

void WebPluginContainerImpl::handleKeyboardEvent(KeyboardEvent* event)
{
    WebKeyboardEventBuilder webEvent(*event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    // TODO(pkasting): http://b/1119691 See above.
    WebCursorInfo cursor_info;
    if (!m_webPlugin->handleInputEvent(webEvent, cursor_info))
        event->setDefaultHandled();
}

void WebPluginContainerImpl::calculateGeometry(const IntRect& frameRect,
                                               IntRect& windowRect,
                                               IntRect& clipRect,
                                               Vector<IntRect>& cutOutRects)
{
    windowRect = IntRect(
        parent()->contentsToWindow(frameRect.location()), frameRect.size());

    // Calculate a clip-rect so that we don't overlap the scrollbars, etc.
    clipRect = windowClipRect();
    clipRect.move(-windowRect.x(), -windowRect.y());

    windowCutOutRects(frameRect, cutOutRects);
    // Convert to the plugin position.
    for (size_t i = 0; i < cutOutRects.size(); i++)
        cutOutRects[i].move(-frameRect.x(), -frameRect.y());
}

WebCore::IntRect WebPluginContainerImpl::windowClipRect() const
{
    // Start by clipping to our bounds.
    IntRect clipRect =
        convertToContainingWindow(IntRect(0, 0, width(), height()));

    // document()->renderer() can be NULL when we receive messages from the
    // plugins while we are destroying a frame.
    if (m_element->renderer()->document()->renderer()) {
        // Take our element and get the clip rect from the enclosing layer and
        // frame view.
        RenderLayer* layer = m_element->renderer()->enclosingLayer();
        clipRect.intersect(
            m_element->document()->view()->windowClipRectForLayer(layer, true));
    }

    return clipRect;
}

void WebPluginContainerImpl::windowCutOutRects(const IntRect& frameRect,
                                               Vector<IntRect>& cutOutRects)
{
    RenderObject* pluginRenderObject = m_element->renderer();
    ASSERT(pluginRenderObject);

    // Find all iframes that stack higher than this plugin.
    bool higher = false;
    StackingOrderIterator iterator;
    RenderLayer* root = m_element->document()->renderer()->enclosingLayer();
    iterator.Reset(frameRect, root);

    while (RenderObject* renderObject = iterator.Next()) {
        if (renderObject == pluginRenderObject)
            higher = true; // All nodes after this one are higher than plugin.
        else if (higher) {
            // Is this a visible iframe?
            Node* node = renderObject->node();
            if (node && node->hasTagName(HTMLNames::iframeTag)) {
                if (!renderObject->style() || renderObject->style()->visibility() == VISIBLE) {
                    IntPoint point = roundedIntPoint(renderObject->localToAbsolute());
                    RenderBox* rbox = toRenderBox(renderObject);
                    IntSize size(rbox->width(), rbox->height());
                    cutOutRects.append(IntRect(point, size));
                }
            }
        }
    }
}

} // namespace WebKit
