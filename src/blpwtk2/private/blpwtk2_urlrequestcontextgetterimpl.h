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

#ifndef INCLUDED_BLPWTK2_URLREQUESTCONTEXTGETTERIMPL_H
#define INCLUDED_BLPWTK2_URLREQUESTCONTEXTGETTERIMPL_H

#include <blpwtk2_config.h>

#include <base/files/file_path.h>
#include <base/memory/ref_counted.h>
#include <base/synchronization/lock.h>
#include <content/public/browser/browser_context.h>  // for ProtocolHandlerMap
#include <net/cookies/cookie_monster.h>
#include <net/url_request/url_request_context_getter.h>
#include <net/url_request/url_request_job_factory.h>

namespace base {
    class SequencedTaskRunner;
}

namespace net {
    class ProxyConfig;
    class ProxyConfigService;
    class ProxyResolutionService;
    class URLRequestContext;
    class URLRequestContextStorage;
}  // close namespace net

namespace blpwtk2 {

// An instance of this is created for each BrowserContextImpl.  This
// implementation is mostly a copy of ShellURLRequestContextGetter from
// content_shell, but cleaned up and modified a bit to make it do what we need.
//
// This implementation has methods to change the proxy configuration, so we
// hold on to the proxy service ourselves, rather than putting it in the
// URLRequestContextStorage.
//
// This object is created on the browser-main thread, but also lives on the IO
// thread.  Thread-safe destruction is maintained because the base interface
// derives from RefCountedThreadSafe.
class URLRequestContextGetterImpl : public net::URLRequestContextGetter {
public:
    URLRequestContextGetterImpl(const base::FilePath& path,
                                bool diskCacheEnabled,
                                bool cookiePersistenceEnabled);

    // Called on the UI thread.
    void setProxyConfig(const net::ProxyConfig& config);
    void useSystemProxyConfig();
    void setProtocolHandlers(content::ProtocolHandlerMap* protocolHandlers,
                             content::URLRequestInterceptorScopedVector requestInterceptors);
    const base::FilePath& path() const { return d_path; }
    bool diskCacheEnabled() const { return d_diskCacheEnabled; }
    bool cookiePersistenceEnabled() const { return d_cookiePersistenceEnabled; }

    // net::URLRequestContextGetter implementation.
    net::URLRequestContext* GetURLRequestContext() override;
    scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner() const override;

private:
    ~URLRequestContextGetterImpl() final;
    // Called on the IO thread.
    void initialize();
    void updateProxyConfig(
        net::ProxyConfigService* proxyConfigService);

    std::unique_ptr<net::ProxyResolutionService> d_proxyService;
    scoped_refptr<net::CookieMonster::PersistentCookieStore> d_cookieStore;
    std::unique_ptr<net::URLRequestContextStorage> d_storage;
    std::unique_ptr<net::URLRequestContext> d_urlRequestContext;

    // accessed on both UI and IO threads
    base::Lock d_protocolHandlersLock;
    content::ProtocolHandlerMap d_protocolHandlers;
    content::URLRequestInterceptorScopedVector d_requestInterceptors;
    bool d_gotProtocolHandlers;

    base::FilePath d_path;
    bool d_diskCacheEnabled;
    bool d_cookiePersistenceEnabled;
    bool d_wasProxyInitialized;

    scoped_refptr<base::SequencedTaskRunner> d_background_task_runner;

    DISALLOW_COPY_AND_ASSIGN(URLRequestContextGetterImpl);
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_URLREQUESTCONTEXTGETTERIMPL_H

// vim: ts=4 et

