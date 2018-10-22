// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_context.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"
#include "components/certificate_transparency/chrome_require_ct_delegate.h"
#include "components/certificate_transparency/features.h"
#include "components/certificate_transparency/sth_distributor.h"
#include "components/certificate_transparency/sth_reporter.h"
#include "components/certificate_transparency/tree_state_tracker.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/layered_network_delegate.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/extras/sqlite/sqlite_channel_id_store.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/http/failing_http_transaction_factory.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/cookie_manager.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/expect_ct_reporter.h"
#include "services/network/host_resolver.h"
#include "services/network/http_server_properties_pref_delegate.h"
#include "services/network/ignore_errors_cert_verifier.h"
#include "services/network/mojo_net_log.h"
#include "services/network/network_service.h"
#include "services/network/network_service_network_delegate.h"
#include "services/network/p2p/socket_manager.h"
#include "services/network/proxy_config_service_mojo.h"
#include "services/network/proxy_lookup_request.h"
#include "services/network/proxy_resolving_socket_factory_mojo.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/resolve_host_request.h"
#include "services/network/resource_scheduler_client.h"
#include "services/network/restricted_cookie_manager.h"
#include "services/network/session_cleanup_channel_id_store.h"
#include "services/network/session_cleanup_cookie_store.h"
#include "services/network/ssl_config_service_mojo.h"
#include "services/network/throttling/network_conditions.h"
#include "services/network/throttling/throttling_controller.h"
#include "services/network/throttling/throttling_network_transaction_factory.h"
#include "services/network/url_request_context_builder_mojo.h"

#if !defined(OS_IOS)
#include "services/network/websocket_factory.h"
#endif  // !defined(OS_IOS)

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if defined(USE_NSS_CERTS)
#include "net/cert_net/nss_ocsp.h"
#endif  // defined(USE_NSS_CERTS)

#if defined(OS_ANDROID) || defined(OS_FUCHSIA) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_MACOSX)
#include "net/cert/cert_net_fetcher.h"
#include "net/cert_net/cert_net_fetcher_impl.h"
#endif

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
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    verify_result->Reset();
    if (!g_cert_verifier_for_testing)
      return net::ERR_FAILED;
    return g_cert_verifier_for_testing->Verify(
        params, verify_result, std::move(callback), out_req, net_log);
  }
  void SetConfig(const Config& config) override {
    if (!g_cert_verifier_for_testing)
      return;
    g_cert_verifier_for_testing->SetConfig(config);
  }
};

// Predicate function to determine if the given |domain| matches the
// |filter_type| and |filter_domains| from a |mojom::ClearDataFilter|.
bool MatchesDomainFilter(mojom::ClearDataFilter_Type filter_type,
                         std::set<std::string> filter_domains,
                         const std::string& domain) {
  bool found_domain = filter_domains.find(domain) != filter_domains.end();
  return (filter_type == mojom::ClearDataFilter_Type::DELETE_MATCHES) ==
         found_domain;
}

// Returns a callback that checks if a domain matches the |filter|. |filter|
// must contain no origins. A null filter matches everything.
base::RepeatingCallback<bool(const std::string& host_name)> MakeDomainFilter(
    mojom::ClearDataFilter* filter) {
  if (!filter)
    return base::BindRepeating([](const std::string&) { return true; });

  DCHECK(filter->origins.empty())
      << "Origin filtering not allowed in a domain-only filter";

  std::set<std::string> filter_domains;
  filter_domains.insert(filter->domains.begin(), filter->domains.end());
  return base::BindRepeating(&MatchesDomainFilter, filter->type,
                             std::move(filter_domains));
}

// Generic functions but currently only used for reporting.
#if BUILDFLAG(ENABLE_REPORTING)
// Predicate function to determine if the given |url| matches the |filter_type|,
// |filter_domains| and |filter_origins| from a |mojom::ClearDataFilter|.
bool MatchesUrlFilter(mojom::ClearDataFilter_Type filter_type,
                      std::set<std::string> filter_domains,
                      std::set<url::Origin> filter_origins,
                      const GURL& url) {
  std::string url_registerable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  bool found_domain =
      (filter_domains.find(url_registerable_domain != ""
                               ? url_registerable_domain
                               : url.host()) != filter_domains.end());

  bool found_origin =
      (filter_origins.find(url::Origin::Create(url)) != filter_origins.end());

  return (filter_type == mojom::ClearDataFilter_Type::DELETE_MATCHES) ==
         (found_domain || found_origin);
}

// Builds a generic GURL-matching predicate function based on |filter|. If
// |filter| is null, creates an always-true predicate.
base::RepeatingCallback<bool(const GURL&)> BuildUrlFilter(
    mojom::ClearDataFilterPtr filter) {
  if (!filter) {
    return base::BindRepeating([](const GURL&) { return true; });
  }

  std::set<std::string> filter_domains;
  filter_domains.insert(filter->domains.begin(), filter->domains.end());

  std::set<url::Origin> filter_origins;
  filter_origins.insert(filter->origins.begin(), filter->origins.end());

  return base::BindRepeating(&MatchesUrlFilter, filter->type,
                             std::move(filter_domains),
                             std::move(filter_origins));
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

void OnClearedChannelIds(net::SSLConfigService* ssl_config_service,
                         base::OnceClosure callback) {
  // Need to close open SSL connections which may be using the channel ids we
  // deleted.
  // TODO(mattm): http://crbug.com/166069 Make the server bound cert
  // service/store have observers that can notify relevant things directly.
  ssl_config_service->NotifySSLConfigChange();
  std::move(callback).Run();
}

}  // namespace

constexpr bool NetworkContext::enable_resource_scheduler_;

NetworkContext::PendingCertVerify::PendingCertVerify() = default;
NetworkContext::PendingCertVerify::~PendingCertVerify() = default;

