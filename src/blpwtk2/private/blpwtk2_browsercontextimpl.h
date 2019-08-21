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

#ifndef INCLUDED_BLPWTK2_BROWSERCONTEXTIMPL_H
#define INCLUDED_BLPWTK2_BROWSERCONTEXTIMPL_H

#include <blpwtk2_config.h>
#include <blpwtk2_profile.h>
#include <blpwtk2_prefstore.h>

#include <base/memory/ref_counted.h>
#include <content/public/browser/browser_context.h>

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace net {
class ProxyConfig;
}

class PrefService;

namespace blpwtk2 {

class ResourceContextImpl;
class URLRequestContextGetterImpl;

                        // ========================
                        // class BrowserContextImpl
                        // ========================

// This is our implementation of the content::BrowserContext interface.  It is
// also the implementation of blpwtk2::Profile that lives on the browser-main
// thread.
//
// This class is responsible for providing net::URLRequestContextGetter
// objects.  URLRequestContextGetter are used to create a "request context" for
// each URL request.  The request context contains the necessary mechanisms to
// control caching/proxying etc.
class BrowserContextImpl final : public base::RefCounted<BrowserContextImpl>
                               , public content::BrowserContext
                               , public Profile
{
    // DATA
    std::unique_ptr<ResourceContextImpl> d_resourceContext;
    scoped_refptr<URLRequestContextGetterImpl> d_requestContextGetter;
    scoped_refptr<user_prefs::PrefRegistrySyncable> d_prefRegistry;
    std::unique_ptr<PrefService> d_prefService;
    scoped_refptr<PrefStore> d_userPrefs;
    std::unique_ptr<net::ProxyConfig> d_proxyConfig;
    int d_numWebViews;
    bool d_isDestroyed;
    bool d_devToolsServerLaunched;
    bool d_isOffTheRecord;

    friend class base::RefCounted<BrowserContextImpl>;
    ~BrowserContextImpl() final;

    DISALLOW_COPY_AND_ASSIGN(BrowserContextImpl);

  public:
    BrowserContextImpl(const std::string& dataDir);

    // Only called from the browser-main thread.
    URLRequestContextGetterImpl *requestContextGetter() const;
    void incrementWebViewCount();
    void decrementWebViewCount();
    void launchDevToolsServerIfNecessary();

    net::ProxyConfig *proxyConfig() { return d_proxyConfig.get(); }

    // Profile overrides, must only be called on the browser-main thread.
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
    void addHttpsProxy(ProxyType        type,
                       const StringRef& host,
                       int              port) override;
    void addFtpProxy(ProxyType        type,
                     const StringRef& host,
                     int              port) override;
    void addFallbackProxy(ProxyType        type,
                          const StringRef& host,
                          int              port) override;
    void clearHttpProxies() override;
    void clearHttpsProxies() override;
    void clearFtpProxies() override;
    void clearFallbackProxies() override;
    void addBypassRule(const StringRef& rule) override;
    void clearBypassRules() override;
    void setPacUrl(const StringRef& url) override;



    // patch section: spellcheck


    // patch section: printing
    void setDefaultPrinter(const StringRef& name) override;


    // patch section: diagnostics
    void dumpDiagnostics(DiagnosticInfoType type,
                         const StringRef&   path) override;

    std::string getGpuInfo() override;
    

    // patch section: embedder ipc


    // patch section: web cache



    // content::BrowserContext overrides
    std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
        const base::FilePath& partition_path) override;
    base::FilePath GetPath() const override;
    bool IsOffTheRecord() const override;
    content::ResourceContext *GetResourceContext() override;
    content::DownloadManagerDelegate *GetDownloadManagerDelegate() override;
    content::BrowserPluginGuestManager *GetGuestManager() override;
    storage::SpecialStoragePolicy *GetSpecialStoragePolicy() override;
    content::PushMessagingService *GetPushMessagingService() override;
    content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
    content::PermissionControllerDelegate* GetPermissionControllerDelegate() override;
    content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate() override;
    content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
    content::BackgroundSyncController *GetBackgroundSyncController() override;
    content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
        override;
    net::URLRequestContextGetter* CreateRequestContext(
        content::ProtocolHandlerMap* protocol_handlers,
        content::URLRequestInterceptorScopedVector request_interceptors)
        override;
    net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
        const base::FilePath&                       partition_path,
        bool                                        in_memory,
        content::ProtocolHandlerMap                *protocol_handlers,
        content::URLRequestInterceptorScopedVector  request_interceptors) override;
    net::URLRequestContextGetter *CreateMediaRequestContext() override;
    net::URLRequestContextGetter *CreateMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool                  in_memory) override;

	content::FontCollection* GetFontCollection() override;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_BROWSERCONTEXTIMPL_H

// vim: ts=4 et

