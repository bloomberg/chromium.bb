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

#ifndef INCLUDED_BLPWTK2_PROFILEPROXY_H
#define INCLUDED_BLPWTK2_PROFILEPROXY_H

#include <blpwtk2_config.h>
#include <blpwtk2_profile.h>
#include <blpwtk2/private/blpwtk2_process.mojom.h>
#include <blpwtk2/private/blpwtk2_webview.mojom.h>

#include <base/memory/ref_counted.h>
#include <base/macros.h>
#include <base/compiler_specific.h>
#include <ipc/ipc_sender.h>
#include <mojo/public/cpp/bindings/binding.h>

#include <string>

namespace base {
class MessageLoop;
}  // close namespace base

namespace blpwtk2 {

class ProxyConfig;
class WebViewProxy;
class WebViewDelegate;
class MainMessagePump;
class ProcessClientDelegate;

                        // =================
                        // class ProfileImpl
                        // =================

// Concrete implementation of the Profile interface when using the
// 'RENDERER_MAIN' thread-mode.
//
// This class is responsible for forwarding the creation of
// 'BrowserContextImpl' to the secondary browser-main thread, and forwarding
// all the 'Profile' methods to the 'BrowserContextImpl' in the browser-main
// thread.
//
// See blpwtk2_toolkit.h for an explanation about the threads.
class ProfileImpl : public Profile, public mojom::ProcessClient {
    // DATA
    mojom::ProcessHostPtr d_hostPtr;
    int d_numWebViews;
    unsigned int d_processId;
    MainMessagePump *d_pump;

    ~ProfileImpl() final;

    DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
    mojo::Binding<mojom::ProcessClient> d_binding;
    ProcessClientDelegate *d_ipcDelegate;
    void onBindProcessDone(mojom::ProcessClientRequest processClientRequest);

  public:
    static Profile *anyInstance();
        // Return any instance of a profile.  The toolkit can use this to send
        // a general request to the browser that is not specific to a profile.

    // CREATORS
    explicit ProfileImpl(MainMessagePump *pump,
                         unsigned int     pid,
                         bool             launchDevToolsServer);
        // Create a new instance of ProfileImpl.  If 'pid' is specified, the
        // profile will be bound to a RenderProcessHost that is coupled with
        // a RenderProcess running on process 'pid'.
        //
        // Note that the caller must specify a 'pid' for the first instance
        // of ProfileImpl in a process.  This is because another renderer
        // process or the browser process must've called createHostChannel()
        // on behalf of process 'pid'.  The createHostChannel() creates an
        // instance of ProcessHost that waits for a connection from the
        // process for which it was created.  The first Mojo connection
        // established by the first ProfileImpl acquires the ownership of the
        // ProcessHost.  The only way the host can associate the "to be bound"
        // ProcessHost with the new renderer process is if the 'pid' is
        // specified here.
        //
        // Also note that by not specifying a 'pid', some functionality of
        // the profile will be unavailable, specifically the ones that require
        // access to the browser context.

    void incrementWebViewCount();
        // Increment the webview count.  This method and its companion
        // decrementWebViewCount() method are called by the WebView class
        // upon creation and destruction.  This allows the profile to assert
        // that no associated webviews are alive when the profile itself is
        // about to be destroyed.

    void decrementWebViewCount();
        // Decrement the webview count.  This method and its companion
        // incrementWebViewCount() method are called by the WebView class
        // upon creation and destruction.  This allows the profile to assert
        // that no associated webviews are alive when the profile itself is
        // about to be destroyed.

    unsigned int getProcessId() const;
        // Returns the pid of the RenderProcess for which this profile is
        // associated with.

    // blpwtk2::Profile overrides
    void destroy() override;

    String createHostChannel(unsigned int     pid,
                             bool             isolated,
                             const StringRef& profileDir) override;

    String registerNativeViewForStreaming(NativeView view) override;
    String registerScreenForStreaming(NativeScreen screen) override;

    void createWebView(
        WebViewDelegate            *delegate,
        const WebViewCreateParams&  params = WebViewCreateParams()) override;

    void addHttpProxy(ProxyType        type,
                      const StringRef& host,
                      int              port) override;
        // Requires browser context

    void addHttpsProxy(ProxyType        type,
                       const StringRef& host,
                       int              port) override;
        // Requires browser context

    void addFtpProxy(ProxyType        type,
                     const StringRef& host,
                     int              port) override;
        // Requires browser context

    void addFallbackProxy(ProxyType        type,
                          const StringRef& host,
                          int              port) override;
        // Requires browser context

    void clearHttpProxies() override;
        // Requires browser context

    void clearHttpsProxies() override;
        // Requires browser context

    void clearFtpProxies() override;
        // Requires browser context

    void clearFallbackProxies() override;
        // Requires browser context

    void addBypassRule(const StringRef& rule) override;
        // Requires browser context

    void clearBypassRules() override;
        // Requires browser context

    void setPacUrl(const StringRef& url) override;



    // patch section: spellcheck


    // patch section: printing
    void setDefaultPrinter(const StringRef& name) override;


    // patch section: diagnostics
    void dumpDiagnostics(DiagnosticInfoType type,
                         const StringRef&   path) override;

    std::string getGpuInfo() override;


    // patch section: embedder ipc
    void opaqueMessageToBrowserAsync(const StringRef& msg) override;

    String opaqueMessageToBrowserSync(const StringRef& msg) override;

    void opaqueMessageToRendererAsync(const std::string& msg) override;

    void setIPCDelegate(ProcessClientDelegate *delegate) override;


    // patch section: web cache



};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_PROFILE_H

// vim: ts=4 et

