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

#ifndef INCLUDED_BLPWTK2_TOOLKIT_H
#define INCLUDED_BLPWTK2_TOOLKIT_H

// The original thread model used in upstream chromium is:
//   * browser-main-thread: used to control the browser UI.  This is
//                          responsible for creating browser tabs, windows,
//                          etc.  It is also responsible for creating the
//                          windows in which webkit renders content.  This is
//                          known as the BrowserMain thread.  However, it does
//                          not run webkit/v8.  This is the main application
//                          thread.
//
//   * browser-background-threads: used to control all kinds of things like IO,
//                                 sub-process management, db, caching, gpu
//                                 coordination, etc.
//                                 See browser_thread.h in the content module
//                                 for more details.
//
//   * render-thread: normally this thread runs in a separate process.
//                    However, if "--single-process" is specified on the
//                    command-line, this thread is run as a background thread
//                    in the same process as the browser.  However, the
//                    single-process mode is not officially supported by
//                    chromium and only exists for debugging purposes.  This
//                    thread runs webkit/v8.
//
// The render-thread makes sync calls to the browser threads.  However, browser
// threads must *not* make sync calls to the render-thread.  See
// https://sites.google.com/a/chromium.org/dev/developers/design-documents/inter-process-communication
//
// Since we want to use webkit/v8 in our application's main thread, we added a
// new thread model called 'RENDERER_MAIN', where the render-thread is on the
// application's main thread.
//
// In this mode, we create a second thread to run browser-main, which spawns
// all the other browser background threads.  When we create a blpwtk2::WebView
// (from the application's main thread, i.e. the render-thread), we post a
// message to the secondary browser-main thread, which sets up the window for
// the web content.  If it is an in-process WebView, then the browser thread
// will post back to the render thread to setup the blink::WebView, otherwise
// it will post to an external renderer process.
//
// Some caveats of this design:
//   * the window that hosts web contents must never be exposed to users of
//     blpwtk2, because it belongs to a different thread (and is created and
//     destroyed asynchronously).  Any window-ey operation we need on the
//     WebView (like show/hide/etc) should be a method on blpwtk2::WebView,
//     which posts to the browser-main thread.  See blpwtk2_webviewproxy where
//     this is done.
//
//   * our WebView<->WebViewDelegate interfaces must be completely asynchronous
//     (i.e. there can be no return values).  Anything that needs to be sent
//     back must be done via callbacks.
//
//   * the WebView::mainFrame() is not available immediately after creating the
//     WebView, because you have to wait for the post to the browser-main
//     thread, and post back to the render thread.  In general, you should wait
//     for the first didFinishLoad() callback on your WebViewDelegate, before
//     attempting to access WebView::mainFrame().

#include <blpwtk2_config.h>

#include <blpwtk2_webviewcreateparams.h>
#include <v8.h>
#include <blpwtk2_stringref.h>

namespace blpwtk2 {

class EmbedderHeapTracer;
class Profile;
class String;
class StringRef;
class WebView;
class WebViewDelegate;
class WebViewHostObserver;
class ProcessHostDelegate;

                        // =============
                        // class Toolkit
                        // =============

// This interface can be used to create profiles and WebViews.  A single
// instance of this class can be created using the 'ToolkitFactory'.
//
// Once a tookit is created using the factory, the application must call
// preHandleMessage() and postHandleMessage() for each message within
// the application message loop. Here is an example code:
///<code>
//     MSG msg;
//     while(GetMessage(&msg, NULL, 0, 0) > 0) {
//         if (!toolkit->preHandleMessage(&msg)) {
//             TranslateMessage(&msg);
//             DispatchMessage(&msg);
//         }
//         toolkit->postHandleMessage(&msg);
//     }
///</code>
// The behavior is undefined if these functions are not invoked as shown
// in the code snippet above.  If 'preHandleMessage' returns true, this
// means that blpwtk2 has consumed the message, and it should not be
// dispatched through the normal Windows mechanism.  However, the
// application must still call 'postHandleMessage'.

class Toolkit {
  public:
    virtual bool hasDevTools() = 0;
        // Return true if the blpwtk2_devtools pak file was detected and has
        // been loaded.  This method determines whether
        // blpwtk2::WebView::loadInspector can be used.  Note that
        // applications should not cache this value because the pak file is
        // only loaded when a WebView has been created, which may happen
        // asynchronously if using the RENDERER_MAIN thread mode.

    virtual void destroy() = 0;
        // Destroy this Toolkit object.  This will shutdown all threads and
        // block until all threads have joined.  The behavior is undefined if
        // this Toolkit object is used after 'destroy' has been called.  The
        // behavior is also undefined if there are any WebViews or Profiles
        // that haven't been destroyed.

    virtual String createHostChannel(int              pid,
                                     bool             isolated,
                                     const StringRef& dataDir) = 0;
        // Create a new process host for use with process 'pid'.  The host
        // will remain waiting for 'pid' to connect to it.  The returned
        // string contains serialized information that can be fed into
        // an instance of ToolkitImpl in the process 'pid' to let it connect
        // to the process host.

    virtual Profile *getProfile(int pid, bool launchDevToolsServer) = 0;
        // Get a new instance of a profile for process 'pid'.  This method can
        // be called multiple times and by multiple processes.  The host will
        // keep a reference count of the profile for each process and will
        // dispose the ProcessHost when the last profile for the process is
        // destroyed.

    virtual bool preHandleMessage(const NativeMsg *msg) = 0;
        // Preprocesses the window message.  This must be called for every
        // message that the application's message loop processes, even if it
        // doesn't belong to blpwtk2.  This function returns true if the
        // message is processed and should not be dispatched by the
        // application.

    virtual void postHandleMessage(const NativeMsg *msg) = 0;
        // Postprocesses the window message.  This must be called for every
        // message that the application's message loop processes, even if it
        // doesn't belong to blpwtk2 and also if preHandleMessage returns true.

    virtual void addOriginToTrustworthyList(const StringRef& originString) = 0;
        // Adds the security origin specified by 'originString' to the list of
        // origins that blink considers 'trustworthy'.

    virtual void setWebViewHostObserver(WebViewHostObserver* observer) = 0;
        // Sets the observer that will be called by the browser thread when
        // WebView's are created and destroyed.

    virtual void setTraceThreshold(unsigned int timeoutMS) = 0;
        // If non-zero, defines the time threshold for enabling trace
        // (in milliseconds)



    // patch section: embedder ipc
    virtual void opaqueMessageToRendererAsync(int pid, const StringRef &message) = 0;

    virtual void setIPCDelegate(ProcessHostDelegate *delegate) = 0;


    // patch section: expose v8 platform


    // patch section: multi-heap tracer
    virtual int addV8HeapTracer(EmbedderHeapTracer *tracer) = 0;
        // Registers an embedder heap tracer with the multi heap tracer.
        // Once an embedder heap is registered, it will be notified of all
        // references during GC.  The embedder is expected to ignore any
        // reference wrapper with an embedder field that does not match the
        // return value of this function.

    virtual void removeV8HeapTracer(int embedder_id) = 0;
        // Unregisters an embedder heap trace from the multi heap tracer.



  protected:
    virtual ~Toolkit();
        // Destroy this Toolkit object.  Note that embedders of blpwtk2 should
        // use the 'destroy()' method instead of deleting the object directly.
        // This ensures that the same allocator that was used to create the
        // Toolkit object is also used to delete the object.
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_TOOLKIT_H

// vim: ts=4 et

