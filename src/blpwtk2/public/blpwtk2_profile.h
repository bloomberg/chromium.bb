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

#ifndef INCLUDED_BLPWTK2_PROFILE_H
#define INCLUDED_BLPWTK2_PROFILE_H

#include <blpwtk2_config.h>
#include <blpwtk2_string.h>
#include <blpwtk2_webviewcreateparams.h>

namespace blpwtk2 {

class ProxyConfig;
class StringRef;
class WebView;
class WebViewDelegate;

// This enum must match ProxyConfigType in blpwtk2_process.mojom
enum class ProxyType {
    kDirect,
    kHTTP,
    kSOCKS4,
    kSOCKS5,
    kHTTPS
};


                        // =============
                        // class Profile
                        // =============

// A profile represents a collection of settings that are used to control how
// WebViews operate.  In a browser, multiple profiles may exist simultaneously,
// where each profile has a different data directory for cookies/cache etc.
// A profile may be provided upon creation of a WebView.  A profile may have
// multiple WebViews associated with it, but a WebView can only be associated
// with a single profile.  If a profile is not provided when creating a
// WebView, blpwtk2 will use a default profile, which is incognito and uses the
// system proxy configuration.
// Note: Right now, WebViews that have affinity to a particular renderer
//       process must use the same Profile.
class Profile
{
  public:
    virtual void destroy() = 0;
        // Destroy this profile.  Note that all WebViews created from this
        // profile must be destroyed before the profile is destroyed.  The
        // behavior is undefined if this Profile object is used after
        // 'destroy' has been called.

    virtual String createHostChannel(unsigned int     pid,
                                     bool             isolated,
                                     const StringRef& profileDir) = 0;
        // Ask the browser to create another ProcessHost and return a channel
        // for it.  The calling renderer can then pass this channel string
        // to another process so that it can connect to the newly created
        // ProcessHost.

    virtual String registerNativeViewForStreaming(NativeView view) = 0;
        // TODO(imran)

    virtual String registerScreenForStreaming(NativeScreen screen) = 0;
        // TODO(imran)

    virtual void createWebView(
        WebViewDelegate            *delegate = 0,
        const WebViewCreateParams&  params = WebViewCreateParams()) = 0;
        // Create a WebView.  If a 'delegate' is provided, the delegate will
        // receive callbacks from this WebView.

    virtual void addHttpProxy(ProxyType        type,
                              const StringRef& host,
                              int              port) = 0;
        // Add a new proxy host 'host':'port'.  This will cause the browser
        // to direct HTTP traffic to 'host':'port' via the 'type' protocol.

    virtual void addHttpsProxy(ProxyType        type,
                               const StringRef& host,
                               int              port) = 0;
        // Add a new proxy host 'host':'port'.  This will cause the browser
        // to direct HTTPS traffic to 'host':'port' via the 'type' protocol.

    virtual void addFtpProxy(ProxyType        type,
                             const StringRef& host,
                             int              port) = 0;
        // Add a new proxy host 'host':'port'.  This will cause the browser
        // to direct FTP traffic to 'host':'port' via the 'type' protocol.

    virtual void addFallbackProxy(ProxyType        type,
                                  const StringRef& host,
                                  int              port) = 0;
        // Add a new proxy host 'host':'port'.  This will cause the browser
        // to direct all other traffic (that is not HTTP, HTTPS, nor FTP) to
        // 'host':'port' via the 'type' protocol.

    virtual void clearHttpProxies() = 0;
        // Clear the registered HTTP proxies.

    virtual void clearHttpsProxies() = 0;
        // Clear the registered HTTPS proxies.

    virtual void clearFtpProxies() = 0;
        // Clear the registered FTP proxies.

    virtual void clearFallbackProxies() = 0;
        // Clear the registered fallback proxies.

    virtual void addBypassRule(const StringRef& rule) = 0;
        // Register an IP destination (in the form of hostname:port) to a
        // proxy blacklist.  This will cause the browser to always skip over
        // the registered proxy server if the destination IP address matches
        // the rule.

    virtual void clearBypassRules() = 0;
        // Clear the proxy blacklist.

    virtual void setPacUrl(const StringRef& url) = 0;



    // patch section: spellcheck


    // patch section: printing
    virtual void setDefaultPrinter(const StringRef& name) = 0;
        // Sets the printer to use by default


    // patch section: diagnostics


    // patch section: embedder ipc


    // patch section: web cache



  protected:
    virtual ~Profile();
        // Destroy this Profile object.  Note that embedders of blpwtk2 should
        // use the 'destroy()' method instead of deleting the object directly.
        // This ensures that the same allocator that was used to create the
        // Profile object is also used to delete the object.
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_PROFILE_H

// vim: ts=4 et