// net::NetworkDelegate that wraps the main NetworkDelegate to remove the
// Referrer from requests, if needed.
// TODO(mmenke): Once the network service has shipped, this can be done in
// URLLoader instead.
class NetworkContext::ContextNetworkDelegate
    : public net::LayeredNetworkDelegate {
 public:
  ContextNetworkDelegate(
      std::unique_ptr<net::NetworkDelegate> nested_network_delegate,
      bool enable_referrers,
      bool validate_referrer_policy_on_initial_request,
      NetworkContext* network_context)
      : LayeredNetworkDelegate(std::move(nested_network_delegate)),
        enable_referrers_(enable_referrers),
        validate_referrer_policy_on_initial_request_(
            validate_referrer_policy_on_initial_request),
        network_context_(network_context) {}

  ~ContextNetworkDelegate() override {}

  // net::NetworkDelegate implementation:

  void OnBeforeURLRequestInternal(net::URLRequest* request,
                                  GURL* new_url) override {
    if (!enable_referrers_)
      request->SetReferrer(std::string());
    if (network_context_->network_service())
      network_context_->network_service()->OnBeforeURLRequest();
  }

  void OnCompletedInternal(net::URLRequest* request,
                           bool started,
                           int net_error) override {
    // TODO(mmenke): Once the network service ships on all platforms, can move
    // this logic into URLLoader's completion method.
    DCHECK_NE(net::ERR_IO_PENDING, net_error);

    // Record network errors that HTTP requests complete with, including OK and
    // ABORTED.
    // TODO(mmenke): Seems like this really should be looking at HTTPS requests,
    // too.
    // TODO(mmenke): We should remove the main frame case from here, and move it
    // into the consumer - the network service shouldn't know what a main frame
    // is.
    if (request->url().SchemeIs("http")) {
      base::UmaHistogramSparse("Net.HttpRequestCompletionErrorCodes",
                               -net_error);

      if (request->load_flags() & net::LOAD_MAIN_FRAME_DEPRECATED) {
        base::UmaHistogramSparse(
            "Net.HttpRequestCompletionErrorCodes.MainFrame", -net_error);
      }
    }
  }

  bool OnCancelURLRequestWithPolicyViolatingReferrerHeaderInternal(
      const net::URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override {
    // TODO(mmenke): Once the network service has shipped on all platforms,
    // consider moving this logic into URLLoader, and removing this method from
    // NetworkDelegate. Can just have a DCHECK in URLRequest instead.
    if (!validate_referrer_policy_on_initial_request_)
      return false;

    LOG(ERROR) << "Cancelling request to " << target_url
               << " with invalid referrer " << referrer_url;
    // Record information to help debug issues like http://crbug.com/422871.
    if (target_url.SchemeIsHTTPOrHTTPS()) {
      auto referrer_policy = request.referrer_policy();
      base::debug::Alias(&referrer_policy);
      DEBUG_ALIAS_FOR_GURL(target_buf, target_url);
      DEBUG_ALIAS_FOR_GURL(referrer_buf, referrer_url);
      base::debug::DumpWithoutCrashing();
    }
    return true;
  }

  void set_enable_referrers(bool enable_referrers) {
    enable_referrers_ = enable_referrers;
  }

 private:
  bool enable_referrers_;
  bool validate_referrer_policy_on_initial_request_;
  NetworkContext* network_context_;

  DISALLOW_COPY_AND_ASSIGN(ContextNetworkDelegate);
};

NetworkContext::NetworkContext(
    NetworkService* network_service,
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params,
    OnConnectionCloseCallback on_connection_close_callback)
    : network_service_(network_service),
      params_(std::move(params)),
      on_connection_close_callback_(std::move(on_connection_close_callback)),
      binding_(this, std::move(request)) {
  url_request_context_owner_ = MakeURLRequestContext();
  url_request_context_ = url_request_context_owner_.url_request_context.get();

  network_service_->RegisterNetworkContext(this);

  // Only register for destruction if |this| will be wholly lifetime-managed
  // by the NetworkService. In the other constructors, lifetime is shared with
  // other consumers, and thus self-deletion is not safe and can result in
  // double-frees.
  binding_.set_connection_error_handler(base::BindOnce(
      &NetworkContext::OnConnectionError, base::Unretained(this)));

  socket_factory_ = std::make_unique<SocketFactory>(
      url_request_context_->net_log(), url_request_context_);
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
      binding_(this, std::move(request)) {
  url_request_context_owner_ = ApplyContextParamsToBuilder(builder.get());
  url_request_context_ = url_request_context_owner_.url_request_context.get();

  network_service_->RegisterNetworkContext(this);
  socket_factory_ = std::make_unique<SocketFactory>(
      url_request_context_->net_log(), url_request_context_);
  resource_scheduler_ =
      std::make_unique<ResourceScheduler>(enable_resource_scheduler_);
}

NetworkContext::NetworkContext(NetworkService* network_service,
                               mojom::NetworkContextRequest request,
                               net::URLRequestContext* url_request_context)
    : network_service_(network_service),
      url_request_context_(url_request_context),
      binding_(this, std::move(request)),
      cookie_manager_(
          std::make_unique<CookieManager>(url_request_context->cookie_store(),
                                          nullptr,
                                          nullptr,
                                          nullptr)),
      socket_factory_(
          std::make_unique<SocketFactory>(url_request_context_->net_log(),
                                          url_request_context)) {
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

  if (IsPrimaryNetworkContext()) {
#if defined(USE_NSS_CERTS)
    net::SetURLRequestContextForNSSHttpIO(nullptr);
#endif

#if defined(OS_ANDROID) || defined(OS_FUCHSIA) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_MACOSX)
    net::ShutdownGlobalCertNetFetcher();
#endif
  }

  if (url_request_context_ &&
      url_request_context_->transport_security_state()) {
    if (certificate_report_sender_) {
      // Destroy |certificate_report_sender_| before |url_request_context_|,
      // since the former has a reference to the latter.
      url_request_context_->transport_security_state()->SetReportSender(
          nullptr);
      certificate_report_sender_.reset();
    }

    if (expect_ct_reporter_) {
      url_request_context_->transport_security_state()->SetExpectCTReporter(
          nullptr);
      expect_ct_reporter_.reset();
    }

    if (require_ct_delegate_) {
      url_request_context_->transport_security_state()->SetRequireCTDelegate(
          nullptr);
    }
  }

  if (url_request_context_ &&
      url_request_context_->cert_transparency_verifier()) {
    url_request_context_->cert_transparency_verifier()->SetObserver(nullptr);
  }

  if (network_service_ && network_service_->sth_reporter() &&
      ct_tree_tracker_) {
    network_service_->sth_reporter()->UnregisterObserver(
        ct_tree_tracker_.get());
  }
}

void NetworkContext::SetCertVerifierForTesting(
    net::CertVerifier* cert_verifier) {
  g_cert_verifier_for_testing = cert_verifier;
}

bool NetworkContext::IsPrimaryNetworkContext() const {
  return params_ && params_->primary_network_context;
}

void NetworkContext::CreateURLLoaderFactory(
    mojom::URLLoaderFactoryRequest request,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client) {
  url_loader_factories_.emplace(std::make_unique<cors::CORSURLLoaderFactory>(
      this, std::move(params), std::move(resource_scheduler_client),
      std::move(request)));
}

void NetworkContext::SetClient(mojom::NetworkContextClientPtr client) {
  client_ = std::move(client);
}

void NetworkContext::CreateURLLoaderFactory(
    mojom::URLLoaderFactoryRequest request,
    mojom::URLLoaderFactoryParamsPtr params) {
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client;
  if (params->process_id != mojom::kBrowserProcessId) {
    // Zero process ID means it's from the browser process and we don't want
    // to throttle the requests.
    resource_scheduler_client = base::MakeRefCounted<ResourceSchedulerClient>(
        params->process_id, ++current_resource_scheduler_client_id_,
        resource_scheduler_.get(),
        url_request_context_->network_quality_estimator());
  }
  CreateURLLoaderFactory(std::move(request), std::move(params),
                         std::move(resource_scheduler_client));
}

void NetworkContext::GetCookieManager(mojom::CookieManagerRequest request) {
  cookie_manager_->AddRequest(std::move(request));
}

void NetworkContext::GetRestrictedCookieManager(
    mojom::RestrictedCookieManagerRequest request,
    const url::Origin& origin) {
  restricted_cookie_manager_bindings_.AddBinding(
      std::make_unique<RestrictedCookieManager>(
          url_request_context_->cookie_store(), origin),
      std::move(request));
}

void NetworkContext::OnProxyLookupComplete(
    ProxyLookupRequest* proxy_lookup_request) {
  auto it = proxy_lookup_requests_.find(proxy_lookup_request);
  DCHECK(it != proxy_lookup_requests_.end());
  proxy_lookup_requests_.erase(it);
}

void NetworkContext::DisableQuic() {
  url_request_context_->http_transaction_factory()->GetSession()->DisableQuic();
}

void NetworkContext::DestroyURLLoaderFactory(
    cors::CORSURLLoaderFactory* url_loader_factory) {
  auto it = url_loader_factories_.find(url_loader_factory);
  DCHECK(it != url_loader_factories_.end());
  url_loader_factories_.erase(it);
}

size_t NetworkContext::GetNumOutstandingResolveHostRequestsForTesting() const {
  size_t sum = 0;
  if (internal_host_resolver_)
    sum += internal_host_resolver_->GetNumOutstandingRequestsForTesting();
  for (const auto& host_resolver : host_resolvers_)
    sum += host_resolver->GetNumOutstandingRequestsForTesting();
  return sum;
}

void NetworkContext::ClearNetworkingHistorySince(
    base::Time time,
    base::OnceClosure completion_callback) {
  // TODO(mmenke): Neither of these methods waits until the changes have been
  // commited to disk. They probably should, as most similar methods net/
  // exposes do.

  // Completes synchronously.
  url_request_context_->transport_security_state()->DeleteAllDynamicDataSince(
      time);

  url_request_context_->http_server_properties()->Clear(
      std::move(completion_callback));
}

void NetworkContext::ClearHttpCache(base::Time start_time,
                                    base::Time end_time,
                                    mojom::ClearDataFilterPtr filter,
                                    ClearHttpCacheCallback callback) {
  // It's safe to use Unretained below as the HttpCacheDataRemover is owned by
  // |this| and guarantees it won't call its callback if deleted.
  http_cache_data_removers_.push_back(HttpCacheDataRemover::CreateAndStart(
      url_request_context_, std::move(filter), start_time, end_time,
      base::BindOnce(&NetworkContext::OnHttpCacheCleared,
                     base::Unretained(this), std::move(callback))));
}

void NetworkContext::ComputeHttpCacheSize(
    base::Time start_time,
    base::Time end_time,
    ComputeHttpCacheSizeCallback callback) {
  // It's safe to use Unretained below as the HttpCacheDataCounter is owned by
  // |this| and guarantees it won't call its callback if deleted.
  http_cache_data_counters_.push_back(HttpCacheDataCounter::CreateAndStart(
      url_request_context_, start_time, end_time,
      base::BindOnce(&NetworkContext::OnHttpCacheSizeComputed,
                     base::Unretained(this), std::move(callback))));
}

void NetworkContext::ClearChannelIds(base::Time start_time,
                                     base::Time end_time,
                                     mojom::ClearDataFilterPtr filter,
                                     ClearChannelIdsCallback callback) {
  net::ChannelIDService* channel_id_service =
      url_request_context_->channel_id_service();
  if (!channel_id_service) {
    std::move(callback).Run();
    return;
  }
  net::ChannelIDStore* channel_id_store =
      channel_id_service->GetChannelIDStore();
  if (!channel_id_store) {
    std::move(callback).Run();
    return;
  }

  channel_id_store->DeleteForDomainsCreatedBetween(
      MakeDomainFilter(filter.get()), start_time, end_time,
      base::BindOnce(&OnClearedChannelIds,
                     url_request_context_->ssl_config_service(),
                     std::move(callback)));
}

void NetworkContext::ClearHostCache(mojom::ClearDataFilterPtr filter,
                                    ClearHostCacheCallback callback) {
  net::HostCache* host_cache =
      url_request_context_->host_resolver()->GetHostCache();
  DCHECK(host_cache);
  host_cache->ClearForHosts(MakeDomainFilter(filter.get()));
  std::move(callback).Run();
}

void NetworkContext::ClearHttpAuthCache(base::Time start_time,
                                        ClearHttpAuthCacheCallback callback) {
  net::HttpNetworkSession* http_session =
      url_request_context_->http_transaction_factory()->GetSession();
  DCHECK(http_session);

  http_session->http_auth_cache()->ClearEntriesAddedSince(start_time);
  http_session->CloseAllConnections();

  std::move(callback).Run();
}

#if BUILDFLAG(ENABLE_REPORTING)
void NetworkContext::ClearReportingCacheReports(
    mojom::ClearDataFilterPtr filter,
    ClearReportingCacheReportsCallback callback) {
  net::ReportingService* reporting_service =
      url_request_context_->reporting_service();
  if (reporting_service) {
    if (filter) {
      reporting_service->RemoveBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS,
          BuildUrlFilter(std::move(filter)));
    } else {
      reporting_service->RemoveAllBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS);
    }
  }

  std::move(callback).Run();
}

