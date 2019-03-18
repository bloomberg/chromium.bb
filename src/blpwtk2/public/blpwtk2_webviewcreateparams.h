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

#ifndef INCLUDED_BLPWTK2_WEBVIEWCREATEPARAMS_H
#define INCLUDED_BLPWTK2_WEBVIEWCREATEPARAMS_H

#include <blpwtk2_config.h>

namespace blpwtk2 {

namespace mojom {
class WebViewCreateParams;
}

class Profile;
class WebViewCreateParams;

mojom::WebViewCreateParams *getWebViewCreateParamsImpl(WebViewCreateParams& obj);
const mojom::WebViewCreateParams *getWebViewCreateParamsImpl(const WebViewCreateParams& obj);

                        // =========================
                        // class WebViewCreateParams
                        // =========================

// This class contains parameters that are passed to blpwtk2 whenever the
// application wants to create a new WebView.
class BLPWTK2_EXPORT WebViewCreateParams {
    // DATA
    mojom::WebViewCreateParams *d_impl;

    friend mojom::WebViewCreateParams *getWebViewCreateParamsImpl(WebViewCreateParams& obj);
    friend const mojom::WebViewCreateParams *getWebViewCreateParamsImpl(const WebViewCreateParams& obj);

  public:
    WebViewCreateParams();
    WebViewCreateParams(const WebViewCreateParams& src);
    ~WebViewCreateParams();

    // MANIPULATORS
    void setDOMPasteEnabled(bool enable);
        // By default, Javascript will not be able to paste into the DOM.
        // However, setting this flag will enable that behavior.  Note that
        // this will only work if "setJavascriptCanAccessClipboard(true)" is
        // also set.

    void setJavascriptCanAccessClipboard(bool enable);
        // By default, Javascript will not be able to access the clipboard.
        // However, setting this flag will enable that behavior.

    void setRendererAffinity(int processId);
        // By default, WebViews dynamically instantiate renderer processes
        // each time a new navigation request is made.  This is known as the
        // "process-per-site-instance" model.  For more information, please
        // refer to this document:
        // https://sites.google.com/a/chromium.org/dev/developers/design-documents/process-models
        // The command-line arguments described on that page will also work on
        // blpwtk2-based applications.
        //
        // However, in order to provide better control to some applications,
        // the 'processid' parameter allows each WebView to be constructed to
        // have affinity to a particular renderer process.
        //
        // If 'processid' is zero (which is the default if
        // 'setRendererAffinity' is not called), then the upstream chromium
        // logic will be used.
        //
        // If 'processId' is positive, then the WebView will be rendered in
        // the specified process (assuming the process is already running an
        // instance of blpwtk2 toolkit.
        //
        // If 'processId' is negative, then the WebView will use that
        // particular (out-of-process) renderer process.

    void setRerouteMouseWheelToAnyRelatedWindow(bool rerouteMouseWheelToAnyRelatedWindow);
        // Setting this flag will cause mouse wheel events that are delivered
        // to the focused webview window while hovering over a different
        // window to be delivered to that window if it shares a 'root' window
        // with the webview.

    // ACCESSORS
    bool domPasteEnabled() const;
    bool javascriptCanAccessClipboard() const;
    int rendererAffinity() const;
    bool rerouteMouseWheelToAnyRelatedWindow() const;
};


}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_CREATEPARAMS_H

// vim: ts=4 et

