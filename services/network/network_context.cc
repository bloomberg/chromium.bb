// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_context.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/extras/sqlite/sqlite_channel_id_store.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/reporting/reporting_policy.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/http_server_properties_pref_delegate.h"
#include "services/network/ignore_errors_cert_verifier.h"
#include "services/network/network_service.h"
#include "services/network/proxy_config_service_mojo.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/resource_scheduler_client.h"
#include "services/network/restricted_cookie_manager.h"
#include "services/network/throttling/network_conditions.h"
#include "services/network/throttling/throttling_controller.h"
#include "services/network/throttling/throttling_network_transaction_factory.h"
#include "services/network/url_loader.h"
#include "services/network/url_loader_factory.h"
#include "services/network/url_request_context_builder_mojo.h"

#if !defined(OS_IOS)
#include "services/network/websocket_factory.h"
#endif  // !defined(OS_IOS)

namespace network {

namespace {

net::CertVerifier* g_cert_verifier_for_testing = nullptr;

// A CertVerifier that forwards all requests to |g_cert_verifier_for_testing|.
// This is used to allow NetworkContexts to have their own
// std::unique_ptr<net::CertVerifier> while forwarding calls to the shared
// verifier.
class WrappedTestingCertVerifier : public net::CertVerifier {
 public:
  ~WrappedTestingCertVerifier() override = default;

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             net::CRLSet* crl_set,
             net::CertVerifyResult* verify_result,
             const net::CompletionCallback& callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    verify_result->Reset();
    if (!g_cert_verifier_for_testing)
      return net::ERR_FAILED;
    return g_cert_verifier_for_testing->Verify(params, crl_set, verify_result,
                                               callback, out_req, net_log);
  }
};

}  // namespace

constexpr bool NetworkContext::enable_resource_scheduler_;

NetworkContext::NetworkContext(NetworkService* network_service,
                               mojom::NetworkContextRequest request,
                               mojom::NetworkContextParamsPtr params)
    : network_service_(network_service),
      params_(std::move(params)),
      binding_(this, std::move(request)),
      socket_factory_(network_service_->net_log()) {
  url_request_context_owner_ = MakeURLRequestContext(params_.get());
  url_request_context_getter_ =
      url_request_context_owner_.url_request_context_getter;
  cookie_manager_ =
      std::make_unique<CookieManager>(GetURLRequestContext()->cookie_store());
  network_service_->RegisterNetworkContext(this);
  binding_.set_connection_error_handler(base::BindOnce(
      &NetworkContext::OnConnectionError, base::Unretained(this)));
  resource_scheduler_ =
      std::make_unique<ResourceScheduler>(enable_resource_scheduler_);
}

// TODO(mmenke): Share URLRequestContextBulder configuration between two
// constructors. Can only share them once consumer code is ready for its
// corresponding options to be overwritten.
NetworkContext::NetworkContext(
    NetworkService* network_service,
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params,
    std::unique_ptr<URLRequestContextBuilderMojo> builder)
    : network_service_(network_service),
      params_(std::move(params)),
      binding_(this, std::move(request)),
      socket_factory_(network_service_->net_log()) {
  url_request_context_owner_ = ApplyContextParamsToBuilder(
      builder.get(), params_.get(), network_service->quic_disabled(),
      network_service->net_log(), network_service->network_quality_estimator(),
      &user_agent_settings_);
  url_request_context_getter_ =
      url_request_context_owner_.url_request_context_getter;
  network_service_->RegisterNetworkContext(this);
  cookie_manager_ =
      std::make_unique<CookieManager>(GetURLRequestContext()->cookie_store());
  resource_scheduler_ =
      std::make_unique<ResourceScheduler>(enable_resource_scheduler_);
}