void NetworkContext::ClearReportingCacheClients(
    mojom::ClearDataFilterPtr filter,
    ClearReportingCacheClientsCallback callback) {
  net::ReportingService* reporting_service =
      url_request_context_->reporting_service();
  if (reporting_service) {
    if (filter) {
      reporting_service->RemoveBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
          BuildUrlFilter(std::move(filter)));
    } else {
      reporting_service->RemoveAllBrowsingData(
          net::ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS);
    }
  }

  std::move(callback).Run();
}

void NetworkContext::ClearNetworkErrorLogging(
    mojom::ClearDataFilterPtr filter,
    ClearNetworkErrorLoggingCallback callback) {
  net::NetworkErrorLoggingService* logging_service =
      url_request_context_->network_error_logging_service();
  if (logging_service) {
    if (filter) {
      logging_service->RemoveBrowsingData(BuildUrlFilter(std::move(filter)));
    } else {
      logging_service->RemoveAllBrowsingData();
    }
  }

  std::move(callback).Run();
}
#else   // BUILDFLAG(ENABLE_REPORTING)
void NetworkContext::ClearReportingCacheReports(
    mojom::ClearDataFilterPtr filter,
    ClearReportingCacheReportsCallback callback) {
  NOTREACHED();
}

void NetworkContext::ClearReportingCacheClients(
    mojom::ClearDataFilterPtr filter,
    ClearReportingCacheClientsCallback callback) {
  NOTREACHED();
}

