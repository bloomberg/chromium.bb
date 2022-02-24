// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/url_request_context_config.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/cronet/stale_host_resolver.h"
#include "net/base/address_family.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/log/net_log.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/reporting/reporting_policy.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_key_logger_impl.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_tag.h"
#include "net/url_request/url_request_context_builder.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/reporting/reporting_policy.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace cronet {

namespace {

// Name of disk cache directory.
const base::FilePath::CharType kDiskCacheDirectoryName[] =
    FILE_PATH_LITERAL("disk_cache");
const char kQuicFieldTrialName[] = "QUIC";
const char kQuicConnectionOptions[] = "connection_options";
const char kQuicClientConnectionOptions[] = "client_connection_options";
const char kQuicStoreServerConfigsInProperties[] =
    "store_server_configs_in_properties";
const char kQuicMaxServerConfigsStoredInProperties[] =
    "max_server_configs_stored_in_properties";
const char kQuicIdleConnectionTimeoutSeconds[] =
    "idle_connection_timeout_seconds";
const char kQuicMaxTimeBeforeCryptoHandshakeSeconds[] =
    "max_time_before_crypto_handshake_seconds";
const char kQuicMaxIdleTimeBeforeCryptoHandshakeSeconds[] =
    "max_idle_time_before_crypto_handshake_seconds";
const char kQuicCloseSessionsOnIpChange[] = "close_sessions_on_ip_change";
const char kQuicGoAwaySessionsOnIpChange[] = "goaway_sessions_on_ip_change";
const char kQuicAllowServerMigration[] = "allow_server_migration";
const char kQuicMigrateSessionsOnNetworkChangeV2[] =
    "migrate_sessions_on_network_change_v2";
const char kQuicMigrateIdleSessions[] = "migrate_idle_sessions";
const char kQuicRetransmittableOnWireTimeoutMilliseconds[] =
    "retransmittable_on_wire_timeout_milliseconds";
const char kQuicIdleSessionMigrationPeriodSeconds[] =
    "idle_session_migration_period_seconds";
const char kQuicMaxTimeOnNonDefaultNetworkSeconds[] =
    "max_time_on_non_default_network_seconds";
const char kQuicMaxMigrationsToNonDefaultNetworkOnWriteError[] =
    "max_migrations_to_non_default_network_on_write_error";
const char kQuicMaxMigrationsToNonDefaultNetworkOnPathDegrading[] =
    "max_migrations_to_non_default_network_on_path_degrading";
const char kQuicUserAgentId[] = "user_agent_id";
const char kQuicMigrateSessionsEarlyV2[] = "migrate_sessions_early_v2";
const char kQuicRetryOnAlternateNetworkBeforeHandshake[] =
    "retry_on_alternate_network_before_handshake";
const char kQuicRaceStaleDNSOnConnection[] = "race_stale_dns_on_connection";
const char kQuicDisableBidirectionalStreams[] =
    "quic_disable_bidirectional_streams";
const char kQuicHostWhitelist[] = "host_whitelist";
const char kQuicEnableSocketRecvOptimization[] =
    "enable_socket_recv_optimization";
const char kQuicVersion[] = "quic_version";
const char kQuicObsoleteVersionsAllowed[] = "obsolete_versions_allowed";
const char kQuicFlags[] = "set_quic_flags";
const char kQuicIOSNetworkServiceType[] = "ios_network_service_type";
const char kRetryWithoutAltSvcOnQuicErrors[] =
    "retry_without_alt_svc_on_quic_errors";

// AsyncDNS experiment dictionary name.
const char kAsyncDnsFieldTrialName[] = "AsyncDNS";
// Name of boolean to enable AsyncDNS experiment.
const char kAsyncDnsEnable[] = "enable";

// Stale DNS (StaleHostResolver) experiment dictionary name.
const char kStaleDnsFieldTrialName[] = "StaleDNS";
// Name of boolean to enable stale DNS experiment.
const char kStaleDnsEnable[] = "enable";
// Name of integer delay in milliseconds before a stale DNS result will be
// used.
const char kStaleDnsDelayMs[] = "delay_ms";
// Name of integer maximum age (past expiration) in milliseconds of a stale DNS
// result that will be used, or 0 for no limit.
const char kStaleDnsMaxExpiredTimeMs[] = "max_expired_time_ms";
// Name of integer maximum times each stale DNS result can be used, or 0 for no
// limit.
const char kStaleDnsMaxStaleUses[] = "max_stale_uses";
// Name of boolean to allow stale DNS results from other networks to be used on
// the current network.
const char kStaleDnsAllowOtherNetwork[] = "allow_other_network";
// Name of boolean to enable persisting the DNS cache to disk.
const char kStaleDnsPersist[] = "persist_to_disk";
// Name of integer minimum time in milliseconds between writes to disk for DNS
// cache persistence. Default value is one minute. Only relevant if
// "persist_to_disk" is true.
const char kStaleDnsPersistTimer[] = "persist_delay_ms";
// Name of boolean to allow use of stale DNS results when network resolver
// returns ERR_NAME_NOT_RESOLVED.
const char kStaleDnsUseStaleOnNameNotResolved[] =
    "use_stale_on_name_not_resolved";

// Rules to override DNS resolution. Intended for testing.
// See explanation of format in net/dns/mapped_host_resolver.h.
const char kHostResolverRulesFieldTrialName[] = "HostResolverRules";
const char kHostResolverRules[] = "host_resolver_rules";

// NetworkQualityEstimator (NQE) experiment dictionary name.
const char kNetworkQualityEstimatorFieldTrialName[] = "NetworkQualityEstimator";

// Network Error Logging experiment dictionary name.
const char kNetworkErrorLoggingFieldTrialName[] = "NetworkErrorLogging";
// Name of boolean to enable Reporting API.
const char kNetworkErrorLoggingEnable[] = "enable";
// Name of list of preloaded "Report-To" headers.
const char kNetworkErrorLoggingPreloadedReportToHeaders[] =
    "preloaded_report_to_headers";
// Name of list of preloaded "NEL" headers.
const char kNetworkErrorLoggingPreloadedNELHeaders[] = "preloaded_nel_headers";
// Name of key (for above two lists) for header origin.
const char kNetworkErrorLoggingOrigin[] = "origin";
// Name of key (for above two lists) for header value.
const char kNetworkErrorLoggingValue[] = "value";

// Disable IPv6 when on WiFi. This is a workaround for a known issue on certain
// Android phones, and should not be necessary when not on one of those devices.
// See https://crbug.com/696569 for details.
const char kDisableIPv6OnWifi[] = "disable_ipv6_on_wifi";

const char kSSLKeyLogFile[] = "ssl_key_log_file";

const char kGoAwayOnPathDegrading[] = "go_away_on_path_degrading";

const char kAllowPortMigration[] = "allow_port_migration";

const char kDisableTlsZeroRtt[] = "disable_tls_zero_rtt";

// Whether SPDY sessions should be closed or marked as going away upon relevant
// network changes. When not specified, /net behavior varies depending on the
// underlying OS.
const char kSpdyGoAwayOnIpChange[] = "spdy_go_away_on_ip_change";

// Whether the connection status of all bidirectional streams (created through
// the Cronet engine) should be monitored.
// The value must be an integer (> 0) and will be interpreted as a suggestion
// for the period of the heartbeat signal. See
// SpdySession#EnableBrokenConnectionDetection for more info.
const char kBidiStreamDetectBrokenConnection[] =
    "bidi_stream_detect_broken_connection";

// "goaway_sessions_on_ip_change" is default on for iOS unless overridden via
// experimental options explicitly.
#if BUILDFLAG(IS_IOS)
const bool kDefaultQuicGoAwaySessionsOnIpChange = true;
#else
const bool kDefaultQuicGoAwaySessionsOnIpChange = false;
#endif

// Serializes a base::Value into a string that can be used as the value of
// JFV-encoded HTTP header [1].  If |value| is a list, we remove the outermost
// [] delimiters from the result.
//
// [1] https://tools.ietf.org/html/draft-reschke-http-jfv
std::string SerializeJFVHeader(const base::Value& value) {
  std::string result;
  if (!base::JSONWriter::Write(value, &result))
    return std::string();
  if (value.is_list()) {
    DCHECK(result.size() >= 2);
    return result.substr(1, result.size() - 2);
  }
  return result;
}

std::vector<URLRequestContextConfig::PreloadedNelAndReportingHeader>
ParseNetworkErrorLoggingHeaders(
    base::Value::ConstListView preloaded_headers_config) {
  std::vector<URLRequestContextConfig::PreloadedNelAndReportingHeader> result;
  for (const auto& preloaded_header_config : preloaded_headers_config) {
    if (!preloaded_header_config.is_dict())
      continue;

    auto* origin_config = preloaded_header_config.FindKeyOfType(
        kNetworkErrorLoggingOrigin, base::Value::Type::STRING);
    if (!origin_config)
      continue;
    GURL origin_url(origin_config->GetString());
    if (!origin_url.is_valid())
      continue;
    auto origin = url::Origin::Create(origin_url);

    auto* value = preloaded_header_config.FindKey(kNetworkErrorLoggingValue);
    if (!value)
      continue;

    result.push_back(URLRequestContextConfig::PreloadedNelAndReportingHeader(
        origin, SerializeJFVHeader(*value)));
  }
  return result;
}

// Applies |f| to the value contained by |maybe|, returns empty optional
// otherwise.
template <typename T, typename F>
auto map(absl::optional<T> maybe, F&& f) {
  if (!maybe)
    return absl::optional<base::invoke_result_t<F, T>>();
  return absl::optional<base::invoke_result_t<F, T>>(f(maybe.value()));
}

}  // namespace

