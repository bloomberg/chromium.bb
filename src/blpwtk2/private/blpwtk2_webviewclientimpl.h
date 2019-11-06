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

#ifndef INCLUDED_BLPWTK2_WEBVIEWCLIENTIMPL_H
#define INCLUDED_BLPWTK2_WEBVIEWCLIENTIMPL_H

#include <blpwtk2_config.h>

#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_webviewclientimpl.h>
#include <blpwtk2_webviewclient.h>
#include <blpwtk2_findonpage.h>
#include <blpwtk2/private/blpwtk2_webview.mojom.h>

#include <base/compiler_specific.h>
#include <ipc/ipc_sender.h>
#include <ui/gfx/geometry/rect.h>

#include <cstdint>
#include <string>
#include <vector>

namespace gfx {
class Point;
}  // close namespace gfx

namespace blpwtk2 {

class BrowserContextImpl;
class ProcessHost;
class WebViewImpl;
struct FindOnPageRequest;
struct WebViewProperties;
class WebViewClientDelegate;

                        // =======================
                        // class WebViewClientImpl
                        // =======================

class WebViewClientImpl final : public WebViewClient
                              , public mojom::WebViewClient
{
    // DATA
    mojom::WebViewHostPtr d_hostPtr;
    WebViewClientDelegate *d_delegate;
    ncHitTestCallback d_ncHitTestCallback;
    NativeView d_nativeView;
    NativeView d_originalParentView;
    std::unique_ptr<FindOnPage> d_findState;
    gfx::Rect d_rect;
    bool d_pendingMoveAck;

    // blpwtk2::WebViewClient overrides
    void releaseHost() override;
        // This method is called by the delegate to dispose references.  This
        // will release the reference to the delegate and will ask the
        // delegate to release the references to it.  It will also close the
        // connection to the WebViewHost, which will cause Mojo to eventually
        // delete the WebViewClient object.

    mojom::WebViewHost *proxy() override;
        // Returns a raw pointer to the Mojo interface pointer proxy.  This
        // can be used to send a message to the WebViewHost on behalf of the
        // WebViewClient.

    void goBack() override;
        // Send a request to the host to navigate back to the previous page.

    void goForward() override;
        // Send a request to the host to navigate forward to the next page.

    void reload() override;
        // Send a request to the host to reload the current page.

    void loadUrl(const std::string& url) override;
        // Send a request to the host to load a resource.

    void move(const gfx::Rect& rect) override;
        // Send a request to the host to resize the webview window.

    void applyRegion(NativeRegion region) override;
        // Apply a window-region over the webview.

    void ncHitTestResult(int x, int y, int result) override;

    void setParent(NativeView parent) override;

    void find(const std::string& text,
              bool               matchCase,
              bool               forward) override;

    void stopFind(bool preserveSelection) override;

    // mojom::WebViewClient overrides
    void ncHitTest(ncHitTestCallback callback) override;
        // Allow the client to intercept hit-test on non-client area of the
        // window.  Note that this operation is asynchronous and the host
        // might send out another call if the mouse moved between the request
        // and the response calls.

    void ncDragMove(
            int x, int y, ncDragMoveCallback callback) override;
        // Allow the client to intercept the drag move event on non-client
        // area of the window.

    void ncDragBegin(int                        hitTestCode,
                     int                        x,
                     int                        y,
                     ncDragBeginCallback callback) override;
        // Notify the client when the drag operation starts non-client area
        // of the webview window.

    void ncDragEnd(int x, int y, ncDragEndCallback callback) override;
        // Notify the client when the drag operation ends on non-client area
        // of the webview window.

    void ncDoubleClick(int x, int y) override;
        // Notify the client when the user double-clicks on a non-client area
        // of the webview window.

    void focused() override;
        // Notify the client that the webview is focused.

    void blurred() override;
        // Notify the client that the webview is blurred (unfocused).

    void showContextMenu(mojom::ContextMenuParamsPtr params) override;
        // Notify the client when the user attempts to open a context menu
        // (ie. by right-clicking an element).

    void notifyNativeViews(int nativeView, int originalParent) override;
        // Notify the client about the native views (platform-dependent views).
        // The client can reparent the 'view' under a different
        // platform-dependent view but it must reparent it under 'hiddenview'
        // before destruction.

    void notifyRoutingId(int id) override;
        // Notify the client of the webview routing id.

    void findReply(int  reqId,
                   int  numberOfMatches,
                   int  activeMatchOrdinal,
                   bool finalUpdate) override;
        // This is the reply message for WebViewHost::find().  The host can
        // send multiple 'findReply' response for a single 'find' request.

    void didFinishLoadForFrame(int routingId, const std::string& url) override;
    void didFailLoadForFrame(int routingId, const std::string& url) override;

    // Mojo callbacks
    void loadStatus(int status);
    void moveAck(int x, int y, int w, int h);

    void setParentStatusUpdate(int status, unsigned int parent);
        // Notify the client if setParent call succeed by value of status

    DISALLOW_COPY_AND_ASSIGN(WebViewClientImpl);

  public:
    WebViewClientImpl(mojom::WebViewHostPtr&&  hostPtr,
                      WebViewClientDelegate   *delegate);

    ~WebViewClientImpl() final;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBVIEWCLIENTIMPL_H

// vim: ts=4 et