void NetworkContext::ClearNetworkErrorLogging(
    mojom::ClearDataFilterPtr filter,
    ClearNetworkErrorLoggingCallback callback) {
  NOTREACHED();
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

void NetworkContext::CloseAllConnections(CloseAllConnectionsCallback callback) {
  net::HttpNetworkSession* http_session =
      url_request_context_->http_transaction_factory()->GetSession();
  DCHECK(http_session);

  http_session->CloseAllConnections();

  std::move(callback).Run();
}

void NetworkContext::SetNetworkConditions(
    const base::UnguessableToken& throttling_profile_id,
    mojom::NetworkConditionsPtr conditions) {
  std::unique_ptr<NetworkConditions> network_conditions;
  if (conditions) {
    network_conditions.reset(new NetworkConditions(
        conditions->offline, conditions->latency.InMillisecondsF(),
        conditions->download_throughput, conditions->upload_throughput));
  }
  ThrottlingController::SetConditions(throttling_profile_id,
                                      std::move(network_conditions));
}

void NetworkContext::SetAcceptLanguage(const std::string& new_accept_language) {
  // This may only be called on NetworkContexts created with a constructor that
  // calls ApplyContextParamsToBuilder.
  DCHECK(user_agent_settings_);
  user_agent_settings_->set_accept_language(new_accept_language);
}

void NetworkContext::SetEnableReferrers(bool enable_referrers) {
  // This may only be called on NetworkContexts created with a constructor that
  // calls ApplyContextParamsToBuilder.
  DCHECK(context_network_delegate_);
  context_network_delegate_->set_enable_referrers(enable_referrers);
}

void NetworkContext::SetCTPolicy(
    const std::vector<std::string>& required_hosts,
    const std::vector<std::string>& excluded_hosts,
    const std::vector<std::string>& excluded_spkis,
    const std::vector<std::string>& excluded_legacy_spkis) {
  if (!require_ct_delegate_)
    return;

  require_ct_delegate_->UpdateCTPolicies(required_hosts, excluded_hosts,
                                         excluded_spkis, excluded_legacy_spkis);
}

void NetworkContext::CreateUDPSocket(mojom::UDPSocketRequest request,
                                     mojom::UDPSocketReceiverPtr receiver) {
  socket_factory_->CreateUDPSocket(std::move(request), std::move(receiver));
}

void NetworkContext::CreateTCPServerSocket(
    const net::IPEndPoint& local_addr,
    uint32_t backlog,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPServerSocketRequest request,
    CreateTCPServerSocketCallback callback) {
  socket_factory_->CreateTCPServerSocket(
      local_addr, backlog,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(request), std::move(callback));
}

void NetworkContext::CreateTCPConnectedSocket(
    const base::Optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::TCPConnectedSocketRequest request,
    mojom::SocketObserverPtr observer,
    CreateTCPConnectedSocketCallback callback) {
  socket_factory_->CreateTCPConnectedSocket(
      local_addr, remote_addr_list,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(request), std::move(observer), std::move(callback));
}

void NetworkContext::CreateProxyResolvingSocketFactory(
    mojom::ProxyResolvingSocketFactoryRequest request) {
  proxy_resolving_socket_factories_.AddBinding(
      std::make_unique<ProxyResolvingSocketFactoryMojo>(url_request_context()),
      std::move(request));
}

void NetworkContext::CreateWebSocket(
    mojom::WebSocketRequest request,
    int32_t process_id,
    int32_t render_frame_id,
    const url::Origin& origin,
    mojom::AuthenticationHandlerPtr auth_handler) {
#if !defined(OS_IOS)
  if (!websocket_factory_)
    websocket_factory_ = std::make_unique<WebSocketFactory>(this);
  websocket_factory_->CreateWebSocket(std::move(request),
                                      std::move(auth_handler), process_id,
                                      render_frame_id, origin);
#endif  // !defined(OS_IOS)
}

void NetworkContext::LookUpProxyForURL(
    const GURL& url,
    mojom::ProxyLookupClientPtr proxy_lookup_client) {
  DCHECK(proxy_lookup_client);
  std::unique_ptr<ProxyLookupRequest> proxy_lookup_request(
      std::make_unique<ProxyLookupRequest>(std::move(proxy_lookup_client),
                                           this));
  ProxyLookupRequest* proxy_lookup_request_ptr = proxy_lookup_request.get();
  proxy_lookup_requests_.insert(std::move(proxy_lookup_request));
  proxy_lookup_request_ptr->Start(url);
}

void NetworkContext::CreateNetLogExporter(
    mojom::NetLogExporterRequest request) {
  net_log_exporter_bindings_.AddBinding(std::make_unique<NetLogExporter>(this),
                                        std::move(request));
}

void NetworkContext::ResolveHost(
    const net::HostPortPair& host,
    mojom::ResolveHostParametersPtr optional_parameters,
    mojom::ResolveHostClientPtr response_client) {
  if (!internal_host_resolver_) {
    internal_host_resolver_ = std::make_unique<HostResolver>(
        url_request_context_->host_resolver(), url_request_context_->net_log());
  }

  internal_host_resolver_->ResolveHost(host, std::move(optional_parameters),
                                       std::move(response_client));
}

void NetworkContext::CreateHostResolver(mojom::HostResolverRequest request) {
  host_resolvers_.emplace(std::make_unique<HostResolver>(
      std::move(request),
      base::BindOnce(&NetworkContext::OnHostResolverShutdown,
                     base::Unretained(this)),
      url_request_context_->host_resolver(), url_request_context_->net_log()));
}

void NetworkContext::VerifyCertForSignedExchange(
    const scoped_refptr<net::X509Certificate>& certificate,
    const GURL& url,
    const std::string& ocsp_result,
    const std::string& sct_list,
    VerifyCertForSignedExchangeCallback callback) {
  int cert_verify_id = ++next_cert_verify_id_;
  auto pending_cert_verify = std::make_unique<PendingCertVerify>();
  pending_cert_verify->callback = std::move(callback);
  pending_cert_verify->result = std::make_unique<net::CertVerifyResult>();
  pending_cert_verify->certificate = certificate;
  pending_cert_verify->url = url;
  pending_cert_verify->ocsp_result = ocsp_result;
  pending_cert_verify->sct_list = sct_list;
  net::CertVerifier* cert_verifier =
      g_cert_verifier_for_testing ? g_cert_verifier_for_testing
                                  : url_request_context_->cert_verifier();
  int result = cert_verifier->Verify(
      net::CertVerifier::RequestParams(certificate, url.host(),
                                       0 /* cert_verify_flags */, ocsp_result),
      pending_cert_verify->result.get(),
      base::BindOnce(&NetworkContext::OnCertVerifyForSignedExchangeComplete,
                     base::Unretained(this), cert_verify_id),
      &pending_cert_verify->request,
      net::NetLogWithSource::Make(url_request_context_->net_log(),
                                  net::NetLogSourceType::CERT_VERIFIER_JOB));
  cert_verifier_requests_[cert_verify_id] = std::move(pending_cert_verify);

  if (result != net::ERR_IO_PENDING)
    OnCertVerifyForSignedExchangeComplete(cert_verify_id, result);
}

void NetworkContext::WriteCacheMetadata(const GURL& url,
                                        net::RequestPriority priority,
                                        base::Time expected_response_time,
                                        const std::vector<uint8_t>& data) {
  net::HttpCache* cache =
      url_request_context_->http_transaction_factory()->GetCache();
  if (!cache)
    return;

  auto buf = base::MakeRefCounted<net::IOBuffer>(data.size());
  memcpy(buf->data(), data.data(), data.size());
  cache->WriteMetadata(url, priority, expected_response_time, buf.get(),
                       data.size());
}

void NetworkContext::IsHSTSActiveForHost(const std::string& host,
                                         IsHSTSActiveForHostCallback callback) {
  net::TransportSecurityState* security_state =
      url_request_context_->transport_security_state();

  if (!security_state) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(security_state->ShouldUpgradeToSSL(host));
}

void NetworkContext::AddHSTSForTesting(const std::string& host,
                                       base::Time expiry,
                                       bool include_subdomains,
                                       AddHSTSForTestingCallback callback) {
  net::TransportSecurityState* state =
      url_request_context_->transport_security_state();
  state->AddHSTS(host, expiry, include_subdomains);
  std::move(callback).Run();
}

void NetworkContext::SetFailingHttpTransactionForTesting(
    int32_t error_code,
    SetFailingHttpTransactionForTestingCallback callback) {
  net::HttpCache* cache(
      url_request_context_->http_transaction_factory()->GetCache());
  DCHECK(cache);
  auto factory = std::make_unique<net::FailingHttpTransactionFactory>(
      cache->GetSession(), static_cast<net::Error>(error_code));

  // Throw away old version; since this is a a browser test, we don't
  // need to restore the old state.
  cache->SetHttpNetworkTransactionFactoryForTesting(std::move(factory));

  std::move(callback).Run();
}

void NetworkContext::PreconnectSockets(uint32_t num_streams,
                                       const GURL& original_url,
                                       int32_t load_flags,
                                       bool privacy_mode_enabled) {
  GURL url = GetHSTSRedirect(original_url);

  // |PreconnectSockets| may receive arguments from the renderer, which is not
  // guaranteed to validate them.
  if (num_streams == 0)
    return;

  std::string user_agent;
  if (url_request_context_->http_user_agent_settings()) {
    user_agent =
        url_request_context_->http_user_agent_settings()->GetUserAgent();
  }
  net::HttpRequestInfo request_info;
  request_info.url = url;
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                       user_agent);

  request_info.privacy_mode = privacy_mode_enabled ? net::PRIVACY_MODE_ENABLED
                                                   : net::PRIVACY_MODE_DISABLED;
  request_info.load_flags = load_flags;

  net::HttpTransactionFactory* factory =
      url_request_context_->http_transaction_factory();
  net::HttpNetworkSession* session = factory->GetSession();
  net::HttpStreamFactory* http_stream_factory = session->http_stream_factory();
  http_stream_factory->PreconnectStreams(
      base::saturated_cast<int32_t>(num_streams), request_info);
}

