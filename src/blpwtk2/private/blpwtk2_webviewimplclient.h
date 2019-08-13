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

#ifndef INCLUDED_BLPWTK2_WEBVIEWIMPLCLIENT_H
#define INCLUDED_BLPWTK2_WEBVIEWIMPLCLIENT_H

#include <blpwtk2_config.h>

namespace blpwtk2 {

class StringRef;

// This interface is used for WebViewImpl to communicate back to WebViewProxy.
// Most communication back to WebViewProxy is done via the public
// WebViewDelegate interface.  However, things that are internal to blpwtk2
// (like the renderer's routingId) should be sent using this interface instead.
class WebViewImplClient {
public:
    virtual ~WebViewImplClient();

    virtual void updateNativeViews(blpwtk2::NativeView webview,
                                   blpwtk2::NativeView hiddenView) = 0;
    virtual void gotNewRenderViewRoutingId(int renderViewRoutingId) = 0;
    virtual void findStateWithReqId(int  reqId,
                                    int  numberOfMatches,
                                    int  activeMatchOrdinal,
                                    bool finalUpdate) = 0;
    virtual void didFinishLoadForFrame(int              routingId,
                                       const StringRef& url) = 0;
    virtual void didFailLoadForFrame(int              routingId,
                                     const StringRef& url) = 0;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBVIEWIMPLCLIENT_H

// vim: ts=4 et