NetworkContext::NetworkContext(
    NetworkService* network_service,
    mojom::NetworkContextRequest request,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : network_service_(network_service),
      url_request_context_getter_(std::move(url_request_context_getter)),
      binding_(this, std::move(request)),
      cookie_manager_(std::make_unique<CookieManager>(
          url_request_context_getter_->GetURLRequestContext()->cookie_store())),
      socket_factory_(network_service_ ? network_service_->net_log()
                                       : nullptr) {
  // May be nullptr in tests.
  if (network_service_)
    network_service_->RegisterNetworkContext(this);
  resource_scheduler_ =
      std::make_unique<ResourceScheduler>(enable_resource_scheduler_);
}

NetworkContext::~NetworkContext() {
  // May be nullptr in tests.
  if (network_service_)
    network_service_->DeregisterNetworkContext(this);
}

std::unique_ptr<NetworkContext> NetworkContext::CreateForTesting() {
  return base::WrapUnique(
      new NetworkContext(mojom::NetworkContextParams::New()));
}

void NetworkContext::SetCertVerifierForTesting(
    net::CertVerifier* cert_verifier) {
  g_cert_verifier_for_testing = cert_verifier;
}

void NetworkContext::CreateURLLoaderFactory(
    mojom::URLLoaderFactoryRequest request,
    uint32_t process_id,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client) {
  url_loader_factories_.emplace(std::make_unique<URLLoaderFactory>(
      this, process_id, std::move(resource_scheduler_client),
      std::move(request)));
}

void NetworkContext::CreateURLLoaderFactory(
    mojom::URLLoaderFactoryRequest request,
    uint32_t process_id) {
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client;
  if (process_id != 0) {
    // Zero process ID means it's from the browser process and we don't want
    // to throttle the requests.
    resource_scheduler_client = base::MakeRefCounted<ResourceSchedulerClient>(
        process_id, ++current_resource_scheduler_client_id_,
        resource_scheduler_.get(),
        GetURLRequestContext()->network_quality_estimator());
  }
  CreateURLLoaderFactory(std::move(request), process_id,
                         std::move(resource_scheduler_client));
}

void NetworkContext::GetCookieManager(mojom::CookieManagerRequest request) {
  cookie_manager_->AddRequest(std::move(request));
}

void NetworkContext::GetRestrictedCookieManager(
    mojom::RestrictedCookieManagerRequest request,
    int32_t render_process_id,
    int32_t render_frame_id) {
  // TODO(crbug.com/729800): RestrictedCookieManager should own its bindings
  //     and NetworkContext should own the RestrictedCookieManager
  //     instances.
  mojo::MakeStrongBinding(std::make_unique<RestrictedCookieManager>(
                              GetURLRequestContext()->cookie_store(),
                              render_process_id, render_frame_id),
                          std::move(request));
}

void NetworkContext::DisableQuic() {
  GetURLRequestContext()
      ->http_transaction_factory()
      ->GetSession()
      ->DisableQuic();
}

net::URLRequestContext* NetworkContext::GetURLRequestContext() {
  return url_request_context_getter_->GetURLRequestContext();
}

void NetworkContext::Cleanup() {
  // The NetworkService is going away, so have to destroy the
  // net::URLRequestContext held by this NetworkContext.
  delete this;
}

NetworkContext::NetworkContext(mojom::NetworkContextParamsPtr params)
    : network_service_(nullptr),
      params_(std::move(params)),
      binding_(this),
      socket_factory_(network_service_->net_log()) {
  url_request_context_owner_ = MakeURLRequestContext(params_.get());
  url_request_context_getter_ =
      url_request_context_owner_.url_request_context_getter;

  resource_scheduler_ =
      std::make_unique<ResourceScheduler>(enable_resource_scheduler_);
}

void NetworkContext::OnConnectionError() {
  // Don't delete |this| in response to connection errors when it was created by
  // CreateForTesting.
  if (network_service_)
    delete this;
}

URLRequestContextOwner NetworkContext::MakeURLRequestContext(
    mojom::NetworkContextParams* network_context_params) {
  URLRequestContextBuilderMojo builder;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kHostResolverRules)) {
    std::unique_ptr<net::HostResolver> host_resolver(
        net::HostResolver::CreateDefaultResolver(nullptr));
    std::unique_ptr<net::MappedHostResolver> remapped_host_resolver(
        new net::MappedHostResolver(std::move(host_resolver)));
    remapped_host_resolver->SetRulesFromString(
        command_line->GetSwitchValueASCII(switches::kHostResolverRules));
    builder.set_host_resolver(std::move(remapped_host_resolver));
  }

  // The cookie configuration is in this method, which is only used by the
  // network process, and not ApplyContextParamsToBuilder which is used by the
  // browser as well. This is because this code path doesn't handle encryption
  // and other configuration done in QuotaPolicyCookieStore yet (and we still
  // have to figure out which of the latter needs to move to the network
  // process). TODO: http://crbug.com/789644
  if (network_context_params->cookie_path) {
    DCHECK(network_context_params->channel_id_path);
    net::CookieCryptoDelegate* crypto_delegate = nullptr;

    scoped_refptr<base::SequencedTaskRunner> client_task_runner =
        base::MessageLoop::current()->task_runner();
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BACKGROUND,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

    scoped_refptr<net::SQLiteChannelIDStore> channel_id_db =
        new net::SQLiteChannelIDStore(
            network_context_params->channel_id_path.value(),
            background_task_runner);
    std::unique_ptr<net::ChannelIDService> channel_id_service(
        std::make_unique<net::ChannelIDService>(
            new net::DefaultChannelIDStore(channel_id_db.get())));

    scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
        new net::SQLitePersistentCookieStore(
            network_context_params->cookie_path.value(), client_task_runner,
            background_task_runner,
            network_context_params->restore_old_session_cookies,
            crypto_delegate));

    std::unique_ptr<net::CookieMonster> cookie_store =
        std::make_unique<net::CookieMonster>(sqlite_store.get(),
                                             channel_id_service.get());
    if (network_context_params->persist_session_cookies)
      cookie_store->SetPersistSessionCookies(true);

    cookie_store->SetChannelIDServiceID(channel_id_service->GetUniqueID());
    builder.SetCookieAndChannelIdStores(std::move(cookie_store),
                                        std::move(channel_id_service));
  } else {
    DCHECK(!network_context_params->restore_old_session_cookies);
    DCHECK(!network_context_params->persist_session_cookies);
  }

  if (g_cert_verifier_for_testing) {
    builder.SetCertVerifier(std::make_unique<WrappedTestingCertVerifier>());
  } else {
    std::unique_ptr<net::CertVerifier> cert_verifier =
        net::CertVerifier::CreateDefault();
    builder.SetCertVerifier(IgnoreErrorsCertVerifier::MaybeWrapCertVerifier(
        *command_line, nullptr, std::move(cert_verifier)));
  }

  // |network_service_| may be nullptr in tests.
  return ApplyContextParamsToBuilder(
      &builder, network_context_params,
      network_service_ ? network_service_->quic_disabled() : false,
      network_service_ ? network_service_->net_log() : nullptr,
      network_service_ ? network_service_->network_quality_estimator()
                       : nullptr,
      &user_agent_settings_);
}