void NetworkContext::CreateP2PSocketManager(
    mojom::P2PTrustedSocketManagerClientPtr client,
    mojom::P2PTrustedSocketManagerRequest trusted_socket_manager,
    mojom::P2PSocketManagerRequest socket_manager_request) {
  std::unique_ptr<P2PSocketManager> socket_manager =
      std::make_unique<P2PSocketManager>(
          std::move(client), std::move(trusted_socket_manager),
          std::move(socket_manager_request),
          base::Bind(&NetworkContext::DestroySocketManager,
                     base::Unretained(this)),
          url_request_context_);
  socket_managers_[socket_manager.get()] = std::move(socket_manager);
}

void NetworkContext::ResetURLLoaderFactories() {
  for (const auto& factory : url_loader_factories_)
    factory->ClearBindings();
}

// ApplyContextParamsToBuilder represents the core configuration for
// translating |network_context_params| into a set of configuration that can
// be used to build a request context. All new initialization should be done
// within this method. If objects need to be created that would not be owned
// by |builder| - that is, objects that would be stored and owned in the
// NetworkContext if this method was not static - should be added as
// (optional) out-params.
URLRequestContextOwner NetworkContext::ApplyContextParamsToBuilder(
    URLRequestContextBuilderMojo* builder) {
  net::NetLog* net_log = nullptr;
  if (network_service_) {
    net_log = network_service_->net_log();
    builder->set_net_log(net_log);
    builder->set_shared_host_resolver(network_service_->host_resolver());
    builder->set_shared_http_auth_handler_factory(
        network_service_->GetHttpAuthHandlerFactory());
    builder->set_network_quality_estimator(
        network_service_->network_quality_estimator());
  }

  scoped_refptr<network::SessionCleanupCookieStore>
      session_cleanup_cookie_store;
  scoped_refptr<SessionCleanupChannelIDStore> session_cleanup_channel_id_store;
  if (params_->cookie_path) {
    scoped_refptr<base::SequencedTaskRunner> client_task_runner =
        base::MessageLoopCurrent::Get()->task_runner();
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

    std::unique_ptr<net::ChannelIDService> channel_id_service;
    if (params_->channel_id_path) {
      session_cleanup_channel_id_store =
          base::MakeRefCounted<SessionCleanupChannelIDStore>(
              params_->channel_id_path.value(), background_task_runner);
      channel_id_service = std::make_unique<net::ChannelIDService>(
          new net::DefaultChannelIDStore(
              session_cleanup_channel_id_store.get()));
    }

    net::CookieCryptoDelegate* crypto_delegate = nullptr;
    if (params_->enable_encrypted_cookies) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(IS_CHROMECAST)
      DCHECK(network_service_->os_crypt_config_set())
          << "NetworkService::SetCryptConfig must be called before creating a "
             "NetworkContext with encrypted cookies.";
#endif
      crypto_delegate = cookie_config::GetCookieCryptoDelegate();
    }
    scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
        new net::SQLitePersistentCookieStore(
            params_->cookie_path.value(), client_task_runner,
            background_task_runner, params_->restore_old_session_cookies,
            crypto_delegate));

    session_cleanup_cookie_store =
        base::MakeRefCounted<network::SessionCleanupCookieStore>(sqlite_store);

    std::unique_ptr<net::CookieMonster> cookie_store =
        std::make_unique<net::CookieMonster>(session_cleanup_cookie_store.get(),
                                             channel_id_service.get(), net_log);
    if (params_->persist_session_cookies)
      cookie_store->SetPersistSessionCookies(true);

    if (channel_id_service) {
      cookie_store->SetChannelIDServiceID(channel_id_service->GetUniqueID());
    }
    builder->SetCookieAndChannelIdStores(std::move(cookie_store),
                                         std::move(channel_id_service));
  } else {
    DCHECK(!params_->restore_old_session_cookies);
    DCHECK(!params_->persist_session_cookies);
  }

  std::unique_ptr<net::StaticHttpUserAgentSettings> user_agent_settings =
      std::make_unique<net::StaticHttpUserAgentSettings>(
          params_->accept_language, params_->user_agent);
  // Borrow an alias for future use before giving the builder ownership.
  user_agent_settings_ = user_agent_settings.get();
  builder->set_http_user_agent_settings(std::move(user_agent_settings));

  builder->set_enable_brotli(params_->enable_brotli);
  if (params_->context_name)
    builder->set_name(*params_->context_name);

  if (params_->proxy_resolver_factory) {
    builder->SetMojoProxyResolverFactory(
        proxy_resolver::mojom::ProxyResolverFactoryPtr(
            std::move(params_->proxy_resolver_factory)));
  }

  if (!params_->http_cache_enabled) {
    builder->DisableHttpCache();
  } else {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    cache_params.max_size = params_->http_cache_max_size;
    if (!params_->http_cache_path) {
      cache_params.type =
          net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
    } else {
      cache_params.path = *params_->http_cache_path;
      cache_params.type = network_session_configurator::ChooseCacheType(
          *base::CommandLine::ForCurrentProcess());
    }

    builder->EnableHttpCache(cache_params);
  }

  std::unique_ptr<SSLConfigServiceMojo> ssl_config_service =
      std::make_unique<SSLConfigServiceMojo>(
          std::move(params_->initial_ssl_config),
          std::move(params_->ssl_config_client_request),
          network_service_->crl_set_distributor());
  SSLConfigServiceMojo* ssl_config_service_raw = ssl_config_service.get();
  builder->set_ssl_config_service(std::move(ssl_config_service));

  if (!params_->initial_proxy_config &&
      !params_->proxy_config_client_request.is_pending()) {
    params_->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
  }
  builder->set_proxy_config_service(std::make_unique<ProxyConfigServiceMojo>(
      std::move(params_->proxy_config_client_request),
      std::move(params_->initial_proxy_config),
      std::move(params_->proxy_config_poller_client)));
  builder->set_pac_quick_check_enabled(params_->pac_quick_check_enabled);
  builder->set_pac_sanitize_url_policy(
      params_->dangerously_allow_pac_access_to_secure_urls
          ? net::ProxyResolutionService::SanitizeUrlPolicy::UNSAFE
          : net::ProxyResolutionService::SanitizeUrlPolicy::SAFE);

  std::unique_ptr<PrefService> pref_service;
  if (params_->http_server_properties_path) {
    scoped_refptr<JsonPrefStore> json_pref_store(new JsonPrefStore(
        *params_->http_server_properties_path, nullptr,
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
             base::TaskPriority::BEST_EFFORT})));
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

  if (params_->transport_security_persister_path) {
    builder->set_transport_security_persister_path(
        *params_->transport_security_persister_path);
  }

  builder->set_data_enabled(params_->enable_data_url_support);