URLRequestContextConfig::QuicHint::QuicHint(const std::string& host,
                                            int port,
                                            int alternate_port)
    : host(host), port(port), alternate_port(alternate_port) {}

URLRequestContextConfig::QuicHint::~QuicHint() {}

URLRequestContextConfig::Pkp::Pkp(const std::string& host,
                                  bool include_subdomains,
                                  const base::Time& expiration_date)
    : host(host),
      include_subdomains(include_subdomains),
      expiration_date(expiration_date) {}

URLRequestContextConfig::Pkp::~Pkp() {}

URLRequestContextConfig::PreloadedNelAndReportingHeader::
    PreloadedNelAndReportingHeader(const url::Origin& origin, std::string value)
    : origin(origin), value(std::move(value)) {}

URLRequestContextConfig::PreloadedNelAndReportingHeader::
    ~PreloadedNelAndReportingHeader() = default;

URLRequestContextConfig::URLRequestContextConfig(
    bool enable_quic,
    const std::string& quic_user_agent_id,
    bool enable_spdy,
    bool enable_brotli,
    HttpCacheType http_cache,
    int http_cache_max_size,
    bool load_disable_cache,
    const std::string& storage_path,
    const std::string& accept_language,
    const std::string& user_agent,
    base::Value::DictStorage experimental_options,
    std::unique_ptr<net::CertVerifier> mock_cert_verifier,
    bool enable_network_quality_estimator,
    bool bypass_public_key_pinning_for_local_trust_anchors,
    absl::optional<double> network_thread_priority)
    : enable_quic(enable_quic),
      quic_user_agent_id(quic_user_agent_id),
      enable_spdy(enable_spdy),
      enable_brotli(enable_brotli),
      http_cache(http_cache),
      http_cache_max_size(http_cache_max_size),
      load_disable_cache(load_disable_cache),
      storage_path(storage_path),
      accept_language(accept_language),
      user_agent(user_agent),
      mock_cert_verifier(std::move(mock_cert_verifier)),
      enable_network_quality_estimator(enable_network_quality_estimator),
      bypass_public_key_pinning_for_local_trust_anchors(
          bypass_public_key_pinning_for_local_trust_anchors),
      effective_experimental_options(
          base::Value(experimental_options).TakeDictDeprecated()),
      experimental_options(std::move(experimental_options)),
      network_thread_priority(network_thread_priority),
      bidi_stream_detect_broken_connection(false),
      heartbeat_interval(base::Seconds(0)) {
  SetContextConfigExperimentalOptions();
}

