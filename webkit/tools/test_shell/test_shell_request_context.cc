// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_request_context.h"

#include "build/build_config.h"

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/threading/worker_pool.h"
#include "net/base/cert_verifier.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/host_resolver.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/cookies/cookie_monster.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_job_factory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebKitPlatformSupport.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/blob_url_request_job_factory.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_url_request_job_factory.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

TestShellRequestContext::TestShellRequestContext()
    : ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {
  Init(FilePath(), net::HttpCache::NORMAL, false);
}

TestShellRequestContext::TestShellRequestContext(
    const FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy)
    : ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {
  Init(cache_path, cache_mode, no_proxy);
}

void TestShellRequestContext::Init(
    const FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  storage_.set_cookie_store(new net::CookieMonster(NULL, NULL));
  storage_.set_server_bound_cert_service(new net::ServerBoundCertService(
      new net::DefaultServerBoundCertStore(NULL),
      base::WorkerPool::GetTaskRunner(true)));

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
  storage_.set_host_resolver(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    net::HostResolver::kDefaultRetryAttempts,
                                    NULL));
  storage_.set_cert_verifier(net::CertVerifier::CreateDefault());
  storage_.set_proxy_service(net::ProxyService::CreateUsingSystemProxyResolver(
      proxy_config_service.release(), 0, NULL));
  storage_.set_ssl_config_service(
      new net::SSLConfigServiceDefaults);

  storage_.set_http_auth_handler_factory(
      net::HttpAuthHandlerFactory::CreateDefault(host_resolver()));
  storage_.set_http_server_properties(
      new net::HttpServerPropertiesImpl);

  net::HttpCache::DefaultBackend* backend = new net::HttpCache::DefaultBackend(
      cache_path.empty() ? net::MEMORY_CACHE : net::DISK_CACHE,
      cache_path, 0, SimpleResourceLoaderBridge::GetCacheThread());

  net::HttpCache* cache =
      new net::HttpCache(host_resolver(),
                         cert_verifier(),
                         server_bound_cert_service(),
                         NULL, /* transport_security_state */
                         proxy_service(),
                         "",  /* ssl_session_cache_shard */
                         ssl_config_service(),
                         http_auth_handler_factory(),
                         NULL,  /* network_delegate */
                         http_server_properties(),
                         NULL,  /* netlog */
                         backend,
                         "" /* trusted_spdy_proxy */ );

  cache->set_mode(cache_mode);
  storage_.set_http_transaction_factory(cache);

  storage_.set_ftp_transaction_factory(
      new net::FtpNetworkLayer(host_resolver()));

  blob_storage_controller_.reset(new webkit_blob::BlobStorageController());
  file_system_context_ = static_cast<SimpleFileSystem*>(
      WebKit::webKitPlatformSupport()->fileSystem())->file_system_context();

  net::URLRequestJobFactory* job_factory = new net::URLRequestJobFactory;
  job_factory->SetProtocolHandler(
      "blob",
      new webkit_blob::BlobProtocolHandler(
          blob_storage_controller_.get(),
          SimpleResourceLoaderBridge::GetIoThread()));
  job_factory->SetProtocolHandler(
      "filesystem",
      fileapi::CreateFileSystemProtocolHandler(file_system_context_.get()));
  storage_.set_job_factory(job_factory);
}

TestShellRequestContext::~TestShellRequestContext() {
}

const std::string& TestShellRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}