#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
  builder->set_file_enabled(params_->enable_file_url_support);
#else  // BUILDFLAG(DISABLE_FILE_SUPPORT)
  DCHECK(!params_->enable_file_url_support);
#endif
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  builder->set_ftp_enabled(params_->enable_ftp_url_support);
#else  // BUILDFLAG(DISABLE_FTP_SUPPORT)
  DCHECK(!params_->enable_ftp_url_support);
#endif

#if BUILDFLAG(ENABLE_REPORTING)
  if (base::FeatureList::IsEnabled(features::kReporting))
    builder->set_reporting_policy(net::ReportingPolicy::Create());
  else
    builder->set_reporting_policy(nullptr);

  builder->set_network_error_logging_enabled(
      base::FeatureList::IsEnabled(features::kNetworkErrorLogging));
#endif  // BUILDFLAG(ENABLE_REPORTING)

  if (params_->enforce_chrome_ct_policy) {
    builder->set_ct_policy_enforcer(
        std::make_unique<certificate_transparency::ChromeCTPolicyEnforcer>());
  }

  net::HttpNetworkSession::Params session_params;
  bool is_quic_force_disabled = false;
  if (network_service_ && network_service_->quic_disabled())
    is_quic_force_disabled = true;

  network_session_configurator::ParseCommandLineAndFieldTrials(
      *base::CommandLine::ForCurrentProcess(), is_quic_force_disabled,
      params_->quic_user_agent_id, &session_params);

  session_params.http_09_on_non_default_ports_enabled =
      params_->http_09_on_non_default_ports_enabled;

  builder->set_http_network_session_params(session_params);

  builder->SetCreateHttpTransactionFactoryCallback(
      base::BindOnce([](net::HttpNetworkSession* session)
                         -> std::unique_ptr<net::HttpTransactionFactory> {
        return std::make_unique<ThrottlingNetworkTransactionFactory>(session);
      }));

  // Can't just overwrite the NetworkDelegate because one might have already
  // been set on the |builder| before it was passed to the NetworkContext.
  // TODO(mmenke): Clean this up, once NetworkContext no longer needs to
  // support taking a URLRequestContextBuilder with a pre-configured
  // NetworkContext.
  builder->SetCreateLayeredNetworkDelegateCallback(base::BindOnce(
      [](mojom::NetworkContextParams* network_context_params,
         ContextNetworkDelegate** out_context_network_delegate,
         NetworkContext* network_context,
         std::unique_ptr<net::NetworkDelegate> nested_network_delegate)
          -> std::unique_ptr<net::NetworkDelegate> {
        std::unique_ptr<ContextNetworkDelegate> context_network_delegate =
            std::make_unique<ContextNetworkDelegate>(
                std::move(nested_network_delegate),
                network_context_params->enable_referrers,
                network_context_params
                    ->validate_referrer_policy_on_initial_request,
                network_context);
        if (out_context_network_delegate)
          *out_context_network_delegate = context_network_delegate.get();
        return context_network_delegate;
      },
      params_.get(), &context_network_delegate_, this));

  std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs;
  if (!params_->ct_logs.empty()) {
    for (const auto& log : params_->ct_logs) {
      scoped_refptr<const net::CTLogVerifier> log_verifier =
          net::CTLogVerifier::Create(log->public_key, log->name,
                                     log->dns_api_endpoint);
      if (!log_verifier) {
        // TODO: Signal bad configuration (such as bad key).
        continue;
      }
      ct_logs.push_back(std::move(log_verifier));
    }
    auto ct_verifier = std::make_unique<net::MultiLogCTVerifier>();
    ct_verifier->AddLogs(ct_logs);
    builder->set_ct_verifier(std::move(ct_verifier));
  }

  auto result =
      URLRequestContextOwner(std::move(pref_service), builder->Build());

  // Subscribe the CertVerifier to configuration changes that are exposed via
  // the mojom::SSLConfig, but which are not part of the
  // net::SSLConfig[Service] interfaces.
  ssl_config_service_raw->SetCertVerifierForConfiguring(
      result.url_request_context->cert_verifier());

  // Attach some things to the URLRequestContextBuilder's
  // TransportSecurityState.  Since no requests have been made yet, safe to do
  // this even after the call to Build().

  if (params_->enable_certificate_reporting) {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("domain_security_policy", R"(
        semantics {
          sender: "Domain Security Policy"
          description:
            "Websites can opt in to have Chrome send reports to them when "
            "Chrome observes connections to that website that do not meet "
            "stricter security policies, such as with HTTP Public Key Pinning. "
            "Websites can use this feature to discover misconfigurations that "
            "prevent them from complying with stricter security policies that "
            "they\'ve opted in to."
          trigger:
            "Chrome observes that a user is loading a resource from a website "
            "that has opted in for security policy reports, and the connection "
            "does not meet the required security policies."
          data:
            "The time of the request, the hostname and port being requested, "
            "the certificate chain, and sometimes certificate revocation "
            "information included on the connection."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
            "Not implemented, this is a feature that websites can opt into and "
            "thus there is no Chrome-wide policy to disable it."
        })");
    certificate_report_sender_ = std::make_unique<net::ReportSender>(
        result.url_request_context.get(), traffic_annotation);
    result.url_request_context->transport_security_state()->SetReportSender(
        certificate_report_sender_.get());
  }

  if (params_->enable_expect_ct_reporting) {
    expect_ct_reporter_ = std::make_unique<ExpectCTReporter>(
        result.url_request_context.get(), base::Closure(), base::Closure());
    result.url_request_context->transport_security_state()->SetExpectCTReporter(
        expect_ct_reporter_.get());
  }

