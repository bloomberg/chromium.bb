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
class ScrollbarGroup;
class TouchEvent;
class WebPlugin;
class WebPluginLoadObserver;
class WebExternalTextureLayer;
class WheelEvent;
class Widget;
struct WebPrintParams;

class WebPluginContainerImpl FINAL
    : public PluginView
    , public WebPluginContainer
#if !ENABLE(OILPAN)
    , public FrameDestructionObserver
#endif
    {
public:
    static PassRefPtr<WebPluginContainerImpl> create(HTMLPlugInElement* element, WebPlugin* webPlugin)
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
    virtual void setFrameRect(const IntRect&) OVERRIDE;
    virtual void paint(GraphicsContext*, const IntRect&) OVERRIDE;
    virtual void invalidateRect(const IntRect&) OVERRIDE;
    virtual void setFocus(bool) OVERRIDE;
    virtual void show() OVERRIDE;
    virtual void hide() OVERRIDE;
    virtual void handleEvent(Event*) OVERRIDE;
    virtual void frameRectsChanged() OVERRIDE;
    virtual void setParentVisible(bool) OVERRIDE;
    virtual void setParent(Widget*) OVERRIDE;
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
    bool printPage(int pageNumber, GraphicsContext*);
    // Ends the print operation.
    void printEnd();

    // Copy the selected text.
    void copy();

    // Pass the edit command to the plugin.
    bool executeEditCommand(const WebString& name);
    bool executeEditCommand(const WebString& name, const WebString& value);

    // Resource load events for the plugin's source data:
    virtual void didReceiveResponse(const ResourceResponse&) OVERRIDE;
    virtual void didReceiveData(const char *data, int dataLength) OVERRIDE;
    virtual void didFinishLoading() OVERRIDE;
    virtual void didFailLoading(const ResourceError&) OVERRIDE;

    void willDestroyPluginLoadObserver(WebPluginLoadObserver*);

    ScrollbarGroup* scrollbarGroup();

    void willStartLiveResize();
    void willEndLiveResize();

    bool paintCustomOverhangArea(GraphicsContext*, const IntRect&, const IntRect&, const IntRect&);

#if ENABLE(OILPAN)
    virtual void detach() OVERRIDE;
#endif

private:
    WebPluginContainerImpl(HTMLPlugInElement*, WebPlugin*);
    virtual ~WebPluginContainerImpl();

    void handleMouseEvent(MouseEvent*);
    void handleDragEvent(MouseEvent*);
    void handleWheelEvent(WheelEvent*);
    void handleKeyboardEvent(KeyboardEvent*);
    void handleTouchEvent(TouchEvent*);
    void handleGestureEvent(GestureEvent*);

    void synthesizeMouseEventIfPossible(TouchEvent*);

    void focusPlugin();

    void calculateGeometry(
        const IntRect& frameRect,
        IntRect& windowRect,
        IntRect& clipRect,
        Vector<IntRect>& cutOutRects);
    IntRect windowClipRect() const;
    void windowCutOutRects(
        const IntRect& frameRect,
        Vector<IntRect>& cutOutRects);

#if ENABLE(OILPAN)
    // FIXME: Oilpan: consider moving Widget to the heap, allowing this
    // container object to be a FrameDestructionObserver. And thereby
    // keep a traced member reference to the frame rather than as a
    // bare pointer. Instead, the owning object (HTMLFrameOwnerElement)
    // explicitly deletes this object when it is disconnected from its
    // frame. Any access to an invalid frame via this bare pointer
    // is therefore not possible.
    //
    // See the HTMLFrameOwnerElement::disconnectContentFrame comment for
    // (even) more.
    LocalFrame* m_frame;

    LocalFrame* frame() const { return m_frame; }
#endif

    // FIXME: see above; for the time being, a bare pointer to the owning
    // HTMLPlugInElement and managed as such.
    HTMLPlugInElement* m_element;
    WebPlugin* m_webPlugin;
    Vector<WebPluginLoadObserver*> m_pluginLoadObservers;

    WebLayer* m_webLayer;

    // The associated scrollbar group object, created lazily. Used for Pepper
    // scrollbars.
    OwnPtr<ScrollbarGroup> m_scrollbarGroup;

    TouchEventRequestType m_touchEventRequestType;
    bool m_wantsWheelEvents;
};

DEFINE_TYPE_CASTS(WebPluginContainerImpl, Widget, widget, widget->isPluginContainer(), widget.isPluginContainer());
// Unlike Widget, we need not worry about object type for container.
// WebPluginContainerImpl is the only subclass of WebPluginContainer.
DEFINE_TYPE_CASTS(WebPluginContainerImpl, WebPluginContainer, container, true, true);

} // namespace blink

#endif
