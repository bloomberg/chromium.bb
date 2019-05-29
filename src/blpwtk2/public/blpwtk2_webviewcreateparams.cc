/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
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

#include <blpwtk2_webviewcreateparams.h>
#include <blpwtk2/private/blpwtk2_process.mojom.h>

#include <base/logging.h>  // for DCHECK

namespace blpwtk2 {

mojom::WebViewCreateParams *getWebViewCreateParamsImpl(WebViewCreateParams& obj)
{
    return obj.d_impl;
}

const mojom::WebViewCreateParams *getWebViewCreateParamsImpl(const WebViewCreateParams& obj)
{
    return obj.d_impl;
}

                        // -------------------------
                        // class WebViewCreateParams
                        // -------------------------

WebViewCreateParams::WebViewCreateParams()
    : d_impl(new mojom::WebViewCreateParams)
{
    d_impl->domPasteEnabled = false;
    d_impl->javascriptCanAccessClipboard = false;
    d_impl->rerouteMouseWheelToAnyRelatedWindow = false;
    d_impl->processId = 0;
    d_impl->takeKeyboardFocusOnMouseDown = true;
    d_impl->takeLogicalFocusOnMouseDown = true;
    d_impl->activateWindowOnMouseDown = true;
}

WebViewCreateParams::WebViewCreateParams(const WebViewCreateParams& src)
{
    d_impl = new mojom::WebViewCreateParams;
    *d_impl = *src.d_impl;
}

WebViewCreateParams::~WebViewCreateParams()
{
    delete d_impl;
}

void WebViewCreateParams::setTakeKeyboardFocusOnMouseDown(bool enable)
{
    d_impl->takeKeyboardFocusOnMouseDown = enable;
}

void WebViewCreateParams::setTakeLogicalFocusOnMouseDown(bool enable)
{
    d_impl->takeLogicalFocusOnMouseDown = enable;
}

void WebViewCreateParams::setActivateWindowOnMouseDown(bool enable)
{
    d_impl->activateWindowOnMouseDown = enable;
}

void WebViewCreateParams::setDOMPasteEnabled(bool enable)
{
    d_impl->domPasteEnabled = enable;
}

void WebViewCreateParams::setJavascriptCanAccessClipboard(bool enable)
{
    d_impl->javascriptCanAccessClipboard = enable;
}

void WebViewCreateParams::setRendererAffinity(int processId)
{
    d_impl->processId = processId;
}

void WebViewCreateParams::setRerouteMouseWheelToAnyRelatedWindow(bool rerouteMouseWheelToAnyRelatedWindow)
{
    d_impl->rerouteMouseWheelToAnyRelatedWindow = rerouteMouseWheelToAnyRelatedWindow;
}

bool WebViewCreateParams::takeKeyboardFocusOnMouseDown() const
{
    return d_impl->takeKeyboardFocusOnMouseDown;
}

bool WebViewCreateParams::takeLogicalFocusOnMouseDown() const
{
    return d_impl->takeLogicalFocusOnMouseDown;
}

bool WebViewCreateParams::activateWindowOnMouseDown() const
{
    return d_impl->activateWindowOnMouseDown;
}

bool WebViewCreateParams::domPasteEnabled() const
{
    return d_impl->domPasteEnabled;
}

bool WebViewCreateParams::javascriptCanAccessClipboard() const
{
    return d_impl->javascriptCanAccessClipboard;
}

int WebViewCreateParams::rendererAffinity() const
{
    return d_impl->processId;
}

bool WebViewCreateParams::rerouteMouseWheelToAnyRelatedWindow() const
{
    return d_impl->rerouteMouseWheelToAnyRelatedWindow;
}


}  // close namespace blpwtk2

// vim: ts=4 et