#if !defined(OS_IOS)
  if (base::FeatureList::IsEnabled(certificate_transparency::kCTLogAuditing) &&
      network_service_ && !ct_logs.empty()) {
    net::URLRequestContext* context = result.url_request_context.get();
    ct_tree_tracker_ =
        std::make_unique<certificate_transparency::TreeStateTracker>(
            ct_logs, context->host_resolver(), net_log);
    context->cert_transparency_verifier()->SetObserver(ct_tree_tracker_.get());
    network_service_->sth_reporter()->RegisterObserver(ct_tree_tracker_.get());
  }
#endif

  if (params_->enforce_chrome_ct_policy) {
    require_ct_delegate_ =
        std::make_unique<certificate_transparency::ChromeRequireCTDelegate>();
    result.url_request_context->transport_security_state()
        ->SetRequireCTDelegate(require_ct_delegate_.get());
  }

  // These must be matched by cleanup code just before the URLRequestContext is
  // destroyed.
  if (params_->primary_network_context) {
#if defined(USE_NSS_CERTS)
    net::SetURLRequestContextForNSSHttpIO(result.url_request_context.get());
#endif
#if defined(OS_ANDROID) || defined(OS_FUCHSIA) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_MACOSX)
    net::SetGlobalCertNetFetcher(
        net::CreateCertNetFetcher(result.url_request_context.get()));
#endif
  }

  cookie_manager_ = std::make_unique<CookieManager>(
      result.url_request_context->cookie_store(),
      std::move(session_cleanup_cookie_store),
      std::move(session_cleanup_channel_id_store),
      std::move(params_->cookie_manager_params));

  return result;
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

