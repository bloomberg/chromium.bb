/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
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

#ifndef WebPluginContainerImpl_h
#define WebPluginContainerImpl_h

#include "core/frame/FrameDestructionObserver.h"
#include "core/plugins/PluginView.h"
#include "platform/Widget.h"
#include "public/web/WebPluginContainer.h"

#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

struct NPObject;

namespace blink {
class GestureEvent;
class HTMLPlugInElement;
class IntRect;
class KeyboardEvent;
class MouseEvent;
class PlatformGestureEvent;
class ResourceError;
class ResourceResponse;
class TouchEvent;
class WheelEvent;
class Widget;
}

namespace blink {

struct WebPrintParams;

class ScrollbarGroup;
class WebPlugin;
class WebPluginLoadObserver;
class WebExternalTextureLayer;

class WebPluginContainerImpl FINAL : public blink::PluginView, public WebPluginContainer, public blink::FrameDestructionObserver {
public:
    static PassRefPtr<WebPluginContainerImpl> create(blink::HTMLPlugInElement* element, WebPlugin* webPlugin)
    {
        return adoptRef(new WebPluginContainerImpl(element, webPlugin));
    }

    // PluginView methods
    virtual WebLayer* platformLayer() const OVERRIDE;
    virtual v8::Local<v8::Object> scriptableObject(v8::Isolate*) OVERRIDE;
    virtual bool getFormValue(String&) OVERRIDE;
    virtual bool supportsKeyboardFocus() const OVERRIDE;
    virtual bool supportsInputMethod() const OVERRIDE;
    virtual bool canProcessDrag() const OVERRIDE;
    virtual bool wantsWheelEvents() OVERRIDE;

    // Widget methods
    virtual void setFrameRect(const blink::IntRect&) OVERRIDE;
    virtual void paint(blink::GraphicsContext*, const blink::IntRect&) OVERRIDE;
    virtual void invalidateRect(const blink::IntRect&) OVERRIDE;
    virtual void setFocus(bool) OVERRIDE;
    virtual void show() OVERRIDE;
    virtual void hide() OVERRIDE;
    virtual void handleEvent(blink::Event*) OVERRIDE;
    virtual void frameRectsChanged() OVERRIDE;
    virtual void setParentVisible(bool) OVERRIDE;
    virtual void setParent(blink::Widget*) OVERRIDE;
    virtual void widgetPositionsUpdated() OVERRIDE;
    virtual bool isPluginContainer() const OVERRIDE { return true; }
    virtual void eventListenersRemoved() OVERRIDE;
    virtual bool pluginShouldPersist() const OVERRIDE;

    // WebPluginContainer methods
    virtual WebElement element() OVERRIDE;
    virtual void invalidate() OVERRIDE;
    virtual void invalidateRect(const WebRect&) OVERRIDE;
    virtual void scrollRect(const WebRect&) OVERRIDE;
    virtual void reportGeometry() OVERRIDE;
    virtual void allowScriptObjects() OVERRIDE;
    virtual void clearScriptObjects() OVERRIDE;
    virtual NPObject* scriptableObjectForElement() OVERRIDE;
    virtual v8::Local<v8::Object> v8ObjectForElement() OVERRIDE;
    virtual WebString executeScriptURL(const WebURL&, bool popupsAllowed) OVERRIDE;
    virtual void loadFrameRequest(const WebURLRequest&, const WebString& target, bool notifyNeeded, void* notifyData) OVERRIDE;
    virtual void zoomLevelChanged(double zoomLevel) OVERRIDE;
    virtual bool isRectTopmost(const WebRect&) OVERRIDE;
    virtual void requestTouchEventType(TouchEventRequestType) OVERRIDE;
    virtual void setWantsWheelEvents(bool) OVERRIDE;
    virtual WebPoint windowToLocalPoint(const WebPoint&) OVERRIDE;
    virtual WebPoint localToWindowPoint(const WebPoint&) OVERRIDE;

    // This cannot be null.
    virtual WebPlugin* plugin() OVERRIDE { return m_webPlugin; }
    virtual void setPlugin(WebPlugin*) OVERRIDE;

