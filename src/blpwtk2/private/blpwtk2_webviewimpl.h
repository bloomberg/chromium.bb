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

#ifndef INCLUDED_BLPWTK2_WEBVIEWIMPL_H
#define INCLUDED_BLPWTK2_WEBVIEWIMPL_H

#include <blpwtk2_config.h>

#include <blpwtk2_findonpage.h>
#include <blpwtk2_nativeviewwidgetdelegate.h>
#include <blpwtk2_webview.h>
#include <blpwtk2_webviewproperties.h>

#include <base/memory/weak_ptr.h>
#include <content/public/browser/web_contents_delegate.h>
#include <content/public/browser/web_contents_observer.h>
#include <content/public/common/context_menu_params.h>
#include <ui/gfx/native_widget_types.h>
#include <third_party/blink/public/web/web_text_direction.h>

namespace content {
class WebContents;
struct WebPreferences;
}  // close namespace content

namespace views {
class Widget;
}  // close namespace views

namespace blpwtk2 {

class BrowserContextImpl;
class ContextMenuParams;
class DevToolsFrontendHostDelegateImpl;
class NativeViewWidget;
class WebViewDelegate;
class WebViewImplClient;
class WebFrameImpl;

                        // =================
                        // class WebViewImpl
                        // =================

// This is the implementation of the blpwtk2::WebView interface.  It creates a
// content::WebContents object, and implements the content::WebContentsDelegate
// interface.  It forwards the content::WebContentsDelegate override events to
// the blpwtk2::WebViewDelegate, which is provided by the application.
//
// This class can only be instantiated from the browser-main thread.
//
// When we are using 'ThreadMode::ORIGINAL', this object is returned to the
// application by 'Toolkit::createWebView'.  When we are using
// 'ThreadMode::RENDERER_MAIN', this object is created on the secondary
// browser-main thread, and the application instead gets a 'WebViewProxy',
// which forwards events back and forth between the secondary browser-main
// thread and the application thread.  See blpwtk2_toolkit.h for an explanation
// about threads.
class WebViewImpl final : public WebView,
                          private NativeViewWidgetDelegate,
                          public content::WebContentsDelegate,
                          private content::WebContentsObserver,
                          private base::SupportsWeakPtr<WebViewImpl>
{
    // DATA
    std::unique_ptr<DevToolsFrontendHostDelegateImpl> d_devToolsFrontEndHost;
    std::unique_ptr<content::WebContents> d_webContents;
    std::unique_ptr<FindOnPage> d_find;
    WebViewDelegate *d_delegate;
    WebViewImplClient *d_implClient;
    content::RenderViewHost *d_renderViewHost;
    BrowserContextImpl *d_browserContext;
    NativeViewWidget *d_widget;  // owned by the views system
    WebViewProperties d_properties;  // TODO(SHEZ): move more properties into this struct
    content::CustomContextMenuContext d_customContext;
    bool d_isReadyForDelete;  // when the underlying WebContents can be deleted
    bool d_wasDestroyed;      // if destroy() has been called
    bool d_isDeletingSoon;    // when DeleteSoon has been called
    bool d_ncHitTestEnabled;
    bool d_ncHitTestPendingAck;
    int d_lastNCHitTestResult;
    int d_hostId;

    int createWidget(blpwtk2::NativeView parent);

    // blpwtk2::NativeViewWidgetDelegate overrides
    void onDestroyed(NativeViewWidget* source) override;
    bool OnNCHitTest(int* result) override;
    bool OnNCDragBegin(int hitTestCode) override;
    void OnNCDragMove() override;
    void OnNCDragEnd() override;
    aura::Window* GetDefaultActivationWindow() override;

    // content::WebContentsDelegate overrides
    void DidNavigateMainFramePostCommit(content::WebContents *source) override;
        // Invoked when a main frame navigation occurs.

    bool TakeFocus(content::WebContents* source, bool reverse) override;
        // This is called when WebKit tells us that it is done tabbing through
        // controls on the page. Provides a way for WebContentsDelegates to
        // handle this. Returns true if the delegate successfully handled it.

    void RequestMediaAccessPermission(
        content::WebContents                  *web_contents,
        const content::MediaStreamRequest&     request,
        content::MediaResponseCallback  callback) override;
        // Asks permission to use the camera and/or microphone. If permission
        // is granted, a call should be made to |callback| with the devices.
        // If the request is denied, a call should be made to |callback| with
        // an empty list of devices. |request| has the details of the request
        // (e.g. which of audio and/or video devices are requested, and lists
        // of available devices).

    bool CheckMediaAccessPermission(content::RenderFrameHost*,
                                    const GURL&,
                                    blink::MediaStreamType) override;
        // Checks if we have permission to access the microphone or camera.
        // Note that this does not query the user. |type| must be
        // MEDIA_DEVICE_AUDIO_CAPTURE or MEDIA_DEVICE_VIDEO_CAPTURE.

    void FindReply(content::WebContents *source_contents,
                   int                   request_id,
                   int                   number_of_matches,
                   const gfx::Rect&      selection_rect,
                   int                   active_match_ordinal,
                   bool                  final_update) override;
        // Information about current find request

    // content::WebContentsObserver overrides
    void RenderViewHostChanged(content::RenderViewHost *old_host,
                               content::RenderViewHost *new_host) override;
        // This method is invoked when a WebContents swaps its visible
        // RenderViewHost with another one, possibly changing processes. The
        // RenderViewHost that has been replaced is in |old_host|, which is
        // NULL if the old RVH was shut down.

    void DidFinishLoad(content::RenderFrameHost *render_frame_host,
                       const GURL&               validated_url) override;
        // This method is invoked when the navigation is done, i.e. the
        // spinner of the tab will stop spinning, and the onload event was
        // dispatched.
        //
        // If the WebContents is displaying replacement content, e.g. network
        // error pages, DidFinishLoad is invoked for frames that were not
        // sending navigational events before. It is safe to ignore these
        // events.

    void DidFailLoad(content::RenderFrameHost *render_frame_host,
                     const GURL&               validated_url,
                     int                       error_code,
                     const base::string16&     error_description) override;
        // This method is like DidFinishLoad, but when the load failed or was
        // cancelled, e.g. window.stop() is invoked.



    // patch section: focus


    // patch section: gpu



    DISALLOW_COPY_AND_ASSIGN(WebViewImpl);

  public:
    WebViewImpl(WebViewDelegate          *delegate,
                blpwtk2::NativeView       parent,
                BrowserContextImpl       *browserContext,
                int                       hostAffinity,
                bool                      initiallyVisible,
                const WebViewProperties&  properties);
    ~WebViewImpl() final;

    void setImplClient(WebViewImplClient *client);
    gfx::NativeView getNativeView() const;
    void showContextMenu(const ContextMenuParams& params);
    void saveCustomContextMenuContext(
        content::RenderFrameHost                 *rfh,
        const content::CustomContextMenuContext&  context);
    void handleFindRequest(const FindOnPageRequest& request);
    void overrideWebkitPrefs(content::WebPreferences* prefs);
    void onRenderViewHostMadeCurrent(content::RenderViewHost* renderViewHost);

    v8::MaybeLocal<v8::Value> callFunction(v8::Local<v8::Function>  func,
                                           v8::Local<v8::Value>     recv,
                                           int                      argc,
                                           v8::Local<v8::Value>    *argv) override;

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

    void handleInputEvents(const InputEvent *events, size_t eventsCount) override;
    void setDelegate(WebViewDelegate* delegate) override;
    int getRoutingId() const override;
    void setBackgroundColor(NativeColor color) override;
    void setRegion(NativeRegion region) override;
    void clearTooltip() override;
    void setSecurityToken(v8::Isolate *isolate,
                          v8::Local<v8::Value> token) override;



    // patch section: gpu


    // patch section: print to pdf



};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBVIEWIMPL_H

// vim: ts=4 et