void NetworkContext::OnHostResolverShutdown(HostResolver* resolver) {
  auto found_resolver = host_resolvers_.find(resolver);
  DCHECK(found_resolver != host_resolvers_.end());
  host_resolvers_.erase(found_resolver);
}

void NetworkContext::OnHttpCacheSizeComputed(
    ComputeHttpCacheSizeCallback callback,
    HttpCacheDataCounter* counter,
    bool is_upper_limit,
    int64_t result_or_error) {
  EraseIf(http_cache_data_counters_, base::MatchesUniquePtr(counter));
  std::move(callback).Run(is_upper_limit, result_or_error);
}

void NetworkContext::OnConnectionError() {
  // If owned by the network service, this call will delete |this|.
  if (on_connection_close_callback_)
    std::move(on_connection_close_callback_).Run(this);
}

URLRequestContextOwner NetworkContext::MakeURLRequestContext() {
  URLRequestContextBuilderMojo builder;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (g_cert_verifier_for_testing) {
    builder.SetCertVerifier(std::make_unique<WrappedTestingCertVerifier>());
  } else {
    std::unique_ptr<net::CertVerifier> cert_verifier =
        net::CertVerifier::CreateDefault();
    builder.SetCertVerifier(IgnoreErrorsCertVerifier::MaybeWrapCertVerifier(
        *command_line, nullptr, std::move(cert_verifier)));
  }

  std::unique_ptr<net::NetworkDelegate> network_delegate =
      std::make_unique<NetworkServiceNetworkDelegate>(this);
  builder.set_network_delegate(std::move(network_delegate));

  // |network_service_| may be nullptr in tests.
  auto result = ApplyContextParamsToBuilder(&builder);

  return result;
}

GURL NetworkContext::GetHSTSRedirect(const GURL& original_url) {
  // TODO(lilyhoughton) This needs to be gotten rid of once explicit
  // construction with a URLRequestContext is no longer supported.
  if (!url_request_context_->transport_security_state() ||
      !original_url.SchemeIs("http") ||
      !url_request_context_->transport_security_state()->ShouldUpgradeToSSL(
          original_url.host())) {
    return original_url;
  }

  url::Replacements<char> replacements;
  const char kNewScheme[] = "https";
  replacements.SetScheme(kNewScheme, url::Component(0, strlen(kNewScheme)));
  return original_url.ReplaceComponents(replacements);
}

void NetworkContext::DestroySocketManager(P2PSocketManager* socket_manager) {
  auto iter = socket_managers_.find(socket_manager);
  DCHECK(iter != socket_managers_.end());
  socket_managers_.erase(iter);
}

void NetworkContext::OnCertVerifyForSignedExchangeComplete(int cert_verify_id,
                                                           int result) {
  auto iter = cert_verifier_requests_.find(cert_verify_id);
  DCHECK(iter != cert_verifier_requests_.end());

  auto pending_cert_verify = std::move(iter->second);
  cert_verifier_requests_.erase(iter);

  net::ct::CTVerifyResult ct_verify_result;
  if (result == net::OK) {
    net::X509Certificate* verified_cert =
        pending_cert_verify->result->verified_cert.get();
    url_request_context_->cert_transparency_verifier()->Verify(
        pending_cert_verify->url.host(), verified_cert,
        pending_cert_verify->ocsp_result, pending_cert_verify->sct_list,
        &ct_verify_result.scts,
        net::NetLogWithSource::Make(
            network_service_ ? url_request_context_->net_log() : nullptr,
            net::NetLogSourceType::CERT_VERIFIER_JOB));

    net::ct::SCTList verified_scts = net::ct::SCTsMatchingStatus(
        ct_verify_result.scts, net::ct::SCT_STATUS_OK);

    ct_verify_result.policy_compliance =
        url_request_context_->ct_policy_enforcer()->CheckCompliance(
            verified_cert, verified_scts,
            net::NetLogWithSource::Make(
                network_service_ ? url_request_context_->net_log() : nullptr,
                net::NetLogSourceType::CERT_VERIFIER_JOB));

    // TODO(https://crbug.com/803774): We should determine whether EV & SXG
    // should be a thing (due to the online/offline signing difference)
    if (pending_cert_verify->result->cert_status & net::CERT_STATUS_IS_EV &&
        ct_verify_result.policy_compliance !=
            net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS &&
        ct_verify_result.policy_compliance !=
            net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY) {
      pending_cert_verify->result->cert_status |=
          net::CERT_STATUS_CT_COMPLIANCE_FAILED;
      pending_cert_verify->result->cert_status &= ~net::CERT_STATUS_IS_EV;
    }

    net::TransportSecurityState::CTRequirementsStatus ct_requirement_status =
        url_request_context_->transport_security_state()->CheckCTRequirements(
            net::HostPortPair::FromURL(pending_cert_verify->url),
            pending_cert_verify->result->is_issued_by_known_root,
            pending_cert_verify->result->public_key_hashes, verified_cert,
            pending_cert_verify->certificate.get(), ct_verify_result.scts,
            net::TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
            ct_verify_result.policy_compliance);

    switch (ct_requirement_status) {
      case net::TransportSecurityState::CT_REQUIREMENTS_NOT_MET:
        result = net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
        break;
      case net::TransportSecurityState::CT_REQUIREMENTS_MET:
        ct_verify_result.policy_compliance_required = true;
        break;
      case net::TransportSecurityState::CT_NOT_REQUIRED:
        // CT is not required if the certificate does not chain to a publicly
        // trusted root certificate.
        if (!pending_cert_verify->result->is_issued_by_known_root) {
          ct_verify_result.policy_compliance_required = false;
          break;
        }
        // For old certificates (issued before 2018-05-01),
        // CheckCTRequirements() may return CT_NOT_REQUIRED, so we check the
        // compliance status here.
        // TODO(https://crbug.com/851778): Remove this condition once we require
        // signing certificates to have CanSignHttpExchanges extension, because
        // such certificates should be naturally after 2018-05-01.
        if (ct_verify_result.policy_compliance ==
                net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS ||
            ct_verify_result.policy_compliance ==
                net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY) {
          ct_verify_result.policy_compliance_required = true;
          break;
        }
        // Require CT compliance, by overriding CT_NOT_REQUIRED and treat it as
        // ERR_CERTIFICATE_TRANSPARENCY_REQUIRED.
        result = net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
    }
  }

  std::move(pending_cert_verify->callback)
      .Run(result, *pending_cert_verify->result.get(), ct_verify_result);
}

}  // namespace network
