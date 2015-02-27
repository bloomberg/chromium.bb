// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_builder.h"

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate_impl.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_persister.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_throttler_manager.h"

#if !defined(DISABLE_FILE_SUPPORT)
#include "net/url_request/file_protocol_handler.h"
#endif

#if !defined(DISABLE_FTP_SUPPORT)
#include "net/url_request/ftp_protocol_handler.h"
#endif

namespace net {

namespace {

class BasicNetworkDelegate : public NetworkDelegateImpl {
 public:
  BasicNetworkDelegate() {}
  ~BasicNetworkDelegate() override {}

 private:
  int OnBeforeURLRequest(URLRequest* request,
                         const CompletionCallback& callback,
                         GURL* new_url) override {
    return OK;
  }

  int OnBeforeSendHeaders(URLRequest* request,
                          const CompletionCallback& callback,
                          HttpRequestHeaders* headers) override {
    return OK;
  }

  void OnSendHeaders(URLRequest* request,
                     const HttpRequestHeaders& headers) override {}

  int OnHeadersReceived(
      URLRequest* request,
      const CompletionCallback& callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override {
    return OK;
  }

  void OnBeforeRedirect(URLRequest* request,
                        const GURL& new_location) override {}

  void OnResponseStarted(URLRequest* request) override {}

  void OnRawBytesRead(const URLRequest& request, int bytes_read) override {}

  void OnCompleted(URLRequest* request, bool started) override {}

  void OnURLRequestDestroyed(URLRequest* request) override {}

  void OnPACScriptError(int line_number, const base::string16& error) override {
  }

  NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      AuthCredentials* credentials) override {
    return NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  bool OnCanGetCookies(const URLRequest& request,
                       const CookieList& cookie_list) override {
    return true;
  }

  bool OnCanSetCookie(const URLRequest& request,
                      const std::string& cookie_line,
                      CookieOptions* options) override {
    return true;
  }

  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& path) const override {
    return true;
  }

  bool OnCanThrottleRequest(const URLRequest& request) const override {
    // Returning true will only enable throttling if there's also a
    // URLRequestThrottlerManager, which there isn't, by default.
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(BasicNetworkDelegate);
};

class BasicURLRequestContext : public URLRequestContext {
 public:
  explicit BasicURLRequestContext(
      const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner)
      : file_task_runner_(file_task_runner), storage_(this) {}

  URLRequestContextStorage* storage() {
    return &storage_;
  }

  scoped_refptr<base::SingleThreadTaskRunner>& GetFileTaskRunner() {
    // Create a new thread to run file tasks, if needed.
    if (!file_task_runner_) {
      DCHECK(!file_thread_);
      file_thread_.reset(new base::Thread("Network File Thread"));
      file_thread_->StartWithOptions(
          base::Thread::Options(base::MessageLoop::TYPE_DEFAULT, 0));
      file_task_runner_ = file_thread_->task_runner();
    }
    return file_task_runner_;
  }

  void set_transport_security_persister(
      scoped_ptr<TransportSecurityPersister> transport_security_persister) {
    transport_security_persister = transport_security_persister.Pass();
  }

 protected:
  ~BasicURLRequestContext() override { AssertNoURLRequests(); }

 private:
  // The thread should be torn down last.
  scoped_ptr<base::Thread> file_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  URLRequestContextStorage storage_;
  scoped_ptr<TransportSecurityPersister> transport_security_persister_;