    virtual float deviceScaleFactor() OVERRIDE;
    virtual float pageScaleFactor() OVERRIDE;
    virtual float pageZoomFactor() OVERRIDE;

    virtual void setWebLayer(WebLayer*);

    // Printing interface. The plugin can support custom printing
    // (which means it controls the layout, number of pages etc).
    // Whether the plugin supports its own paginated print. The other print
    // interface methods are called only if this method returns true.
    bool supportsPaginatedPrint() const;
    // If the plugin content should not be scaled to the printable area of
    // the page, then this method should return true.
    bool isPrintScalingDisabled() const;
    // Returns number of copies to be printed.
    int getCopiesToPrint() const;
    // Sets up printing at the specified WebPrintParams. Returns the number of pages to be printed at these settings.
    int printBegin(const WebPrintParams&) const;
    // Prints the page specified by pageNumber (0-based index) into the supplied canvas.
    bool printPage(int pageNumber, blink::GraphicsContext* gc);
    // Ends the print operation.
    void printEnd();

    // Copy the selected text.
    void copy();

    // Pass the edit command to the plugin.
    bool executeEditCommand(const WebString& name);
    bool executeEditCommand(const WebString& name, const WebString& value);

    // Resource load events for the plugin's source data:
    virtual void didReceiveResponse(const blink::ResourceResponse&) OVERRIDE;
    virtual void didReceiveData(const char *data, int dataLength) OVERRIDE;
    virtual void didFinishLoading() OVERRIDE;
    virtual void didFailLoading(const blink::ResourceError&) OVERRIDE;

    void willDestroyPluginLoadObserver(WebPluginLoadObserver*);

    ScrollbarGroup* scrollbarGroup();

    void willStartLiveResize();
    void willEndLiveResize();

    bool paintCustomOverhangArea(blink::GraphicsContext*, const blink::IntRect&, const blink::IntRect&, const blink::IntRect&);

#if ENABLE(OILPAN)
    virtual void detach() OVERRIDE;
#endif

private:
    WebPluginContainerImpl(blink::HTMLPlugInElement* element, WebPlugin* webPlugin);
    virtual ~WebPluginContainerImpl();

    void handleMouseEvent(blink::MouseEvent*);
    void handleDragEvent(blink::MouseEvent*);
    void handleWheelEvent(blink::WheelEvent*);
    void handleKeyboardEvent(blink::KeyboardEvent*);
    void handleTouchEvent(blink::TouchEvent*);
    void handleGestureEvent(blink::GestureEvent*);

    void synthesizeMouseEventIfPossible(blink::TouchEvent*);

    void focusPlugin();

    void calculateGeometry(const blink::IntRect& frameRect,
                           blink::IntRect& windowRect,
                           blink::IntRect& clipRect,
                           Vector<blink::IntRect>& cutOutRects);
    blink::IntRect windowClipRect() const;
    void windowCutOutRects(const blink::IntRect& frameRect,
                           Vector<blink::IntRect>& cutOutRects);

    // FIXME: Oilpan: consider moving Widget to the heap and turn this
    // into a traced member. For the time being, it is a bare pointer
    // of its owning PlugInElement and managed as such.
    blink::HTMLPlugInElement* m_element;
    WebPlugin* m_webPlugin;
    Vector<WebPluginLoadObserver*> m_pluginLoadObservers;

    WebLayer* m_webLayer;

    // The associated scrollbar group object, created lazily. Used for Pepper
    // scrollbars.
    OwnPtr<ScrollbarGroup> m_scrollbarGroup;

    TouchEventRequestType m_touchEventRequestType;
    bool m_wantsWheelEvents;
};

DEFINE_TYPE_CASTS(WebPluginContainerImpl, blink::Widget, widget, widget->isPluginContainer(), widget.isPluginContainer());
// Unlike Widget, we need not worry about object type for container.
// WebPluginContainerImpl is the only subclass of WebPluginContainer.
DEFINE_TYPE_CASTS(WebPluginContainerImpl, WebPluginContainer, container, true, true);

} // namespace blink

#endif
