// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/os_crypt/os_crypt.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/logging_network_change_observer.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_posix.h"
#include "net/base/port_util.h"
#include "net/cert/cert_database.h"
#include "net/cert/ct_log_response_parser.h"
#include "net/cert/signed_tree_head.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_overrides.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_util.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/ssl/ssl_key_logger_impl.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/crl_set_distributor.h"
#include "services/network/cross_origin_read_blocking.h"
#include "services/network/dns_config_change_manager.h"
#include "services/network/http_auth_cache_copier.h"
#include "services/network/net_log_exporter.h"
#include "services/network/network_context.h"
#include "services/network/network_usage_accumulator.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/url_loader.h"
#include "services/network/url_request_context_builder_mojo.h"

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/cpu.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(IS_CHROMECAST)
#include "components/os_crypt/key_storage_config_linux.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#include "net/android/http_auth_negotiate_android.h"
#endif

namespace network {

namespace {

NetworkService* g_network_service = nullptr;

net::NetLog* GetNetLog() {
  static base::NoDestructor<net::NetLog> instance;
  return instance.get();
}

// The interval for calls to NetworkService::UpdateLoadStates
constexpr auto kUpdateLoadStatesInterval =
    base::TimeDelta::FromMilliseconds(250);

std::unique_ptr<net::NetworkChangeNotifier> CreateNetworkChangeNotifierIfNeeded(
    net::NetworkChangeNotifier::ConnectionType initial_connection_type,
    net::NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype) {
  // There is a global singleton net::NetworkChangeNotifier if NetworkService
  // is running inside of the browser process.
  if (!net::NetworkChangeNotifier::HasNetworkChangeNotifier()) {
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    // On Android and ChromeOS, network change events are synced from the
    // browser process.
    return std::make_unique<net::NetworkChangeNotifierPosix>(
        initial_connection_type, initial_connection_subtype);
#elif defined(OS_IOS)
    // iOS doesn't embed //content.
    // TODO(xunjieli): Figure out what to do for iOS.
    NOTIMPLEMENTED();
    return nullptr;
#else
    return net::NetworkChangeNotifier::Create();
#endif
  }
  return nullptr;
}

// This is duplicated in content/browser/loader/resource_dispatcher_host_impl.cc
bool LoadInfoIsMoreInteresting(const mojom::LoadInfo& a,
                               const mojom::LoadInfo& b) {
  // Set |*_uploading_size| to be the size of the corresponding upload body if
  // it's currently being uploaded.

  uint64_t a_uploading_size = 0;
  if (a.load_state == net::LOAD_STATE_SENDING_REQUEST)
    a_uploading_size = a.upload_size;

  uint64_t b_uploading_size = 0;
  if (b.load_state == net::LOAD_STATE_SENDING_REQUEST)
    b_uploading_size = b.upload_size;

  if (a_uploading_size != b_uploading_size)
    return a_uploading_size > b_uploading_size;

  return a.load_state > b.load_state;
}

void OnGetNetworkList(std::unique_ptr<net::NetworkInterfaceList> networks,
                      mojom::NetworkService::GetNetworkListCallback callback,
                      bool success) {
  if (success) {
    std::move(callback).Run(*networks);
  } else {
    std::move(callback).Run(base::nullopt);
  }
}

#if defined(OS_ANDROID) && BUILDFLAG(USE_KERBEROS)
// Used for Negotiate authentication on Android, which needs to generate tokens
// in the browser process.
class NetworkServiceAuthNegotiateAndroid : public net::HttpNegotiateAuthSystem {
 public:
  NetworkServiceAuthNegotiateAndroid(NetworkService* network_service,
                                     const net::HttpAuthPreferences* prefs)
      : network_service_(network_service), auth_negotiate_(prefs) {}
  ~NetworkServiceAuthNegotiateAndroid() override = default;

  // HttpNegotiateAuthSystem implementation:
  bool Init(const net::NetLogWithSource& net_log) override {
    return auth_negotiate_.Init(net_log);
  }

  bool NeedsIdentity() const override {
    return auth_negotiate_.NeedsIdentity();
  }

  bool AllowsExplicitCredentials() const override {
    return auth_negotiate_.AllowsExplicitCredentials();
  }

