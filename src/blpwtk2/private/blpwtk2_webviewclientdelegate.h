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

#ifndef INCLUDED_BLPWTK2_WEBVIEWCLIENTDELEGATE_H
#define INCLUDED_BLPWTK2_WEBVIEWCLIENTDELEGATE_H

#include <blpwtk2_config.h>
#include <ui/gfx/geometry/point.h>
#include <ui/gfx/geometry/size.h>

namespace blpwtk2 {

class StringRef;
class WebViewClient;
class ContextMenuParams;

                        // ===========================
                        // class WebViewClientDelegate
                        // ===========================

class WebViewClientDelegate
{
  public:
    virtual ~WebViewClientDelegate();

    virtual void setClient(WebViewClient *client) = 0;
        // This method is called by the WebViewClient to give a reference
        // to itself to the delegate.

    virtual void ncHitTest() = 0;
        // This method is called when the browser observes a hit-test event
        // on the non-client area of the window.
        //
        // Note: This is part of an async function call.  The embedder is
        // required to respond by calling blpwtk2::WebView::onNCHitTestResult.

    virtual void ncDragBegin(int hitTestCode, const gfx::Point& point) = 0;
        // This method is called when the browser observes the beginning
        // of a drag event on a non-client area of this webview's window.

    virtual void ncDragMove(const gfx::Point& point) = 0;
        // This method is called when the browser observes a mouse move event
        // during a drag operation on a non-client area of this webview's
        // window.

    virtual void ncDragEnd(const gfx::Point& point) = 0;
        // This method is called when the browser observes the end of a drag
        // event on the non-client area of this webview's window.

    virtual void ncDoubleClick(const gfx::Point& point) = 0;
        // This method is called when the browser observes a double-click
        // event on the non-client area of this webview's window.

    virtual void focused() = 0;
        // This method is called when the browser observes a focus event on
        // this webview's window.

    virtual void blurred() = 0;
        // This method is called when the browser observes a blur (or
        // focus-out) event on this webview's window.

    virtual void showContextMenu(const ContextMenuParams& params) = 0;
        // This method is called when the user right-clicks (or through some
        // other means) to bring up the context menu.  The browser itself
        // does not show any popup or dropdown but it passes the 'params' to
        // tell the embedder on what to include in the dropdown menu.

    virtual void findReply(int  numberOfMatches,
                           int  activeMatchOrdinal,
                           bool finalUpdate) = 0;

    virtual void preResize(const gfx::Size& size) = 0;
        // This method is called right before an asynchronous call is made to
        // resize the webview window.  If the WebViewClientDelegate object has
        // direct access (within the same thread) to the render view, it can
        // tell it to resize the content ahead of time.  This will effectively
        // cause the resize of the content to lead ahead (instead of lag
        // behind) the resize of the webview window.  This has the advantage
        // of running the relayout in parallel with (as opposed to after) the
        // resize of the webview.

    virtual void notifyRoutingId(int id) = 0;
        // This method is called when the webview host notifies the webview
        // client of the renderview's routing id.  The client's delegate can
        // use this notification to know when to start polling Content for the
        // renderview with the matching routing id.

    virtual void onLoadStatus(int status) = 0;
        // This method is called when the client receives the status from the
        // host for a load operation of an URL resource.  The status number
        // maps to the error codes in errno.

    virtual void didFinishLoadForFrame(int              routingId,
                                       const StringRef& url) = 0;
        // This method is called when the client receives notification from
        // the host that a URL load for a particular IFRAME succeeded.

    virtual void didFailLoadForFrame(int              routingId,
                                     const StringRef& url) = 0;
        // This method is called when the client receives notification from
        // the host that a URL load for a particular IFRAME failed.

    virtual void didParentStatus(int status, NativeView parent) = 0;
        // This method is called when the setParent call is done   
        // If successful, status 0 will be returned, otherwise error code from GetLassError()
        // will be returned as status 
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBVIEWCLIENTDELEGATE_H

// vim: ts=4 et