URLRequestContextConfig::~URLRequestContextConfig() {}

// static
std::unique_ptr<URLRequestContextConfig>
URLRequestContextConfig::CreateURLRequestContextConfig(
    bool enable_quic,
    const std::string& quic_user_agent_id,
    bool enable_spdy,
    bool enable_brotli,
    HttpCacheType http_cache,
    int http_cache_max_size,
    bool load_disable_cache,
    const std::string& storage_path,
    const std::string& accept_language,
    const std::string& user_agent,
    const std::string& unparsed_experimental_options,
    std::unique_ptr<net::CertVerifier> mock_cert_verifier,
    bool enable_network_quality_estimator,
    bool bypass_public_key_pinning_for_local_trust_anchors,
    absl::optional<double> network_thread_priority) {
  absl::optional<base::Value::DictStorage> experimental_options =
      ParseExperimentalOptions(unparsed_experimental_options);
  if (!experimental_options) {
    // For the time being maintain backward compatibility by only failing to
    // parse when DCHECKs are enabled.
    if (ExperimentalOptionsParsingIsAllowedToFail())
      return nullptr;
    else
      experimental_options = base::Value::DictStorage();
  }
  return base::WrapUnique(new URLRequestContextConfig(
      enable_quic, quic_user_agent_id, enable_spdy, enable_brotli, http_cache,
      http_cache_max_size, load_disable_cache, storage_path, accept_language,
      user_agent, std::move(experimental_options.value()),
      std::move(mock_cert_verifier), enable_network_quality_estimator,
      bypass_public_key_pinning_for_local_trust_anchors,
      network_thread_priority));
}