  net::HttpAuth::AuthorizationResult ParseChallenge(
      net::HttpAuthChallengeTokenizer* tok) override {
    return auth_negotiate_.ParseChallenge(tok);
  }

  int GenerateAuthToken(const net::AuthCredentials* credentials,
                        const std::string& spn,
                        const std::string& channel_bindings,
                        std::string* auth_token,
                        const net::NetLogWithSource& net_log,
                        net::CompletionOnceCallback callback) override {
    network_service_->client()->OnGenerateHttpNegotiateAuthToken(
        auth_negotiate_.server_auth_token(), auth_negotiate_.can_delegate(),
        auth_negotiate_.GetAuthAndroidNegotiateAccountType(), spn,
        base::BindOnce(&NetworkServiceAuthNegotiateAndroid::Finish,
                       weak_factory_.GetWeakPtr(), auth_token,
                       std::move(callback)));
    return net::ERR_IO_PENDING;
  }

  void SetDelegation(net::HttpAuth::DelegationType delegation_type) override {
    auth_negotiate_.SetDelegation(delegation_type);
  }

 private:
  void Finish(std::string* auth_token_out,
              net::CompletionOnceCallback callback,
              int result,
              const std::string& auth_token) {
    *auth_token_out = auth_token;
    std::move(callback).Run(result);
  }

