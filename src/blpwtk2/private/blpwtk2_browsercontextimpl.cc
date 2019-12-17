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

#include <blpwtk2_browsercontextimpl.h>

#include <blpwtk2_processhostimpl.h>
#include <blpwtk2_resourcecontextimpl.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_urlrequestcontextgetterimpl.h>
#include <blpwtk2_devtoolsmanagerdelegateimpl.h>
#include <blpwtk2_desktopstreamsregistry.h>
#include <blpwtk2_webviewproperties.h>
#include <blpwtk2_webviewimpl.h>
#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_requestinterceptorimpl.h>



// patch section: diagnostics


// patch section: custom fonts



#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>  // for CHECK
#include <components/prefs/pref_service_factory.h>
#include <components/prefs/pref_service.h>
#include <base/threading/thread_restrictions.h>
#include <chrome/common/pref_names.h>
#include <content/public/browser/browser_thread.h>
#include <content/public/browser/render_process_host.h>
#include <content/public/browser/storage_partition.h>
#include <components/keyed_service/content/browser_context_dependency_manager.h>
#include <components/pref_registry/pref_registry_syncable.h>
#include <components/user_prefs/user_prefs.h>
#include <net/proxy_resolution/proxy_config.h>
#include <printing/backend/print_backend.h>

namespace blpwtk2 {
namespace {
bool g_devToolsServerLaunched;
}


                        // ------------------------
                        // class BrowserContextImpl
                        // ------------------------

BrowserContextImpl::BrowserContextImpl(const std::string& dataDir)
    : d_numWebViews(0)
    , d_isDestroyed(false)
    , d_devToolsServerLaunched(false)
    , d_isOffTheRecord(dataDir.empty())
{
    base::FilePath path;

    if (!d_isOffTheRecord) {
        // allow IO during creation of data directory
        base::ThreadRestrictions::ScopedAllowIO allowIO;

        path = base::FilePath::FromUTF8Unsafe(dataDir);
        if (!base::PathExists(path)) {
            base::CreateDirectory(path);
        }
    }
    else {
        // It seems that even incognito browser contexts need to return a
        // valid data path.  Not providing one causes a DCHECK failure in
        // fileapi::DeviceMediaAsyncFileUtil::Create.
        // For now, just create a temporary directory for this.

        // allow IO during creation of temporary directory
        base::ThreadRestrictions::ScopedAllowIO allowIO;
        base::CreateNewTempDirectory(L"blpwtk2_", &path);
    }

    d_requestContextGetter =
        new URLRequestContextGetterImpl(path, false, false);

    {
        // Initialize prefs for this context.
        d_prefRegistry = new user_prefs::PrefRegistrySyncable();
        d_userPrefs = new PrefStore();

        PrefServiceFactory factory;
        factory.set_user_prefs(d_userPrefs);
        d_prefService = factory.Create(d_prefRegistry.get());
        user_prefs::UserPrefs::Set(this, d_prefService.get());

    }

    // GetInstance() should be called here for all service factories.  This
    // will cause the constructor of the class to register itself to the
    // dependency manager.
    {
    }

    // Register this context with the dependency manager.
    BrowserContextDependencyManager* dependencyManager = BrowserContextDependencyManager::GetInstance();
    dependencyManager->CreateBrowserContextServices(this);

    // Register our preference registry to the dependency manager.
    dependencyManager->RegisterProfilePrefsForServices(d_prefRegistry.get());

    // Initialize the browser context.  During this initialization, the
    // context will ask the dependency manager to register profile
    // preferences for all services associated with this context.
    content::BrowserContext::Initialize(this, base::FilePath());

    // GetForContext(this) should be called here for all service factories.
    // This will create an instance of the service for this context.  It's
    // possible for the service to do lookups of its preference keys in the
    // preference service.  For this reason, it is important to call this
    // after content::BrowserContext::Initialize().
    {
    }

    d_proxyConfig = std::make_unique<net::ProxyConfig>();
    d_proxyConfig->proxy_rules().type =
        net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
}

BrowserContextImpl::~BrowserContextImpl()
{
    NotifyWillBeDestroyed(this);
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(0 == d_numWebViews);
    DCHECK(!d_isDestroyed);

    if (d_devToolsServerLaunched) {
        DevToolsManagerDelegateImpl::StopHttpHandler();
        d_devToolsServerLaunched = false;
        g_devToolsServerLaunched = false;
    }

    BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(this);

    d_userPrefs = 0;
    d_prefRegistry = 0;

    content::BrowserThread::DeleteSoon(content::BrowserThread::UI,
                                       FROM_HERE,
                                       d_prefService.release());

    if (d_resourceContext.get()) {
        content::BrowserThread::DeleteSoon(content::BrowserThread::IO,
                                           FROM_HERE,
                                           d_resourceContext.release());
    }

    if (d_isOffTheRecord) {
        // Delete the temporary directory that we created in the constructor.

        // allow IO during deletion of temporary directory
        base::ThreadRestrictions::ScopedAllowIO allowIO;
        DCHECK(base::PathExists(d_requestContextGetter->path()));
        base::DeleteFile(d_requestContextGetter->path(), true);
    }

    d_requestContextGetter = 0;
    d_isDestroyed = true;

    ShutdownStoragePartitions();
}

URLRequestContextGetterImpl* BrowserContextImpl::requestContextGetter() const
{
    DCHECK(!d_isDestroyed);
    return d_requestContextGetter.get();
}

void BrowserContextImpl::incrementWebViewCount()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);
    ++d_numWebViews;
}

