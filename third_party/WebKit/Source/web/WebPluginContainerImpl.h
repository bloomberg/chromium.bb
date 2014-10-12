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
class ScriptController;
class ScrollbarGroup;
class TouchEvent;
class WebPlugin;
class WebPluginLoadObserver;
class WebExternalTextureLayer;
class WheelEvent;
class Widget;
struct WebPrintParams;

class WebPluginContainerImpl final : public PluginView, public WebPluginContainer, public FrameDestructionObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(WebPluginContainerImpl);
public:
    static PassRefPtrWillBeRawPtr<WebPluginContainerImpl> create(HTMLPlugInElement* element, WebPlugin* webPlugin)
    {
        return adoptRefWillBeNoop(new WebPluginContainerImpl(element, webPlugin));
    }

    // PluginView methods
    virtual WebLayer* platformLayer() const override;
    virtual v8::Local<v8::Object> scriptableObject(v8::Isolate*) override;
    virtual bool getFormValue(String&) override;
    virtual bool supportsKeyboardFocus() const override;
    virtual bool supportsInputMethod() const override;
    virtual bool canProcessDrag() const override;
    virtual bool wantsWheelEvents() override;

    // Widget methods
    virtual void setFrameRect(const IntRect&) override;
    virtual void paint(GraphicsContext*, const IntRect&) override;
    virtual void invalidateRect(const IntRect&) override;
    virtual void setFocus(bool) override;
    virtual void show() override;
    virtual void hide() override;
    virtual void handleEvent(Event*) override;
    virtual void frameRectsChanged() override;
    virtual void setParentVisible(bool) override;
    virtual void setParent(Widget*) override;
    virtual void widgetPositionsUpdated() override;
    virtual bool isPluginContainer() const override { return true; }
    virtual void eventListenersRemoved() override;
    virtual bool pluginShouldPersist() const override;

    // WebPluginContainer methods
    virtual WebElement element() override;
    virtual void invalidate() override;
    virtual void invalidateRect(const WebRect&) override;
    virtual void scrollRect(const WebRect&) override;
    virtual void reportGeometry() override;
    virtual void allowScriptObjects() override;
    virtual void clearScriptObjects() override;
    virtual NPObject* scriptableObjectForElement() override;
    virtual v8::Local<v8::Object> v8ObjectForElement() override;
    virtual WebString executeScriptURL(const WebURL&, bool popupsAllowed) override;
    virtual void loadFrameRequest(const WebURLRequest&, const WebString& target, bool notifyNeeded, void* notifyData) override;
    virtual void zoomLevelChanged(double zoomLevel) override;
    virtual bool isRectTopmost(const WebRect&) override;
    virtual void requestTouchEventType(TouchEventRequestType) override;
    virtual void setWantsWheelEvents(bool) override;
    virtual WebPoint windowToLocalPoint(const WebPoint&) override;
    virtual WebPoint localToWindowPoint(const WebPoint&) override;

    // This cannot be null.
    virtual WebPlugin* plugin() override { return m_webPlugin; }
    virtual void setPlugin(WebPlugin*) override;

    virtual float deviceScaleFactor() override;
    virtual float pageScaleFactor() override;
    virtual float pageZoomFactor() override;

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
    virtual void didReceiveResponse(const ResourceResponse&) override;
    virtual void didReceiveData(const char *data, int dataLength) override;
    virtual void didFinishLoading() override;
    virtual void didFailLoading(const ResourceError&) override;

    void willDestroyPluginLoadObserver(WebPluginLoadObserver*);

    ScrollbarGroup* scrollbarGroup();

    void willStartLiveResize();
    void willEndLiveResize();

    bool paintCustomOverhangArea(GraphicsContext*, const IntRect&, const IntRect&, const IntRect&);

    virtual void trace(Visitor*) override;
    virtual void dispose() override;

#if ENABLE(OILPAN)
    virtual LocalFrame* pluginFrame() const override { return frame(); }
    virtual void shouldDisposePlugin() override;
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

    RawPtrWillBeMember<HTMLPlugInElement> m_element;
    WebPlugin* m_webPlugin;
    Vector<WebPluginLoadObserver*> m_pluginLoadObservers;

    WebLayer* m_webLayer;

    // The associated scrollbar group object, created lazily. Used for Pepper
    // scrollbars.
    OwnPtr<ScrollbarGroup> m_scrollbarGroup;

    TouchEventRequestType m_touchEventRequestType;
    bool m_wantsWheelEvents;

#if ENABLE(OILPAN)
    // Oilpan: if true, the plugin container must dispose
    // of its plugin when being finalized.
    bool m_shouldDisposePlugin;
#endif
};

DEFINE_TYPE_CASTS(WebPluginContainerImpl, Widget, widget, widget->isPluginContainer(), widget.isPluginContainer());
// Unlike Widget, we need not worry about object type for container.
// WebPluginContainerImpl is the only subclass of WebPluginContainer.
DEFINE_TYPE_CASTS(WebPluginContainerImpl, WebPluginContainer, container, true, true);

} // namespace blink

#endif