  NetworkService* network_service_ = nullptr;
  net::android::HttpAuthNegotiateAndroid auth_negotiate_;
  base::WeakPtrFactory<NetworkServiceAuthNegotiateAndroid> weak_factory_{this};
};

std::unique_ptr<net::HttpNegotiateAuthSystem> CreateAuthSystem(
    NetworkService* network_service,
    const net::HttpAuthPreferences* prefs) {
  return std::make_unique<NetworkServiceAuthNegotiateAndroid>(network_service,
                                                              prefs);
}
#endif

// Called when NetworkService received a bad IPC message (but only when
// NetworkService is running in a separate process - otherwise the existing bad
// message handling inside the Browser process is sufficient).
void HandleBadMessage(const std::string& error) {
  LOG(WARNING) << "Mojo error in NetworkService:" << error;
  static auto* bad_message_reason = base::debug::AllocateCrashKeyString(
      "bad_message_reason", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(bad_message_reason, error);
  base::debug::DumpWithoutCrashing();
}

}  // namespace

NetworkService::NetworkService(
    std::unique_ptr<service_manager::BinderRegistry> registry,
    mojom::NetworkServiceRequest request,
    service_manager::mojom::ServiceRequest service_request,
    bool delay_initialization_until_set_client)
    : net_log_(GetNetLog()), registry_(std::move(registry)), binding_(this) {
  DCHECK(!g_network_service);
  g_network_service = this;

  // In testing environments, |service_request| may not be provided.
  if (service_request.is_pending())
    service_binding_.Bind(std::move(service_request));

  // |registry_| is nullptr when an in-process NetworkService is
  // created directly. The latter is done in concert with using
  // CreateNetworkContextWithBuilder to ease the transition to using the
  // network service.
  if (registry_) {
    mojo::core::SetDefaultProcessErrorCallback(
        base::BindRepeating(&HandleBadMessage));

    DCHECK(!request.is_pending());
    registry_->AddInterface<mojom::NetworkService>(
        base::BindRepeating(&NetworkService::Bind, base::Unretained(this)));
  } else if (request.is_pending()) {
    Bind(std::move(request));
  }

  if (!delay_initialization_until_set_client)
    Initialize(mojom::NetworkServiceParams::New());
}

void NetworkService::Initialize(mojom::NetworkServiceParamsPtr params) {
  if (initialized_)
    return;

  initialized_ = true;

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
  // Make sure OpenSSL is initialized before using it to histogram data.
  crypto::EnsureOpenSSLInit();

  // Measure CPUs with broken NEON units. See https://crbug.com/341598.
  UMA_HISTOGRAM_BOOLEAN("Net.HasBrokenNEON", CRYPTO_has_broken_NEON());
  // Measure Android kernels with missing AT_HWCAP2 auxv fields. See
  // https://crbug.com/boringssl/46.
  UMA_HISTOGRAM_BOOLEAN("Net.NeedsHWCAP2Workaround",
                        CRYPTO_needs_hwcap2_workaround());
#endif

  if (!params->environment.empty())
    SetEnvironment(std::move(params->environment));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Set-up the global port overrides.
  if (command_line->HasSwitch(switches::kExplicitlyAllowedPorts)) {
    std::string allowed_ports =
        command_line->GetSwitchValueASCII(switches::kExplicitlyAllowedPorts);
    net::SetExplicitlyAllowedPorts(allowed_ports);
  }

  // Record this once per session, though the switch is appled on a
  // per-NetworkContext basis.
  UMA_HISTOGRAM_BOOLEAN(
      "Net.Certificate.IgnoreCertificateErrorsSPKIListPresent",
      command_line->HasSwitch(switches::kIgnoreCertificateErrorsSPKIList));

  network_change_manager_ = std::make_unique<NetworkChangeManager>(
      CreateNetworkChangeNotifierIfNeeded(
          net::NetworkChangeNotifier::ConnectionType(
              params->initial_connection_type),
          net::NetworkChangeNotifier::ConnectionSubtype(
              params->initial_connection_subtype)));

  trace_net_log_observer_.WatchForTraceStart(net_log_);

  // Add an observer that will emit network change events to |net_log_|.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_ =
      std::make_unique<net::LoggingNetworkChangeObserver>(net_log_);

  network_quality_estimator_manager_ =
      std::make_unique<NetworkQualityEstimatorManager>(net_log_);

  dns_config_change_manager_ = std::make_unique<DnsConfigChangeManager>();

  host_resolver_manager_ = std::make_unique<net::HostResolverManager>(
      net::HostResolver::ManagerOptions(), net_log_);
  host_resolver_factory_ = std::make_unique<net::HostResolver::Factory>();

  network_usage_accumulator_ = std::make_unique<NetworkUsageAccumulator>();

  http_auth_cache_copier_ = std::make_unique<HttpAuthCacheCopier>();

  crl_set_distributor_ = std::make_unique<CRLSetDistributor>();
}

NetworkService::~NetworkService() {
  DCHECK_EQ(this, g_network_service);
  g_network_service = nullptr;
  // Destroy owned network contexts.
  DestroyNetworkContexts();

  // All NetworkContexts (Owned and unowned) must have been deleted by this
  // point.
  DCHECK(network_contexts_.empty());

  if (file_net_log_observer_) {
    file_net_log_observer_->StopObserving(nullptr /*polled_data*/,
                                          base::OnceClosure());
  }

  if (initialized_)
    trace_net_log_observer_.StopWatchForTraceStart();
}

void NetworkService::set_os_crypt_is_configured() {
  os_crypt_config_set_ = true;
}

std::unique_ptr<NetworkService> NetworkService::Create(
    mojom::NetworkServiceRequest request,
    service_manager::mojom::ServiceRequest service_request) {
  return std::make_unique<NetworkService>(nullptr, std::move(request),
                                          std::move(service_request));
}

std::unique_ptr<mojom::NetworkContext>
NetworkService::CreateNetworkContextWithBuilder(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params,
    std::unique_ptr<URLRequestContextBuilderMojo> builder,
    net::URLRequestContext** url_request_context) {
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  std::unique_ptr<NetworkContext> network_context =
      std::make_unique<NetworkContext>(this, std::move(request),
                                       std::move(params), std::move(builder));
  *url_request_context = network_context->url_request_context();
  return network_context;
}

std::unique_ptr<NetworkService> NetworkService::CreateForTesting() {
  return CreateForTesting(nullptr);
}

std::unique_ptr<NetworkService> NetworkService::CreateForTesting(
    service_manager::mojom::ServiceRequest service_request) {
  return std::make_unique<NetworkService>(
      std::make_unique<service_manager::BinderRegistry>(),
      nullptr /* request */, std::move(service_request));
}

void NetworkService::RegisterNetworkContext(NetworkContext* network_context) {
  // If IsPrimaryNetworkContext() is true, there must be no other
  // NetworkContexts created yet.
  DCHECK(!network_context->IsPrimaryNetworkContext() ||
         network_contexts_.empty());

  DCHECK_EQ(0u, network_contexts_.count(network_context));
  network_contexts_.insert(network_context);
  if (quic_disabled_)
    network_context->DisableQuic();
}

void NetworkService::DeregisterNetworkContext(NetworkContext* network_context) {
  // If the NetworkContext is the primary network context, all other
  // NetworkContexts must already have been destroyed.
  DCHECK(!network_context->IsPrimaryNetworkContext() ||
         network_contexts_.size() == 1);

  DCHECK_EQ(1u, network_contexts_.count(network_context));
  network_contexts_.erase(network_context);
}

#if defined(OS_CHROMEOS)
void NetworkService::ReinitializeLogging(mojom::LoggingSettingsPtr settings) {
  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = settings->logging_dest;
  logging_settings.log_file = settings->log_file.value().c_str();
  if (!logging::InitLogging(logging_settings))
    LOG(ERROR) << "Unable to reinitialize logging to " << settings->log_file;
}
#endif

void NetworkService::CreateNetLogEntriesForActiveObjects(
    net::NetLog::ThreadSafeObserver* observer) {
  std::set<net::URLRequestContext*> contexts;
  for (NetworkContext* nc : network_contexts_)
    contexts.insert(nc->url_request_context());
  return net::CreateNetLogEntriesForActiveObjects(contexts, observer);
}

void NetworkService::SetClient(mojom::NetworkServiceClientPtr client,
                               mojom::NetworkServiceParamsPtr params) {
  client_ = std::move(client);
  Initialize(std::move(params));
}

void NetworkService::StartNetLog(base::File file,
                                 net::NetLogCaptureMode capture_mode,
                                 base::Value client_constants) {
  DCHECK(client_constants.is_dict());
  std::unique_ptr<base::DictionaryValue> constants = net::GetNetConstants();
  constants->MergeDictionary(&client_constants);

  file_net_log_observer_ = net::FileNetLogObserver::CreateUnboundedPreExisting(
      std::move(file), std::move(constants));
  file_net_log_observer_->StartObserving(net_log_, capture_mode);
}

void NetworkService::SetSSLKeyLogFile(const base::FilePath& file) {
  net::SSLClientSocket::SetSSLKeyLogger(
      std::make_unique<net::SSLKeyLoggerImpl>(file));
}

void NetworkService::CreateNetworkContext(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params) {
  // Only the first created NetworkContext can have |primary_next_context| set
  // to true.
  DCHECK(!params->primary_network_context || network_contexts_.empty());

  owned_network_contexts_.emplace(std::make_unique<NetworkContext>(
      this, std::move(request), std::move(params),
      base::BindOnce(&NetworkService::OnNetworkContextConnectionClosed,
                     base::Unretained(this))));
}

void NetworkService::ConfigureStubHostResolver(
    bool stub_resolver_enabled,
    base::Optional<std::vector<mojom::DnsOverHttpsServerPtr>>
        dns_over_https_servers) {
  // If the stub resolver is not enabled, |dns_over_https_servers| has no
  // effect.
  DCHECK(stub_resolver_enabled || !dns_over_https_servers);
  DCHECK(!dns_over_https_servers || !dns_over_https_servers->empty());

  // Enable or disable the stub resolver, as needed. "DnsClient" is class that
  // implements the stub resolver.
  host_resolver_manager_->SetDnsClientEnabled(stub_resolver_enabled);

  // Configure DNS over HTTPS.
  if (!dns_over_https_servers || dns_over_https_servers.value().empty()) {
    host_resolver_manager_->SetDnsConfigOverrides(net::DnsConfigOverrides());
    return;
  }

  net::DnsConfigOverrides overrides;
  overrides.dns_over_https_servers.emplace();
  for (const auto& doh_server : *dns_over_https_servers) {
    overrides.dns_over_https_servers.value().emplace_back(
        doh_server->server_template, doh_server->use_post);
  }
  // TODO(dalyk): Allow the secure dns mode to be set.
  overrides.secure_dns_mode = net::DnsConfig::SecureDnsMode::AUTOMATIC;
  host_resolver_manager_->SetDnsConfigOverrides(overrides);
}

void NetworkService::DisableQuic() {
  quic_disabled_ = true;

  for (auto* network_context : network_contexts_) {
    network_context->DisableQuic();
  }
}

void NetworkService::SetUpHttpAuth(
    mojom::HttpAuthStaticParamsPtr http_auth_static_params) {
  DCHECK(!http_auth_handler_factory_);

  http_auth_handler_factory_ = net::HttpAuthHandlerRegistryFactory::Create(
      &http_auth_preferences_, http_auth_static_params->supported_schemes
#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
      ,
      http_auth_static_params->gssapi_library_name
#endif
#if defined(OS_ANDROID) && BUILDFLAG(USE_KERBEROS)
      ,
      base::BindRepeating(&CreateAuthSystem, this)
#endif
  );
}

void NetworkService::ConfigureHttpAuthPrefs(
    mojom::HttpAuthDynamicParamsPtr http_auth_dynamic_params) {
  http_auth_preferences_.SetServerWhitelist(
      http_auth_dynamic_params->server_whitelist);
  http_auth_preferences_.SetDelegateWhitelist(
      http_auth_dynamic_params->delegate_whitelist);
  http_auth_preferences_.set_delegate_by_kdc_policy(
      http_auth_dynamic_params->delegate_by_kdc_policy);
  http_auth_preferences_.set_negotiate_disable_cname_lookup(
      http_auth_dynamic_params->negotiate_disable_cname_lookup);
  http_auth_preferences_.set_negotiate_enable_port(
      http_auth_dynamic_params->enable_negotiate_port);

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  http_auth_preferences_.set_ntlm_v2_enabled(
      http_auth_dynamic_params->ntlm_v2_enabled);
#endif

#if defined(OS_ANDROID)
  http_auth_preferences_.set_auth_android_negotiate_account_type(
      http_auth_dynamic_params->android_negotiate_account_type);
#endif

#if defined(OS_CHROMEOS)
  http_auth_preferences_.set_allow_gssapi_library_load(
      http_auth_dynamic_params->allow_gssapi_library_load);
#endif
}

void NetworkService::SetRawHeadersAccess(
    uint32_t process_id,
    const std::vector<url::Origin>& origins) {
  DCHECK(process_id);
  if (!origins.size()) {
    raw_headers_access_origins_by_pid_.erase(process_id);
  } else {
    raw_headers_access_origins_by_pid_[process_id] =
        base::flat_set<url::Origin>(origins.begin(), origins.end());
  }
}

void NetworkService::SetMaxConnectionsPerProxy(int32_t max_connections) {
  int new_limit = max_connections;
  if (new_limit < 0)
    new_limit = net::kDefaultMaxSocketsPerProxyServer;

  // Clamp the value between min_limit and max_limit.
  int max_limit = 99;
  int min_limit = net::ClientSocketPoolManager::max_sockets_per_group(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL);
  new_limit = std::max(std::min(new_limit, max_limit), min_limit);

  // Assign the global limit.
  net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL, new_limit);
}

bool NetworkService::HasRawHeadersAccess(uint32_t process_id,
                                         const GURL& resource_url) const {
  // Allow raw headers for browser-initiated requests.
  if (!process_id)
    return true;
  auto it = raw_headers_access_origins_by_pid_.find(process_id);
  if (it == raw_headers_access_origins_by_pid_.end())
    return false;
  return it->second.find(url::Origin::Create(resource_url)) != it->second.end();
}

net::NetLog* NetworkService::net_log() const {
  return net_log_;
}

void NetworkService::GetNetworkChangeManager(
    mojom::NetworkChangeManagerRequest request) {
  network_change_manager_->AddRequest(std::move(request));
}

void NetworkService::GetNetworkQualityEstimatorManager(
    mojom::NetworkQualityEstimatorManagerRequest request) {
  network_quality_estimator_manager_->AddRequest(std::move(request));
}

void NetworkService::GetDnsConfigChangeManager(
    mojom::DnsConfigChangeManagerRequest request) {
  dns_config_change_manager_->AddBinding(std::move(request));
}

void NetworkService::GetTotalNetworkUsages(
    mojom::NetworkService::GetTotalNetworkUsagesCallback callback) {
  std::move(callback).Run(network_usage_accumulator_->GetTotalNetworkUsages());
}

void NetworkService::GetNetworkList(
    uint32_t policy,
    mojom::NetworkService::GetNetworkListCallback callback) {
  auto networks = std::make_unique<net::NetworkInterfaceList>();
  auto* raw_networks = networks.get();
  // net::GetNetworkList may block depending on platform.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&net::GetNetworkList, raw_networks, policy),
      base::BindOnce(&OnGetNetworkList, std::move(networks),
                     std::move(callback)));
}

