// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_request_context.h"

#include "build/build_config.h"

#include "base/file_path.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service.h"
#include "net/base/static_cookie_policy.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

TestShellRequestContext::TestShellRequestContext() {
  Init(FilePath(), net::HttpCache::NORMAL, false);
}

TestShellRequestContext::TestShellRequestContext(
    const FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  Init(cache_path, cache_mode, no_proxy);
}

void TestShellRequestContext::Init(
    const FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  set_cookie_store(new net::CookieMonster(NULL, NULL));
  set_cookie_policy(new net::StaticCookiePolicy());

  // hard-code A-L and A-C for test shells
  set_accept_language("en-us,en");
  set_accept_charset("iso-8859-1,*,utf-8");

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Use no proxy to avoid ProxyConfigServiceLinux.
  // Enabling use of the ProxyConfigServiceLinux requires:
  // -Calling from a thread with a TYPE_UI MessageLoop,
  // -If at all possible, passing in a pointer to the IO thread's MessageLoop,
  // -Keep in mind that proxy auto configuration is also
  //  non-functional on linux in this context because of v8 threading
  //  issues.
  // TODO(port): rename "linux" to some nonspecific unix.
  scoped_ptr<net::ProxyConfigService> proxy_config_service(
      new net::ProxyConfigServiceFixed(net::ProxyConfig()));
#else
  // Use the system proxy settings.
  scoped_ptr<net::ProxyConfigService> proxy_config_service(
      net::ProxyService::CreateSystemProxyConfigService(
          MessageLoop::current(), NULL));
#endif
  set_host_resolver(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    NULL, NULL));
  set_cert_verifier(new net::CertVerifier);
  set_proxy_service(net::ProxyService::CreateUsingSystemProxyResolver(
      proxy_config_service.release(), 0, NULL));
  set_ssl_config_service(net::SSLConfigService::CreateSystemSSLConfigService());

  set_http_auth_handler_factory(net::HttpAuthHandlerFactory::CreateDefault(
      host_resolver()));

  net::HttpCache::DefaultBackend* backend = new net::HttpCache::DefaultBackend(
      cache_path.empty() ? net::MEMORY_CACHE : net::DISK_CACHE,
      cache_path, 0, SimpleResourceLoaderBridge::GetCacheThread());

  net::HttpCache* cache =
      new net::HttpCache(host_resolver(), cert_verifier(), NULL, NULL,
                         proxy_service(), ssl_config_service(),
                         http_auth_handler_factory(), NULL, NULL, backend);

  cache->set_mode(cache_mode);
  set_http_transaction_factory(cache);

  set_ftp_transaction_factory(new net::FtpNetworkLayer(host_resolver()));

  blob_storage_controller_.reset(new webkit_blob::BlobStorageController());
}

TestShellRequestContext::~TestShellRequestContext() {
  delete ftp_transaction_factory();
  delete http_transaction_factory();
  delete http_auth_handler_factory();
  delete cookie_policy();
  delete cert_verifier();
  delete host_resolver();
}

const std::string& TestShellRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}