void BrowserContextImpl::decrementWebViewCount()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(0 < d_numWebViews);
    DCHECK(!d_isDestroyed);
    --d_numWebViews;
}

void BrowserContextImpl::launchDevToolsServerIfNecessary()
{
    if (!d_devToolsServerLaunched && !g_devToolsServerLaunched) {
        // Start the DevTools server.  The browser context is provided so that
        // it can extract the profile directory from it.  If the context does
        // have an associated profile directory, the DevTools agent host
        // writes the HTTP server port number into well known file.  Telemetry
        // can pick up the port number from this file and establish a
        // connection to the DevTools server.
        d_devToolsServerLaunched = true;

        // The DevTools server is only launched for the first browser context
        // in the process.
        g_devToolsServerLaunched = true;
        DevToolsManagerDelegateImpl::StartHttpHandler(this);
    }
}

// Profile overrides
void BrowserContextImpl::destroy()
{
    // This is a no-op because BrowserContextImplManager needs to keep the
    // BrowserContextImpl objects alive until Toolkit is destroyed.
}

String BrowserContextImpl::createHostChannel(unsigned int     pid,
                                             bool             isolated,
                                             const StringRef& profileDir)
{
    // TODO(imran): use toolkit to create host channel for now.
    return String();
}

String BrowserContextImpl::registerNativeViewForStreaming(NativeView view)
{
    std::string media_id =
        DesktopStreamsRegistry::RegisterNativeViewForStreaming(view);
    return String(media_id);
}

String BrowserContextImpl::registerScreenForStreaming(NativeScreen screen)
{
    std::string media_id =
        DesktopStreamsRegistry::RegisterScreenForStreaming(screen);
    return String(media_id);
}

void BrowserContextImpl::createWebView(
    WebViewDelegate            *delegate,
    const WebViewCreateParams&  params)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(Statics::isOriginalThreadMode());

    int hostId;
    BrowserContextImpl *context;
    ProcessHostImpl::getHostId(&hostId, &context, params.rendererAffinity());

    WebViewProperties properties;

    properties.takeKeyboardFocusOnMouseDown =
        params.takeKeyboardFocusOnMouseDown();
    properties.takeLogicalFocusOnMouseDown =
        params.takeLogicalFocusOnMouseDown();
    properties.activateWindowOnMouseDown =
        params.activateWindowOnMouseDown();
    properties.domPasteEnabled =
        params.domPasteEnabled();
    properties.javascriptCanAccessClipboard =
        params.javascriptCanAccessClipboard();
    properties.rerouteMouseWheelToAnyRelatedWindow =
        params.rerouteMouseWheelToAnyRelatedWindow();

    WebView *webView =
        new WebViewImpl(delegate,                  // delegate
                        0,                         // parent window
                        context? context: this,    // browser context
                        hostId,                    // host affinity
                        false,                     // initially visible
                        properties);               // properties

    delegate->created(webView);
}

static net::ProxyServer makeServer(ProxyType          type,
                                   const std::string& host,
                                   int                port)
{
    net::ProxyServer::Scheme scheme = net::ProxyServer::SCHEME_INVALID;
    switch (type) {
    case ProxyType::kDirect:
        scheme = net::ProxyServer::SCHEME_DIRECT;
        break;
    case ProxyType::kHTTP:
        scheme = net::ProxyServer::SCHEME_HTTP;
        break;
    case ProxyType::kSOCKS4:
        scheme = net::ProxyServer::SCHEME_SOCKS4;
        break;
    case ProxyType::kSOCKS5:
        scheme = net::ProxyServer::SCHEME_SOCKS5;
        break;
    case ProxyType::kHTTPS:
        scheme = net::ProxyServer::SCHEME_HTTPS;
        break;
    }

    DCHECK(net::ProxyServer::SCHEME_INVALID != scheme);
    DCHECK(0 < port);

    return net::ProxyServer(scheme, net::HostPortPair(host, port));
}

void BrowserContextImpl::addHttpProxy(ProxyType        type,
                                      const StringRef& host,
                                      int              port)
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->proxy_rules().proxies_for_http.AddProxyServer(
        makeServer(type, std::string(host.data(), host.size()), port));

    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}

