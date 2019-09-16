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

#ifndef INCLUDED_BLPWTK2_WEBVIEWHOSTIMPL_H
#define INCLUDED_BLPWTK2_WEBVIEWHOSTIMPL_H

#include <blpwtk2_config.h>

#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_webviewimplclient.h>
#include <blpwtk2_processhostimpl.h>
#include <blpwtk2/private/blpwtk2_webview.mojom.h>
#include <blpwtk2/private/blpwtk2_process.mojom.h>

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
struct FindOnPageRequest;
class WebViewImpl;
struct WebViewProperties;

                        // =====================
                        // class WebViewHostImpl
                        // =====================

class WebViewHostImpl final : private WebViewImplClient
                            , private WebViewDelegate
                            , public mojom::WebViewHost
{
    // TYPES
    struct DragState {
        gfx::Point startPoint;
        gfx::Point movePoint;
        gfx::Point endPoint;
        bool startPointSet;
        bool movePointSet;
        bool endPointSet;
        int hitTestCode;
        bool isDragging;
        bool pendingAck;
    };

    // DATA
    mojom::WebViewClientPtr d_clientPtr;
    base::OnceCallback<void(int32_t)> d_loadUrlCallback;
    WebViewImpl *d_impl;
    DragState d_dragState;

    scoped_refptr<ProcessHostImpl::Impl> d_processHost;
        // This object is not used within this class.  This refptr merely
        // keeps it alive until WebViewHost is ready to be disposed.

    // blpwtk2::WebViewImplClient overrides
    void updateNativeViews(blpwtk2::NativeView webview,
                           blpwtk2::NativeView hiddenView) override;
    void gotNewRenderViewRoutingId(int renderViewRoutingId) override;
    void findStateWithReqId(int  reqId,
                            int  numberOfMatches,
                            int  activeMatchOrdinal,
                            bool finalUpdate) override;
    void didFinishLoadForFrame(int              routingId,
                               const StringRef& url) override;
    void didFailLoadForFrame(int              routingId,
                             const StringRef& url) override;

    // blpwtk2::WebViewDelegate overrides
    void created(WebView *source) override;
    void didFinishLoad(WebView *source, const StringRef& url) override;
    void didFailLoad(WebView *source, const StringRef& url) override;
    void focused(WebView* source) override;
    void blurred(WebView* source) override;
    void showContextMenu(WebView                  *source,
                         const ContextMenuParams&  params) override;
    void requestNCHitTest(WebView *source) override;
    void ncDragBegin(WebView      *source,
                     int           hitTestCode,
                     const POINT&  startPoint) override;
    void ncDragMove(WebView *source, const POINT& movePoint) override;
    void ncDragEnd(WebView *source, const POINT& endPoint) override;
    void ncDoubleClick(WebView *source, const POINT& point) override;
    void findState(WebView *source,
                   int      numberOfMatches,
                   int      activeMatchOrdinal,
                   bool     finalUpdate) override;

    void devToolsAgentHostAttached(WebView *source) override;
    void devToolsAgentHostDetached(WebView *source) override;

    // Mojo callbacks
    void onNCDragAck();

    // mojom::WebViewHost overrides
    void loadUrl(
            const std::string& url, loadUrlCallback callback) override;
    void loadInspector(unsigned int pid, int routingId) override;
    void inspectElementAt(int x, int y) override;
    void back(backCallback callback) override;
    void forward(forwardCallback callback) override;
    void reload(reloadCallback callback) override;
    void stop() override;
    void takeKeyboardFocus() override;
    void setLogicalFocus(bool focused) override;
    void performCustomContextMenuAction(int id) override;
    void show() override;
    void hide() override;
    void move(int                 x,
              int                 y,
              int                 width,
              int                 height,
              moveCallback callback) override;
    void cutSelection() override;
    void copySelection() override;
    void paste() override;
    void deleteSelection() override;
    void enableNCHitTest(bool enabled) override;
    void enableAltDragRubberbanding(bool enabled) override;
    void find(int                reqId,
              const std::string& text,
              bool               matchCase,
              bool               findNext,
              bool               forward) override;
    void stopFind(bool preserveSelection) override;
    void replaceMisspelledRange(const std::string& text) override;
    void rootWindowPositionChanged() override;
    void rootWindowSettingsChanged() override;

    void setBackgroundColor(int r, int g, int b) override;
    void applyRegion(const std::string&         blob,
                     applyRegionCallback callback) override;
    void activateKeyboardLayout(unsigned int hkl) override;
    void clearTooltip() override;
    void setParent(unsigned int window, setParentCallback callback) override;
    void rootWindowCompositionChanged() override;

    DISALLOW_COPY_AND_ASSIGN(WebViewHostImpl);

  public:
    WebViewHostImpl(
            mojom::WebViewClientPtr&&                    clientPtr,
            const mojom::WebViewCreateParams&            params,
            BrowserContextImpl                          *browserContext,
            unsigned int                                 hostAffinity,
            const scoped_refptr<ProcessHostImpl::Impl>&  processHost);
    ~WebViewHostImpl() final;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBVIEWHOST_H

// vim: ts=4 et