URLRequestContextOwner NetworkContext::ApplyContextParamsToBuilder(
    URLRequestContextBuilderMojo* builder,
    mojom::NetworkContextParams* network_context_params,
    bool quic_disabled,
    net::NetLog* net_log,
    net::NetworkQualityEstimator* network_quality_estimator,
    net::StaticHttpUserAgentSettings** out_http_user_agent_settings) {
  if (net_log)
    builder->set_net_log(net_log);

  if (network_quality_estimator)
    builder->set_network_quality_estimator(network_quality_estimator);

  std::string accept_language = network_context_params->accept_language
                                    ? *network_context_params->accept_language
                                    : "en-us,en";
  std::unique_ptr<net::StaticHttpUserAgentSettings> user_agent_settings =
      std::make_unique<net::StaticHttpUserAgentSettings>(
          accept_language, network_context_params->user_agent);
  // Borrow an alias for future use before giving the builder ownership.
  if (out_http_user_agent_settings)
    *out_http_user_agent_settings = user_agent_settings.get();
  builder->set_http_user_agent_settings(std::move(user_agent_settings));

  builder->set_enable_brotli(network_context_params->enable_brotli);
  if (network_context_params->context_name)
    builder->set_name(*network_context_params->context_name);

  if (network_context_params->proxy_resolver_factory) {
    builder->SetMojoProxyResolverFactory(
        proxy_resolver::mojom::ProxyResolverFactoryPtr(
            std::move(network_context_params->proxy_resolver_factory)));
  }

  if (!network_context_params->http_cache_enabled) {
    builder->DisableHttpCache();
  } else {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    cache_params.max_size = network_context_params->http_cache_max_size;
    if (!network_context_params->http_cache_path) {
      cache_params.type =
          net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
    } else {
      cache_params.path = *network_context_params->http_cache_path;
      cache_params.type = network_session_configurator::ChooseCacheType(
          *base::CommandLine::ForCurrentProcess());
    }

    builder->EnableHttpCache(cache_params);
  }

  if (!network_context_params->initial_proxy_config &&
      !network_context_params->proxy_config_client_request.is_pending()) {
    network_context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
  }
  builder->set_proxy_config_service(std::make_unique<ProxyConfigServiceMojo>(
      std::move(network_context_params->proxy_config_client_request),
      std::move(network_context_params->initial_proxy_config),
      std::move(network_context_params->proxy_config_poller_client)));

  std::unique_ptr<PrefService> pref_service;
  if (network_context_params->http_server_properties_path) {
    scoped_refptr<JsonPrefStore> json_pref_store(new JsonPrefStore(
        *network_context_params->http_server_properties_path,
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
             base::TaskPriority::BACKGROUND})));
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(json_pref_store);
    pref_service_factory.set_async(true);
    scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple());
    HttpServerPropertiesPrefDelegate::RegisterPrefs(pref_registry.get());
    pref_service = pref_service_factory.Create(pref_registry.get());

    builder->SetHttpServerProperties(
        std::make_unique<net::HttpServerPropertiesManager>(
            std::make_unique<HttpServerPropertiesPrefDelegate>(
                pref_service.get()),
            net_log));
  }

  builder->set_data_enabled(network_context_params->enable_data_url_support);
