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

#ifndef INCLUDED_BLPWTK2_WEBVIEWDELEGATE_H
#define INCLUDED_BLPWTK2_WEBVIEWDELEGATE_H

#include <blpwtk2_config.h>
#include <blpwtk2_textdirection.h>

namespace blpwtk2 {

class ContextMenuParams;
class StringRef;
class WebView;
class MediaRequest;

                        // =====================
                        // class WebViewDelegate
                        // =====================

// This class can be implemented by the application to receive notifications
// for various events pertaining to a particular WebView.  The delegate is
// provided at the time the WebView is created, and must remain alive until the
// WebView is destroyed.
//
// All methods on the delegate are invoked in the application's main thread.
class BLPWTK2_EXPORT WebViewDelegate {
  public:
    virtual ~WebViewDelegate();

    virtual void created(WebView *source) = 0;
        // Invoked when the webview is created.

    virtual void didFinishLoad(WebView          *source,
                               const StringRef&  url);
        // Invoked when the main frame finished loading the specified 'url'.
        // This is the notification that guarantees that the 'mainFrame()'
        // method on the WebView can be used (for in-process WebViews, and in
        // the renderer thread).

    virtual void didFailLoad(WebView          *source,
                             const StringRef&  url);
        // Invoked when the main frame failed loading the specified 'url', or
        // was cancelled (e.g. window.stop() was called).

    virtual void focused(WebView *source);
        // Invoked when the WebView has gained focus.

    virtual void blurred(WebView *source);
        // Invoked when the WebView has lost focus.

    virtual void showContextMenu(WebView                  *source,
                                 const ContextMenuParams&  params);
        // Invoked when the user wants a context menu, for example upon
        // right-clicking inside the WebView.  The specified 'params' contain
        // information about the operations that may be performed on the
        // WebView (for example, Copy/Paste etc).

    virtual void requestNCHitTest(WebView *source);
        // Invoked when the WebView requests a non-client hit test.  This is
        // called only if non-client hit testing has been enabled via
        // 'enableNCHitTest' on the WebView.  The delegate is expected to
        // invoke 'onNCHitTestResult' on the WebView with the result of the
        // hit test.  Note that there will only be one outstanding hit test
        // per WebView.  The hit test should be performed using the current
        // mouse coordinates.

    virtual void ncDragBegin(WebView      *source,
                             int           hitTestCode,
                             const POINT&  startPoint);
        // Invoked when the user starts dragging inside a non-client region
        // in the WebView.  This is called only if non-client hit testing has
        // been enabled via 'enableNCHitTest' on the WebView.  The specified
        // 'startPoint' contains the mouse position where the drag began, in
        // screen coordinates.

    virtual void ncDragMove(WebView *source, const POINT& movePoint);
        // Invoked when the user moves the mouse while dragging inside a
        // non-client region in the WebView.  The specified 'movePoint'
        // contains the current mouse position in screen coordinates.

    virtual void ncDragEnd(WebView *source, const POINT& endPoint);
        // Invoked when the user releases the mouse after dragging inside a
        // non-client region in the WebView.  The specified 'endPoint'
        // contains the mouse position where the drag ended, in screen
        // coordinates.

    virtual void ncDoubleClick(WebView *source, const POINT& endPoint);
        // TODO(imran)

    virtual void findState(WebView *source,
                           int      numberOfMatches,
                           int      activeMatchOrdinal,
                           bool     finalUpdate);
        // Invoked response to a WebView::find method call to report
        // find-on-page status update.

    virtual void didParentStatus(WebView *source, int status, NativeView parent);
        // Invoked when the call to parent the webview specified by 'source' to the 
        // given parent is done
        // If successful, status 0 will be returned, otherwise error code from GetLassError()
        // will be return as status

    

    // patch section: devtools integration


    // patch section: performance monitor



};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBVIEWDELEGATE_H

// vim: ts=4 et

