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

#ifndef INCLUDED_BLPWTK2_STATICS_H
#define INCLUDED_BLPWTK2_STATICS_H

#include <blpwtk2_config.h>
#include <blpwtk2_threadmode.h>
#include <blpwtk2_toolkitcreateparams.h>

#include <base/memory/scoped_refptr.h>
#include <base/threading/platform_thread.h>

namespace base {
class MessageLoop;
class SingleThreadTaskRunner;
}  // close namespace base

namespace devtools_http_handler {
    class DevToolsHttpHandler;
}  // close namespace content

namespace blpwtk2 {

class ProcessHostManager;
class Profile;
class ResourceLoader;
class WebViewHostObserver;
class ToolkitDelegate;

// Hold any static variables.  This will be used to store global state that
// may be setup before ToolkitImpl is instantiated, or any other global
// variables, or pointers to global objects that may be used by multiple
// components.
struct Statics {
    // The thread mode selected by the application.  See blpwtk_toolkit.h
    // for an explanation about this.
    static ThreadMode threadMode;

    // The id of the application's main thread (i.e. the thread that can enter
    // the blpwtk2 library).
    static base::PlatformThreadId applicationMainThreadId;

    // The id of the browser main thread.  In ORIGINAL thread mode, this is
    // the same as applicationMainThreadId.
    static base::PlatformThreadId browserMainThreadId;

    // The optional in-process resource loader installed by the application.
    static ResourceLoader* inProcessResourceLoader;

    // TaskRunner for the browser main thread.
    static scoped_refptr<base::SingleThreadTaskRunner> browserMainTaskRunner;

    // The single WebViewHostObserver. It is called from the browser's main thread.
    static WebViewHostObserver* webViewHostObserver;

    // The external handler that will be invoked whenever a channel error occurs.
    static ToolkitCreateParams::ChannelErrorHandler channelErrorHandler;

    // Whether or not devtools is available.
    static bool hasDevTools;

    // Whether or not the in-process renderer is enabled.
    static bool isInProcessRendererEnabled;

    // Whether or not an optimization used when resizing a WebView from an
    // in-process renderer should be enabled.
    static bool inProcessResizeOptimizationDisabled;

    static ToolkitDelegate *toolkitDelegate;

    static bool rendererUIEnabled;

    // ====== some utility functions =============

    static bool isOriginalThreadMode()
    {
        return ThreadMode::ORIGINAL == threadMode;
    }

    static bool isRendererMainThreadMode()
    {
        return ThreadMode::RENDERER_MAIN == threadMode;
    }

    static bool isInApplicationMainThread()
    {
        return base::PlatformThread::CurrentId() == applicationMainThreadId;
    }

    static bool isInBrowserMainThread()
    {
        return base::PlatformThread::CurrentId() == browserMainThreadId;
    }

    static void initApplicationMainThread();
    static void initBrowserMainThread();

    static int getUniqueRoutingId();
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_STATICS_H

// vim: ts=4 et