void NetworkService::UpdateCRLSet(base::span<const uint8_t> crl_set) {
  crl_set_distributor_->OnNewCRLSet(crl_set);
}

void NetworkService::OnCertDBChanged() {
  net::CertDatabase::GetInstance()->NotifyObserversCertDBChanged();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void NetworkService::SetCryptConfig(mojom::CryptConfigPtr crypt_config) {
#if !defined(IS_CHROMECAST)
  DCHECK(!os_crypt_config_set_);
  auto config = std::make_unique<os_crypt::Config>();
  config->store = crypt_config->store;
  config->product_name = crypt_config->product_name;
  config->main_thread_runner = base::ThreadTaskRunnerHandle::Get();
  config->should_use_preference = crypt_config->should_use_preference;
  config->user_data_path = crypt_config->user_data_path;
  OSCrypt::SetConfig(std::move(config));
  os_crypt_config_set_ = true;
#endif
}
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
void NetworkService::SetEncryptionKey(const std::string& encryption_key) {
  OSCrypt::SetRawEncryptionKey(encryption_key);
}
#endif  // OS_MACOSX

void NetworkService::AddCorbExceptionForPlugin(uint32_t process_id) {
  DCHECK_NE(mojom::kBrowserProcessId, process_id);
  CrossOriginReadBlocking::AddExceptionForPlugin(process_id);
}

void NetworkService::RemoveCorbExceptionForPlugin(uint32_t process_id) {
  DCHECK_NE(mojom::kBrowserProcessId, process_id);
  CrossOriginReadBlocking::RemoveExceptionForPlugin(process_id);
}

void NetworkService::AddExtraMimeTypesForCorb(
    const std::vector<std::string>& mime_types) {
  CrossOriginReadBlocking::AddExtraMimeTypesForCorb(mime_types);
}

void NetworkService::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  base::MemoryPressureListener::NotifyMemoryPressure(memory_pressure_level);
}