  DISALLOW_COPY_AND_ASSIGN(BasicURLRequestContext);
};

}  // namespace

URLRequestContextBuilder::HttpCacheParams::HttpCacheParams()
    : type(IN_MEMORY),
      max_size(0) {}
URLRequestContextBuilder::HttpCacheParams::~HttpCacheParams() {}

URLRequestContextBuilder::HttpNetworkSessionParams::HttpNetworkSessionParams()
    : ignore_certificate_errors(false),
      host_mapping_rules(NULL),
      testing_fixed_http_port(0),
      testing_fixed_https_port(0),
      next_protos(NextProtosDefaults()),
      use_alternate_protocols(true),
      enable_quic(false) {
}

URLRequestContextBuilder::HttpNetworkSessionParams::~HttpNetworkSessionParams()
{}

URLRequestContextBuilder::SchemeFactory::SchemeFactory(
    const std::string& auth_scheme,
    net::HttpAuthHandlerFactory* auth_handler_factory)
    : scheme(auth_scheme), factory(auth_handler_factory) {
}

URLRequestContextBuilder::SchemeFactory::~SchemeFactory() {
}

URLRequestContextBuilder::URLRequestContextBuilder()
    : data_enabled_(false),
#if !defined(DISABLE_FILE_SUPPORT)
      file_enabled_(false),
#endif
#if !defined(DISABLE_FTP_SUPPORT)
      ftp_enabled_(false),
#endif
      http_cache_enabled_(true),
      throttling_enabled_(false) {
}

URLRequestContextBuilder::~URLRequestContextBuilder() {}

void URLRequestContextBuilder::EnableHttpCache(const HttpCacheParams& params) {
  http_cache_enabled_ = true;
  http_cache_params_ = params;
}

void URLRequestContextBuilder::DisableHttpCache() {
  http_cache_enabled_ = false;
  http_cache_params_ = HttpCacheParams();
}

void URLRequestContextBuilder::SetSpdyAndQuicEnabled(bool spdy_enabled,
                                                     bool quic_enabled) {
  http_network_session_params_.next_protos =
      NextProtosWithSpdyAndQuic(spdy_enabled, quic_enabled);
  http_network_session_params_.enable_quic = quic_enabled;
}

void URLRequestContextBuilder::SetCookieAndChannelIdStores(
      const scoped_refptr<CookieStore>& cookie_store,
      scoped_ptr<ChannelIDService> channel_id_service) {
  DCHECK(cookie_store);
  cookie_store_ = cookie_store;
  channel_id_service_ = channel_id_service.Pass();
}

void URLRequestContextBuilder::SetFileTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  file_task_runner_ = task_runner;
}

URLRequestContext* URLRequestContextBuilder::Build() {
  BasicURLRequestContext* context =
      new BasicURLRequestContext(file_task_runner_);
  URLRequestContextStorage* storage = context->storage();

  storage->set_http_user_agent_settings(new StaticHttpUserAgentSettings(
      accept_language_, user_agent_));

  if (!network_delegate_)
    network_delegate_.reset(new BasicNetworkDelegate);
  NetworkDelegate* network_delegate = network_delegate_.release();
  storage->set_network_delegate(network_delegate);

  if (net_log_) {
    storage->set_net_log(net_log_.release());
  } else {
    storage->set_net_log(new net::NetLog);
  }

  if (!host_resolver_) {
    host_resolver_ = net::HostResolver::CreateDefaultResolver(
        context->net_log());
  }
  storage->set_host_resolver(host_resolver_.Pass());

  if (!proxy_service_) {
    // TODO(willchan): Switch to using this code when
    // ProxyService::CreateSystemProxyConfigService()'s signature doesn't suck.
  #if defined(OS_LINUX) || defined(OS_ANDROID)
    ProxyConfigService* proxy_config_service = proxy_config_service_.release();
  #else
    ProxyConfigService* proxy_config_service = NULL;
    if (proxy_config_service_) {
      proxy_config_service = proxy_config_service_.release();
    } else {
      proxy_config_service = ProxyService::CreateSystemProxyConfigService(
          base::ThreadTaskRunnerHandle::Get().get(),
          context->GetFileTaskRunner());
    }
  #endif  // defined(OS_LINUX) || defined(OS_ANDROID)
    proxy_service_.reset(
        ProxyService::CreateUsingSystemProxyResolver(
            proxy_config_service,
            0,  // This results in using the default value.
            context->net_log()));
  }
  storage->set_proxy_service(proxy_service_.release());

  storage->set_ssl_config_service(new net::SSLConfigServiceDefaults);
  HttpAuthHandlerRegistryFactory* http_auth_handler_registry_factory =
      net::HttpAuthHandlerRegistryFactory::CreateDefault(
           context->host_resolver());
  for (size_t i = 0; i < extra_http_auth_handlers_.size(); ++i) {
    http_auth_handler_registry_factory->RegisterSchemeFactory(
        extra_http_auth_handlers_[i].scheme,
        extra_http_auth_handlers_[i].factory);
  }
  storage->set_http_auth_handler_factory(http_auth_handler_registry_factory);

  if (cookie_store_) {
    storage->set_cookie_store(cookie_store_.get());
    storage->set_channel_id_service(channel_id_service_.Pass());
  } else {
    storage->set_cookie_store(new CookieMonster(NULL, NULL));
    // TODO(mmenke):  This always creates a file thread, even when it ends up
    // not being used.  Consider lazily creating the thread.
    storage->set_channel_id_service(make_scoped_ptr(new ChannelIDService(
        new DefaultChannelIDStore(NULL), context->GetFileTaskRunner())));
  }

  storage->set_transport_security_state(new net::TransportSecurityState());
  if (!transport_security_persister_path_.empty()) {
    context->set_transport_security_persister(
        make_scoped_ptr<TransportSecurityPersister>(
            new TransportSecurityPersister(context->transport_security_state(),
                                           transport_security_persister_path_,
                                           context->GetFileTaskRunner(),
                                           false)));
  }

  storage->set_http_server_properties(
      scoped_ptr<net::HttpServerProperties>(
          new net::HttpServerPropertiesImpl()));
  storage->set_cert_verifier(CertVerifier::CreateDefault());

  if (throttling_enabled_)
    storage->set_throttler_manager(new URLRequestThrottlerManager());

  net::HttpNetworkSession::Params network_session_params;
  network_session_params.host_resolver = context->host_resolver();
  network_session_params.cert_verifier = context->cert_verifier();
  network_session_params.transport_security_state =
      context->transport_security_state();
  network_session_params.proxy_service = context->proxy_service();
  network_session_params.ssl_config_service =
      context->ssl_config_service();
  network_session_params.http_auth_handler_factory =
      context->http_auth_handler_factory();
  network_session_params.network_delegate = network_delegate;
  network_session_params.http_server_properties =
      context->http_server_properties();
  network_session_params.net_log = context->net_log();

  network_session_params.ignore_certificate_errors =
      http_network_session_params_.ignore_certificate_errors;
  network_session_params.host_mapping_rules =
      http_network_session_params_.host_mapping_rules;
  network_session_params.testing_fixed_http_port =
      http_network_session_params_.testing_fixed_http_port;
  network_session_params.testing_fixed_https_port =
      http_network_session_params_.testing_fixed_https_port;
  network_session_params.use_alternate_protocols =
    http_network_session_params_.use_alternate_protocols;
  network_session_params.trusted_spdy_proxy =
      http_network_session_params_.trusted_spdy_proxy;
  network_session_params.next_protos = http_network_session_params_.next_protos;
  network_session_params.enable_quic = http_network_session_params_.enable_quic;
  network_session_params.quic_connection_options =
      http_network_session_params_.quic_connection_options;

  HttpTransactionFactory* http_transaction_factory = NULL;
  if (http_cache_enabled_) {
    network_session_params.channel_id_service =
        context->channel_id_service();
    HttpCache::BackendFactory* http_cache_backend = NULL;
    if (http_cache_params_.type == HttpCacheParams::DISK) {
      http_cache_backend = new HttpCache::DefaultBackend(
          DISK_CACHE, net::CACHE_BACKEND_DEFAULT, http_cache_params_.path,
          http_cache_params_.max_size, context->GetFileTaskRunner());
    } else {
      http_cache_backend =
          HttpCache::DefaultBackend::InMemory(http_cache_params_.max_size);
    }

    http_transaction_factory = new HttpCache(
        network_session_params, http_cache_backend);
  } else {
    scoped_refptr<net::HttpNetworkSession> network_session(
        new net::HttpNetworkSession(network_session_params));

    http_transaction_factory = new HttpNetworkLayer(network_session.get());
  }
  storage->set_http_transaction_factory(http_transaction_factory);

  URLRequestJobFactoryImpl* job_factory = new URLRequestJobFactoryImpl;
  if (data_enabled_)
    job_factory->SetProtocolHandler("data", new DataProtocolHandler);

#if !defined(DISABLE_FILE_SUPPORT)
  if (file_enabled_) {
    job_factory->SetProtocolHandler(
        "file", new FileProtocolHandler(context->GetFileTaskRunner()));
  }
#endif  // !defined(DISABLE_FILE_SUPPORT)

#if !defined(DISABLE_FTP_SUPPORT)
  if (ftp_enabled_) {
    ftp_transaction_factory_.reset(
        new FtpNetworkLayer(context->host_resolver()));
    job_factory->SetProtocolHandler("ftp",
        new FtpProtocolHandler(ftp_transaction_factory_.get()));
  }
#endif  // !defined(DISABLE_FTP_SUPPORT)

  storage->set_job_factory(job_factory);

  // TODO(willchan): Support sdch.

  return context;
}

}  // namespace net