// static
absl::optional<base::Value::DictStorage>
URLRequestContextConfig::ParseExperimentalOptions(
    std::string unparsed_experimental_options) {
  // From a user perspective no experimental options means an empty string. The
  // underlying code instead expects and empty dictionary. Normalize this.
  if (unparsed_experimental_options.empty())
    unparsed_experimental_options = "{}";
  DVLOG(1) << "Experimental Options:" << unparsed_experimental_options;
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(
          unparsed_experimental_options);
  if (!parsed_json.value) {
    LOG(ERROR) << "Parsing experimental options failed: '"
               << unparsed_experimental_options << "', error "
               << parsed_json.error_message;
    return absl::nullopt;
  }

  base::Value experimental_options_value = std::move(parsed_json.value.value());
  if (!experimental_options_value.is_dict()) {
    LOG(ERROR) << "Experimental options string is not a dictionary: "
               << experimental_options_value;
    return absl::nullopt;
  }

  return std::move(experimental_options_value).TakeDictDeprecated();
}

void URLRequestContextConfig::SetContextConfigExperimentalOptions() {
  auto iter = experimental_options.find(kBidiStreamDetectBrokenConnection);
  if (iter == experimental_options.end())
    return;

  const base::Value& heartbeat_interval_value = iter->second;
  if (!heartbeat_interval_value.is_int()) {
    LOG(ERROR) << "\"" << kBidiStreamDetectBrokenConnection
               << "\" config params \"" << heartbeat_interval_value
               << "\" is not an int";
    experimental_options.erase(iter);
    effective_experimental_options.erase(kBidiStreamDetectBrokenConnection);
    return;
  }

  int heartbeat_interval_secs = heartbeat_interval_value.GetInt();
  heartbeat_interval = base::Seconds(heartbeat_interval_secs);
  bidi_stream_detect_broken_connection = heartbeat_interval_secs > 0;
  experimental_options.erase(iter);
}

