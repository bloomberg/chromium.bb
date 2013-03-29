// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_request_context.h"

#include "build/build_config.h"

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/default_server_bound_cert_store.h"
#include "net/ssl/server_bound_cert_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/blob_url_request_job_factory.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_url_request_job_factory.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/user_agent/user_agent.h"

class TestShellHttpUserAgentSettings : public net::HttpUserAgentSettings {
 public:
  TestShellHttpUserAgentSettings() {}
  virtual ~TestShellHttpUserAgentSettings() {}

  // Hard-code Accept-Language for test shells.
  virtual std::string GetAcceptLanguage() const OVERRIDE {
    return "en-us,en";
  }

  virtual std::string GetUserAgent(const GURL& url) const OVERRIDE {
    return webkit_glue::GetUserAgent(url);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestShellHttpUserAgentSettings);
};

TestShellRequestContext::TestShellRequestContext()
    : ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {
  Init(base::FilePath(), net::HttpCache::NORMAL, false);
}

TestShellRequestContext::TestShellRequestContext(
    const base::FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy)
    : ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {
  Init(cache_path, cache_mode, no_proxy);
}

void TestShellRequestContext::Init(
    const base::FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  storage_.set_cookie_store(new net::CookieMonster(NULL, NULL));
  storage_.set_server_bound_cert_service(new net::ServerBoundCertService(
      new net::DefaultServerBoundCertStore(NULL),
      base::WorkerPool::GetTaskRunner(true)));

  storage_.set_http_user_agent_settings(new TestShellHttpUserAgentSettings);

  // Use no proxy; it's not needed for testing and just breaks things.
  scoped_ptr<net::ProxyConfigService> proxy_config_service(
      new net::ProxyConfigServiceFixed(net::ProxyConfig()));

  storage_.set_host_resolver(net::HostResolver::CreateDefaultResolver(NULL));
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

  net::HttpNetworkSession::Params network_session_params;
  network_session_params.host_resolver = host_resolver();
  network_session_params.cert_verifier = cert_verifier();
  network_session_params.server_bound_cert_service =
      server_bound_cert_service();
  network_session_params.proxy_service = proxy_service();
  network_session_params.ssl_config_service = ssl_config_service();
  network_session_params.http_auth_handler_factory =
      http_auth_handler_factory();
  network_session_params.http_server_properties = http_server_properties();
  network_session_params.host_resolver = host_resolver();

  net::HttpCache* cache = new net::HttpCache(
      network_session_params, backend);
  cache->set_mode(cache_mode);
  storage_.set_http_transaction_factory(cache);

  storage_.set_ftp_transaction_factory(
      new net::FtpNetworkLayer(host_resolver()));

  blob_storage_controller_.reset(new webkit_blob::BlobStorageController());
  file_system_context_ = static_cast<SimpleFileSystem*>(
      WebKit::Platform::current()->fileSystem())->file_system_context();

  net::URLRequestJobFactoryImpl* job_factory =
      new net::URLRequestJobFactoryImpl();
  job_factory->SetProtocolHandler(
      "blob",
      new webkit_blob::BlobProtocolHandler(
          blob_storage_controller_.get(),
          file_system_context_,
          SimpleResourceLoaderBridge::GetIoThread()));
  job_factory->SetProtocolHandler(
      "filesystem",
      fileapi::CreateFileSystemProtocolHandler(file_system_context_.get()));
  storage_.set_job_factory(job_factory);
}

TestShellRequestContext::~TestShellRequestContext() {
}