void BrowserContextImpl::addHttpsProxy(ProxyType        type,
                                       const StringRef& host,
                                       int              port)
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->proxy_rules().proxies_for_https.AddProxyServer(
        makeServer(type, std::string(host.data(), host.size()), port));

    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}

void BrowserContextImpl::addFtpProxy(ProxyType        type,
                                     const StringRef& host,
                                     int              port)
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->proxy_rules().proxies_for_ftp.AddProxyServer(
        makeServer(type, std::string(host.data(), host.size()), port));

    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}

void BrowserContextImpl::addFallbackProxy(ProxyType        type,
                                          const StringRef& host,
                                          int              port)
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->proxy_rules().fallback_proxies.AddProxyServer(
        makeServer(type, std::string(host.data(), host.size()), port));

    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}

void BrowserContextImpl::clearHttpProxies()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->proxy_rules().proxies_for_http.Clear();
    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}

void BrowserContextImpl::clearHttpsProxies()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->proxy_rules().proxies_for_https.Clear();
    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}

void BrowserContextImpl::clearFtpProxies()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->proxy_rules().proxies_for_ftp.Clear();
    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}

void BrowserContextImpl::clearFallbackProxies()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->proxy_rules().fallback_proxies.Clear();
    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}

void BrowserContextImpl::addBypassRule(const StringRef& rule)
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->proxy_rules().bypass_rules.AddRuleFromString(
            std::string(rule.data(), rule.size()));
    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}

void BrowserContextImpl::clearBypassRules()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->proxy_rules().bypass_rules.Clear();
    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}

void BrowserContextImpl::setPacUrl(const StringRef& url)
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    d_proxyConfig->set_pac_url(GURL(std::string(url.data(), url.size())));
    d_requestContextGetter->setProxyConfig(*d_proxyConfig);
}



// patch section: spellcheck


// patch section: printing


// patch section: diagnostics


// patch section: embedder ipc


// patch section: web cache



// content::BrowserContext overrides
std::unique_ptr<content::ZoomLevelDelegate>
BrowserContextImpl::CreateZoomLevelDelegate(
        const base::FilePath& partition_path)
{
    return std::unique_ptr<content::ZoomLevelDelegate>();
}

base::FilePath BrowserContextImpl::GetPath() const
{
    DCHECK(!d_isDestroyed);
    return d_requestContextGetter->path();
}

bool BrowserContextImpl::IsOffTheRecord() const
{
    DCHECK(!d_isDestroyed);
    return d_isOffTheRecord;
}

content::ResourceContext* BrowserContextImpl::GetResourceContext()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    DCHECK(!d_isDestroyed);

    if (!d_resourceContext.get()) {
        d_resourceContext.reset(
            new ResourceContextImpl(d_requestContextGetter.get()));
    }
    return d_resourceContext.get();
}

content::DownloadManagerDelegate*
BrowserContextImpl::GetDownloadManagerDelegate()
{
    return nullptr;
}

content::BrowserPluginGuestManager *BrowserContextImpl::GetGuestManager()
{
    return nullptr;
}

storage::SpecialStoragePolicy *BrowserContextImpl::GetSpecialStoragePolicy()
{
    return nullptr;
}

content::PushMessagingService *BrowserContextImpl::GetPushMessagingService()
{
    return nullptr;
}

content::SSLHostStateDelegate* BrowserContextImpl::GetSSLHostStateDelegate()
{
    return nullptr;
}

content::PermissionControllerDelegate* BrowserContextImpl::GetPermissionControllerDelegate()
{
    return nullptr;
}

content::ClientHintsControllerDelegate* BrowserContextImpl::GetClientHintsControllerDelegate()
{
    return nullptr;
}

content::BackgroundFetchDelegate* BrowserContextImpl::GetBackgroundFetchDelegate()
{
    return nullptr;
}

content::BackgroundSyncController*
BrowserContextImpl::GetBackgroundSyncController()
{
    return nullptr;
}

content::BrowsingDataRemoverDelegate*
BrowserContextImpl::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

net::URLRequestContextGetter* BrowserContextImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors)
{
    request_interceptors.push_back(std::make_unique<RequestInterceptorImpl>());
    requestContextGetter()->setProtocolHandlers(
            protocol_handlers, std::move(request_interceptors));
    return requestContextGetter();
}

net::URLRequestContextGetter*
BrowserContextImpl::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors)
{
    return nullptr;
}

net::URLRequestContextGetter* BrowserContextImpl::CreateMediaRequestContext()
{
    return requestContextGetter();
}

net::URLRequestContextGetter*
BrowserContextImpl::CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path, bool in_memory)
{
    return nullptr;
}

}  // close namespace blpwtk2

// vim: ts=4 et