void NetworkService::OnPeerToPeerConnectionsCountChange(uint32_t count) {
  network_quality_estimator_manager_->GetNetworkQualityEstimator()
      ->OnPeerToPeerConnectionsCountChange(count);
}

#if defined(OS_ANDROID)
void NetworkService::OnApplicationStateChange(
    base::android::ApplicationState state) {
  for (auto* network_context : network_contexts_)
    network_context->app_status_listener()->Notify(state);
}
#endif

void NetworkService::SetEnvironment(
    std::vector<mojom::EnvironmentVariablePtr> environment) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  for (const auto& variable : environment)
    env->SetVar(variable->name, variable->value);
}

#if defined(OS_ANDROID)
void NetworkService::DumpWithoutCrashing(base::Time dump_request_time) {
  static base::debug::CrashKeyString* time_key =
      base::debug::AllocateCrashKeyString("time_since_dump_request_ms",
                                          base::debug::CrashKeySize::Size32);
  base::debug::ScopedCrashKeyString(
      time_key, base::NumberToString(
                    (base::Time::Now() - dump_request_time).InMilliseconds()));
  base::debug::DumpWithoutCrashing();
}
#endif

net::HttpAuthHandlerFactory* NetworkService::GetHttpAuthHandlerFactory() {
  if (!http_auth_handler_factory_) {
    http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault(
        &http_auth_preferences_
#if defined(OS_ANDROID) && BUILDFLAG(USE_KERBEROS)
        ,
        base::BindRepeating(&CreateAuthSystem, this)
#endif
    );
  }
  return http_auth_handler_factory_.get();
}

