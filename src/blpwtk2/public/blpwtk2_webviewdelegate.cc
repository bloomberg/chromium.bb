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

#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_webview.h>

namespace blpwtk2 {

WebViewDelegate::~WebViewDelegate() {}

void WebViewDelegate::didFinishLoad(WebView* source, const StringRef& url) {}

void WebViewDelegate::didFailLoad(WebView* source, const StringRef& url) {}

void WebViewDelegate::focused(WebView* source) {}

void WebViewDelegate::blurred(WebView* source) {}

void WebViewDelegate::showContextMenu(WebView* source,
                                      const ContextMenuParams& params) {}

void WebViewDelegate::requestNCHitTest(WebView* source) {}

void WebViewDelegate::ncDragBegin(WebView* source,
                                  int hitTestCode,
                                  const POINT& startPoint) {}

void WebViewDelegate::ncDragMove(WebView* source, const POINT& movePoint) {}

void WebViewDelegate::ncDragEnd(WebView* source, const POINT& endPoint) {}

void WebViewDelegate::ncDoubleClick(WebView* source, const POINT& endPoint) {}

void WebViewDelegate::findState(WebView* source,
                                int numberOfMatches,
                                int activeMatchOrdinal,
                                bool finalUpdate) {}

void WebViewDelegate::devToolsAgentHostAttached(WebView* source) {}

void WebViewDelegate::devToolsAgentHostDetached(WebView* source) {}

void WebViewDelegate::startPerformanceTiming() {}

void WebViewDelegate::stopPerformanceTiming() {}

void WebViewDelegate::didParentStatus(WebView *source, int status, NativeView parent) {}

}  // close namespace blpwtk2

// vim: ts=4 et