void URLRequestContextConfig::SetContextBuilderExperimentalOptions(
    net::URLRequestContextBuilder* context_builder,
    net::HttpNetworkSessionParams* session_params,
    net::QuicParams* quic_params) {
  if (experimental_options.empty())
    return;

  bool async_dns_enable = false;
  bool stale_dns_enable = false;
  bool host_resolver_rules_enable = false;
  bool disable_ipv6_on_wifi = false;
  bool nel_enable = false;

  StaleHostResolver::StaleOptions stale_dns_options;
  const std::string* host_resolver_rules_string;

  for (const auto& iter : experimental_options) {
    if (iter.first == kQuicFieldTrialName) {
      if (!iter.second.is_dict()) {
        LOG(ERROR) << "Quic config params \"" << iter.second
                   << "\" is not a dictionary value";
        effective_experimental_options.erase(iter.first);
        continue;
      }

      const base::Value& quic_args = iter.second;
      const std::string* quic_version_string =
          quic_args.FindStringKey(kQuicVersion);
      if (quic_version_string) {
        quic::ParsedQuicVersionVector supported_versions =
            quic::ParseQuicVersionVectorString(*quic_version_string);
        if (!quic_args.FindBoolKey(kQuicObsoleteVersionsAllowed)
                 .value_or(false)) {
          quic::ParsedQuicVersionVector filtered_versions;
          quic::ParsedQuicVersionVector obsolete_versions =
              net::ObsoleteQuicVersions();
          for (const quic::ParsedQuicVersion& version : supported_versions) {
            if (version == quic::ParsedQuicVersion::Q043()) {
              // TODO(dschinazi) Remove this special-casing of Q043 once we no
              // longer have cronet applications that require it.
              filtered_versions.push_back(version);
              continue;
            }
            if (std::find(obsolete_versions.begin(), obsolete_versions.end(),
                          version) == obsolete_versions.end()) {
              filtered_versions.push_back(version);
            }
          }
          supported_versions = filtered_versions;
        }
        if (!supported_versions.empty())
          quic_params->supported_versions = supported_versions;
      }

      const std::string* quic_connection_options =
          quic_args.FindStringKey(kQuicConnectionOptions);
      if (quic_connection_options) {
        quic_params->connection_options =
            quic::ParseQuicTagVector(*quic_connection_options);
      }

      const std::string* quic_client_connection_options =
          quic_args.FindStringKey(kQuicClientConnectionOptions);
      if (quic_client_connection_options) {
        quic_params->client_connection_options =
            quic::ParseQuicTagVector(*quic_client_connection_options);
      }

      // TODO(rtenneti): Delete this option after apps stop using it.
      // Added this for backward compatibility.
      if (quic_args.FindBoolKey(kQuicStoreServerConfigsInProperties)
              .value_or(false)) {
        quic_params->max_server_configs_stored_in_properties =
            net::kDefaultMaxQuicServerEntries;
      }

      quic_params->max_server_configs_stored_in_properties =
          static_cast<size_t>(
              quic_args.FindIntKey(kQuicMaxServerConfigsStoredInProperties)
                  .value_or(
                      quic_params->max_server_configs_stored_in_properties));

      quic_params->idle_connection_timeout =
          map(quic_args.FindIntKey(kQuicIdleConnectionTimeoutSeconds),
              base::Seconds<int>)
              .value_or(quic_params->idle_connection_timeout);

      quic_params->max_time_before_crypto_handshake =
          map(quic_args.FindIntKey(kQuicMaxTimeBeforeCryptoHandshakeSeconds),
              base::Seconds<int>)
              .value_or(quic_params->max_time_before_crypto_handshake);

      quic_params->max_idle_time_before_crypto_handshake =
          map(quic_args.FindIntKey(
                  kQuicMaxIdleTimeBeforeCryptoHandshakeSeconds),
              base::Seconds<int>)
              .value_or(quic_params->max_idle_time_before_crypto_handshake);

      quic_params->close_sessions_on_ip_change =
          quic_args.FindBoolKey(kQuicCloseSessionsOnIpChange)
              .value_or(quic_params->close_sessions_on_ip_change);
      if (quic_params->close_sessions_on_ip_change &&
          kDefaultQuicGoAwaySessionsOnIpChange) {
        // "close_sessions_on_ip_change" and "goaway_sessions_on_ip_change"
        // are mutually exclusive. Turn off the goaway option which is
        // default on for iOS if "close_sessions_on_ip_change" is set via
        // experimental options.
        quic_params->goaway_sessions_on_ip_change = false;
      }

      quic_params->goaway_sessions_on_ip_change =
          quic_args.FindBoolKey(kQuicGoAwaySessionsOnIpChange)
              .value_or(quic_params->goaway_sessions_on_ip_change);
      quic_params->go_away_on_path_degrading =
          quic_args.FindBoolKey(kGoAwayOnPathDegrading)
              .value_or(quic_params->go_away_on_path_degrading);
      quic_params->allow_server_migration =
          quic_args.FindBoolKey(kQuicAllowServerMigration)
              .value_or(quic_params->allow_server_migration);

      const std::string* user_agent_id =
          quic_args.FindStringKey(kQuicUserAgentId);
      if (user_agent_id) {
        quic_params->user_agent_id = *user_agent_id;
      }

      quic_params->enable_socket_recv_optimization =
          quic_args.FindBoolKey(kQuicEnableSocketRecvOptimization)
              .value_or(quic_params->enable_socket_recv_optimization);

      absl::optional<bool> quic_migrate_sessions_on_network_change_v2_in =
          quic_args.FindBoolKey(kQuicMigrateSessionsOnNetworkChangeV2);
      if (quic_migrate_sessions_on_network_change_v2_in.has_value()) {
        quic_params->migrate_sessions_on_network_change_v2 =
            quic_migrate_sessions_on_network_change_v2_in.value();
        quic_params->max_time_on_non_default_network =
            map(quic_args.FindIntKey(kQuicMaxTimeOnNonDefaultNetworkSeconds),
                base::Seconds<int>)
                .value_or(quic_params->max_time_on_non_default_network);
        quic_params->max_migrations_to_non_default_network_on_write_error =
            quic_args
                .FindIntKey(kQuicMaxMigrationsToNonDefaultNetworkOnWriteError)
                .value_or(
                    quic_params
                        ->max_migrations_to_non_default_network_on_write_error);
        quic_params->max_migrations_to_non_default_network_on_path_degrading =
            quic_args
                .FindIntKey(
                    kQuicMaxMigrationsToNonDefaultNetworkOnPathDegrading)
                .value_or(
                    quic_params
                        ->max_migrations_to_non_default_network_on_path_degrading);
      }

      absl::optional<bool> quic_migrate_idle_sessions_in =
          quic_args.FindBoolKey(kQuicMigrateIdleSessions);
      if (quic_migrate_idle_sessions_in.has_value()) {
        quic_params->migrate_idle_sessions =
            quic_migrate_idle_sessions_in.value();
        quic_params->idle_session_migration_period =
            map(quic_args.FindIntKey(kQuicIdleSessionMigrationPeriodSeconds),
                base::Seconds<int>)
                .value_or(quic_params->idle_session_migration_period);
      }

      quic_params->migrate_sessions_early_v2 =
          quic_args.FindBoolKey(kQuicMigrateSessionsEarlyV2)
              .value_or(quic_params->migrate_sessions_early_v2);

      quic_params->retransmittable_on_wire_timeout =
          map(quic_args.FindIntKey(
                  kQuicRetransmittableOnWireTimeoutMilliseconds),
              base::Milliseconds<int>)
              .value_or(quic_params->retransmittable_on_wire_timeout);

      quic_params->retry_on_alternate_network_before_handshake =
          quic_args.FindBoolKey(kQuicRetryOnAlternateNetworkBeforeHandshake)
              .value_or(
                  quic_params->retry_on_alternate_network_before_handshake);

      quic_params->race_stale_dns_on_connection =
          quic_args.FindBoolKey(kQuicRaceStaleDNSOnConnection)
              .value_or(quic_params->race_stale_dns_on_connection);

      quic_params->allow_port_migration =
          quic_args.FindBoolKey(kAllowPortMigration)
              .value_or(quic_params->allow_port_migration);

      quic_params->retry_without_alt_svc_on_quic_errors =
          quic_args.FindBoolKey(kRetryWithoutAltSvcOnQuicErrors)
              .value_or(quic_params->retry_without_alt_svc_on_quic_errors);

      quic_params->disable_tls_zero_rtt =
          quic_args.FindBoolKey(kDisableTlsZeroRtt)
              .value_or(quic_params->disable_tls_zero_rtt);

      quic_params->disable_bidirectional_streams =
          quic_args.FindBoolKey(kQuicDisableBidirectionalStreams)
              .value_or(quic_params->disable_bidirectional_streams);

      const std::string* quic_host_allowlist =
          quic_args.FindStringKey(kQuicHostWhitelist);
      if (quic_host_allowlist) {
        std::vector<std::string> host_vector =
            base::SplitString(*quic_host_allowlist, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL);
        session_params->quic_host_allowlist.clear();
        for (const std::string& host : host_vector) {
          session_params->quic_host_allowlist.insert(host);
        }
      }

      const std::string* quic_flags = quic_args.FindStringKey(kQuicFlags);
      if (quic_flags) {
        for (const auto& flag :
             base::SplitString(*quic_flags, ",", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_ALL)) {
          std::vector<std::string> tokens = base::SplitString(
              flag, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
          if (tokens.size() != 2)
            continue;
          SetQuicFlagByName(tokens[0], tokens[1]);
        }
      }

      quic_params->ios_network_service_type =
          quic_args.FindIntKey(kQuicIOSNetworkServiceType)
              .value_or(quic_params->ios_network_service_type);
    } else if (iter.first == kAsyncDnsFieldTrialName) {
      if (!iter.second.is_dict()) {
        LOG(ERROR) << "\"" << iter.first << "\" config params \"" << iter.second
                   << "\" is not a dictionary value";
        effective_experimental_options.erase(iter.first);
        continue;
      }
      const base::Value& async_dns_args = iter.second;
      async_dns_enable = async_dns_args.FindBoolKey(kAsyncDnsEnable)
                             .value_or(async_dns_enable);
    } else if (iter.first == kStaleDnsFieldTrialName) {
      if (!iter.second.is_dict()) {
        LOG(ERROR) << "\"" << iter.first << "\" config params \"" << iter.second
                   << "\" is not a dictionary value";
        effective_experimental_options.erase(iter.first);
        continue;
      }
      const base::Value& stale_dns_args = iter.second;
      stale_dns_enable =
          stale_dns_args.FindBoolKey(kStaleDnsEnable).value_or(false);

      if (stale_dns_enable) {
        stale_dns_options.delay =
            map(stale_dns_args.FindIntKey(kStaleDnsDelayMs),
                base::Milliseconds<int>)
                .value_or(stale_dns_options.delay);
        stale_dns_options.max_expired_time =
            map(stale_dns_args.FindIntKey(kStaleDnsMaxExpiredTimeMs),
                base::Milliseconds<int>)
                .value_or(stale_dns_options.max_expired_time);
        stale_dns_options.max_stale_uses =
            stale_dns_args.FindIntKey(kStaleDnsMaxStaleUses)
                .value_or(stale_dns_options.max_stale_uses);
        stale_dns_options.allow_other_network =
            stale_dns_args.FindBoolKey(kStaleDnsAllowOtherNetwork)
                .value_or(stale_dns_options.allow_other_network);
        enable_host_cache_persistence =
            stale_dns_args.FindBoolKey(kStaleDnsPersist)
                .value_or(enable_host_cache_persistence);
        host_cache_persistence_delay_ms =
            stale_dns_args.FindIntKey(kStaleDnsPersistTimer)
                .value_or(host_cache_persistence_delay_ms);
        stale_dns_options.use_stale_on_name_not_resolved =
            stale_dns_args.FindBoolKey(kStaleDnsUseStaleOnNameNotResolved)
                .value_or(stale_dns_options.use_stale_on_name_not_resolved);
      }
    } else if (iter.first == kHostResolverRulesFieldTrialName) {
      if (!iter.second.is_dict()) {
        LOG(ERROR) << "\"" << iter.first << "\" config params \"" << iter.second
                   << "\" is not a dictionary value";
        effective_experimental_options.erase(iter.first);
        continue;
      }
      const base::Value& host_resolver_rules_args = iter.second;
      host_resolver_rules_string =
          host_resolver_rules_args.FindStringKey(kHostResolverRules);
      host_resolver_rules_enable = !!host_resolver_rules_string;
    } else if (iter.first == kNetworkErrorLoggingFieldTrialName) {
      if (!iter.second.is_dict()) {
        LOG(ERROR) << "\"" << iter.first << "\" config params \"" << iter.second
                   << "\" is not a dictionary value";
        effective_experimental_options.erase(iter.first);
        continue;
      }
      const base::Value& nel_args = iter.second;
      nel_enable =
          nel_args.FindBoolKey(kNetworkErrorLoggingEnable).value_or(nel_enable);

      const auto* preloaded_report_to_headers_config =
          nel_args.FindListKey(kNetworkErrorLoggingPreloadedReportToHeaders);
      if (preloaded_report_to_headers_config) {
        preloaded_report_to_headers = ParseNetworkErrorLoggingHeaders(
            preloaded_report_to_headers_config->GetListDeprecated());
      }

      const auto* preloaded_nel_headers_config =
          nel_args.FindListKey(kNetworkErrorLoggingPreloadedNELHeaders);
      if (preloaded_nel_headers_config) {
        preloaded_nel_headers = ParseNetworkErrorLoggingHeaders(
            preloaded_nel_headers_config->GetListDeprecated());
      }
    } else if (iter.first == kDisableIPv6OnWifi) {
      if (!iter.second.is_bool()) {
        LOG(ERROR) << "\"" << iter.first << "\" config params \"" << iter.second
                   << "\" is not a bool";
        effective_experimental_options.erase(iter.first);
        continue;
      }
      disable_ipv6_on_wifi = iter.second.GetBool();
    } else if (iter.first == kSSLKeyLogFile) {
      if (iter.second.is_string()) {
        base::FilePath ssl_key_log_file(
            base::FilePath::FromUTF8Unsafe(iter.second.GetString()));
        if (!ssl_key_log_file.empty()) {
          // SetSSLKeyLogger is only safe to call before any SSLClientSockets
          // are created. This should not be used if there are multiple
          // CronetEngine.
          // TODO(xunjieli): Expose this as a stable API after crbug.com/458365
          // is resolved.
          net::SSLClientSocket::SetSSLKeyLogger(
              std::make_unique<net::SSLKeyLoggerImpl>(ssl_key_log_file));
        }
      }
    } else if (iter.first == kNetworkQualityEstimatorFieldTrialName) {
      if (!iter.second.is_dict()) {
        LOG(ERROR) << "\"" << iter.first << "\" config params \"" << iter.second
                   << "\" is not a dictionary value";
        effective_experimental_options.erase(iter.first);
        continue;
      }

      const base::Value& nqe_args = iter.second;
      const std::string* nqe_option =
          nqe_args.FindStringKey(net::kForceEffectiveConnectionType);
      if (nqe_option) {
        nqe_forced_effective_connection_type =
            net::GetEffectiveConnectionTypeForName(*nqe_option);
        if (!nqe_option->empty() && !nqe_forced_effective_connection_type) {
          LOG(ERROR) << "\"" << nqe_option
                     << "\" is not a valid effective connection type value";
        }
      }
    } else if (iter.first == kSpdyGoAwayOnIpChange) {
      if (!iter.second.is_bool()) {
        LOG(ERROR) << "\"" << iter.first << "\" config params \"" << iter.second
                   << "\" is not a bool";
        effective_experimental_options.erase(iter.first);
        continue;
      }
      session_params->spdy_go_away_on_ip_change = iter.second.GetBool();
    } else {
      LOG(WARNING) << "Unrecognized Cronet experimental option \"" << iter.first
                   << "\" with params \"" << iter.second;
      effective_experimental_options.erase(iter.first);
    }
  }

  if (async_dns_enable || stale_dns_enable || host_resolver_rules_enable ||
      disable_ipv6_on_wifi) {
    std::unique_ptr<net::HostResolver> host_resolver;
    net::HostResolver::ManagerOptions host_resolver_manager_options;
    host_resolver_manager_options.insecure_dns_client_enabled =
        async_dns_enable;
    host_resolver_manager_options.check_ipv6_on_wifi = !disable_ipv6_on_wifi;
    // TODO(crbug.com/934402): Consider using a shared HostResolverManager for
    // Cronet HostResolvers.
    if (stale_dns_enable) {
      DCHECK(!disable_ipv6_on_wifi);
      host_resolver = std::make_unique<StaleHostResolver>(
          net::HostResolver::CreateStandaloneContextResolver(
              net::NetLog::Get(), std::move(host_resolver_manager_options)),
          stale_dns_options);
    } else {
      host_resolver = net::HostResolver::CreateStandaloneResolver(
          net::NetLog::Get(), std::move(host_resolver_manager_options));
    }
    if (host_resolver_rules_enable) {
      std::unique_ptr<net::MappedHostResolver> remapped_resolver(
          new net::MappedHostResolver(std::move(host_resolver)));
      remapped_resolver->SetRulesFromString(*host_resolver_rules_string);
      host_resolver = std::move(remapped_resolver);
    }
    context_builder->set_host_resolver(std::move(host_resolver));
  }

#if BUILDFLAG(ENABLE_REPORTING)
  if (nel_enable) {
    auto policy = net::ReportingPolicy::Create();

    // Apps (like Cronet embedders) are generally allowed to run in the
    // background, even across network changes, so use more relaxed privacy
    // settings than when Reporting is running in the browser.
    policy->persist_reports_across_restarts = true;
    policy->persist_clients_across_restarts = true;
    policy->persist_reports_across_network_changes = true;
    policy->persist_clients_across_network_changes = true;

    context_builder->set_reporting_policy(std::move(policy));
    context_builder->set_network_error_logging_enabled(true);
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)
}

void URLRequestContextConfig::ConfigureURLRequestContextBuilder(
    net::URLRequestContextBuilder* context_builder) {
  std::string config_cache;
  if (http_cache != DISABLED) {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    if (http_cache == DISK && !storage_path.empty()) {
      cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
      cache_params.path = base::FilePath::FromUTF8Unsafe(storage_path)
                              .Append(kDiskCacheDirectoryName);
    } else {
      cache_params.type =
          net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
    }
    cache_params.max_size = http_cache_max_size;
    context_builder->EnableHttpCache(cache_params);
  } else {
    context_builder->DisableHttpCache();
  }
  context_builder->set_accept_language(accept_language);
  context_builder->set_user_agent(user_agent);
  net::HttpNetworkSessionParams session_params;
  session_params.enable_http2 = enable_spdy;
  session_params.enable_quic = enable_quic;
  auto quic_context = std::make_unique<net::QuicContext>();
  if (enable_quic) {
    quic_context->params()->user_agent_id = quic_user_agent_id;
    // Note goaway sessions on ip change will be turned on by default
    // for iOS unless overrided via experiemental options.
    quic_context->params()->goaway_sessions_on_ip_change =
        kDefaultQuicGoAwaySessionsOnIpChange;
  }

  SetContextBuilderExperimentalOptions(context_builder, &session_params,
                                       quic_context->params());

  context_builder->set_http_network_session_params(session_params);
  context_builder->set_quic_context(std::move(quic_context));

  if (mock_cert_verifier)
    context_builder->SetCertVerifier(std::move(mock_cert_verifier));
  // Certificate Transparency is intentionally ignored in Cronet.
  // See //net/docs/certificate-transparency.md for more details.
  context_builder->set_ct_policy_enforcer(
      std::make_unique<net::DefaultCTPolicyEnforcer>());
  // TODO(mef): Use |config| to set cookies.
}

URLRequestContextConfigBuilder::URLRequestContextConfigBuilder() {}
URLRequestContextConfigBuilder::~URLRequestContextConfigBuilder() {}

std::unique_ptr<URLRequestContextConfig>
URLRequestContextConfigBuilder::Build() {
  return URLRequestContextConfig::CreateURLRequestContextConfig(
      enable_quic, quic_user_agent_id, enable_spdy, enable_brotli, http_cache,
      http_cache_max_size, load_disable_cache, storage_path, accept_language,
      user_agent, experimental_options, std::move(mock_cert_verifier),
      enable_network_quality_estimator,
      bypass_public_key_pinning_for_local_trust_anchors,
      network_thread_priority);
}

}  // namespace cronet
