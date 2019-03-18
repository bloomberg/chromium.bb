/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_WEBVIEWCLIENT_H
#define INCLUDED_BLPWTK2_WEBVIEWCLIENT_H

#include <blpwtk2_config.h>
#include <blpwtk2/private/blpwtk2_webview.mojom.h>

namespace gfx {
class Rect;
}

namespace blpwtk2 {
class ContextMenuParams;

                        // ===================
                        // class WebViewClient
                        // ===================

class WebViewClient
{
  public:
    virtual ~WebViewClient();

    virtual void releaseHost() = 0;
        // This method is called by the delegate to dispose references.  This
        // will release the reference to the delegate and will ask the
        // delegate to release the references to it.  It will also close the
        // connection to the WebViewHost, which will cause Mojo to eventually
        // delete the WebViewClient object.

    virtual mojom::WebViewHost *proxy() = 0;
        // Returns a raw pointer to the Mojo interface pointer proxy.  This
        // can be used to send a message to the WebViewHost on behalf of the
        // WebViewClient.

    virtual void goBack() = 0;
        // Send a request to the host to navigate back to the previous page.

    virtual void goForward() = 0;
        // Send a request to the host to navigate forward to the next page.

    virtual void reload() = 0;
        // Send a request to the host to reload the current page.

    virtual void loadUrl(const std::string& url) = 0;
        // Send a request to the host to load a resource.

    virtual void move(const gfx::Rect& rect) = 0;
        // Send a request to the host to resize the webview window.

    virtual void applyRegion(NativeRegion region) = 0;
        // Apply a window-region over the webview.

    virtual void ncHitTestResult(int x, int y, int result) = 0;

    virtual void setParent(NativeView parent) = 0;

    virtual void find(const std::string& text,
                      bool               matchCase,
                      bool               forward) = 0;

    virtual void stopFind(bool preserveSelection) = 0;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBVIEWCLIENT_H

// vim: ts=4 et