void NetworkService::OnBeforeURLRequest() {
  if (base::FeatureList::IsEnabled(features::kNetworkService))
    MaybeStartUpdateLoadInfoTimer();
}

void NetworkService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_->BindInterface(interface_name, std::move(interface_pipe));
}

void NetworkService::DestroyNetworkContexts() {
  // If DNS over HTTPS is enabled, the HostResolver is currently using
  // NetworkContexts to do DNS lookups, so need to tell the HostResolver
  // to stop using DNS over HTTPS before destroying any NetworkContexts.
  // The SetDnsConfigOverrides() call will will fail any in-progress DNS
  // lookups, but only if there are current config overrides (which there will
  // be if DNS over HTTPS is currently enabled).
  if (host_resolver_manager_) {
    host_resolver_manager_->SetDnsConfigOverrides(net::DnsConfigOverrides());
  }

  // Delete NetworkContexts. If there's a primary NetworkContext, it must be
  // deleted after all other NetworkContexts, to avoid use-after-frees.
  for (auto it = owned_network_contexts_.begin();
       it != owned_network_contexts_.end();) {
    const auto last = it;
    ++it;
    if (!(*last)->IsPrimaryNetworkContext())
      owned_network_contexts_.erase(last);
  }

  DCHECK_LE(owned_network_contexts_.size(), 1u);
  owned_network_contexts_.clear();
}