#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
  builder->set_file_enabled(network_context_params->enable_file_url_support);
#else  // BUILDFLAG(DISABLE_FILE_SUPPORT)
  DCHECK(!network_context_params->enable_file_url_support);
#endif
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  builder->set_ftp_enabled(network_context_params->enable_ftp_url_support);
#else  // BUILDFLAG(DISABLE_FTP_SUPPORT)
  DCHECK(!network_context_params->enable_ftp_url_support);
#endif

#if BUILDFLAG(ENABLE_REPORTING)
  if (base::FeatureList::IsEnabled(features::kReporting))
    builder->set_reporting_policy(std::make_unique<net::ReportingPolicy>());
  else
    builder->set_reporting_policy(nullptr);

  builder->set_network_error_logging_enabled(
      base::FeatureList::IsEnabled(features::kNetworkErrorLogging));
#endif  // BUILDFLAG(ENABLE_REPORTING)

  net::HttpNetworkSession::Params session_params;
  bool is_quic_force_disabled = false;
  if (quic_disabled)
    is_quic_force_disabled = true;

  network_session_configurator::ParseCommandLineAndFieldTrials(
      *base::CommandLine::ForCurrentProcess(), is_quic_force_disabled,
      network_context_params->quic_user_agent_id, &session_params);

  session_params.http_09_on_non_default_ports_enabled =
      network_context_params->http_09_on_non_default_ports_enabled;

  builder->set_http_network_session_params(session_params);

  builder->SetCreateHttpTransactionFactoryCallback(
      base::BindOnce([](net::HttpNetworkSession* session)
                         -> std::unique_ptr<net::HttpTransactionFactory> {
        return std::make_unique<ThrottlingNetworkTransactionFactory>(session);
      }));
  return URLRequestContextOwner(std::move(pref_service), builder->Build());
}

void NetworkContext::DestroyURLLoaderFactory(
    URLLoaderFactory* url_loader_factory) {
  auto it = url_loader_factories_.find(url_loader_factory);
  DCHECK(it != url_loader_factories_.end());
  url_loader_factories_.erase(it);
}

