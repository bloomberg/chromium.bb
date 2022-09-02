/*
 * Copyright (C) 2016 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_WEBVIEWHOSTOBSERVER_H
#define INCLUDED_BLPWTK2_WEBVIEWHOSTOBSERVER_H

#include <blpwtk2_config.h>
#include <blpwtk2_stringref.h>

namespace blpwtk2 {

class WebView;

// This class allows an application running a browser thread to access the
// browser-side implementation of WebView's as they are created and to be
// notified of when they are destroyed. This class's functions are called
// from the browser thread.
class BLPWTK2_EXPORT WebViewHostObserver {
  public:
    WebViewHostObserver();
    virtual ~WebViewHostObserver();

    // Notification that a new WebView has been created.
    virtual void webViewHostCreated(const StringRef& channelId, int routingId, WebView* webView) = 0;

    // Notification that a WebView has been destroyed.
    virtual void webViewHostDestroyed(const StringRef& channelId, int routingId) = 0;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBVIEWHOSTOBSERVER_H

// vim: ts=4 et