void NetworkService::OnNetworkContextConnectionClosed(
    NetworkContext* network_context) {
  if (network_context->IsPrimaryNetworkContext()) {
    DestroyNetworkContexts();
    return;
  }

  auto it = owned_network_contexts_.find(network_context);
  DCHECK(it != owned_network_contexts_.end());
  owned_network_contexts_.erase(it);
}

void NetworkService::MaybeStartUpdateLoadInfoTimer() {
  if (waiting_on_load_state_ack_ || update_load_info_timer_.IsRunning())
    return;

  bool has_loader = false;
  for (auto* network_context : network_contexts_) {
    if (!network_context->url_request_context()->url_requests()->empty()) {
      has_loader = true;
      break;
    }
  }

  if (!has_loader)
    return;

  update_load_info_timer_.Start(FROM_HERE, kUpdateLoadStatesInterval, this,
                                &NetworkService::UpdateLoadInfo);
}

void NetworkService::UpdateLoadInfo() {
  // For requests from the same {process_id, routing_id} pair, pick the most
  // important. For ones from the browser, return all of them.
  std::vector<mojom::LoadInfoPtr> infos;
  std::map<std::pair<uint32_t, uint32_t>, mojom::LoadInfoPtr> frame_infos;

  for (auto* network_context : network_contexts_) {
    for (auto* loader :
         *network_context->url_request_context()->url_requests()) {
      auto* url_loader = URLLoader::ForRequest(*loader);
      if (!url_loader)
        continue;

      auto process_id = url_loader->GetProcessId();
      auto routing_id = url_loader->GetRenderFrameId();
      if (routing_id == static_cast<uint32_t>(MSG_ROUTING_NONE)) {
        // If there is no routing_id, then the browser can't associate this with
        // a page so no need to send.
        continue;
      }

      auto load_info = mojom::LoadInfo::New();
      load_info->process_id = process_id;
      load_info->routing_id = routing_id;
      load_info->host = loader->url().host();
      auto load_state = loader->GetLoadState();
      load_info->load_state = static_cast<uint32_t>(load_state.state);
      load_info->state_param = std::move(load_state.param);
      auto upload_progress = loader->GetUploadProgress();
      load_info->upload_size = upload_progress.size();
      load_info->upload_position = upload_progress.position();

      if (process_id == 0) {
        // Requests from the browser can't be compared to ones from child
        // processes, so send them all without looking for the most interesting.
        infos.push_back(std::move(load_info));
        continue;
      }

      auto key = std::make_pair(process_id, routing_id);
      auto existing = frame_infos.find(key);
      if (existing == frame_infos.end() ||
          LoadInfoIsMoreInteresting(*load_info, *existing->second)) {
        frame_infos[key] = std::move(load_info);
      }
    }
  }

  for (auto& it : frame_infos)
    infos.push_back(std::move(it.second));

  if (infos.empty())
    return;

  DCHECK(!waiting_on_load_state_ack_);
  waiting_on_load_state_ack_ = true;
  client_->OnLoadingStateUpdate(
      std::move(infos), base::BindOnce(&NetworkService::AckUpdateLoadInfo,
                                       base::Unretained(this)));
}

void NetworkService::AckUpdateLoadInfo() {
  DCHECK(waiting_on_load_state_ack_);
  waiting_on_load_state_ack_ = false;
  MaybeStartUpdateLoadInfoTimer();
}

void NetworkService::Bind(mojom::NetworkServiceRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

// static
NetworkService* NetworkService::GetNetworkServiceForTesting() {
  return g_network_service;
}

}  // namespace network