void NetworkContext::ClearNetworkingHistorySince(
    base::Time time,
    base::OnceClosure completion_callback) {
  // TODO(mmenke): Neither of these methods waits until the changes have been
  // commited to disk. They probably should, as most similar methods net/
  // exposes do.

  // Completes synchronously.
  GetURLRequestContext()->transport_security_state()->DeleteAllDynamicDataSince(
      time);

  GetURLRequestContext()->http_server_properties()->Clear(
      std::move(completion_callback));
}

void NetworkContext::ClearHttpCache(base::Time start_time,
                                    base::Time end_time,
                                    mojom::ClearCacheUrlFilterPtr filter,
                                    ClearHttpCacheCallback callback) {
  // It's safe to use Unretained below as the HttpCacheDataRemover is owner by
  // |this| and guarantees it won't call its callback if deleted.
  http_cache_data_removers_.push_back(HttpCacheDataRemover::CreateAndStart(
      url_request_context_getter_->GetURLRequestContext(), std::move(filter),
      start_time, end_time,
      base::BindOnce(&NetworkContext::OnHttpCacheCleared,
                     base::Unretained(this), std::move(callback))));
}

void NetworkContext::OnHttpCacheCleared(ClearHttpCacheCallback callback,
                                        HttpCacheDataRemover* remover) {
  bool removed = false;
  for (auto iter = http_cache_data_removers_.begin();
       iter != http_cache_data_removers_.end(); ++iter) {
    if (iter->get() == remover) {
      removed = true;
      http_cache_data_removers_.erase(iter);
      break;
    }
  }
  DCHECK(removed);
  std::move(callback).Run();
}

void NetworkContext::SetNetworkConditions(
    const std::string& profile_id,
    mojom::NetworkConditionsPtr conditions) {
  std::unique_ptr<NetworkConditions> network_conditions;
  if (conditions) {
    network_conditions.reset(new NetworkConditions(
        conditions->offline, conditions->latency.InMillisecondsF(),
        conditions->download_throughput, conditions->upload_throughput));
  }
  ThrottlingController::SetConditions(profile_id,
                                      std::move(network_conditions));
}

void NetworkContext::SetAcceptLanguage(const std::string& new_accept_language) {
  // This may only be called on NetworkContexts created with a constructor that
  // calls ApplyContextParamsToBuilder.
  DCHECK(user_agent_settings_);
  user_agent_settings_->set_accept_language(new_accept_language);
}

void NetworkContext::CreateUDPSocket(mojom::UDPSocketRequest request,
                                     mojom::UDPSocketReceiverPtr receiver) {
  socket_factory_.CreateUDPSocket(std::move(request), std::move(receiver));
}

void NetworkContext::CreateTCPServerSocket(
    const net::IPEndPoint& local_addr,
    uint32_t backlog,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPServerSocketRequest request,
    CreateTCPServerSocketCallback callback) {
  socket_factory_.CreateTCPServerSocket(
      local_addr, backlog,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(request), std::move(callback));
}

void NetworkContext::CreateTCPConnectedSocket(
    const base::Optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPConnectedSocketRequest request,
    mojom::TCPConnectedSocketObserverPtr observer,
    CreateTCPConnectedSocketCallback callback) {
  socket_factory_.CreateTCPConnectedSocket(
      local_addr, remote_addr_list,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(request), std::move(observer), std::move(callback));
}

void NetworkContext::CreateWebSocket(mojom::WebSocketRequest request,
                                     int32_t process_id,
                                     int32_t render_frame_id,
                                     const url::Origin& origin) {
#if !defined(OS_IOS)
  if (!websocket_factory_)
    websocket_factory_ = std::make_unique<WebSocketFactory>(this);
  websocket_factory_->CreateWebSocket(std::move(request), process_id,
                                      render_frame_id, origin);
#endif  // !defined(OS_IOS)
}

void NetworkContext::AddHSTSForTesting(const std::string& host,
                                       base::Time expiry,
                                       bool include_subdomains,
                                       AddHSTSForTestingCallback callback) {
  net::TransportSecurityState* state =
      GetURLRequestContext()->transport_security_state();
  state->AddHSTS(host, expiry, include_subdomains);
  std::move(callback).Run();
}

}  // namespace network
