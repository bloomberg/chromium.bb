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

#ifndef INCLUDED_BLPWTK2_RENDERWEBVIEW_H
#define INCLUDED_BLPWTK2_RENDERWEBVIEW_H

#include <blpwtk2.h>
#include <blpwtk2_config.h>

#include <blpwtk2_scopedhwnd.h>
#include <blpwtk2_webview.h>
#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_webviewproperties.h>
#include <blpwtk2_webviewproxydelegate.h>

#include <ipc/ipc_listener.h>
#include <ui/gfx/geometry/rect.h>

namespace blpwtk2 {

class ProfileImpl;

                        // ===================
                        // class RenderWebView
                        // ===================

class RenderWebView final : public WebView
                          , public WebViewDelegate
                          , private WebViewProxyDelegate
                          , private IPC::Listener
{
    class RenderViewObserver;

    // DATA
    WebView *d_proxy;
    WebViewDelegate *d_delegate;
#if defined(BLPWTK2_FEATURE_FOCUS) || defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
    WebViewProperties d_properties;
#endif

    bool d_pendingDestroy;

    ScopedHWND d_hwnd;

    bool d_hasParent = false;
    bool d_shown = false, d_visible = false;
    gfx::Rect d_geometry;

    // Track the 'content::RenderWidget':
    bool d_gotRenderViewInfo = false;
    int d_renderViewRoutingId, d_renderWidgetRoutingId, d_mainFrameRoutingId;
    RenderViewObserver *d_renderViewObserver = nullptr;

    // blpwtk2::WebView overrides
    void destroy() override;
    WebFrame *mainFrame() override;
    int loadUrl(const StringRef& url) override;
    void loadInspector(unsigned int pid, int routingId) override;
    void inspectElementAt(const POINT& point) override;
    int goBack() override;
    int goForward() override;
    int reload() override;
    void stop() override;
#if defined(BLPWTK2_FEATURE_FOCUS)
    void takeKeyboardFocus() override;
    void setLogicalFocus(bool focused) override;
#endif
    void show() override;
    void hide() override;
    void setParent(NativeView parent) override;
    void move(int left, int top, int width, int height) override;
    void cutSelection() override;
    void copySelection() override;
    void paste() override;
    void deleteSelection() override;
    void enableNCHitTest(bool enabled) override;
    void onNCHitTestResult(int x, int y, int result) override;
    void performCustomContextMenuAction(int actionId) override;
#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    void enableAltDragRubberbanding(bool enabled) override;
    bool forceStartRubberbanding(int x, int y) override;
    bool isRubberbanding() const override;
    void abortRubberbanding() override;
    String getTextInRubberband(const NativeRect&) override;
#endif
    void find(const StringRef& text, bool matchCase, bool forward) override;
    void stopFind(bool preserveSelection) override;
    void replaceMisspelledRange(const StringRef& text) override;
    void rootWindowPositionChanged() override;
    void rootWindowSettingsChanged() override;

    void handleInputEvents(const InputEvent *events,
                           size_t            eventsCount) override;
    void setDelegate(WebViewDelegate *delegate) override;
#if defined(BLPWTK2_FEATURE_SCREENPRINT)
    void drawContentsToBlob(Blob *blob, const DrawParams& params) override;
#endif
    int getRoutingId() const override;
    void setBackgroundColor(NativeColor color) override;
    void setRegion(NativeRegion region) override;
#if defined(BLPWTK2_FEATURE_KEYBOARD_LAYOUT)
    void activateKeyboardLayout(unsigned int hkl) override;
#endif
    void clearTooltip() override;
#if defined(BLPWTK2_FEATURE_DWM)
    void rootWindowCompositionChanged() override;
#endif
    v8::MaybeLocal<v8::Value> callFunction(
            v8::Local<v8::Function>  func,
            v8::Local<v8::Value>     recv,
            int                      argc,
            v8::Local<v8::Value>    *argv) override;
#if defined(BLPWTK2_FEATURE_PRINTPDF)
    String printToPDF(const StringRef& propertyName) override;
#endif

#if defined(BLPWTK2_FEATURE_FASTRESIZE)
    void disableResizeOptimization() override;
#endif

    void setSecurityToken(v8::Isolate *isolate,
                          v8::Local<v8::Value> token) override;

    // WebViewDelegate overrides
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
#if defined(BLPWTK2_FEATURE_DEVTOOLSINTEGRATION)
    void devToolsAgentHostAttached(WebView *source) override;
    void devToolsAgentHostDetached(WebView *source) override;
#endif

    // WebViewProxyDelegate overrides:
    void notifyRoutingId(int id) override;

    // IPC::Listener overrides
    bool OnMessageReceived(const IPC::Message& message) override;

    // PRIVATE FUNCTIONS:
    static LPCTSTR GetWindowClass();
    static LRESULT CALLBACK WindowProcedure(HWND   hWnd,
                                            UINT   uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam);
    LRESULT windowProcedure(UINT   uMsg,
                            WPARAM wParam,
                            LPARAM lParam);

    void initialize();
    void updateVisibility();
    void updateGeometry();
    void detachFromRoutingId();

    DISALLOW_COPY_AND_ASSIGN(RenderWebView);

  public:
    explicit RenderWebView(WebViewDelegate          *delegate,
                           ProfileImpl              *profile,
                           const WebViewProperties&  properties);
    ~RenderWebView() final;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERWEBVIEW_H

// vim: ts=4 et
