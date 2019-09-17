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

#ifndef INCLUDED_BLPWTK2_WEBVIEWPROXY_H
#define INCLUDED_BLPWTK2_WEBVIEWPROXY_H

#include <base/macros.h>
#include <blpwtk2_config.h>
#include <blpwtk2_webview.h>
#include <blpwtk2_webviewclientdelegate.h>

namespace gfx {
class Point;
}  // close namespace gfx

namespace blpwtk2 {

class WebFrameImpl;
class WebViewProxyDelegate;
class ProfileImpl;

                        // ==================
                        // class WebViewProxy
                        // ==================

class WebViewProxy final : public WebView
                         , public WebViewClientDelegate
{
    // DATA
    WebViewClient *d_client;
    WebViewDelegate *d_delegate;
    WebViewProxyDelegate *d_proxyDelegate;

    ProfileImpl *d_profile;
    int d_renderViewRoutingId;
    bool d_gotRenderViewInfo;
    bool d_pendingLoadStatus;
    bool d_isMainFrameAccessible;
    bool d_pendingDestroy;
    std::string d_url;
    std::unique_ptr<WebFrameImpl> d_mainFrame;
    v8::Global<v8::Value> d_securityToken;

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
    void show() override;
    void hide() override;
    int setParent(NativeView parent) override;
    void move(int left, int top, int width, int height) override;
    void cutSelection() override;
    void copySelection() override;
    void paste() override;
    void deleteSelection() override;
    void enableNCHitTest(bool enabled) override;
    void onNCHitTestResult(int x, int y, int result) override;
    void performCustomContextMenuAction(int actionId) override;
    void find(const StringRef& text, bool matchCase, bool forward) override;
    void stopFind(bool preserveSelection) override;
    void replaceMisspelledRange(const StringRef& text) override;
    void rootWindowPositionChanged() override;
    void rootWindowSettingsChanged() override;

    void handleInputEvents(const InputEvent *events,
                           size_t            eventsCount) override;
    void setDelegate(WebViewDelegate *delegate) override;
    int getRoutingId() const override;
    void setBackgroundColor(NativeColor color) override;
    void setRegion(NativeRegion region) override;
    void clearTooltip() override;
    v8::MaybeLocal<v8::Value> callFunction(
            v8::Local<v8::Function>  func,
            v8::Local<v8::Value>     recv,
            int                      argc,
            v8::Local<v8::Value>    *argv) override;

    void setSecurityToken(v8::Isolate *isolate,
                          v8::Local<v8::Value> token) override;

    DISALLOW_COPY_AND_ASSIGN(WebViewProxy);

  public:
    explicit WebViewProxy(WebViewDelegate *delegate, ProfileImpl *profile);
    ~WebViewProxy() final;

    void setProxyDelegate(WebViewProxyDelegate *proxyDelegate);

    // blpwtk2::WebViewClientDelegate overrides
    void setClient(WebViewClient *client) override;
    void ncHitTest() override;
    void ncDragBegin(int hitTestCode, const gfx::Point& point) override;
    void ncDragMove(const gfx::Point& point) override;
    void ncDragEnd(const gfx::Point& point) override;
    void ncDoubleClick(const gfx::Point& point) override;
    void focused() override;
    void blurred() override;
    void showContextMenu(const ContextMenuParams& params) override;
    void findReply(int  numberOfMatches,
                   int  activeMatchOrdinal,
                   bool finalUpdate) override;
    void preResize(const gfx::Size& size) override;
    void notifyRoutingId(int id) override;
    void onLoadStatus(int status) override;
    void didFinishLoadForFrame(int              routingId,
                               const StringRef& url) override;
    void didFailLoadForFrame(int              routingId,
                             const StringRef& url) override;
    void didParentStatus(int status, NativeView parent) override;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBVIEWPROXY_H

// vim: ts=4 et

