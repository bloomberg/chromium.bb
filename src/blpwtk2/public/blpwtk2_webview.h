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

#ifndef INCLUDED_BLPWTK2_WEBVIEW_H
#define INCLUDED_BLPWTK2_WEBVIEW_H

#include <blpwtk2_config.h>
#include <blpwtk2_stringref.h>
#include <v8.h>

namespace blpwtk2 {

class Blob;
class String;
class WebFrame;
class WebViewDelegate;

                        // =============
                        // class WebView
                        // =============

// This class represents a single WebView.  Instances of this class can be
// created using blpwtk2::Toolkit::createWebView().
//
// All methods on this class can only be invoked from the application's main
// thread.
class WebView
{
  public:
    struct InputEvent {
        HWND hwnd;
        UINT message;
        WPARAM wparam;
        LPARAM lparam;
        bool shiftKey;
        bool controlKey;
        bool altKey;
        bool metaKey;
        bool isKeyPad;
        bool isAutoRepeat;
        bool capsLockOn;
        bool numLockOn;
        bool isLeft;
        bool isRight;
    };

    virtual void destroy() = 0;
        // Destroy the WebView and release any resources.  Do not use this
        // WebView after calling this method.

    virtual WebFrame* mainFrame() = 0;
        // Return the main WebFrame for this WebView.  This can only be used
        // if this is an in-process WebView, and only from the renderer
        // thread. See blpwtk2_toolkit.h for an explanation about the threads.
        //
        // The main frame will not be available immediately after creating a
        // WebView, or loading a URL.  The application should wait for the
        // 'didFinishLoad()' callback to be invoked on the WebViewDelegate
        // before trying to access the main frame.

    virtual int loadUrl(const StringRef& url) = 0;
        // Load the specified 'url' into this WebView, replacing whatever
        // contents are currently in this WebView.

    virtual void loadInspector(unsigned int pid, int routingId) = 0;
        // Load an inspector for the specified 'inspectedView' into this
        // WebView, replacing whatever contents are currently in this
        // WebView.  This method depends on the blpwtk2_devtools pak file
        // being present in the same directory as blpwtk2.dll.  This can be
        // checked using the blpwtk2::Toolkit::hasDevTools() method.  The
        // behavior if this method is undefined unless
        // blpwtk2::Toolkit::hasDevTools() returns true.

    virtual void inspectElementAt(const POINT& point) = 0;
        // The the element at the specified 'point'.  The behavior is
        // undefined unless 'loadInspector' has been called.

    virtual int goBack() = 0;
        // Go back.  This is a no-op if there is nothing to go back to.

    virtual int goForward() = 0;
        // Go forward.  This is a no-op if there is nothing to go forward to.

    virtual int reload() = 0;
        // Reload the contents of this WebView.  The behavior is undefined
        // unless 'loadUrl' or 'loadInspector' has been called.

    virtual void stop() = 0;
        // Stop loading the current contents.  This is a no-op if the WebView
        // is not loading any content.

    virtual void show() = 0;
        // Show this WebView.

    virtual void hide() = 0;
        // Hide this WebView.

    virtual int setParent(NativeView parent) = 0;
        // Reparent this WebView into the specified 'parent'.

    virtual void move(int left, int top, int width, int height) = 0;
        // Move this WebView to the specified 'left' and the specified 'top'
        // position, and resize it to have the specified 'width' and the
        // specified 'height'.

    virtual void cutSelection() = 0;
        // Remove the current selection, placing its contents into the system
        // clipboard.  This method has no effect if there is no selected
        // editable content.

    virtual void copySelection() = 0;
        // Copy the contents of the current selection into the system
        // clipboard.  This method has no effect if there is nothing selected.

    virtual void paste() = 0;
        // Paste the contents of the clipboard, replacing the current
        // selection, if any.  This method has no effect if the caret is not
        // inside editable content.

    virtual void deleteSelection() = 0;
        // Remove the current selection, without placing its contents into the
        // system clipboard.  This method has no effect if there is no
        // selected editable content.

    virtual void enableNCHitTest(bool enabled) = 0;
        // If set to 'true', the WebViewDelegate will be requested to provide
        // non-client hit testing.  The WebViewDelegate will also receive
        // notifications when the user drags inside non-client areas.  If this
        // is set to 'false', WebViews assume they do not include non-client
        // regions.

    virtual void onNCHitTestResult(int x, int y, int result) = 0;
        // When the WebViewDelegate's requestNCHitTest method is called, this
        // method should be invoked to provide the hit test result.  This
        // should only be called in response to 'requestNCHitTest' on the
        // delegate.

    virtual void performCustomContextMenuAction(int actionId) = 0;
        // Execute a custom context menu action provided by blink.  An example
        // of such custom action is the selection of a spellcheck suggestion.

    virtual void find(const StringRef& text,
                      bool             matchCase,
                      bool             forward = true) = 0;
        // Find text on the current page.  Sending the same search string
        // again implies continuing the search forward or backward depending
        // on the value of the third argument.

    virtual void stopFind(bool preserveSelection) = 0;
        // Stop the current find operation, optionally preserving the
        // selection of the active item.

    virtual void replaceMisspelledRange(const StringRef& text) = 0;
        // Replace current misspelling with 'text'. This should be called by
        // context menu handlers.

    virtual void rootWindowPositionChanged() = 0;
        // This function must be called by the application whenever the root
        // window containing this WebView receives WM_WINDOWPOSCHANGED.  This
        // notification is used to update the WebView's screen configuration.

    virtual void rootWindowSettingsChanged() = 0;
        // This function must be called by the application whenever the root
        // window containing this WebView receives WM_SETTINGCHANGE.  This
        // notification is used to update the WebView's screen configuration.

    virtual void handleInputEvents(const InputEvent *events, size_t eventsCount) = 0;
        // Inform the web widget of a sequence of input events

    virtual void setDelegate(WebViewDelegate* delegate) = 0;
        // Set a new web view delegate. From this point on, all callbacks will
        // be sent to the new delegate.

    virtual int getRoutingId() const = 0;
        // Return the routingId for this WebView.  This can only be used in
        // the RENDERER_MAIN mode.  The routingId is a unique number generated
        // for each WebView (it is used to route IPC messages to the host
        // process).

    virtual void setBackgroundColor(NativeColor color) = 0;
        // Set the default background color of the WebView. This function
        // should only be called after loadUrl().

    virtual void setRegion(NativeRegion region) = 0;
        // Sets the region of the webview's window. Painting and hit-testing
        // will be restricted to the region.

    virtual void clearTooltip() = 0;
        // TODO(imran)

    virtual v8::MaybeLocal<v8::Value> callFunction(v8::Local<v8::Function> func,
                                                   v8::Local<v8::Value> recv,
                                                   int argc,
                                                   v8::Local<v8::Value> *argv) = 0;
        // Call the specified V8 function with instrumentation

    virtual void setSecurityToken(v8::Isolate *isolate,
                                  v8::Local<v8::Value> token) = 0;
        // Set an optional value for the webview's security token. If the
        // security token it set, then it will be applied to the V8 script
        // contexts created for all subsequent URL loads in the main
        // frame and in any IFRAME.
        //
        // Calling this function does not modify the security token of
        // any already-loaded frames.

  protected:
    virtual ~WebView();
        // Destroy this WebView.  Note that clients of blpwtk2 should use the
        // 'destroy()' method, instead of deleting the object directly.
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBVIEW_H

// vim: ts=4 et

