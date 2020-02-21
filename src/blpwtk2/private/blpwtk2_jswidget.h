/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_JSWIDGET_H
#define INCLUDED_BLPWTK2_JSWIDGET_H

#include <blpwtk2_config.h>

#include <third_party/blink/public/web/web_element.h>
#include <third_party/blink/public/web/web_plugin.h>

namespace blink {
class WebDOMEvent;
class WebLocalFrame;
}  // close namespace blink

namespace blpwtk2 {

// This is a WebPlugin implementation that is created whenever there is an
// object element with "application/x-bloomberg-jswidget" mime type.  All it
// does is raise custom events on the DOM element whenever certain plugin
// callbacks are invoked.
class JsWidget : public blink::WebPlugin {
  public:
    explicit JsWidget(blink::WebLocalFrame* frame);
    ~JsWidget() final;

    void DispatchEvent(const blink::WebDOMEvent& event);

    // blink::WebPlugin overrides
    bool Initialize(blink::WebPluginContainer*) override;
    void Destroy() override;
    blink::WebPluginContainer* Container() const override;
    void Paint(cc::PaintCanvas*, const blink::WebRect&) override {}
    void UpdateGeometry(
        const blink::WebRect& windowRect, const blink::WebRect& clipRect,
        const blink::WebRect& unobscuredRect, bool isVisible) override;
    void UpdateFocus(bool, blink::WebFocusType) override {}
    void UpdateVisibility(bool isVisible) override;
    blink::WebInputEventResult HandleInputEvent(
        const blink::WebCoalescedInputEvent&, blink::WebCursorInfo&) override;
    void DidReceiveResponse(const blink::WebURLResponse&) override {}
    void DidReceiveData(const char* data, size_t dataLength) override {}
    void DidFinishLoading() override {}
    void DidFailLoading(const blink::WebURLError&) override {}
    void UpdateAllLifecyclePhases(blink::WebWidget::LifecycleUpdateReason) override {}
    void AttachToLayout() override;
    void DetachFromLayout() override;

  private:
    blink::WebRect LocalToRootFrameRect(const blink::WebRect& localRect) const;

    blink::WebPluginContainer* d_container;
    blink::WebLocalFrame* d_frame;
    bool d_hasParent;
    bool d_pendingVisible; // Whether to make visible when added to a parent

    DISALLOW_COPY_AND_ASSIGN(JsWidget);
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_JSWIDGET_H
